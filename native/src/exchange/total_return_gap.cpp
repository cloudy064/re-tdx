#include "tdx/total_return_gap.hpp"
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

int current_year() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return local.tm_year + 1900;
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
    throw Error("market must be sz, sh, or bj");
}

std::string market_name(int market) {
    return market == 0 ? "sz" : market == 1 ? "sh" : "bj";
}

std::string market_prefix(int market) {
    return market == 0 ? "SZ" : market == 1 ? "SH" : "BJ";
}

std::string display_date(const std::string& value) {
    return value.size() == 8
        ? value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2)
        : value;
}

Json index_identity(const Json& row, const char* code_key, const char* market_key,
                    const char* name_key, const char* entity_type) {
    const auto code = text(row, code_key);
    const auto raw_market = text(row, market_key);
    int market = -1;
    try {
        std::size_t used = 0;
        market = std::stoi(raw_market, &used);
        if (used != raw_market.size()) market = -1;
    } catch (...) {}
    Json result = Json::object();
    result["entity_type"] = entity_type;
    result["market_id"] = market >= 0 ? Json(market) : Json(nullptr);
    if (market == 0 || market == 1 || market == 2 || market == 44) {
        const int normalized = market == 44 ? 2 : market;
        result["market"] = market_name(normalized);
        result["index_id"] = market_prefix(normalized) + code;
    } else {
        result["market"] = raw_market.empty() ? Json(nullptr)
            : Json("tdx-internal-" + raw_market);
        result["index_id"] = raw_market + ':' + code;
    }
    result["code"] = code;
    result["name"] = text(row, name_key);
    return result;
}

Json total_return_index(const Json& row) {
    return index_identity(row, "wholeCode", "wholeSet", "wholeAbbre",
                          "total-return-index");
}

bool transient_error(const std::string& message) {
    return detail::is_transient_cloud_error(message);
}

std::string cache_key(const TotalReturnGapQuery& query) {
    return lower_ascii(query.query) + '|' + query.market + '|' + query.code + '|' +
           std::to_string(query.year) + '|' + std::to_string(query.month) + '|' +
           std::to_string(query.limit);
}

}  // namespace

Json normalize_total_return_gap_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("total-return-gap rows must be an array");
    Json result = Json::array();
    std::set<std::string> identities;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object() || text(row, "basicCode").empty()) continue;
        Json price_index = index_identity(row, "basicCode", "basicSet", "basicAbbre",
                                          "price-index");
        Json total_index = total_return_index(row);
        const auto identity = price_index.at("index_id").as_string() + "->" +
                              total_index.at("index_id").as_string();
        if (!identities.insert(identity).second)
            throw Error("duplicate total-return index pair: " + identity);
        Json price_performance = Json::object();
        price_performance["start_value"] = number_json(number(row, "basicPricePreZone"));
        price_performance["end_value"] = number_json(number(row, "basicPriceZone"));
        price_performance["return_pct"] = number_json(number(row, "basicZDFZone"));
        Json total_performance = Json::object();
        total_performance["start_value"] = number_json(number(row, "wholePricePreZone"));
        total_performance["end_value"] = number_json(number(row, "wholePriceZone"));
        total_performance["return_pct"] = number_json(number(row, "wholeZDFZone"));
        Json item = Json::object();
        item["pair_id"] = identity;
        item["start_date"] = display_date(text(row, "startDate"));
        item["end_date"] = display_date(text(row, "endDate"));
        item["price_index"] = std::move(price_index);
        item["total_return_index"] = std::move(total_index);
        item["price_index_performance"] = std::move(price_performance);
        item["total_return_index_performance"] = std::move(total_performance);
        item["total_return_advantage_pct"] = number_json(number(row, "zoneZDFGap"));
        result.push_back(std::move(item));
    }
    return result;
}

TotalReturnGapService::TotalReturnGapService(fs::path root) : root_(std::move(root)) {}

