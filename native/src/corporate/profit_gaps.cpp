#include "tdx/profit_gaps.hpp"
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

struct DisclosureType {
    const char* period;
    const char* kind;
    const char* label;
};

const std::map<std::string, DisclosureType>& disclosure_types() {
    static const std::map<std::string, DisclosureType> values{
        {"30000", {"q1", "forecast", "一季度预告"}},
        {"30300", {"q1", "forecast", "一季度预告"}},
        {"30003", {"q1", "forecast", "一季度预告"}},
        {"30303", {"q1", "forecast", "一季度预告"}},
        {"31200", {"q1", "forecast", "一季度预告"}},
        {"30012", {"q1", "forecast", "一季度预告"}},
        {"31212", {"q1", "forecast", "一季度预告"}},
        {"300", {"q1", "express", "一季度快报"}},
        {"303", {"q1", "express", "一季度快报"}},
        {"312", {"q1", "express", "一季度快报"}},
        {"3", {"q1", "report", "一季度报告"}},
        {"60000", {"half-year", "forecast", "半年度预告"}},
        {"60600", {"half-year", "forecast", "半年度预告"}},
        {"60006", {"half-year", "forecast", "半年度预告"}},
        {"60606", {"half-year", "forecast", "半年度预告"}},
        {"600", {"half-year", "express", "半年度快报"}},
        {"606", {"half-year", "express", "半年度快报"}},
        {"6", {"half-year", "report", "半年度报告"}},
        {"90000", {"q3", "forecast", "三季度预告"}},
        {"90006", {"q3", "forecast", "三季度预告"}},
        {"90009", {"q3", "forecast", "三季度预告"}},
        {"90909", {"q3", "forecast", "三季度预告"}},
        {"90900", {"q3", "forecast", "三季度预告"}},
        {"900", {"q3", "express", "三季度快报"}},
        {"909", {"q3", "express", "三季度快报"}},
        {"9", {"q3", "report", "三季度报告"}},
        {"120000", {"annual", "forecast", "年度预告"}},
        {"121200", {"annual", "forecast", "年度预告"}},
        {"120012", {"annual", "forecast", "年度预告"}},
        {"121212", {"annual", "forecast", "年度预告"}},
        {"1200", {"annual", "express", "年度快报"}},
        {"1212", {"annual", "express", "年度快报"}},
        {"12", {"annual", "report", "年度报告"}},
    };
    return values;
}

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

int market_id(std::string raw) {
    raw = lower_ascii(trim(std::move(raw)));
    if (raw == "sz" || raw == "0") return 0;
    if (raw == "sh" || raw == "1") return 1;
    if (raw == "bj" || raw == "2" || raw == "44") return 2;
    throw Error("profit-gap row has invalid TDX market: " + raw);
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

bool digits(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch >= '0' && ch <= '9';
    });
}

bool transient_error(const std::string& message) {
    return detail::is_transient_cloud_error(message);
}

std::string cache_key(const ProfitGapsQuery& query) {
    return lower_ascii(query.query) + '|' + lower_ascii(query.market) + '|' + query.code +
           '|' + std::to_string(query.minimum_safety) + '|' + std::to_string(query.limit);
}

