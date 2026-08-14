#include "tdx/exchange_supervision.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <tuple>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* current_resource = "list/func_jysjk101_1.jsn";
constexpr const char* history_resource = "list/func_jysjk102_1.jsn";

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto exact = value.as_object().find(key);
    if (exact != value.as_object().end()) return &exact->second;
    const auto wanted = lower_ascii(std::string(key));
    for (const auto& [name, child] : value.as_object())
        if (lower_ascii(name) == wanted) return &child;
    return nullptr;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    auto raw = text_value(value, key);
    if (raw.empty() || raw == "-" || raw == "--") return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(raw, &used);
        return used == raw.size() && std::isfinite(parsed)
            ? std::optional<double>(parsed) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

std::optional<double> json_number(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_number()
        ? std::optional<double>(found->as_number()) : std::nullopt;
}

std::string json_text(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found && found->is_string() ? found->as_string() : std::string{};
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

std::string iso_date(const std::string& value) {
    if (!digits(value, 8)) return value;
    return value.substr(0, 4) + "-" + value.substr(4, 2) + "-" + value.substr(6, 2);
}

std::string compact_date(std::string value, const std::string& name) {
    value = trim(std::move(value));
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
    if (!value.empty() && !digits(value, 8))
        throw Error(name + " must be YYYYMMDD or YYYY-MM-DD");
    return value;
}

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : "bj";
}

std::string market_prefix(int id) {
    return id == 0 ? "SZ" : id == 1 ? "SH" : "BJ";
}

Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto found = securities.find({id, code});
    Json result = Json::object();
    result["market"] = market_name(id);
    result["market_id"] = id;
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    return result;
}

bool json_contains(const Json& value, const std::string& needle) {
    if (value.is_string())
        return lower_ascii(value.as_string()).find(needle) != std::string::npos;
    if (value.is_number() || value.is_bool())
        return lower_ascii(jsn_scalar_text(value)).find(needle) != std::string::npos;
    if (value.is_array()) {
        for (const auto& child : value.as_array())
            if (json_contains(child, needle)) return true;
    } else if (value.is_object()) {
        for (const auto& [key, child] : value.as_object())
            if (lower_ascii(key).find(needle) != std::string::npos ||
                json_contains(child, needle)) return true;
    }
    return false;
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const auto value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

Json catalog_rows() {
    Json result = Json::array();
    for (const auto& [view, label, resource, fields] :
         std::vector<std::tuple<std::string, std::string, std::string, std::string>>{
             {"current", "当前监管个股", current_resource,
              "security,start/end date,start/live price,20-day return,announcement PDF"},
             {"history", "历史监管记录", history_resource,
              "security,start/end date,start/end price,period return,announcement PDF"},
             {"security", "单一证券监管记录", "current + history",
              "current and historical supervision periods for one security"}}) {
        Json row = Json::object();
        row["view"] = view; row["label"] = label; row["resource"] = resource;
        row["fields"] = fields; result.push_back(std::move(row));
    }
    return result;
}

Json summarize(const Json& rows) {
    std::set<std::string> securities;
    std::uint64_t current = 0, history = 0, pdf = 0, quoted = 0;
    std::string first, latest;
    for (const auto& row : rows.as_array()) {
        securities.insert(row.at("security").at("security_id").as_string());
        if (row.at("record_kind").as_string() == "current") ++current;
        else ++history;
        if (!row.at("announcement_url").is_null()) ++pdf;
        if (row.at("quote_available").as_bool()) ++quoted;
        const auto start = row.at("start_date").as_string();
        const auto end = row.at("end_date").as_string();
        if (first.empty() || start < first) first = start;
        latest = std::max(latest, end);
    }
    Json result = Json::object();
    result["rows"] = static_cast<std::uint64_t>(rows.size());
    result["unique_securities"] = static_cast<std::uint64_t>(securities.size());
    result["current_rows"] = current; result["history_rows"] = history;
    result["pdf_links"] = pdf; result["quoted_rows"] = quoted;
    result["first_start_date"] = first.empty() ? Json(nullptr) : Json(first);
    result["latest_end_date"] = latest.empty() ? Json(nullptr) : Json(latest);
    return result;
}

}  // namespace

