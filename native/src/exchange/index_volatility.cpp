#include "tdx/index_volatility.hpp"
#include "tdx/time.hpp"

#include "tdx/cloud_resilience.hpp"
#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"
#include "tdx/tqlex.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace tdx {
namespace {

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

const Json* field(const Json& row, std::string_view key) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(key);
    if (found != row.as_object().end()) return &found->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, value] : row.as_object())
        if (lower_ascii(name) == wanted) return &value;
    return nullptr;
}

std::string text(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return {};
    if (value->is_string()) return trim(value->as_string());
    if (value->is_number()) {
        std::ostringstream output;
        output << std::setprecision(15) << value->as_number();
        return output.str();
    }
    return {};
}

std::optional<double> number(const Json& row, std::string_view key) {
    const auto* value = field(row, key);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string()) return std::nullopt;
    auto raw = trim(value->as_string());
    raw.erase(std::remove(raw.begin(), raw.end(), ','), raw.end());
    if (!raw.empty() && raw.back() == '%') raw.pop_back();
    if (raw.empty() || raw == "-" || raw == "--") return std::nullopt;
    try {
        std::size_t used = 0;
        const double result = std::stod(raw, &used);
        if (used == raw.size() && std::isfinite(result)) return result;
    } catch (...) {}
    return std::nullopt;
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch >= '0' && ch <= '9';
    });
}

int integer(const std::string& raw, std::string_view name, int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(raw, &used);
        if (used != raw.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

int market_id(std::string raw) {
    raw = lower_ascii(trim(std::move(raw)));
    if (raw == "sz" || raw == "0") return 0;
    if (raw == "sh" || raw == "1") return 1;
    if (raw == "bj" || raw == "2" || raw == "44") return 2;
    throw Error("market must be sz, sh, bj, 0, 1, 2, or 44");
}

std::string market_name(int market) {
    return market == 0 ? "sz" : market == 1 ? "sh" : "bj";
}

std::string market_prefix(int market) {
    return market == 0 ? "SZ" : market == 1 ? "SH" : "BJ";
}

Json index_document(int market, const std::string& code, const BlockData& blocks) {
    auto name = std::string{};
    const auto known = blocks.securities.find({market, code});
    if (known != blocks.securities.end()) name = known->second.name;
    Json result = Json::object();
    result["entity_type"] = "index";
    result["market"] = market_name(market);
    result["market_id"] = market;
    result["code"] = code;
    result["index_id"] = market_prefix(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}

std::string compact_date(std::string value, std::string_view name) {
    value = trim(std::move(value));
    if (value.size() == 10 && value[4] == '-' && value[7] == '-')
        value = value.substr(0, 4) + value.substr(5, 2) + value.substr(8, 2);
    if (value.size() != 8 || !digits(value))
        throw Error(std::string(name) + " must be YYYYMMDD or YYYY-MM-DD");
    std::tm date{};
    date.tm_year = std::stoi(value.substr(0, 4)) - 1900;
    date.tm_mon = std::stoi(value.substr(4, 2)) - 1;
    date.tm_mday = std::stoi(value.substr(6, 2));
    date.tm_hour = 12;
    const int year = date.tm_year, month = date.tm_mon, day = date.tm_mday;
    if (std::mktime(&date) == -1 || date.tm_year != year || date.tm_mon != month ||
        date.tm_mday != day)
        throw Error(std::string(name) + " is outside the supported calendar range");
    return value;
}

std::string display_date(const std::string& value) {
    return value.size() == 8
        ? value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2)
        : value;
}

std::pair<std::string, std::string> default_dates() {
    auto now = std::time(nullptr);
    std::tm end{};
#ifdef _WIN32
    localtime_s(&end, &now);
#else
    localtime_r(&now, &end);
#endif
    std::tm start = end;
    start.tm_year -= 2;
    if (start.tm_mon == 1 && start.tm_mday == 29) start.tm_mday = 28;
    std::ostringstream left, right;
    left << std::put_time(&start, "%Y%m%d");
    right << std::put_time(&end, "%Y%m%d");
    return {left.str(), right.str()};
}

bool transient_error(const std::string& message) {
    return detail::is_transient_cloud_error(message);
}

std::string cache_key(const IndexVolatilityQuery& query) {
    return query.view + '|' + lower_ascii(query.query) + '|' + query.market + '|' +
           query.code + '|' + query.start_date + '|' + query.end_date + '|' +
           std::to_string(query.window_days) + '|' + std::to_string(query.limit);
}

bool searchable(const Json& record, const std::string& raw_query) {
    if (raw_query.empty()) return true;
    return lower_ascii(record.dump(-1)).find(lower_ascii(raw_query)) != std::string::npos;
}

Json source_document(const Json& upstream, const char* request_id) {
    Json result = Json::object();
    result["transport"] = "TQLEX reqformat=2";
    result["entry"] = upstream.at("entry");
    result["request_id"] = request_id;
    result["source_file"] = upstream.at("source_file");
    result["module"] = "mod_indexCorr64.dll";
    return result;
}

}  // namespace

Json normalize_index_volatility_catalog_rows(const Json& rows, int window_days,
                                             const BlockData& blocks) {
    if (!rows.is_array()) throw Error("index-volatility catalog rows must be an array");
    Json result = Json::array();
    std::set<std::string> identities;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object() || text(row, "Code").empty()) continue;
        const int market = market_id(text(row, "Market"));
        const auto code = text(row, "Code");
        if (code.size() != 6 || !digits(code))
            throw Error("index-volatility row has invalid index code: " + code);
        const auto identity = market_prefix(market) + code;
        if (!identities.insert(identity).second)
            throw Error("duplicate index-volatility identity: " + identity);
        Json means = Json::object();
        means["two_years"] = number_json(number(row, "MeanStdOfLastTwoYears"));
        means["one_year"] = number_json(number(row, "MeanStdOfLastOneYear"));
        means["half_year"] = number_json(number(row, "MeanStdOfLastHalfYear"));
        means["quarter"] = number_json(number(row, "MeanStdOfLastQuarter"));
        Json percentiles = Json::object();
        percentiles["within_two_years"] = number_json(number(row, "QuantileOfMQO2Y"));
        percentiles["within_one_year"] = number_json(number(row, "QuantileOfMQO1Y"));
        percentiles["within_half_year"] = number_json(number(row, "QuantileOfMQOhY"));
        Json item = Json::object();
        item["index"] = index_document(market, code, blocks);
        item["window_days"] = window_days;
        item["mean_realized_volatility_pct"] = std::move(means);
        item["current_quarter_mean_percentile_pct"] = std::move(percentiles);
        result.push_back(std::move(item));
    }
    return result;
}