Json TotalReturnGapService::query(const TotalReturnGapQuery& input) {
    TotalReturnGapQuery query = input;
    query.query = trim(query.query);
    query.market = lower_ascii(trim(query.market));
    query.code = trim(query.code);
    if (!query.market.empty()) query.market = market_name(market_id(query.market));
    if (query.year == 0) query.year = current_year();
    if (query.year < 1999 || query.year > 2049 || query.month < 0 || query.month > 12 ||
        query.limit < 1 || query.limit > 5000 || query.cache_ttl_seconds < 0 ||
        query.cache_ttl_seconds > 3600 || query.timeout_ms < 100 ||
        query.timeout_ms > 60000)
        throw Error("total-return-gap query limits are invalid");

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

    Json upstream;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            upstream = execute_tqlex_config(root_, "200770",
                {{"N", std::to_string(query.year)}, {"M", std::to_string(query.month)}},
                {}, {}, "sc_qsyzs.xml", {}, false, -1, 0, 100,
                cloud_endpoints::tqlex, query.timeout_ms);
            break;
        } catch (const Error& error) {
            if (!transient_error(error.what()) || attempt == 2) throw;
            std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
        }
    }
    const auto raw_rows = cloud_result_rows(upstream.at("response"));
    const auto normalized = normalize_total_return_gap_rows(raw_rows);
    std::vector<const Json*> matched_records;
    const auto wanted = lower_ascii(query.query);
    for (const auto& record : normalized.as_array()) {
        const auto& index = record.at("price_index");
        if (!query.market.empty() && index.at("market").as_string() != query.market) continue;
        if (!query.code.empty() && index.at("code").as_string() != query.code) continue;
        if (!wanted.empty() && lower_ascii(record.dump(-1)).find(wanted) == std::string::npos)
            continue;
        matched_records.push_back(&record);
    }
    std::sort(matched_records.begin(), matched_records.end(), [](const Json* left,
                                                                 const Json* right) {
        const auto* left_gap = &left->at("total_return_advantage_pct");
        const auto* right_gap = &right->at("total_return_advantage_pct");
        const double a = left_gap->is_number() ? left_gap->as_number() : -1e300;
        const double b = right_gap->is_number() ? right_gap->as_number() : -1e300;
        return a > b;
    });
    const auto matched = matched_records.size();
    const bool truncated = matched_records.size() > static_cast<std::size_t>(query.limit);
    Json records = Json::array();
    const auto returned = std::min(matched_records.size(), static_cast<std::size_t>(query.limit));
    for (std::size_t index = 0; index < returned; ++index)
        records.push_back(*matched_records[index]);

    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_rows.size());
    counts["matched_rows"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["truncated"] = truncated;
    Json parameters = Json::object();
    parameters["year"] = query.year;
    parameters["month"] = query.month;
    parameters["month_scope"] = query.month == 0 ? "whole-year-to-date" : "selected-month";
    parameters["query"] = query.query;
    parameters["market"] = query.market.empty() ? Json(nullptr) : Json(query.market);
    parameters["code"] = query.code.empty() ? Json(nullptr) : Json(query.code);
    Json source = Json::object();
    source["transport"] = "TQLEX reqformat=2";
    source["entry"] = upstream.at("entry");
    source["request_id"] = "200770";
    source["source_file"] = upstream.at("source_file");
    source["module"] = "mod_copilot.dll";
    Json cache = Json::object();
    cache["hit"] = false;
    cache["stale"] = false;
    cache["age_seconds"] = 0;
    cache["ttl_seconds"] = query.cache_ttl_seconds;
    Json result = Json::object();
    result["schema"] = "tdx-total-return-gap-native-v1";
    result["availability"] = records.size() ? "live" : "empty";
    result["generated_at"] = now_text();
    result["definition"] =
        "total-return index period return minus corresponding price-index period return";
    result["unit"] = "percentage-points";
    result["parameters"] = std::move(parameters);
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["source"] = std::move(source);
    result["cache"] = std::move(cache);
    result["daily_gap_available"] = false;
    result["daily_gap_boundary"] =
        "the client computes today's gap from live syscols; ReqId 200770 only returns the period gap";
    result["raw_response_retained"] = false;
    cache_[key] = {result, std::time(nullptr)};
    return result;
}

int command_market_total_return_gap(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market total-return-gap [options]\n\n"
            "  --year YYYY --month 0..12      Month 0 means whole year to date\n"
            "  --query TEXT                   Search either side of the index pair\n"
            "  --market sz|sh|bj --code CODE Filter the price index\n"
            "  --root PATH --limit N --refresh --cache-ttl-seconds N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    TotalReturnGapQuery query;
    query.query = args.take_option("--query");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.year = integer(args.take_option("--year", std::to_string(current_year())),
                         "--year", 1999, 2049);
    query.month = integer(args.take_option("--month", "0"), "--month", 0, 12);
    query.refresh = args.take_flag("--refresh");
    query.limit = integer(args.take_option("--limit", "1000"), "--limit", 1, 5000);
    query.cache_ttl_seconds = integer(args.take_option("--cache-ttl-seconds", "300"),
                                      "--cache-ttl-seconds", 0, 3600);
    query.timeout_ms = integer(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-total-return-gap.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    TotalReturnGapService service(root);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("counts").at("returned").as_number()
              << " total-return index pairs -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