Json normalize_exchange_supervision_rows(
    const Json& rows, const std::string& record_kind_value,
    const Json& quote_rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("exchange-supervision rows must be an array");
    if (!quote_rows.is_array()) throw Error("exchange-supervision quotes must be an array");
    const auto record_kind = lower_ascii(trim(record_kind_value));
    if (record_kind != "current" && record_kind != "history")
        throw Error("record_kind must be current or history");
    std::map<std::pair<int, std::string>, const Json*> quotes;
    for (const auto& quote : quote_rows.as_array()) {
        const auto market = json_number(quote, "market_id");
        const auto code = json_text(quote, "code");
        if (market && digits(code, 6)) quotes[{static_cast<int>(*market), code}] = &quote;
    }
    Json result = Json::array();
    std::set<std::string> identities;
    std::uint64_t source_rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++source_rank;
        const auto code = text_value(raw, "$ZQDM");
        const auto start_raw = text_value(raw, "jgksrq");
        const auto end_raw = text_value(raw, "jgjsrq");
        if (!digits(code, 6) || !digits(start_raw, 8) || !digits(end_raw, 8)) continue;
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        const auto identity = record_kind + ":" + std::to_string(id) + code +
            ":" + start_raw + ":" + end_raw;
        if (!identities.insert(identity).second)
            throw Error("duplicate exchange-supervision period: " + identity);
        auto security = security_document(id, code, securities);
        const auto quote_it = quotes.find({id, code});
        const Json* quote = quote_it == quotes.end() ? nullptr : quote_it->second;
        if (quote && !json_text(*quote, "name").empty()) {
            security["name"] = json_text(*quote, "name");
            security["name_resolved"] = true;
        }
        const auto start_price = number_value(raw, "price1");
        const auto end_price = number_value(raw, "price2");
        const auto live_price = quote ? json_number(*quote, "last_price") : std::nullopt;
        std::optional<double> period_return;
        if (record_kind == "history" && start_price && end_price && *start_price != 0)
            period_return = (*end_price - *start_price) / *start_price * 100.0;
        std::optional<double> since_start;
        if (record_kind == "current" && start_price && live_price && *start_price != 0)
            since_start = (*live_price - *start_price) / *start_price * 100.0;
        Json row = Json::object();
        row["supervision_id"] = identity;
        row["record_kind"] = record_kind;
        row["source_rank"] = source_rank;
        row["security"] = std::move(security);
        row["start_date"] = iso_date(start_raw);
        row["end_date"] = iso_date(end_raw);
        row["start_price"] = number_json(start_price);
        row["end_price"] = number_json(end_price);
        row["last_price"] = number_json(live_price);
        row["period_return_pct"] = number_json(period_return);
        row["since_start_return_pct"] = number_json(since_start);
        row["pe_ttm"] = number_json(number_value(raw, "syl"));
        const auto url = text_value(raw, "ydgg");
        row["announcement_url"] = url.empty() ? Json(nullptr) : Json(url);
        row["quote_available"] = live_price.has_value();
        row["quote_change_pct"] = quote && value_ptr(*quote, "change_pct")
            ? *value_ptr(*quote, "change_pct") : Json(nullptr);
        row["turnover_amount_yuan"] = quote && value_ptr(*quote, "amount")
            ? *value_ptr(*quote, "amount") : Json(nullptr);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

void sort_exchange_supervision_rows(Json& rows, const std::string& sort_value,
                                    const std::string& order_value) {
    if (!rows.is_array()) throw Error("exchange-supervision sort requires an array");
    const auto sort = lower_ascii(trim(sort_value));
    const auto order = lower_ascii(trim(order_value));
    const std::set<std::string> allowed{"start-date", "end-date", "period-return",
        "since-start", "price", "pe", "turnover", "code", "source-rank"};
    if (!allowed.count(sort))
        throw Error("sort must be start-date, end-date, period-return, since-start, price, pe, turnover, code, or source-rank");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    const std::map<std::string, std::string> number_keys{
        {"period-return", "period_return_pct"}, {"since-start", "since_start_return_pct"},
        {"price", "last_price"}, {"pe", "pe_ttm"}, {"turnover", "turnover_amount_yuan"},
        {"source-rank", "source_rank"}};
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort == "start-date" || sort == "end-date") {
                const auto key = sort == "start-date" ? "start_date" : "end_date";
                const auto a = json_text(left, key), b = json_text(right, key);
                if (a != b) return descending ? a > b : a < b;
            } else if (sort == "code") {
                const auto a = left.at("security").at("security_id").as_string();
                const auto b = right.at("security").at("security_id").as_string();
                if (a != b) return descending ? a > b : a < b;
            } else {
                const auto a = json_number(left, number_keys.at(sort));
                const auto b = json_number(right, number_keys.at(sort));
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return descending ? *a > *b : *a < *b;
            }
            return left.at("supervision_id").as_string() <
                   right.at("supervision_id").as_string();
        });
}