Json disclosure_catalog() {
    Json result = Json::array();
    for (const auto& [code, value] : disclosure_types()) {
        Json item = Json::object();
        item["code"] = code;
        item["report_period"] = value.period;
        item["disclosure_kind"] = value.kind;
        item["name"] = value.label;
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace

Json normalize_profit_gap_rows(const Json& rows, const BlockData& blocks) {
    if (!rows.is_array()) throw Error("profit-gap rows must be an array");
    Json result = Json::array();
    std::set<std::string> identities;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object() || text(row, "Code").empty()) continue;
        const int market = market_id(text(row, "SetCode"));
        const auto code = text(row, "Code");
        if (code.size() != 6 || !digits(code))
            throw Error("profit-gap row has invalid security code: " + code);
        auto name = text(row, "Name");
        const auto known = blocks.securities.find({market, code});
        if (name.empty() && known != blocks.securities.end()) name = known->second.name;
        const auto event_date_raw = text(row, "Date");
        const auto disclosure_code = text(row, "Bglx");
        const auto identity = market_prefix(market) + code + ':' + event_date_raw + ':' +
                              disclosure_code;
        if (!identities.insert(identity).second)
            throw Error("duplicate profit-gap identity: " + identity);

        Json security = Json::object();
        security["market"] = market_name(market);
        security["market_id"] = market;
        security["code"] = code;
        security["security_id"] = market_prefix(market) + code;
        security["name"] = name;
        security["name_resolved"] = !name.empty();

        Json disclosure = Json::object();
        disclosure["code"] = disclosure_code;
        const auto definition = disclosure_types().find(disclosure_code);
        disclosure["report_period"] = definition == disclosure_types().end()
            ? Json("unknown") : Json(definition->second.period);
        disclosure["kind"] = definition == disclosure_types().end()
            ? Json("unknown") : Json(definition->second.kind);
        disclosure["name"] = definition == disclosure_types().end()
            ? Json("未知披露类型") : Json(definition->second.label);
        disclosure["recognized"] = definition != disclosure_types().end();

        Json item = Json::object();
        item["event_id"] = identity;
        item["security"] = std::move(security);
        item["event_date"] = display_date(event_date_raw);
        item["gap_change_pct"] = number_json(number(row, "Dchzf%"));
        item["safety_score"] = number_json(number(row, "Aqf"));
        item["disclosure"] = std::move(disclosure);
        result.push_back(std::move(item));
    }
    return result;
}

ProfitGapsService::ProfitGapsService(fs::path root, BlockData blocks)
    : root_(std::move(root)), blocks_(std::move(blocks)) {}

