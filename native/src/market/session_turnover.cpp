#include "tdx/session_turnover.hpp"
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
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* a_after_hours_resource = "list/func_phcje101_1.jsn";
constexpr const char* a_opening_resource = "list/func_phcje101_2.jsn";
constexpr const char* etf_after_hours_resource = "list/func_phcje103_1.jsn";
constexpr const char* etf_opening_resource = "list/func_phcje104_1.jsn";

const Json* value_ptr(const Json& value, std::string_view key) {
    if (!value.is_object()) return nullptr;
    const auto found = value.as_object().find(key);
    return found == value.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& value, std::string_view key) {
    const auto* found = value_ptr(value, key);
    return found ? trim(jsn_scalar_text(*found)) : std::string{};
}

std::optional<double> number_value(const Json& value, std::string_view key) {
    const auto text = text_value(value, key);
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto parsed = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(parsed)) return std::nullopt;
        return parsed;
    } catch (...) { return std::nullopt; }
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

int market_id(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "0" || value == "sz") return 0;
    if (value == "1" || value == "sh") return 1;
    if (value == "2" || value == "44" || value == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string market_name(int id) {
    return id == 0 ? "sz" : id == 1 ? "sh" : id == 2 ? "bj"
                                                          : "m" + std::to_string(id);
}

std::string market_prefix(int id) {
    return id == 0 ? "SZ" : id == 1 ? "SH" : id == 2 ? "BJ"
                                                          : "M" + std::to_string(id);
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

std::optional<double> metric(const Json& row, const std::string& sort) {
    const char* field = sort == "after-hours" ? "after_hours_turnover_yuan" :
        sort == "opening" ? "opening_turnover_yuan" :
        sort == "total" ? "total_turnover_yuan" :
        sort == "after-hours-share" ? "after_hours_share_total_pct" :
        sort == "opening-share" ? "opening_share_total_pct" :
        sort == "close-change" ? "close_change_pct" : "open_change_pct";
    const auto* value = value_ptr(row, field);
    return value && value->is_number()
        ? std::optional<double>(value->as_number()) : std::nullopt;
}

Json summary_document(const Json& rows) {
    std::set<std::string> dates;
    std::uint64_t opening_active = 0, after_hours_active = 0;
    double total = 0, opening = 0, after_hours = 0;
    for (const auto& row : rows.as_array()) {
        const auto date = text_value(row, "statistics_date");
        if (!date.empty()) dates.insert(date);
        const auto total_value = number_value(row, "total_turnover_yuan");
        const auto opening_value = number_value(row, "opening_turnover_yuan");
        const auto after_value = number_value(row, "after_hours_turnover_yuan");
        if (total_value) total += *total_value;
        if (opening_value) {
            opening += *opening_value;
            if (*opening_value > 0) ++opening_active;
        }
        if (after_value) {
            after_hours += *after_value;
            if (*after_value > 0) ++after_hours_active;
        }
    }
    Json date_values = Json::array();
    for (const auto& date : dates) date_values.push_back(date);
    Json result = Json::object();
    result["available"] = static_cast<std::uint64_t>(rows.size());
    result["statistics_dates"] = std::move(date_values);
    result["opening_active"] = opening_active;
    result["after_hours_active"] = after_hours_active;
    result["total_turnover_yuan"] = total;
    result["opening_turnover_yuan"] = opening;
    result["after_hours_turnover_yuan"] = after_hours;
    result["opening_share_total_pct"] = std::abs(total) > 0.000001
        ? Json(opening * 100.0 / total) : Json(nullptr);
    result["after_hours_share_total_pct"] = std::abs(total) > 0.000001
        ? Json(after_hours * 100.0 / total) : Json(nullptr);
    return result;
}

int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
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

}  // namespace

std::string session_turnover_resource_for(const std::string& universe_value,
                                          const std::string& sort_value) {
    const auto universe = lower_ascii(trim(universe_value));
    const auto sort = lower_ascii(trim(sort_value));
    if (universe != "a" && universe != "etf")
        throw Error("universe must be a or etf");
    const bool opening = sort == "opening" || sort == "opening-share";
    if (universe == "a") return opening ? a_opening_resource : a_after_hours_resource;
    return opening ? etf_opening_resource : etf_after_hours_resource;
}

Json normalize_session_turnover_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("session-turnover rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); }
        catch (...) { continue; }
        const auto close = number_value(raw, "price1");
        const auto previous = number_value(raw, "price2");
        const auto open = number_value(raw, "price3");
        const auto total = number_value(raw, "zcje");
        const auto opening = number_value(raw, "kpje");
        const auto after_hours = number_value(raw, "phje");
        Json row = Json::object();
        row["security"] = security_document(id, code, securities);
        row["statistics_date"] = text_value(raw, "date");
        row["close_price"] = number_json(close);
        row["previous_close_price"] = number_json(previous);
        row["open_price"] = number_json(open);
        row["close_change_pct"] = close && previous && std::abs(*previous) > 0.000001
            ? Json((*close - *previous) * 100.0 / *previous) : Json(nullptr);
        row["open_change_pct"] = open && previous && std::abs(*previous) > 0.000001
            ? Json((*open - *previous) * 100.0 / *previous) : Json(nullptr);
        row["total_turnover_yuan"] = number_json(total);
        row["opening_turnover_yuan"] = number_json(opening);
        row["after_hours_turnover_yuan"] = number_json(after_hours);
        row["opening_share_total_pct"] = opening && total && std::abs(*total) > 0.000001
            ? Json(*opening * 100.0 / *total) : Json(nullptr);
        row["after_hours_share_total_pct"] = after_hours && total &&
                std::abs(*total) > 0.000001
            ? Json(*after_hours * 100.0 / *total) : Json(nullptr);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

void sort_session_turnover_rows(Json& rows, const std::string& sort,
                                const std::string& order) {
    if (!rows.is_array()) throw Error("session-turnover sort requires an array");
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            const auto a = metric(left, sort);
            const auto b = metric(right, sort);
            if (!a) return false;
            if (!b) return true;
            if (*a == *b) {
                const auto* a_security = value_ptr(left, "security");
                const auto* b_security = value_ptr(right, "security");
                return text_value(*a_security, "security_id") <
                       text_value(*b_security, "security_id");
            }
            return descending ? *a > *b : *a < *b;
        });
}