ExchangeSupervisionService::ExchangeSupervisionService(
    fs::path root, std::map<std::pair<int, std::string>, Security> securities)
    : root_(std::move(root)) { blocks_.securities = std::move(securities); }

Json ExchangeSupervisionService::fetch_resource(
    const std::string& resource, bool refresh, int cache_ttl_seconds,
    int timeout_ms, bool& fetched) {
    const auto now = std::time(nullptr);
    const auto found = resource_cache_.find(resource);
    if (!refresh && found != resource_cache_.end() &&
        now - found->second.fetched_at < cache_ttl_seconds) {
        fetched = false;
        return found->second.document;
    }
    auto document = fetch_jsn_resource_rows(resource, "bi", timeout_ms);
    resource_cache_[resource] = {document, std::time(nullptr)};
    fetched = true;
    return document;
}

Json ExchangeSupervisionService::fetch_quotes(
    const std::vector<std::string>& securities, bool refresh,
    int cache_ttl_seconds, int timeout_ms, bool& fetched) {
    const auto now = std::time(nullptr);
    if (!refresh && quote_cache_.document.is_object() &&
        now - quote_cache_.fetched_at < cache_ttl_seconds) {
        fetched = false;
        return quote_cache_.document;
    }
    auto document = fetch_market_snapshot_document(root_, securities, timeout_ms, &blocks_);
    quote_cache_ = {document, std::time(nullptr)};
    fetched = true;
    return document;
}

