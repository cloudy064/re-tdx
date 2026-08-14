#include "tdx/corporate_orders.hpp"
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
#include <limits>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* kTender = "list/func_zb101_1.jsn";
constexpr const char* kContract = "list/func_zdht101_1.jsn";
const std::vector<std::string> kResources{kTender, kContract};

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
bool digits(const std::string& value) {
    return value.size() == 6 && std::all_of(value.begin(), value.end(),
        [](char ch) { return ch >= '0' && ch <= '9'; });
}
int integer_value(const Json& row, std::string_view name) {
    try { return std::stoi(text_value(row, name)); } catch (...) { return -1; }
}
std::string market_name(int market) {
    return market == 0 ? "sz" : market == 1 ? "sh" : market == 2 || market == 44
        ? "bj" : "m" + std::to_string(market);
}
std::string market_prefix(int market) {
    return market == 0 ? "SZ" : market == 1 ? "SH" : market == 2 || market == 44
        ? "BJ" : "M" + std::to_string(market);
}
Json security_document(
    const Json& row,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    int market = integer_value(row, "$SC");
    if (market == 44) market = 2;
    const auto code = text_value(row, "$ZQDM");
    if (market < 0 || !digits(code)) return Json(nullptr);
    const auto found = securities.find({market, code});
    Json result = Json::object();
    result["market_id"] = market;
    result["market"] = market_name(market);
    result["code"] = code;
    result["security_id"] = market_prefix(market) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}
std::pair<std::string, std::string> announcement(const std::string& value) {
    const auto marker = value.rfind("TXT:");
    return marker == std::string::npos
        ? std::make_pair(trim(value), std::string{})
        : std::make_pair(trim(value.substr(0, marker)), trim(value.substr(marker + 4)));
}
Json ratio(const std::optional<double>& amount, const std::optional<double>& revenue) {
    return amount && revenue && std::abs(*revenue) > 0.000001
        ? Json(*amount * 100.0 / *revenue) : Json(nullptr);
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
    try {
        std::size_t used = 0; const auto parsed = std::stoi(value, &used);
        if (used != value.size() || parsed < low || parsed > high) throw std::invalid_argument("range");
        return parsed;
    } catch (...) { throw Error(std::string(name) + " must be in " + std::to_string(low) + ".." + std::to_string(high)); }
}
Json load_local(const fs::path& root, const std::string& resource) {
    const auto path = root / native_path(resource);
    if (!fs::is_regular_file(path)) throw Error("local corporate-order resource is unavailable: " + path_utf8(path));
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
const Json& document_for(const Json& documents, const std::string& resource) {
    for (const auto& document : documents.as_array()) if (text_value(document, "resource") == resource) return document;
    throw Error("missing corporate-order resource: " + resource);
}
Json source_summary(const Json& document, std::size_t normalized) {
    Json result = Json::object(); for (const auto* key : {"resource", "size", "row_count", "endpoint"}) result[key] = document.at(key);
    result["normalized_row_count"] = static_cast<std::uint64_t>(normalized); return result;
}
std::optional<double> record_number(const Json& row, std::string_view key) {
    const auto* value = field(row, key); return value && value->is_number() ? std::optional<double>(value->as_number()) : std::nullopt;
}
}  // namespace

