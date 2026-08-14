#include "tdx/foreign_alerts.hpp"
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

constexpr std::string_view master_resource =
    "list/func_wzmryj101_1.jsn";

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
    if (localtime_s(&local, &now)) throw Error("cannot read local time");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local time");
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

const Json* value_ptr(const Json& object, std::string_view name) {
    if (!object.is_object()) throw Error("foreign-alert row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> number_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    const auto text = trim(jsn_scalar_text(*value));
    if (text.empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const auto result = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(result)) return std::nullopt;
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

Json number_or_null(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

bool digits(const std::string& value, std::size_t size) {
    return value.size() == size &&
        std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        });
}

int mainland_market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
    throw Error("market must be sz, sh, bj, 0, 1, 2, or 44");
}

std::optional<int> row_market(const Json& row) {
    try { return mainland_market_id(text_value(row, "$SC")); }
    catch (...) { return std::nullopt; }
}

std::string market_name(int id) {
    if (id == 0) return "sz";
    if (id == 1) return "sh";
    if (id == 2) return "bj";
    return std::to_string(id);
}

std::string market_prefix(int id) {
    if (id == 0) return "SZ";
    if (id == 1) return "SH";
    if (id == 2) return "BJ";
    return "M" + std::to_string(id) + "-";
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

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
}

Json summary_document(const Json& rows) {
    std::uint64_t forced = 0, suspended = 0, warning = 0, approaching = 0, unknown = 0;
    double holdings = 0.0, daily_change = 0.0, maximum_ratio = 0.0;
    std::string date;
    for (const auto& row : rows.as_array()) {
        const auto key = text_value(row, "status_key");
        if (key == "forced-reduction") ++forced;
        else if (key == "suspended-buy") ++suspended;
        else if (key == "warning") ++warning;
        else if (key == "approaching") ++approaching;
        else ++unknown;
        holdings += number_value(row, "foreign_holding_shares").value_or(0.0);
        daily_change += number_value(row, "daily_change_shares").value_or(0.0);
        maximum_ratio = std::max(maximum_ratio,
            number_value(row, "foreign_holding_ratio_pct").value_or(0.0));
        date = std::max(date, text_value(row, "date"));
    }
    Json result = Json::object();
    result["securities"] = static_cast<std::uint64_t>(rows.size());
    result["forced_reduction"] = forced;
    result["suspended_buy"] = suspended;
    result["warning"] = warning;
    result["approaching"] = approaching;
    result["unknown"] = unknown;
    result["foreign_holding_shares"] = holdings;
    result["daily_change_shares"] = daily_change;
    result["maximum_holding_ratio_pct"] = maximum_ratio;
    result["date"] = date;
    return result;
}

int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int result = std::stoi(text, &used);
        if (used != text.size() || result < minimum || result > maximum)
            throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw Error(name + " must be in " + std::to_string(minimum) + ".." +
                    std::to_string(maximum));
    }
}

}  // namespace

std::string foreign_alert_status_key(const std::string& status) {
    const auto value = trim(status);
    if (value == "强制减仓") return "forced-reduction";
    if (value == "暂停买入") return "suspended-buy";
    if (value == "预警") return "warning";
    if (value == "逼近预警") return "approaching";
    return "unknown";
}

Json normalize_foreign_alert_master_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("foreign-alert master rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = row_market(row);
        const auto code = text_value(row, "$ZQDM");
        if (!market || !digits(code, 6)) continue;
        const auto status = text_value(row, "yjzt");
        Json item = Json::object();
        item["security"] = security_document(*market, code, securities);
        item["date"] = text_value(row, "date");
        const auto holding_ten_thousand = number_value(row, "wzcgs");
        item["foreign_holding_shares"] = holding_ten_thousand
            ? Json(*holding_ten_thousand * 10000.0) : Json(nullptr);
        item["foreign_holding_ratio_pct"] = number_or_null(number_value(row, "zzgb"));
        item["status"] = status;
        item["status_key"] = foreign_alert_status_key(status);
        item["daily_change_shares"] = number_or_null(number_value(row, "cgbd"));
        item["daily_change_pct"] = number_or_null(number_value(row, "bdbl"));
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "foreign_holding_ratio_pct").value_or(0.0) >
                number_value(right, "foreign_holding_ratio_pct").value_or(0.0);
        });
    return result;
}