Json ProfitGapsService::query(const ProfitGapsQuery& input) {
    ProfitGapsQuery query = input;
    query.query = trim(query.query);
    query.market = lower_ascii(trim(query.market));
    query.code = trim(query.code);
    if (!query.market.empty() && query.market != "sz" && query.market != "sh" &&
        query.market != "bj")
        throw Error("market must be sz, sh, or bj");
    if (!query.code.empty() && (query.code.size() != 6 || !digits(query.code)))
        throw Error("code must contain exactly six digits");
    if (query.minimum_safety < 0 || query.minimum_safety > 100 || query.limit < 1 ||
        query.limit > 10000 || query.cache_ttl_seconds < 0 ||
        query.cache_ttl_seconds > 3600 || query.timeout_ms < 100 ||
        query.timeout_ms > 60000)
        throw Error("profit-gap query limits are invalid");

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
            upstream = execute_tqlex_config(root_, "500601", {}, {}, {},
                "gp_gz_lrdc.xml", {}, false, -1, 0, 100,
                cloud_endpoints::tqlex, query.timeout_ms);
            break;
        } catch (const Error& error) {
            if (!transient_error(error.what()) || attempt == 2) throw;
            std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
        }
    }

    const auto raw_rows = cloud_result_rows(upstream.at("response"));
    const auto normalized = normalize_profit_gap_rows(raw_rows, blocks_);
    Json records = Json::array();
    std::size_t below_safety = 0;
    std::map<std::string, std::size_t> by_kind, by_period;
    std::string earliest, latest;
    const auto wanted = lower_ascii(query.query);
    for (const auto& item : normalized.as_array()) {
        const auto score = item.at("safety_score").is_number()
            ? item.at("safety_score").as_number() : -1.0;
        if (score < query.minimum_safety) {
            ++below_safety;
            continue;
        }
        const auto& security = item.at("security");
        if (!query.market.empty() && security.at("market").as_string() != query.market) continue;
        if (!query.code.empty() && security.at("code").as_string() != query.code) continue;
        if (!wanted.empty() && lower_ascii(item.dump(-1)).find(wanted) == std::string::npos)
            continue;
        const auto kind = item.at("disclosure").at("kind").as_string();
        const auto period = item.at("disclosure").at("report_period").as_string();
        ++by_kind[kind];
        ++by_period[period];
        const auto date = item.at("event_date").as_string();
        if (earliest.empty() || date < earliest) earliest = date;
        if (latest.empty() || date > latest) latest = date;
        records.push_back(item);
    }
    std::sort(records.as_array().begin(), records.as_array().end(), [](const Json& left,
                                                                       const Json& right) {
        if (left.at("event_date").as_string() != right.at("event_date").as_string())
            return left.at("event_date").as_string() > right.at("event_date").as_string();
        const auto left_score = left.at("safety_score").is_number()
            ? left.at("safety_score").as_number() : -1.0;
        const auto right_score = right.at("safety_score").is_number()
            ? right.at("safety_score").as_number() : -1.0;
        return left_score > right_score;
    });
    const auto eligible = records.size();
    const bool truncated = records.size() > static_cast<std::size_t>(query.limit);
    if (truncated) records.as_array().resize(static_cast<std::size_t>(query.limit));

    Json counts = Json::object();
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_rows.size());
    counts["below_safety_threshold"] = static_cast<std::uint64_t>(below_safety);
    counts["matched_rows"] = static_cast<std::uint64_t>(eligible);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["truncated"] = truncated;
    Json kind_counts = Json::object();
    for (const auto& [name, count] : by_kind)
        kind_counts[name] = static_cast<std::uint64_t>(count);
    Json period_counts = Json::object();
    for (const auto& [name, count] : by_period)
        period_counts[name] = static_cast<std::uint64_t>(count);

    Json parameters = Json::object();
    parameters["query"] = query.query;
    parameters["market"] = query.market.empty() ? Json(nullptr) : Json(query.market);
    parameters["code"] = query.code.empty() ? Json(nullptr) : Json(query.code);
    parameters["minimum_safety"] = query.minimum_safety;
    parameters["client_default_reproduced"] = query.minimum_safety == 60;

    Json source = Json::object();
    source["transport"] = "TQLEX reqformat=2";
    source["entry"] = upstream.at("entry");
    source["request_id"] = "500601";
    source["source_file"] = upstream.at("source_file");
    source["module"] = "mod_caps.dll";
    source["upstream_data_date"] = "0";
    source["upstream_set_code"] = "3";

    Json cache = Json::object();
    cache["hit"] = false;
    cache["stale"] = false;
    cache["age_seconds"] = 0;
    cache["ttl_seconds"] = query.cache_ttl_seconds;

    Json result = Json::object();
    result["schema"] = "tdx-profit-gaps-native-v1";
    result["availability"] = "live";
    result["generated_at"] = now_text();
    result["definition"] = "业绩披露后出现明显价格断层的客户端模型结果，不等同于投资建议";
    result["parameters"] = std::move(parameters);
    result["counts"] = std::move(counts);
    result["counts_by_disclosure_kind"] = std::move(kind_counts);
    result["counts_by_report_period"] = std::move(period_counts);
    result["earliest_event_date"] = earliest.empty() ? Json(nullptr) : Json(earliest);
    result["latest_event_date"] = latest.empty() ? Json(nullptr) : Json(latest);
    result["disclosure_type_catalog"] = disclosure_catalog();
    result["records"] = std::move(records);
    result["source"] = std::move(source);
    result["cache"] = std::move(cache);
    result["raw_response_retained"] = false;
    cache_[key] = {result, std::time(nullptr)};
    return result;
}

int command_market_profit_gaps(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market profit-gaps [options]\n\n"
            "  --query TEXT                   Search normalized event fields\n"
            "  --market sz|sh|bj --code CODE Filter one market/security\n"
            "  --minimum-safety N             Default 60, matching the TDX client filter\n"
            "  --root PATH --limit N --refresh --cache-ttl-seconds N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    ProfitGapsQuery query;
    query.query = args.take_option("--query");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.minimum_safety = integer(args.take_option("--minimum-safety", "60"),
                                   "--minimum-safety", 0, 100);
    query.refresh = args.take_flag("--refresh");
    query.limit = integer(args.take_option("--limit", "5000"), "--limit", 1, 10000);
    query.cache_ttl_seconds = integer(args.take_option("--cache-ttl-seconds", "300"),
                                      "--cache-ttl-seconds", 0, 3600);
    query.timeout_ms = integer(args.take_option("--timeout-ms", "15000"),
                               "--timeout-ms", 100, 60000);
    const auto root_text = args.take_option("--root");
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-profit-gaps.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    ProfitGapsService service(root, load_blocks(root, {}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "received " << document.at("counts").at("returned").as_number()
              << " profit-gap rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
