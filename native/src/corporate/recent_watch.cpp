#include "tdx/recent_watch.hpp"
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

constexpr const char* kEarningsDivergence = "list/func_jqgz101_1.jsn";
constexpr const char* kForeignBusiness = "list/func_jqgz104_1.jsn";
constexpr const char* kStTurnaround = "list/func_jqgz111_1.jsn";
const std::vector<std::string> kResources{
    kEarningsDivergence, kForeignBusiness, kStTurnaround};

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
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(value, &used);
        if (used == value.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}
Json number(const Json& row, std::string_view name) {
    const auto value = number_value(row, name);
    return value ? Json(*value) : Json(nullptr);
}
Json scaled(const Json& row, std::string_view name, double scale) {
    const auto value = number_value(row, name);
    return value ? Json(*value * scale) : Json(nullptr);
}
bool digits(const std::string& value) {
    return value.size() == 6 && std::all_of(value.begin(), value.end(),
        [](char ch) { return ch >= '0' && ch <= '9'; });
}
int integer_value(const Json& row, std::string_view name) {
    const auto value = text_value(row, name);
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        return used == value.size() ? parsed : -1;
    } catch (...) { return -1; }
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
Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (market == 44) market = 2;
    if (market < 0 || !digits(code)) return Json(nullptr);
    std::string name;
    const auto found = securities.find({market, code});
    if (found != securities.end()) name = found->second.name;
    Json result = Json::object();
    result["market_id"] = market;
    result["market"] = market_name(market);
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = name;
    result["name_resolved"] = !name.empty();
    return result;
}
std::string kind_for(const std::string& resource) {
    if (resource == kEarningsDivergence) return "earnings-divergence";
    if (resource == kForeignBusiness) return "foreign-business";
    if (resource == kStTurnaround) return "st-turnaround";
    throw Error("unknown recent-watch resource: " + resource);
}
std::string label_for(const std::string& kind) {
    if (kind == "earnings-divergence") return "绩价背离";
    if (kind == "foreign-business") return "涉外经营";
    return "ST业绩预盈";
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
fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}
int bounded(const std::string& value, std::string_view name, int low, int high) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < low || parsed > high)
            throw std::invalid_argument("range");
        return parsed;
    } catch (...) {
        throw Error(std::string(name) + " must be in " +
                    std::to_string(low) + ".." + std::to_string(high));
    }
}
Json load_local_resource_rows(const fs::path& root, const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path))
        throw Error("local recent-watch resource is unavailable: " + path_utf8(path));
    const auto tables = load_jsn_tables(path);
    Json rows = Json::array();
    for (std::size_t group = 0; group < tables.size(); ++group) {
        const auto& table = tables[group];
        for (const auto& cells : table.rows) {
            Json row = Json::object();
            for (std::size_t column = 0; column < table.headers.size(); ++column)
                row[table.headers[column]] = cells[column];
            row["_group"] = static_cast<std::uint64_t>(group);
            rows.push_back(std::move(row));
        }
    }
    Json result = Json::object();
    result["resource"] = resource;
    result["size"] = static_cast<std::uint64_t>(fs::file_size(path));
    result["row_count"] = static_cast<std::uint64_t>(rows.size());
    result["endpoint"] = "local-jsn:" + path_utf8(path);
    result["rows"] = std::move(rows);
    return result;
}
const Json& document_for(const Json& documents, std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing recent-watch resource: " + std::string(resource));
}
Json source_summary(const Json& document, std::size_t normalized_rows) {
    Json result = Json::object();
    for (const auto* name : {"resource", "size", "row_count", "endpoint"})
        result[name] = document.at(name);
    result["normalized_row_count"] = static_cast<std::uint64_t>(normalized_rows);
    return result;
}
void add_number(Json& item, const Json& row, const char* output, const char* input) {
    item[output] = number(row, input);
}
}  // namespace

