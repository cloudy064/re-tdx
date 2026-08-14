#include "tdx/benchmark_analysis.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct Stage {
    const char* id; const char* start_date; const char* end_date;
    double start_anchor; double end_anchor;
};

constexpr Stage kStages[]{
    {"201", "20090804", "20130625", 3478.04, 1949.65},
    {"202", "20130625", "20150612", 1949.65, 5178.19},
    {"203", "20150612", "20160127", 5178.19, 2638.30},
    {"204", "20160127", "20180129", 2638.30, 3587.03},
    {"208", "20180129", "20190104", 3587.03, 2440.91},
    {"209", "20190104", "20190408", 2440.91, 3288.45},
    {"210", "20190408", "20200319", 3288.45, 2646.80},
    {"211", "20200319", "20210218", 2646.80, 3731.69},
    {"212", "20210218", "20220427", 3731.69, 2863.65},
    {"213", "20220427", "20220705", 2863.65, 3424.84},
    {"214", "20220705", "20240205", 3424.84, 2635.09},
    {"215", "20240205", "20240918", 2635.09, 2689.70},
    {"216", "20240918", "", 2689.70, 0.0}
};

const Json* field(const Json& row, std::string_view name) {
    if (!row.is_object()) return nullptr;
    const auto found = row.as_object().find(name);
    return found == row.as_object().end() ? nullptr : &found->second;
}
std::string text_value(const Json& row, std::string_view name) {
    const auto* value = field(row, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}
std::optional<double> number_value(const Json& row, std::string_view name) {
    const auto value = text_value(row, name);
    if (value.empty()) return std::nullopt;
    try { std::size_t used = 0; const auto parsed = std::stod(value, &used);
        if (used == value.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}
Json number(const Json& row, std::string_view name) {
    const auto value = number_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}
Json difference(const Json& row, std::string_view left, std::string_view right) {
    const auto a = number_value(row, left), b = number_value(row, right);
    return a && b ? Json(*a - *b) : Json(nullptr);
}
bool digits(const std::string& value, std::size_t count) {
    return value.size() == count && std::all_of(value.begin(), value.end(),
        [](char ch) { return ch >= '0' && ch <= '9'; });
}
int integer_value(const Json& row, std::string_view name) {
    const auto value = text_value(row, name);
    try { std::size_t used = 0; const auto parsed = std::stoi(value, &used);
        return used == value.size() ? parsed : -1;
    } catch (...) { return -1; }
}
int parsed_market(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    return -1;
}
std::string market_name(int market) {
    if (market == 0) return "sz";
    if (market == 1) return "sh";
    if (market == 2 || market == 44) return "bj";
    return "m" + std::to_string(market);
}
std::string market_prefix(int market) {
    if (market == 0) return "SZ";
    if (market == 1) return "SH";
    if (market == 2 || market == 44) return "BJ";
    return "M" + std::to_string(market);
}
Json security_document(int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (market == 44) market = 2;
    if (market < 0 || !digits(code, 6)) return Json(nullptr);
    std::string name;
    const auto found = securities.find({market, code});
    if (found != securities.end()) name = found->second.name;
    Json result = Json::object(); result["market_id"] = market;
    result["market"] = market_name(market); result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name; result["name_resolved"] = !name.empty();
    return result;
}
std::string variant(const std::string& resource) {
    const auto begin = resource.find("func_jzfx");
    if (begin == std::string::npos) return {};
    const auto value_begin = begin + 9;
    const auto end = resource.find('_', value_begin);
    return resource.substr(value_begin, end - value_begin);
}
const Stage* stage_for(std::string_view id) {
    auto suffix = std::string(id);
    if (suffix.size() == 3 && suffix.front() == '3') suffix.front() = '2';
    for (const auto& stage : kStages) if (suffix == stage.id) return &stage;
    return nullptr;
}
std::string kind_for(std::string_view id) {
    if (id == "401") return "suspensions";
    if (id == "501") return "new-stocks";
    if (id.size() == 3 && id.front() == '2') return "stocks";
    if (id.size() == 3 && id.front() == '3') return "industries";
    throw Error("unknown benchmark-analysis variant: " + std::string(id));
}
std::string label_for(const std::string& kind) {
    if (kind == "stocks") return "个股牛熊阶段";
    if (kind == "industries") return "行业牛熊阶段";
    if (kind == "suspensions") return "停复牌表现";
    return "次新股表现";
}
std::vector<std::string> resources() {
    std::vector<std::string> result;
    for (const auto& stage : kStages) {
        result.push_back("list/func_jzfx" + std::string(stage.id) + "_1.jsn");
        auto industry = std::string(stage.id); industry.front() = '3';
        result.push_back("list/func_jzfx" + industry + "_1.jsn");
    }
    result.push_back("list/func_jzfx401_1.jsn");
    result.push_back("list/func_jzfx501_1.jsn");
    return result;
}
std::vector<std::string> resources_for(const BenchmarkAnalysisQuery& options) {
    if (options.view == "suspensions") return {"list/func_jzfx401_1.jsn"};
    if (options.view == "new-stocks") return {"list/func_jzfx501_1.jsn"};
    if (!options.stage.empty()) {
        std::vector<std::string> result;
        if (options.view == "stocks" || options.view == "all")
            result.push_back("list/func_jzfx" + options.stage + "_1.jsn");
        if (options.view == "industries" || options.view == "all") {
            auto industry = options.stage; industry.front() = '3';
            result.push_back("list/func_jzfx" + industry + "_1.jsn");
        }
        return result;
    }
    if (options.view == "stocks" || options.view == "industries") {
        std::vector<std::string> result;
        for (const auto& stage : kStages) {
            auto id = std::string(stage.id);
            if (options.view == "industries") id.front() = '3';
            result.push_back("list/func_jzfx" + id + "_1.jsn");
        }
        return result;
    }
    return resources();
}
std::string now_text() {
    const auto now = std::time(nullptr); std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output; output << local_timestamp_text(local);
    return output.str();
}
fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}
int bounded(const std::string& value, std::string_view name, int low, int high) {
    try { std::size_t used = 0; const auto parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < low || parsed > high) throw std::invalid_argument("range");
        return parsed;
    } catch (...) { throw Error(std::string(name) + " must be in " + std::to_string(low) + ".." + std::to_string(high)); }
}
Json load_local(const fs::path& root, const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path)) throw Error("local benchmark resource is unavailable: " + path_utf8(path));
    const auto tables = load_jsn_tables(path); Json rows = Json::array();
    for (std::size_t group = 0; group < tables.size(); ++group)
        for (const auto& cells : tables[group].rows) {
            Json row = Json::object();
            for (std::size_t column = 0; column < tables[group].headers.size(); ++column)
                row[tables[group].headers[column]] = cells[column];
            row["_group"] = static_cast<std::uint64_t>(group); rows.push_back(std::move(row));
        }
    Json result = Json::object(); result["resource"] = resource;
    result["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    result["row_count"] = static_cast<std::uint64_t>(rows.size());
    result["endpoint"] = "local-jsn:" + path_utf8(path); result["rows"] = std::move(rows); return result;
}
const Json& document_for(const Json& documents, std::string_view resource) {
    for (const auto& document : documents.as_array()) if (text_value(document, "resource") == resource) return document;
    throw Error("missing benchmark-analysis resource: " + std::string(resource));
}
Json source_summary(const Json& document, std::size_t count) {
    Json result = Json::object(); for (const auto* name : {"resource", "size", "row_count", "endpoint"}) result[name] = document.at(name);
    result["normalized_row_count"] = static_cast<std::uint64_t>(count); return result;
}
}  // namespace