SessionTurnoverService::SessionTurnoverService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

SessionTurnoverService::FetchResult SessionTurnoverService::fetch(
    const std::string& resource, const SessionTurnoverQuery& options) {
    const auto now = std::time(nullptr);
    const auto cached = cache_.find(resource);
    const int age = cached == cache_.end() ? 0 :
        static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
    if (!options.refresh && cached != cache_.end() && age < options.cache_ttl_seconds)
        return {cached->second.document, false, age};
    const auto source = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
    Json document = Json::object();
    document["rows"] = normalize_session_turnover_rows(source.at("rows"), securities_);
    document["summary"] = summary_document(document.at("rows"));
    document["source"] = jsn_source_metadata(source);
    cache_[resource] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

Json SessionTurnoverService::query(const SessionTurnoverQuery& options) {
    const auto universe = lower_ascii(trim(options.universe));
    const auto sort = lower_ascii(trim(options.sort));
    const auto order = lower_ascii(trim(options.order));
    const auto activity = lower_ascii(trim(options.activity));
    const std::set<std::string> sorts{
        "after-hours", "opening", "total", "after-hours-share",
        "opening-share", "close-change", "open-change"};
    if (!sorts.count(sort))
        throw Error("sort must be after-hours, opening, total, after-hours-share, opening-share, close-change, or open-change");
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    if (!std::set<std::string>{"all", "after-hours", "opening", "both"}.count(activity))
        throw Error("activity must be all, after-hours, opening, or both");
    if (!options.code.empty() && options.market.empty())
        throw Error("market is required when code is supplied");
    if (!options.code.empty() && !digits(options.code, 6))
        throw Error("code must contain six digits");
    const int selected_market = options.market.empty() ? -1 : market_id(options.market);
    if (options.offset < 0 || options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");

    const auto resource = session_turnover_resource_for(universe, sort);
    auto fetched = fetch(resource, options);
    auto candidates = fetched.document.at("rows");
    sort_session_turnover_rows(candidates, sort, order);
    const auto needle = lower_ascii(trim(options.query));
    Json matched_rows = Json::array();
    for (const auto& row : candidates.as_array()) {
        const auto& security = row.at("security");
        if (selected_market >= 0 &&
            static_cast<int>(security.at("market_id").as_number()) != selected_market)
            continue;
        if (!options.code.empty() && security.at("code").as_string() != options.code)
            continue;
        const auto opening = metric(row, "opening");
        const auto after_hours = metric(row, "after-hours");
        const bool opening_active = opening && *opening > 0;
        const bool after_hours_active = after_hours && *after_hours > 0;
        if (activity == "opening" && !opening_active) continue;
        if (activity == "after-hours" && !after_hours_active) continue;
        if (activity == "both" && (!opening_active || !after_hours_active)) continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        matched_rows.push_back(row);
    }
    const auto matched = matched_rows.size();
    Json records = Json::array();
    for (std::size_t index = static_cast<std::size_t>(options.offset);
         index < matched_rows.size() && records.size() < static_cast<std::size_t>(options.limit);
         ++index)
        records.push_back(matched_rows.as_array()[index]);

    const auto& source = fetched.document.at("source");
    const auto* stale_value = value_ptr(source, "stale");
    const bool stale = stale_value && stale_value->is_bool() && stale_value->as_bool();
    Json result = Json::object();
    result["schema"] = "tdx-market-session-turnover-native-v1";
    result["generated_at"] = now_text();
    result["universe"] = universe;
    result["mode"] = options.code.empty() ? "market" : "security";
    result["availability"] = stale ? "stale-cache" : matched == 0 ? "empty" : "live";
    result["statistics_date"] = Json(nullptr);
    const auto& dates = fetched.document.at("summary").at("statistics_dates");
    if (dates.size() == 1) result["statistics_date"] = dates.as_array().front();
    result["summary"] = fetched.document.at("summary");
    Json filters = Json::object();
    filters["sort"] = sort;
    filters["order"] = order;
    filters["activity"] = activity;
    filters["market"] = options.market.empty()
        ? Json(nullptr) : Json(market_name(selected_market));
    filters["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    filters["query"] = options.query;
    result["filters"] = std::move(filters);
    Json counts = Json::object();
    counts["source_rows"] = static_cast<std::uint64_t>(candidates.size());
    counts["matched"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["source"] = source;
    Json cache = Json::object();
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["refreshed"] = fetched.refreshed;
    cache["age_seconds"] = fetched.age_seconds;
    result["cache"] = std::move(cache);
    result["semantics"] =
        "TDX KPHPHJY statistics-day opening and after-hours transaction amounts. The four resources differ by A-share/ETF universe and upstream primary sort; current quote, market-cap and industry syscols are not fabricated.";
    return result;
}

int command_market_session_turnover(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market session-turnover [options]\n\n"
            "Typed TDX opening and after-hours transaction rankings.\n\n"
            "  --root PATH             TDX installation root\n"
            "  --universe a|etf        Default a\n"
            "  --sort NAME             after-hours|opening|total|after-hours-share|opening-share|close-change|open-change\n"
            "  --order asc|desc        Default desc\n"
            "  --activity NAME         all|after-hours|opening|both\n"
            "  --market sz|sh|bj       Optional market filter; required with code\n"
            "  --code CODE             Optional six-digit security filter\n"
            "  --query TEXT            Filter normalized content\n"
            "  --offset N              Default 0\n"
            "  --limit N               Default 200, maximum 10000\n"
            "  --refresh               Bypass service cache\n"
            "  --cache-ttl N           Default 300 seconds\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output FILE           Write JSON instead of stdout\n"
            "  --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    SessionTurnoverQuery query;
    query.universe = lower_ascii(trim(args.take_option("--universe", "a")));
    query.sort = lower_ascii(trim(args.take_option("--sort", "after-hours")));
    query.order = lower_ascii(trim(args.take_option("--order", "desc")));
    query.activity = lower_ascii(trim(args.take_option("--activity", "all")));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.query = trim(args.take_option("--query"));
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1000000);
    query.limit = bounded(args.take_option("--limit", "200"), "limit", 1, 10000);
    query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(
        args.take_option("--cache-ttl", "300"), "cache-ttl", 0, 86400);
    query.timeout_ms = bounded(
        args.take_option("--timeout-ms", "15000"), "timeout-ms", 100, 60000);
    const auto output_name = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();

    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    SessionTurnoverService service(load_blocks(root, {}).securities);
    const auto result = service.query(query);
    const auto rendered = result.dump(compact ? -1 : 2) + '\n';
    if (output_name.empty()) std::cout << rendered;
    else {
        const auto output = native_path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "completed session-turnover query -> " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