Json normalize_foreign_alert_history_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("foreign-alert history rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto status = text_value(row, "yjzt");
        Json item = Json::object();
        item["date"] = text_value(row, "date");
        item["foreign_holding_shares"] = number_or_null(number_value(row, "wzcgs"));
        item["foreign_holding_ratio_pct"] = number_or_null(number_value(row, "zzgb"));
        item["status"] = status;
        item["status_key"] = foreign_alert_status_key(status);
        item["daily_change_shares"] = number_or_null(number_value(row, "cgbd"));
        item["daily_change_pct"] = number_or_null(number_value(row, "bdbl"));
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "date") > text_value(right, "date");
        });
    return result;
}

ForeignAlertService::ForeignAlertService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

ForeignAlertService::FetchResult ForeignAlertService::fetch_master(
    const ForeignAlertQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};
    const auto source = fetch_jsn_resource_rows(
        std::string(master_resource), "bi", options.timeout_ms);
    Json document = Json::object();
    document["alerts"] = normalize_foreign_alert_master_rows(
        source.at("rows"), securities_);
    document["summary"] = summary_document(document.at("alerts"));
    document["source"] = source_summary(source);
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

ForeignAlertService::FetchResult ForeignAlertService::fetch_resource(
    const std::string& resource, const ForeignAlertQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = resource_cache_.begin(); item != resource_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = resource_cache_.erase(item);
        else ++item;
    }
    for (auto item = failure_cache_.begin(); item != failure_cache_.end();) {
        if (now - item->second.second >= options.detail_cache_ttl_seconds)
            item = failure_cache_.erase(item);
        else ++item;
    }
    if (options.refresh) {
        resource_cache_.erase(resource);
        failure_cache_.erase(resource);
    }
    const auto cached = resource_cache_.find(resource);
    if (cached != resource_cache_.end()) {
        const int age = static_cast<int>(std::max<std::time_t>(
            0, now - cached->second.fetched_at));
        return {cached->second.document, false, age};
    }
    const auto failed = failure_cache_.find(resource);
    if (failed != failure_cache_.end()) throw Error(failed->second.first);
    try {
        const auto document = fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
        resource_cache_[resource] = {document, std::time(nullptr)};
        return {document, true, 0};
    } catch (const std::exception& error) {
        failure_cache_[resource] = {error.what(), std::time(nullptr)};
        throw;
    }
}