Json normalize_benchmark_analysis_rows(const std::string& resource,
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("benchmark-analysis rows must be an array");
    const auto id = variant(resource); const auto kind = kind_for(id);
    const auto* stage = stage_for(id); Json result = Json::array(); std::size_t index = 0;
    for (const auto& row : rows.as_array()) {
        auto security = security_document(integer_value(row, "$SC"), text_value(row, "$ZQDM"), securities);
        if (security.is_null()) continue;
        Json item = Json::object(); item["kind"] = kind; item["kind_label"] = label_for(kind);
        item["security"] = std::move(security); item["source_resource"] = resource; item["raw"] = row;
        std::string date;
        if (stage) {
            item["stage_id"] = stage->id; item["stage_start_date"] = stage->start_date;
            item["stage_end_date"] = stage->end_date; item["stage_open"] = !*stage->end_date;
            item["stage_start_anchor"] = stage->start_anchor;
            item["stage_end_anchor"] = *stage->end_date ? Json(stage->end_anchor) : Json(nullptr);
            item["market_start_price"] = number(row, "price3"); item["market_end_price"] = number(row, "price4");
            item["industry_start_price"] = number(row, "price5"); item["industry_end_price"] = number(row, "price6");
            item["market_return_pct"] = number(row, "dpzaf"); item["industry_return_pct"] = number(row, "hyzaf");
            item["excess_market_pct"] = number(row, "zaf2"); item["recent_1m_pct"] = number(row, "zaf5");
            item["recent_3m_pct"] = number(row, "zaf6");
            if (kind == "stocks") {
                item["security_start_price"] = number(row, "price1"); item["security_end_price"] = number(row, "price2");
                item["security_return_pct"] = number(row, "zaf1"); item["excess_industry_pct"] = number(row, "zaf3");
            } else item["year_to_date_pct"] = number(row, "zaf7");
            date = *stage->end_date ? stage->end_date : stage->start_date;
        } else if (kind == "suspensions") {
            item["suspension_date"] = text_value(row, "tpdate"); item["resumption_date"] = text_value(row, "fpdate");
            item["suspension_trading_days"] = number(row, "tpts"); item["reason"] = text_value(row, "yy");
            item["industry_return_during_suspension_pct"] = number(row, "zaf1");
            item["market_return_during_suspension_pct"] = number(row, "zaf2");
            item["security_pre_suspension_close"] = number(row, "price3");
            date = text_value(row, "fpdate");
        } else {
            item["issue_price"] = number(row, "price1");
            item["return_3m_pct"] = number(row, "zaf1"); item["market_return_3m_pct"] = number(row, "zaf2");
            item["industry_return_3m_pct"] = number(row, "zaf3"); item["excess_market_3m_pct"] = difference(row, "zaf1", "zaf2");
            item["excess_industry_3m_pct"] = difference(row, "zaf1", "zaf3");
            item["return_1m_pct"] = number(row, "zaf4"); item["market_return_1m_pct"] = number(row, "zaf5");
            item["industry_return_1m_pct"] = number(row, "zaf6"); item["excess_market_1m_pct"] = difference(row, "zaf4", "zaf5");
            item["excess_industry_1m_pct"] = difference(row, "zaf4", "zaf6");
            item["return_1w_pct"] = number(row, "zaf7"); item["market_return_1w_pct"] = number(row, "zaf8");
            item["industry_return_1w_pct"] = number(row, "zaf9"); item["excess_market_1w_pct"] = difference(row, "zaf7", "zaf8");
            item["excess_industry_1w_pct"] = difference(row, "zaf7", "zaf9");
        }
        item["date"] = date;
        item["event_id"] = kind + ":" + item.at("security").at("security_id").as_string() + ":" + id + ":" + std::to_string(index++);
        result.push_back(std::move(item));
    }
    return result;
}