Json normalize_corporate_order_rows(
    const std::string& resource, const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("corporate-order rows must be an array");
    if (resource != kTender && resource != kContract) throw Error("unknown corporate-order resource: " + resource);
    const bool tender = resource == kTender; Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        auto security = security_document(raw, securities); if (security.is_null()) continue;
        const auto payload = announcement(text_value(raw, tender ? "nrgg" : "htnr"));
        const auto amount = number_value(raw, tender ? "zbje" : "htje");
        const auto revenue = number_value(raw, "yysr");
        const auto source_ratio = number_value(raw, tender ? "znyszb" : "htzb");
        const auto calculated = ratio(amount, revenue);
        Json item = Json::object(); item["kind"] = tender ? "tender" : "major-contract";
        item["kind_label"] = tender ? "招标/中标" : "重大合同";
        item["security"] = std::move(security); item["announcement_date"] = text_value(raw, "zbrq");
        item["stage"] = tender ? text_value(raw, "zbjd") : "已签署";
        item["amount_yuan"] = amount ? Json(*amount) : Json(nullptr);
        item["revenue_yuan"] = revenue ? Json(*revenue) : Json(nullptr);
        item["source_revenue_share_pct"] = source_ratio ? Json(*source_ratio) : Json(nullptr);
        item["calculated_revenue_share_pct"] = calculated;
        item["revenue_share_formula_matches"] = source_ratio && calculated.is_number()
            ? Json(std::abs(*source_ratio - calculated.as_number()) <= 0.011) : Json(nullptr);
        item["non_recurring_profit_yuan"] = number(raw, "fjcxsy");
        item["title"] = payload.first; item["source_url"] = payload.second;
        item["source_resource"] = resource; item["raw"] = raw;
        item["record_id"] = std::string(tender ? "tender:" : "contract:") +
            item.at("announcement_date").as_string() + ":" + item.at("security").at("security_id").as_string() + ":" + std::to_string(result.size());
        result.push_back(std::move(item));
    }
    return result;
}

CorporateOrdersService::CorporateOrdersService(
    std::map<std::pair<int, std::string>, Security> securities, fs::path jsn_root)
    : securities_(std::move(securities)), jsn_root_(std::move(jsn_root)) {}

Json CorporateOrdersService::fetch_master(const CorporateOrdersQuery& options, bool& refreshed, int& age_seconds) {
    const auto now = std::time(nullptr); age_seconds = cache_time_ ? static_cast<int>(std::max<std::time_t>(0, now - cache_time_)) : 0;
    if (!options.refresh && cache_time_ && age_seconds < options.cache_ttl_seconds) { refreshed = false; return cache_; }
    Json documents = Json::array();
    if (!options.refresh && !jsn_root_.empty()) try { for (const auto& resource : kResources) documents.push_back(load_local(jsn_root_, resource)); } catch (...) { documents = Json::array(); }
    if (!documents.size()) documents = fetch_jsn_resources_rows(kResources, "bi", options.timeout_ms);
    Json records = Json::array(), sources = Json::array();
    for (const auto& resource : kResources) { const auto& document = document_for(documents, resource); auto rows = normalize_corporate_order_rows(resource, document.at("rows"), securities_); sources.push_back(source_summary(document, rows.size())); for (auto& row : rows.as_array()) records.push_back(std::move(row)); }
    Json result = Json::object(); result["records"] = std::move(records); result["sources"] = std::move(sources);
    cache_ = result; cache_time_ = std::time(nullptr); refreshed = true; age_seconds = 0; return result;
}