Json ForeignAlertService::query(const ForeignAlertQuery& options) {
    const std::set<std::string> statuses{
        "all", "forced-reduction", "suspended-buy", "warning", "approaching", "unknown"};
    if (!statuses.count(options.status))
        throw Error("status must be all, forced-reduction, suspended-buy, warning, approaching, or unknown");
    if (options.limit < 1 || options.limit > 5000 ||
        options.detail_limit < 1 || options.detail_limit > 5000)
        throw Error("foreign-alert limits are outside the supported range");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    const bool security_mode = !options.code.empty();
    const int market = security_mode ? mainland_market_id(options.market) : -1;
    if (security_mode && !digits(options.code, 6))
        throw Error("code must contain six digits");

    auto master = fetch_master(options);
    Json selected = Json(nullptr);
    for (const auto& row : master.document.at("alerts").as_array()) {
        if (!security_mode) continue;
        const auto& security = row.at("security");
        if (static_cast<int>(security.at("market_id").as_number()) == market &&
            text_value(security, "code") == options.code) {
            selected = row;
            break;
        }
    }
    Json candidates = security_mode ? Json::array() : master.document.at("alerts");
    if (security_mode && !selected.is_null()) candidates.push_back(selected);
    Json alerts = Json::array();
    const auto needle = lower_ascii(trim(options.query));
    for (const auto& row : candidates.as_array()) {
        if (options.status != "all" && text_value(row, "status_key") != options.status)
            continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        if (static_cast<int>(alerts.size()) >= options.limit) break;
        alerts.push_back(row);
    }

    Json history = Json::array(), errors = Json::array(), sources = Json::array();
    sources.push_back(master.document.at("source"));
    bool detail_refreshed = false;
    int detail_age = 0;
    if (security_mode && options.include_details && !selected.is_null()) {
            const auto resource = "wzmryj/" + std::to_string(market) +
                options.code + ".jsn";
            try {
                auto detail = fetch_resource(resource, options);
                detail_refreshed = detail.refreshed;
                detail_age = detail.age_seconds;
                sources.push_back(source_summary(detail.document));
                const auto normalized = normalize_foreign_alert_history_rows(
                    detail.document.at("rows"));
                int returned = 0;
                for (const auto& item : normalized.as_array()) {
                    if (returned++ >= options.detail_limit) break;
                    auto linked = item;
                    linked["security"] = selected.at("security");
                    history.push_back(std::move(linked));
                }
            } catch (const std::exception& error) {
                Json failure = Json::object();
                failure["resource"] = resource;
                failure["message"] = error.what();
                errors.push_back(std::move(failure));
            }
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-foreign-alerts-native-v1";
    result["generated_at"] = now_text();
    result["mode"] = security_mode ? "security" : "catalog";
    result["alerts"] = std::move(alerts);
    result["history"] = std::move(history);
    result["selected_alert"] = std::move(selected);
    result["summary"] = master.document.at("summary");
    result["detail_errors"] = std::move(errors);
    Json filters = Json::object();
    filters["status"] = options.status;
    filters["query"] = options.query;
    result["filters"] = std::move(filters);
    const auto upstream_health = jsn_sources_health(sources);
    const bool stale = upstream_health.at("stale").as_bool();
    result["availability"] = stale ? "stale-cache" :
        result.at("detail_errors").size() ? "partial" : "live";
    result["sources"] = std::move(sources);
    Json cache = Json::object();
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    cache["detail_refreshed"] = detail_refreshed;
    cache["detail_age_seconds"] = detail_age;
    cache["stale"] = stale;
    cache["upstream"] = upstream_health;
    result["cache"] = std::move(cache);
    Json counts = Json::object();
    counts["alerts"] = static_cast<std::uint64_t>(result.at("alerts").size());
    counts["history"] = static_cast<std::uint64_t>(result.at("history").size());
    counts["detail_errors"] =
        static_cast<std::uint64_t>(result.at("detail_errors").size());
    result["counts"] = std::move(counts);
    return result;
}

int command_market_foreign_alerts(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market foreign-alerts [options]\n\n"
            "Native foreign-ownership alert list and selected security history.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --status NAME           all|forced-reduction|suspended-buy|warning|approaching|unknown\n"
            "  --query TEXT            Filter returned securities\n"
            "  --market sz|sh|bj       Select a security with --code\n"
            "  --code CODE             Six-digit security code\n"
            "  --details               Fetch selected security history\n"
            "  --limit N               Alert rows, default 1000\n"
            "  --detail-limit N        History rows, default 2000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-foreign-alerts-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    ForeignAlertQuery query;
    query.status = lower_ascii(trim(args.take_option("--status", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.include_details = args.take_flag("--details");
    query.limit = bounded_integer(args.take_option("--limit", "1000"),
                                  "--limit", 1, 5000);
    query.detail_limit = bounded_integer(args.take_option("--detail-limit", "2000"),
                                         "--detail-limit", 1, 5000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-foreign-alerts-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    ForeignAlertService service(load_blocks(root, {}).securities);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed foreign-ownership alert query -> "
              << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