Json normalize_recent_watch_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("recent-watch rows must be an array");
    const auto kind = kind_for(resource);
    Json result = Json::array();
    std::uint64_t rank = 0;
    for (const auto& row : rows.as_array()) {
        ++rank;
        auto security = security_document(integer_value(row, "$SC"),
                                          text_value(row, "$ZQDM"), securities);
        if (security.is_null()) continue;
        Json item = Json::object();
        item["kind"] = kind;
        item["kind_label"] = label_for(kind);
        item["security"] = std::move(security);
        item["source_resource"] = resource;
        item["source_rank"] = rank;
        item["raw"] = row;
        std::string event_date;
        if (kind == "earnings-divergence") {
            item["announcement_date"] = text_value(row, "ygrq");
            item["report_period"] = text_value(row, "bgq");
            item["forecast_type"] = text_value(row, "lx");
            add_number(item, row, "return_since_announcement_pct", "zf_gj");
            add_number(item, row, "forecast_profit_yoy_pct", "zf_yc");
            add_number(item, row, "actual_profit_yoy_pct", "zf_sj");
            item["reason"] = text_value(row, "yy");
            event_date = text_value(row, "ygrq");
        } else if (kind == "foreign-business") {
            item["report_period"] = text_value(row, "bgq");
            for (const auto& [out, in] : std::vector<std::pair<const char*, const char*>>{
                    {"revenue_100m_yuan", "yysr"}, {"cost_100m_yuan", "yycb"},
                    {"profit_100m_yuan", "yylr"}, {"foreign_revenue_100m_yuan", "srje"},
                    {"foreign_cost_100m_yuan", "cbje"}, {"foreign_profit_100m_yuan", "lrje"}})
                add_number(item, row, out, in);
            item["revenue_yuan"] = scaled(row, "yysr", 100000000.0);
            item["cost_yuan"] = scaled(row, "yycb", 100000000.0);
            item["profit_yuan"] = scaled(row, "yylr", 100000000.0);
            item["foreign_revenue_yuan"] = scaled(row, "srje", 100000000.0);
            item["foreign_cost_yuan"] = scaled(row, "cbje", 100000000.0);
            item["foreign_profit_yuan"] = scaled(row, "lrje", 100000000.0);
            add_number(item, row, "foreign_revenue_pct", "srzb");
            add_number(item, row, "foreign_cost_pct", "cbzb");
            add_number(item, row, "foreign_profit_pct", "lrzb");
            item["effect"] = text_value(row, "lx");
            event_date = text_value(row, "bgq");
        } else {
            item["announcement_date"] = text_value(row, "ggrq");
            item["report_period"] = text_value(row, "bgq");
            item["forecast_type"] = text_value(row, "lx");
            add_number(item, row, "return_since_announcement_pct", "zf_gj");
            add_number(item, row, "profit_lower_yuan", "ygjl1");
            add_number(item, row, "profit_upper_yuan", "ygjl2");
            add_number(item, row, "growth_lower_pct", "ygjl3");
            add_number(item, row, "growth_upper_pct", "ygjl4");
            event_date = text_value(row, "ggrq");
        }
        item["event_date"] = event_date;
        item["event_id"] = kind + ":" +
            item.at("security").at("security_id").as_string() + ":" +
            event_date + ":" + std::to_string(rank);
        result.push_back(std::move(item));
    }
    return result;
}

RecentWatchService::RecentWatchService(
    fs::path root, std::map<std::pair<int, std::string>, Security> securities,
    fs::path jsn_root)
    : root_(std::move(root)), jsn_root_(std::move(jsn_root)),
      securities_(std::move(securities)) {}

Json RecentWatchService::fetch_master(
    const RecentWatchQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr);
    age_seconds = cache_time_ ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) {
        refreshed = false; return cache_;
    }
    Json documents = Json::array();
    if (!options.refresh && !jsn_root_.empty()) {
        try {
            for (const auto& resource : kResources)
                documents.push_back(load_local_resource_rows(jsn_root_, resource));
        } catch (...) { documents = Json::array(); }
    }
    if (documents.as_array().empty())
        documents = fetch_jsn_resources_rows(kResources, "bi", options.timeout_ms);
    Json records = Json::array(), sources = Json::array();
    for (const auto& resource : kResources) {
        const auto& document = document_for(documents, resource);
        auto normalized = normalize_recent_watch_rows(resource, document.at("rows"), securities_);
        sources.push_back(source_summary(document, normalized.size()));
        for (auto& row : normalized.as_array()) records.push_back(std::move(row));
    }
    std::sort(records.as_array().begin(), records.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto ld = text_value(left, "event_date"), rd = text_value(right, "event_date");
            if (ld != rd) return ld > rd;
            return left.at("event_id").as_string() < right.at("event_id").as_string();
        });
    Json result = Json::object();
    result["records"] = std::move(records); result["sources"] = std::move(sources);
    cache_ = result; cache_time_ = std::time(nullptr); refreshed = true; age_seconds = 0;
    return result;
}