BenchmarkAnalysisService::BenchmarkAnalysisService(fs::path root,
    std::map<std::pair<int, std::string>, Security> securities, fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)), securities_(std::move(securities)) {}

Json BenchmarkAnalysisService::fetch_master(const BenchmarkAnalysisQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr); age_seconds = cache_time_ ? static_cast<int>(std::max<std::time_t>(0, now-cache_time_)) : 0;
    const auto key = options.view + ":" + options.stage;
    if (!options.refresh && cache_time_ && cache_key_ == key && age_seconds < options.cache_ttl_seconds) { refreshed = false; return cache_; }
    const auto names = resources_for(options); Json documents = Json::array();
    if (!options.refresh && !jsn_root_.empty()) { try { for (const auto& resource : names) documents.push_back(load_local(jsn_root_, resource)); } catch (...) { documents = Json::array(); } }
    if (documents.as_array().empty()) documents = fetch_jsn_resources_rows(names, "bi", options.timeout_ms);
    Json records = Json::array(), sources = Json::array();
    for (const auto& resource : names) { const auto& document = document_for(documents, resource);
        auto normalized = normalize_benchmark_analysis_rows(resource, document.at("rows"), securities_);
        sources.push_back(source_summary(document, normalized.size()));
        for (auto& row : normalized.as_array()) records.push_back(std::move(row)); }
    Json result = Json::object(); result["records"] = std::move(records); result["sources"] = std::move(sources);
    cache_ = result; cache_key_ = key; cache_time_ = std::time(nullptr); refreshed = true; age_seconds = 0; return result;
}