Json CorporateOrdersService::query(const CorporateOrdersQuery& options) {
    if (options.view != "all" && options.view != "tenders" && options.view != "contracts") throw Error("view must be all, tenders, or contracts");
    if (options.sort != "date" && options.sort != "amount" && options.sort != "revenue-share") throw Error("sort must be date, amount, or revenue-share");
    if (options.order != "asc" && options.order != "desc") throw Error("order must be asc or desc");
    if (options.limit < 1 || options.limit > 20000) throw Error("limit must be in 1..20000");
    if (options.market.empty() != options.code.empty()) throw Error("market and code must be provided together");
    const auto market = lower_ascii(trim(options.market)); if (!market.empty() && market != "sz" && market != "sh" && market != "bj") throw Error("market must be sz/sh/bj");
    if (!options.code.empty() && !digits(options.code)) throw Error("code must contain six digits");
    bool refreshed = false; int age_seconds = 0; const auto master = fetch_master(options, refreshed, age_seconds);
    const auto needle = lower_ascii(trim(options.query)); Json records = Json::array();
    for (const auto& row : master.at("records").as_array()) {
        if (options.view == "tenders" && text_value(row, "kind") != "tender") continue;
        if (options.view == "contracts" && text_value(row, "kind") != "major-contract") continue;
        const auto& security = row.at("security");
        if (!market.empty() && (security.at("market").as_string() != market || security.at("code").as_string() != options.code)) continue;
        if (!needle.empty() && lower_ascii(row.dump(-1)).find(needle) == std::string::npos) continue;
        records.push_back(row);
    }
    const bool asc = options.order == "asc"; std::stable_sort(records.as_array().begin(), records.as_array().end(), [&](const Json& left, const Json& right) {
        if (options.sort == "date") { const auto a = text_value(left, "announcement_date"), b = text_value(right, "announcement_date"); if (a != b) return asc ? a < b : a > b; }
        else { const auto key = options.sort == "amount" ? "amount_yuan" : "source_revenue_share_pct"; const auto a = record_number(left, key), b = record_number(right, key); if (a && b && *a != *b) return asc ? *a < *b : *a > *b; if (a.has_value() != b.has_value()) return a.has_value(); }
        return left.at("record_id").as_string() < right.at("record_id").as_string();
    });
    std::map<std::string, std::uint64_t> counts; std::set<std::string> securities, dates; std::uint64_t checked = 0, mismatches = 0;
    for (const auto& row : records.as_array()) { ++counts[text_value(row, "kind")]; securities.insert(row.at("security").at("security_id").as_string()); dates.insert(text_value(row, "announcement_date")); const auto* match = field(row, "revenue_share_formula_matches"); if (match && match->is_bool()) { ++checked; if (!match->as_bool()) ++mismatches; } }
    const auto matched = records.size(); if (records.size() > static_cast<std::size_t>(options.limit)) records.as_array().resize(options.limit);
    if (!options.include_raw) for (auto& row : records.as_array()) row.as_object().erase("raw");
    Json summary = Json::object(); summary["tenders"] = counts["tender"]; summary["major_contracts"] = counts["major-contract"];
    summary["unique_securities"] = static_cast<std::uint64_t>(securities.size()); summary["date_count"] = static_cast<std::uint64_t>(dates.size());
    summary["latest_date"] = dates.empty() ? "" : *dates.rbegin(); summary["formula_checked"] = checked; summary["formula_mismatches"] = mismatches;
    Json result = Json::object(); result["schema"] = "tdx-market-corporate-orders-native-v1"; result["generated_at"] = now_text(); result["view"] = options.view;
    result["mode"] = market.empty() ? "catalog" : "security"; result["availability"] = matched ? "live" : "empty"; result["match_count"] = static_cast<std::uint64_t>(matched); result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records); result["summary"] = std::move(summary); result["sources"] = master.at("sources");
    result["semantics"] = "Tender and material-contract announcements. Amount and revenue are source yuan; the displayed revenue share is checked against amount*100/revenue. TXT links are split without rewriting the source announcement.";
    Json cache = Json::object(); cache["refreshed"] = refreshed; cache["age_seconds"] = age_seconds; result["cache"] = std::move(cache); return result;
}

int command_market_corporate_orders(const std::vector<std::string>& raw_args) {
    Args args(raw_args); if (args.take_flag("--help") || args.take_flag("-h")) { std::cout << "Usage: tdx-tool market corporate-orders [--view all|tenders|contracts] [--sort date|amount|revenue-share] [--order asc|desc] [--query TEXT] [--market sz|sh|bj --code CODE] [--without-raw] [--refresh] [--root PATH] [--input-dir PATH] [--limit N] [--output PATH] [--compact]\n"; return 0; }
    const auto root_text = args.take_option("--root"); CorporateOrdersQuery query; query.view = lower_ascii(trim(args.take_option("--view", "all"))); query.sort = lower_ascii(trim(args.take_option("--sort", "date"))); query.order = lower_ascii(trim(args.take_option("--order", "desc"))); query.query = trim(args.take_option("--query")); query.market = lower_ascii(trim(args.take_option("--market"))); query.code = trim(args.take_option("--code")); query.include_raw = !args.take_flag("--without-raw"); query.refresh = args.take_flag("--refresh"); query.limit = bounded(args.take_option("--limit", "10000"), "--limit", 1, 20000); query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "--timeout-ms", 100, 60000);
    const auto input = native_path(args.take_option("--input-dir", "output/tdx-jsn")); const auto output = native_path(args.take_option("--output", "output/tdx-market-corporate-orders-native.json")); const bool compact = args.take_flag("--compact"); args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text)); CorporateOrdersService service(load_blocks(root, {}).securities, input); const auto document = service.query(query); atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n"); std::cout << "returned " << document.at("returned").as_number() << " corporate-order rows -> " << path_utf8(output) << '\n'; return 0;
}

}  // namespace tdx