Json ExchangeSupervisionService::query(const ExchangeSupervisionQuery& input) {
    ExchangeSupervisionQuery options = input;
    options.view = lower_ascii(trim(options.view));
    options.market = lower_ascii(trim(options.market));
    options.code = trim(options.code); options.query = trim(options.query);
    options.from = compact_date(options.from, "from");
    options.to = compact_date(options.to, "to");
    options.sort = lower_ascii(trim(options.sort));
    options.order = lower_ascii(trim(options.order));
    if (!std::set<std::string>{"current", "history", "security", "catalog"}.count(options.view))
        throw Error("view must be current, history, security, or catalog");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    int selected_market = -1;
    if (!options.market.empty()) {
        selected_market = market_id(options.market);
        options.market = market_name(selected_market);
        if (!digits(options.code, 6)) throw Error("code must contain six digits");
    }
    if (options.view == "security" && selected_market < 0)
        throw Error("security view requires market and code");
    if (!options.from.empty() && !options.to.empty() && options.from > options.to)
        throw Error("from must not be after to");
    if (options.offset < 0 || options.offset > 1000000 ||
        options.limit < 1 || options.limit > 5000)
        throw Error("exchange-supervision pagination is invalid");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400 ||
        options.quote_cache_ttl_seconds < 0 || options.quote_cache_ttl_seconds > 3600 ||
        options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("exchange-supervision cache or timeout value is invalid");

    Json sources = Json::array(), warnings = Json::array(), quote_source = Json(nullptr);
    Json rows = Json::array();
    bool current_fetched = false, history_fetched = false, quote_fetched = false;
    if (options.view == "catalog") {
        rows = catalog_rows();
    } else {
        Json current = Json(nullptr), history = Json(nullptr);
        if (options.view == "current" || options.view == "security") {
            current = fetch_resource(current_resource, options.refresh,
                options.cache_ttl_seconds, options.timeout_ms, current_fetched);
            sources.push_back(jsn_source_metadata(current));
        }
        if (options.view == "history" || options.view == "security") {
            history = fetch_resource(history_resource, options.refresh,
                options.cache_ttl_seconds, options.timeout_ms, history_fetched);
            sources.push_back(jsn_source_metadata(history));
        }
        Json quotes = Json::array();
        if (options.include_quotes && !current.is_null()) {
            std::vector<std::string> requested;
            for (const auto& raw : current.at("rows").as_array()) {
                const auto code = text_value(raw, "$ZQDM");
                try {
                    if (digits(code, 6))
                        requested.push_back(market_name(market_id(text_value(raw, "$SC"))) + ":" + code);
                } catch (...) {}
            }
            try {
                const auto live = fetch_quotes(requested, options.refresh,
                    options.quote_cache_ttl_seconds, options.timeout_ms, quote_fetched);
                quotes = live.at("records");
                quote_source = Json::object();
                for (const auto* key : {"command", "endpoint", "server_name", "generated_at",
                                        "requested", "received"})
                    quote_source[key] = live.at(key);
                quote_source["cache_refreshed"] = quote_fetched;
            } catch (const std::exception& error) {
                Json warning = Json::object();
                warning["source"] = "public-l1-snapshot";
                warning["message"] = error.what();
                warnings.push_back(std::move(warning));
            }
        }
        if (!current.is_null()) {
            auto normalized = normalize_exchange_supervision_rows(
                current.at("rows"), "current", quotes, blocks_.securities);
            for (auto& row : normalized.as_array()) rows.push_back(std::move(row));
        }
        if (!history.is_null()) {
            auto normalized = normalize_exchange_supervision_rows(
                history.at("rows"), "history", Json::array(), blocks_.securities);
            for (auto& row : normalized.as_array()) rows.push_back(std::move(row));
        }
    }

    Json filtered = Json::array();
    const auto needle = lower_ascii(options.query);
    for (const auto& row : rows.as_array()) {
        if (options.view == "catalog") {
            if (needle.empty() || json_contains(row, needle)) filtered.push_back(row);
            continue;
        }
        const auto& security = row.at("security");
        if (selected_market >= 0 &&
            static_cast<int>(security.at("market_id").as_number()) != selected_market) continue;
        if (!options.code.empty() && security.at("code").as_string() != options.code) continue;
        const auto start = row.at("start_date").as_string();
        const auto end = row.at("end_date").as_string();
        if (!options.from.empty() && end < iso_date(options.from)) continue;
        if (!options.to.empty() && start > iso_date(options.to)) continue;
        if (options.pdf_only && row.at("announcement_url").is_null()) continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        filtered.push_back(row);
    }
    if (options.view != "catalog")
        sort_exchange_supervision_rows(filtered, options.sort, options.order);
    const auto summary = options.view == "catalog" ? Json::object() : summarize(filtered);
    const auto matched = filtered.size();
    Json paged = Json::array();
    for (std::size_t index = static_cast<std::size_t>(options.offset);
         index < filtered.size() && paged.size() < static_cast<std::size_t>(options.limit);
         ++index) paged.push_back(filtered.as_array()[index]);
    const auto health = jsn_sources_health(sources);
    const auto quoted = options.view == "catalog" ? 0 :
        static_cast<int>(summary.at("quoted_rows").as_number());
    Json result = Json::object();
    result["schema"] = "tdx-market-exchange-supervision-native-v1";
    result["generated_at"] = now_text(); result["view"] = options.view;
    result["availability"] = options.view == "catalog" ? "catalog" :
        health.at("stale").as_bool() ? "stale-cache" : matched == 0 ? "empty" :
        options.view == "history" || !options.include_quotes || quoted > 0 ? "live" : "records-only";
    Json filters = Json::object();
    filters["market"] = options.market.empty() ? Json(nullptr) : Json(options.market);
    filters["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    filters["query"] = options.query; filters["from"] = options.from.empty() ? Json(nullptr) : Json(iso_date(options.from));
    filters["to"] = options.to.empty() ? Json(nullptr) : Json(iso_date(options.to));
    filters["pdf_only"] = options.pdf_only; filters["include_quotes"] = options.include_quotes;
    filters["sort"] = options.sort; filters["order"] = options.order;
    result["filters"] = std::move(filters); result["summary"] = summary;
    Json counts = Json::object();
    counts["matched"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(paged.size());
    counts["sources"] = static_cast<std::uint64_t>(sources.size());
    counts["warnings"] = static_cast<std::uint64_t>(warnings.size());
    result["counts"] = std::move(counts); result["records"] = std::move(paged);
    result["catalog"] = catalog_rows(); result["sources"] = std::move(sources);
    result["quote_source"] = std::move(quote_source); result["upstream_health"] = health;
    result["warnings"] = std::move(warnings);
    Json cache = Json::object();
    cache["resource_ttl_seconds"] = options.cache_ttl_seconds;
    cache["quote_ttl_seconds"] = options.quote_cache_ttl_seconds;
    cache["current_refreshed"] = current_fetched; cache["history_refreshed"] = history_fetched;
    cache["quote_refreshed"] = quote_fetched; result["cache"] = std::move(cache);
    Json units = Json::object();
    units["prices"] = "CNY or fund net-value quotation according to security";
    units["period_return_pct"] = "percentage-points";
    units["since_start_return_pct"] = "percentage-points";
    units["turnover_amount_yuan"] = "yuan"; result["units"] = std::move(units);
    result["semantics"] =
        "TDX JYJGFX exchange-supervision watch periods. jysjk101 is the current monitored-security list and jysjk102 is completed history; these are distinct from anomaly thresholds and from enforcement/inquiry announcements. The client formula (NOW-price1)/price1*100 is reproduced as since_start_return_pct from public 0x054C L1 quotes; historical period_return_pct reproduces (price2-price1)/price1*100. The lists include stocks, funds and convertible bonds, so no equity-only assumption is made. Missing quotes degrade current rows to records-only. PDF links are upstream exchange/company announcements and can be absent.";
    return result;
}

int command_market_exchange_supervision(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market exchange-supervision [options]\n\n"
            "Typed TDX current and historical exchange-supervision periods.\n\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             current|history|security|catalog\n"
            "  --market sz|sh|bj --code CODE  Required together; security view requires both\n"
            "  --query TEXT --from YYYYMMDD --to YYYYMMDD --pdf-only\n"
            "  --no-quotes             Skip current public L1 price/return join\n"
            "  --sort NAME             start-date|end-date|period-return|since-start|price|pe|turnover|code|source-rank\n"
            "  --order asc|desc --offset N --limit N\n"
            "  --refresh --cache-ttl N --quote-cache-ttl N --timeout-ms N\n"
            "  --output FILE --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    ExchangeSupervisionQuery query;
    query.view = args.take_option("--view", "current");
    query.market = args.take_option("--market"); query.code = args.take_option("--code");
    query.query = args.take_option("--query"); query.from = args.take_option("--from");
    query.to = args.take_option("--to"); query.pdf_only = args.take_flag("--pdf-only");
    query.include_quotes = !args.take_flag("--no-quotes");
    query.sort = args.take_option("--sort", "end-date");
    query.order = args.take_option("--order", "desc");
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "500"), "limit", 1, 5000);
    query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(args.take_option("--cache-ttl", "300"), "cache-ttl", 0, 86400);
    query.quote_cache_ttl_seconds = bounded(args.take_option("--quote-cache-ttl", "5"), "quote-cache-ttl", 0, 3600);
    query.timeout_ms = bounded(args.take_option("--timeout-ms", "15000"), "timeout-ms", 100, 60000);
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact"); args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    ExchangeSupervisionService service(root, load_blocks(root, {}).securities);
    const auto rendered = service.query(query).dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_name); atomic_write_text(output, rendered);
        std::cout << "completed exchange-supervision query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