Json BenchmarkAnalysisService::query(const BenchmarkAnalysisQuery& options) {
    const std::set<std::string> views{"all", "stocks", "industries", "suspensions", "new-stocks"};
    if (!views.count(options.view)) throw Error("unsupported benchmark-analysis view");
    if (!options.stage.empty() && !stage_for(options.stage)) throw Error("stage must be one of 201..216 client stage ids");
    if (options.limit < 1 || options.limit > 50000) throw Error("limit must be in 1..50000");
    if (options.market.empty() != options.code.empty()) throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) { selected_market = parsed_market(options.market);
        if (selected_market < 0) throw Error("market must be sz/sh/bj or 0/1/2");
        if (!digits(options.code, 6)) throw Error("code must contain six digits"); }
    bool refreshed = false; int age_seconds = 0; const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query)); Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (options.view != "all" && text_value(row, "kind") != options.view) continue;
        if (!options.stage.empty() && text_value(row, "stage_id") != options.stage) continue;
        if (selected_market >= 0) { const auto& security = row.at("security");
            if (static_cast<int>(security.at("market_id").as_number()) != selected_market || security.at("code").as_string() != options.code) continue; }
        if (!needle.empty() && lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue;
        auto projected = row;
        if (!options.include_raw) projected.as_object().erase("raw");
        records.push_back(std::move(projected));
    }
    std::map<std::string, std::uint64_t> counts; std::set<std::string> securities;
    for (const auto& row : records.as_array()) { ++counts[text_value(row, "kind")]; securities.insert(row.at("security").at("security_id").as_string()); }
    const auto matched = records.size(); if (static_cast<int>(records.size()) > options.limit) records.as_array().resize(static_cast<std::size_t>(options.limit));
    Json summary = Json::object(); summary["stocks"] = counts["stocks"]; summary["industries"] = counts["industries"];
    summary["suspensions"] = counts["suspensions"]; summary["new_stocks"] = counts["new-stocks"];
    summary["stages"] = static_cast<std::uint64_t>(std::size(kStages)); summary["unique_entities"] = static_cast<std::uint64_t>(securities.size());
    Json result = Json::object(); result["schema"] = "tdx-market-benchmark-analysis-native-v1"; result["generated_at"] = now_text();
    result["view"] = options.view; result["stage"] = options.stage; result["mode"] = selected_market < 0 ? "catalog" : "security";
    result["availability"] = matched ? "live" : "empty"; result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size()); result["records"] = std::move(records);
    result["summary"] = std::move(summary); result["sources"] = master.at("sources");
    result["semantics"] = "JZFX client benchmark analysis: thirteen explicit Shanghai-index landmark stages for stocks and industries, plus suspension/resumption and newly listed stock relative performance. Source percentage fields remain percentages. Calculations requiring host-injected current quotes are not fabricated.";
    Json cache = Json::object(); cache["refreshed"] = refreshed; cache["age_seconds"] = age_seconds; result["cache"] = std::move(cache); return result;
}

int command_market_benchmark_analysis(const std::vector<std::string>& raw_args) {
    Args args(raw_args); if (args.take_flag("--help") || args.take_flag("-h")) { std::cout <<
        "Usage: tdx-tool market benchmark-analysis [options]\n\n"
        "  --view all|stocks|industries|suspensions|new-stocks --stage 201..216\n"
        "  --query TEXT --market sz|sh|bj --code CODE --omit-raw\n"
        "  --refresh --root PATH --input-dir PATH --limit N --output PATH --compact\n"; return 0; }
    const auto root_text = args.take_option("--root"); BenchmarkAnalysisQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "stocks"))); query.stage = trim(args.take_option("--stage"));
    query.query = trim(args.take_option("--query")); query.market = lower_ascii(trim(args.take_option("--market"))); query.code = trim(args.take_option("--code"));
    query.include_raw = !args.take_flag("--omit-raw");
    query.refresh = args.take_flag("--refresh"); query.limit = bounded(args.take_option("--limit", "5000"), "--limit", 1, 50000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option("--output", "output/tdx-market-benchmark-analysis-native.json"));
    const bool compact = args.take_flag("--compact"); args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text)); auto blocks = load_blocks(root, {});
    BenchmarkAnalysisService service(root, std::move(blocks.securities), input); const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n"); std::cout << "returned " << document.at("returned").as_number() << " benchmark-analysis rows -> " << path_utf8(output) << '\n'; return 0;
}
}  // namespace tdx