Json normalize_index_volatility_history_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("index-volatility history rows must be an array");
    Json result = Json::array();
    std::set<std::string> dates;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object()) continue;
        const auto raw_date = text(row, "Date");
        if (raw_date.empty()) continue;
        const auto date = display_date(raw_date);
        if (!dates.insert(date).second)
            throw Error("duplicate index-volatility history date: " + date);
        Json point = Json::object();
        point["date"] = date;
        point["realized_volatility_pct"] = number_json(number(row, "RealizedVolatility"));
        result.push_back(std::move(point));
    }
    std::sort(result.as_array().begin(), result.as_array().end(), [](const Json& left,
                                                                     const Json& right) {
        return left.at("date").as_string() < right.at("date").as_string();
    });
    return result;
}

IndexVolatilityService::IndexVolatilityService(fs::path root, BlockData blocks)
    : root_(std::move(root)), blocks_(std::move(blocks)) {}

Json IndexVolatilityService::query(const IndexVolatilityQuery& input) {
    IndexVolatilityQuery query = input;
    query.view = lower_ascii(trim(query.view));
    query.query = trim(query.query);
    query.market = lower_ascii(trim(query.market));
    query.code = trim(query.code);
    if (query.view != "catalog" && query.view != "history" && query.view != "security")
        throw Error("view must be catalog, history, or security");
    if (!query.market.empty()) query.market = market_name(market_id(query.market));
    if (!query.code.empty() && (query.code.size() != 6 || !digits(query.code)))
        throw Error("code must contain exactly six digits");
    if ((query.view == "history" || query.view == "security") &&
        (query.market.empty() || query.code.empty()))
        throw Error("history and security views require market and code");
    if (query.window_days < 1 || query.window_days > 250 || query.limit < 1 ||
        query.limit > 20000 || query.cache_ttl_seconds < 0 ||
        query.cache_ttl_seconds > 3600 || query.timeout_ms < 100 ||
        query.timeout_ms > 60000)
        throw Error("index-volatility query limits are invalid");
    if (query.view != "catalog") {
        const auto defaults = default_dates();
        query.start_date = compact_date(query.start_date.empty() ? defaults.first : query.start_date,
                                        "start date");
        query.end_date = compact_date(query.end_date.empty() ? defaults.second : query.end_date,
                                      "end date");
        if (query.start_date > query.end_date)
            throw Error("start date must not be later than end date");
    }

    const auto key = cache_key(query);
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(key);
    if (!query.refresh && cached != cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
        if (age < query.cache_ttl_seconds) {
            auto document = cached->second.document;
            document["cache"]["hit"] = true;
            document["cache"]["age_seconds"] = age;
            return document;
        }
    }

    auto fetch = [&](const std::string& request_id,
                     const std::map<std::string, std::string>& replacements) {
        Json upstream;
        for (int attempt = 0; attempt < 3; ++attempt) {
            try {
                upstream = execute_tqlex_config(root_, request_id, replacements, {}, {},
                    "zs_zsbdl.xml", {}, false, -1, 0, 100,
                    cloud_endpoints::tqlex, query.timeout_ms);
                return upstream;
            } catch (const Error& error) {
                if (!transient_error(error.what()) || attempt == 2) throw;
                std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
            }
        }
        return upstream;
    };

    Json catalog_upstream = Json(nullptr), history_upstream = Json(nullptr);
    Json catalog_records = Json::array(), history_records = Json::array();
    if (query.view == "catalog" || query.view == "security") {
        catalog_upstream = fetch("200004", {{"Window", std::to_string(query.window_days)}});
        catalog_records = normalize_index_volatility_catalog_rows(
            cloud_result_rows(catalog_upstream.at("response")), query.window_days, blocks_);
    }
    if (query.view == "history" || query.view == "security") {
        const int market = market_id(query.market);
        history_upstream = fetch("200003", {
            {"StartDate", query.start_date}, {"EndDate", query.end_date},
            {"Code", query.code}, {"Market", std::to_string(market)},
            {"Window", std::to_string(query.window_days)}});
        history_records = normalize_index_volatility_history_rows(
            cloud_result_rows(history_upstream.at("response")));
    }

    Json records = Json::array();
    std::size_t raw_count = 0;
    bool selected_catalog_found = false;
    if (query.view == "catalog") {
        raw_count = catalog_records.size();
        for (const auto& record : catalog_records.as_array()) {
            const auto& index = record.at("index");
            if (!query.market.empty() && index.at("market").as_string() != query.market) continue;
            if (!query.code.empty() && index.at("code").as_string() != query.code) continue;
            if (!searchable(record, query.query)) continue;
            records.push_back(record);
        }
    } else if (query.view == "history") {
        raw_count = history_records.size();
        records = history_records;
    } else {
        raw_count = history_records.size();
        Json selected = Json::object();
        const int selected_market = market_id(query.market);
        selected["index"] = index_document(selected_market, query.code, blocks_);
        selected["statistics"] = Json(nullptr);
        for (const auto& record : catalog_records.as_array()) {
            const auto& index = record.at("index");
            if (index.at("market").as_string() == query.market &&
                index.at("code").as_string() == query.code) {
                selected["index"] = index;
                Json statistics = Json::object();
                statistics["window_days"] = record.at("window_days");
                statistics["mean_realized_volatility_pct"] =
                    record.at("mean_realized_volatility_pct");
                statistics["current_quarter_mean_percentile_pct"] =
                    record.at("current_quarter_mean_percentile_pct");
                selected["statistics"] = std::move(statistics);
                selected_catalog_found = true;
                break;
            }
        }
        selected["history"] = history_records;
        selected["latest_point"] = history_records.size()
            ? history_records.as_array().back() : Json(nullptr);
        records.push_back(std::move(selected));
    }

    const bool data_available = query.view == "security"
        ? selected_catalog_found || history_records.size() != 0
        : records.size() != 0;
    const auto matched = query.view == "security"
        ? static_cast<std::size_t>(data_available ? 1 : 0) : records.size();
    const bool truncated = records.size() > static_cast<std::size_t>(query.limit);
    if (truncated) records.as_array().resize(static_cast<std::size_t>(query.limit));
    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_count);
    counts["matched_rows"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["truncated"] = truncated;
    if (query.view == "security") {
        counts["catalog_rows"] = static_cast<std::uint64_t>(catalog_records.size());
        counts["history_points"] = static_cast<std::uint64_t>(history_records.size());
        counts["selected_catalog_found"] = selected_catalog_found;
        counts["selected_index_found"] = data_available;
    }

    Json parameters = Json::object();
    parameters["view"] = query.view;
    parameters["query"] = query.query;
    parameters["market"] = query.market.empty() ? Json(nullptr) : Json(query.market);
    parameters["code"] = query.code.empty() ? Json(nullptr) : Json(query.code);
    parameters["window_days"] = query.window_days;
    parameters["start_date"] = query.start_date.empty()
        ? Json(nullptr) : Json(display_date(query.start_date));
    parameters["end_date"] = query.end_date.empty()
        ? Json(nullptr) : Json(display_date(query.end_date));

    Json sources = Json::array();
    if (!catalog_upstream.is_null()) sources.push_back(source_document(catalog_upstream, "200004"));
    if (!history_upstream.is_null()) sources.push_back(source_document(history_upstream, "200003"));
    Json cache = Json::object();
    cache["hit"] = false;
    cache["stale"] = false;
    cache["age_seconds"] = 0;
    cache["ttl_seconds"] = query.cache_ttl_seconds;

    Json result = Json::object();
    result["schema"] = "tdx-index-volatility-native-v1";
    result["availability"] = data_available ? "live" : "empty";
    result["generated_at"] = now_text();
    result["view"] = query.view;
    result["available_views"] = Json::parse("[\"catalog\",\"history\",\"security\"]");
    result["unit"] = "percentage-points";
    result["methodology"] =
        "TDX server-computed rolling realized volatility; Window is the rolling trading-day count";
    result["interpretation_boundary"] =
        "historical volatility level and percentile are descriptive statistics, not an investment signal";
    result["parameters"] = std::move(parameters);
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["sources"] = std::move(sources);
    result["cache"] = std::move(cache);
    result["raw_response_retained"] = false;
    cache_[key] = {result, std::time(nullptr)};
    return result;
}

int command_market_index_volatility(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market index-volatility [options]\n\n"
            "  --view catalog|history|security Default catalog\n"
            "  --query TEXT                   Search catalog fields\n"
            "  --market sz|sh|bj --code CODE Required for history/security\n"
            "  --start DATE --end DATE        Default latest two calendar years\n"
            "  --window N                     Rolling trading-day window, default 5\n"
            "  --root PATH --limit N --refresh --cache-ttl-seconds N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    IndexVolatilityQuery query;
    query.view = args.take_option("--view", "catalog");
    query.query = args.take_option("--query");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.start_date = args.take_option("--start");
    query.end_date = args.take_option("--end");
    query.window_days = integer(args.take_option("--window", "5"), "--window", 1, 250);
    query.refresh = args.take_flag("--refresh");
    query.limit = integer(args.take_option("--limit", "10000"), "--limit", 1, 20000);
    query.cache_ttl_seconds = integer(args.take_option("--cache-ttl-seconds", "300"),
                                      "--cache-ttl-seconds", 0, 3600);
    query.timeout_ms = integer(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-index-volatility.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    IndexVolatilityService service(root, load_blocks(root, {}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("counts").at("returned").as_number()
              << " index-volatility rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