Json RecentWatchService::query(const RecentWatchQuery& options) {
    const std::set<std::string> views{"all", "earnings-divergence", "foreign-business", "st-turnaround"};
    if (!views.count(options.view)) throw Error("unsupported recent-watch view");
    if (options.limit < 1 || options.limit > 10000) throw Error("limit must be in 1..10000");
    if (options.market.empty() != options.code.empty()) throw Error("market and code must be provided together");
    const auto selected_market = lower_ascii(trim(options.market));
    if (!selected_market.empty() && selected_market != "sz" && selected_market != "sh" && selected_market != "bj")
        throw Error("market must be sz/sh/bj");
    if (!options.code.empty() && !digits(options.code)) throw Error("code must contain six digits");
    bool refreshed = false; int age_seconds = 0;
    const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query));
    Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (options.view != "all" && text_value(row, "kind") != options.view) continue;
        if (!selected_market.empty()) {
            const auto& security = row.at("security");
            if (security.at("market").as_string() != selected_market ||
                security.at("code").as_string() != options.code) continue;
        }
        if (!needle.empty() && lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue;
        auto projected = row;
        if (!options.include_raw) projected.as_object().erase("raw");
        records.push_back(std::move(projected));
    }
    std::map<std::string, std::uint64_t> counts;
    std::set<std::string> securities;
    for (const auto& row : records.as_array()) {
        ++counts[text_value(row, "kind")];
        securities.insert(row.at("security").at("security_id").as_string());
    }
    const auto matched = records.size();
    if (static_cast<int>(records.size()) > options.limit)
        records.as_array().resize(static_cast<std::size_t>(options.limit));
    Json summary = Json::object();
    summary["earnings-divergence"] = counts["earnings-divergence"];
    summary["foreign-business"] = counts["foreign-business"];
    summary["st-turnaround"] = counts["st-turnaround"];
    summary["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    Json result = Json::object();
    result["schema"] = "tdx-market-recent-watch-native-v1";
    result["generated_at"] = now_text(); result["view"] = options.view;
    result["mode"] = selected_market.empty() ? "catalog" : "security";
    result["availability"] = matched ? "live" : "empty";
    result["match_count"] = static_cast<std::uint64_t>(matched);
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records); result["summary"] = std::move(summary);
    result["sources"] = master.at("sources");
    result["semantics"] =
        "JQGZ earnings/price divergence, foreign-business exposure and ST turnaround "
        "watchlists. Foreign-business amount fields are hundred-million yuan and are "
        "also exposed as yuan. Return-since-announcement is the source snapshot, not live.";
    Json cache = Json::object(); cache["refreshed"] = refreshed; cache["age_seconds"] = age_seconds;
    result["cache"] = std::move(cache); return result;
}

int command_market_recent_watch(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout << "Usage: tdx-tool market recent-watch [options]\n\n"
            "  --view all|earnings-divergence|foreign-business|st-turnaround\n"
            "  --query TEXT --market sz|sh|bj --code CODE --without-raw --refresh\n"
            "  --root PATH --input-dir PATH --limit N --output PATH --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    RecentWatchQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = lower_ascii(trim(args.take_option("--market")));
    query.code = trim(args.take_option("--code"));
    query.include_raw = !args.take_flag("--without-raw"); query.refresh = args.take_flag("--refresh");
    query.limit = bounded(args.take_option("--limit", "5000"), "--limit", 1, 10000);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn"));
    const auto output = native_path(args.take_option("--output", "output/tdx-market-recent-watch-native.json"));
    const bool compact = args.take_flag("--compact"); args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto blocks = load_blocks(root, {});
    RecentWatchService service(root, std::move(blocks.securities), input);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "returned " << document.at("returned").as_number()
              << " recent-watch rows -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
