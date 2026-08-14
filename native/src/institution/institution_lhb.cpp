#include "tdx/institution_lhb.hpp"
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
    if (!object.is_object()) throw Error("institution-LHB row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::string first_text(const Json& object,
                       std::initializer_list<std::string_view> names) {
    for (const auto name : names) {
        const auto value = text_value(object, name);
        if (!value.empty()) return value;
    }
    return {};
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

std::optional<double> first_number(
    const Json& object, std::initializer_list<std::string_view> names) {
    for (const auto name : names) {
        const auto value = number_value(object, name);
        if (value) return value;
    }
    return std::nullopt;
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
    try {
        return mainland_market_id(first_text(row, {"$SC", "sc"}));
    } catch (...) {
        return std::nullopt;
    }
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
    return "M" + std::to_string(id);
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

std::string direction(double value) {
    constexpr double epsilon = 0.000001;
    if (value > epsilon) return "net-buy";
    if (value < -epsilon) return "net-sell";
    return "flat";
}

bool direction_matches(const Json& row, const std::string& expected) {
    return expected == "all" || text_value(row, "direction") == expected;
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

Json filtered_rows(const Json& rows, const std::string& query,
                   const std::string& expected_direction, int limit) {
    Json result = Json::array();
    const auto needle = lower_ascii(trim(query));
    for (const auto& row : rows.as_array()) {
        if (!direction_matches(row, expected_direction)) continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        if (static_cast<int>(result.size()) >= limit) break;
        result.push_back(row);
    }
    return result;
}

const Json& document_for_resource(const Json& documents,
                                  std::string_view resource) {
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing institution-LHB resource: " + std::string(resource));
}

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
}

const InstitutionLhbPeriod& find_period(const std::string& name) {
    const auto found = std::find_if(
        institution_lhb_periods().begin(), institution_lhb_periods().end(),
        [&](const InstitutionLhbPeriod& period) { return period.name == name; });
    if (found == institution_lhb_periods().end())
        throw Error("period must be week, month, quarter, or year");
    return *found;
}

Json period_summary(const Json& rows, const InstitutionLhbPeriod& period) {
    std::uint64_t net_buy_count = 0, net_sell_count = 0, flat_count = 0;
    std::uint64_t participations = 0;
    double buy = 0.0, sell = 0.0;
    std::string start_date, end_date;
    for (const auto& row : rows.as_array()) {
        const auto row_direction = text_value(row, "direction");
        if (row_direction == "net-buy") ++net_buy_count;
        else if (row_direction == "net-sell") ++net_sell_count;
        else ++flat_count;
        participations += static_cast<std::uint64_t>(std::max(
            0.0, number_value(row, "institution_participations").value_or(0.0)));
        buy += number_value(row, "institution_buy_amount_yuan").value_or(0.0);
        sell += number_value(row, "institution_sell_amount_yuan").value_or(0.0);
        const auto row_start = text_value(row, "earliest_trade_date");
        const auto row_end = text_value(row, "latest_trade_date");
        if (!row_start.empty() && (start_date.empty() || row_start < start_date))
            start_date = row_start;
        if (!row_end.empty() && row_end > end_date) end_date = row_end;
    }
    Json result = Json::object();
    result["period"] = period.name;
    result["period_label"] = period.label;
    result["securities"] = static_cast<std::uint64_t>(rows.size());
    result["net_buy_securities"] = net_buy_count;
    result["net_sell_securities"] = net_sell_count;
    result["flat_securities"] = flat_count;
    result["institution_participations"] = participations;
    result["institution_buy_amount_yuan"] = buy;
    result["institution_sell_amount_yuan"] = sell;
    result["net_institution_amount_yuan"] = buy - sell;
    result["earliest_trade_date"] = start_date;
    result["latest_trade_date"] = end_date;
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

const std::vector<InstitutionLhbPeriod>& institution_lhb_periods() {
    static const std::vector<InstitutionLhbPeriod> periods{
        {"week", "最近一周", "list/func_jgzc101_1.jsn", "22801"},
        {"month", "最近一月", "list/func_jgzc102_1.jsn", "22802"},
        {"quarter", "最近三月", "list/func_jgzc103_1.jsn", "22803"},
        {"year", "最近一年", "list/func_jgzc104_1.jsn", "22804"},
    };
    return periods;
}

Json normalize_institution_lhb_rows(
    const Json& rows, const InstitutionLhbPeriod& period,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("institution-LHB master rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = row_market(row);
        const auto code = first_text(row, {"$ZQDM", "stockcode"});
        if (!market || !digits(code, 6)) continue;
        const double buy = number_value(row, "jgmr").value_or(0.0);
        const double sell = number_value(row, "jgmc").value_or(0.0);
        const double net = buy - sell;
        Json item = Json::object();
        item["period"] = period.name;
        item["period_label"] = period.label;
        item["unit_id"] = period.unit_id;
        item["security"] = security_document(*market, code, securities);
        item["institution_participations"] =
            number_value(row, "jgcys").value_or(0.0);
        item["institution_buy_amount_yuan"] = buy;
        item["institution_sell_amount_yuan"] = sell;
        item["net_institution_amount_yuan"] = net;
        item["buy_sell_ratio"] = sell > 0.0 ? Json(buy / sell) : Json(nullptr);
        item["direction"] = direction(net);
        item["latest_trade_date"] = text_value(row, "jyr1");
        item["earliest_trade_date"] = text_value(row, "jyr2");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "net_institution_amount_yuan").value_or(0.0) >
                number_value(right, "net_institution_amount_yuan").value_or(0.0);
        });
    return result;
}

Json normalize_institution_lhb_event_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("institution-LHB detail rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = row_market(row);
        const auto code = first_text(row, {"stockcode", "$ZQDM"});
        if (!market || !digits(code, 6)) continue;
        const double buy = first_number(row, {"bje", "buy_amount"}).value_or(0.0);
        const double sell = first_number(row, {"sje", "sell_amount"}).value_or(0.0);
        const double net = buy - sell;
        Json item = Json::object();
        item["security"] = security_document(*market, code, {});
        item["event_date"] = text_value(row, "date");
        item["event_type"] = text_value(row, "ydlx");
        item["event_change_pct"] =
            first_number(row, {"ydrzf", "change_pct"}).value_or(0.0);
        item["event_total_buy_amount_yuan"] = buy;
        item["event_total_sell_amount_yuan"] = sell;
        item["event_net_buy_amount_yuan"] = net;
        item["direction"] = direction(net);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "event_date") > text_value(right, "event_date");
        });
    return result;
}

InstitutionLhbService::InstitutionLhbService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

InstitutionLhbService::FetchResult InstitutionLhbService::fetch_master(
    const InstitutionLhbQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};
    std::vector<std::string> resources;
    for (const auto& period : institution_lhb_periods())
        resources.push_back(period.resource);
    const auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    Json rankings = Json::object(), summaries = Json::object(), sources = Json::array();
    for (const auto& period : institution_lhb_periods()) {
        const auto& source = document_for_resource(documents, period.resource);
        rankings[period.name] = normalize_institution_lhb_rows(
            source.at("rows"), period, securities_);
        summaries[period.name] = period_summary(rankings.at(period.name), period);
        sources.push_back(source_summary(source));
    }
    Json document = Json::object();
    document["rankings"] = std::move(rankings);
    document["period_summaries"] = std::move(summaries);
    document["sources"] = std::move(sources);
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

InstitutionLhbService::FetchResult InstitutionLhbService::fetch_resource(
    const std::string& resource, const InstitutionLhbQuery& options) {
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

Json InstitutionLhbService::query(const InstitutionLhbQuery& options) {
    const auto& period = find_period(options.period);
    const std::set<std::string> directions{"all", "net-buy", "net-sell", "flat"};
    if (!directions.count(options.direction))
        throw Error("direction must be all, net-buy, net-sell, or flat");
    if (options.limit < 1 || options.limit > 5000 ||
        options.detail_limit < 1 || options.detail_limit > 5000)
        throw Error("institution-LHB limits are outside the supported range");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    const bool security_mode = !options.code.empty();
    const int market = security_mode ? mainland_market_id(options.market) : -1;
    if (security_mode && !digits(options.code, 6))
        throw Error("code must contain six digits");

    auto master = fetch_master(options);
    const auto& period_rows = master.document.at("rankings").at(period.name);
    Json selected = Json(nullptr);
    for (const auto& row : period_rows.as_array()) {
        if (!security_mode) continue;
        const auto& security = row.at("security");
        if (static_cast<int>(security.at("market_id").as_number()) == market &&
            text_value(security, "code") == options.code) {
            selected = row;
            break;
        }
    }

    Json candidates = security_mode ? Json::array() : period_rows;
    if (security_mode && !selected.is_null()) candidates.push_back(selected);
    Json rankings = filtered_rows(
        candidates, options.query, options.direction, options.limit);
    Json events = Json::array(), errors = Json::array();
    Json sources = master.document.at("sources");
    bool detail_refreshed = false;
    int detail_age = 0;
    if (security_mode && options.include_details && !selected.is_null()) {
            const auto resource = "lsyd" + period.unit_id + "/" +
                std::to_string(market) + options.code + ".jsn";
            try {
                auto detail = fetch_resource(resource, options);
                detail_refreshed = detail.refreshed;
                detail_age = detail.age_seconds;
                sources.push_back(source_summary(detail.document));
                const auto normalized = normalize_institution_lhb_event_rows(
                    detail.document.at("rows"));
                int returned = 0;
                for (const auto& event : normalized.as_array()) {
                    if (returned++ >= options.detail_limit) break;
                    auto linked = event;
                    linked["security"] = selected.at("security");
                    const auto event_date = text_value(linked, "event_date");
                    const auto range_start = text_value(selected, "earliest_trade_date");
                    const auto range_end = text_value(selected, "latest_trade_date");
                    linked["outside_master_date_range"] = !event_date.empty() &&
                        ((!range_start.empty() && event_date < range_start) ||
                         (!range_end.empty() && event_date > range_end));
                    events.push_back(std::move(linked));
                }
            } catch (const std::exception& error) {
                Json failure = Json::object();
                failure["resource"] = resource;
                failure["message"] = error.what();
                errors.push_back(std::move(failure));
            }
    }

    Json result = Json::object();
    result["schema"] = "tdx-market-institution-lhb-native-v1";
    result["generated_at"] = now_text();
    result["period"] = period.name;
    result["period_label"] = period.label;
    result["mode"] = security_mode ? "security" : "catalog";
    result["rankings"] = std::move(rankings);
    result["events"] = std::move(events);
    result["selected_ranking"] = std::move(selected);
    result["summary"] = master.document.at("period_summaries").at(period.name);
    result["period_summaries"] = master.document.at("period_summaries");
    result["detail_errors"] = std::move(errors);
    Json filters = Json::object();
    filters["direction"] = options.direction;
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
    counts["rankings"] = static_cast<std::uint64_t>(result.at("rankings").size());
    counts["events"] = static_cast<std::uint64_t>(result.at("events").size());
    counts["detail_errors"] =
        static_cast<std::uint64_t>(result.at("detail_errors").size());
    result["counts"] = std::move(counts);
    return result;
}

int command_market_institution_lhb(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market institution-lhb [options]\n\n"
            "Native institution-seat rankings and linked anomaly events.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --period NAME           week|month|quarter|year\n"
            "  --direction NAME        all|net-buy|net-sell|flat\n"
            "  --query TEXT            Filter returned stocks\n"
            "  --market sz|sh|bj       Select a security with --code\n"
            "  --code CODE             Six-digit security code\n"
            "  --details               Fetch selected stock anomaly events\n"
            "  --limit N               Ranking rows, default 3000\n"
            "  --detail-limit N        Event rows, default 1000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-institution-lhb-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    InstitutionLhbQuery query;
    query.period = lower_ascii(trim(args.take_option("--period", "week")));
    query.direction = lower_ascii(trim(args.take_option("--direction", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.include_details = args.take_flag("--details");
    query.limit = bounded_integer(args.take_option("--limit", "3000"),
                                  "--limit", 1, 5000);
    query.detail_limit = bounded_integer(args.take_option("--detail-limit", "1000"),
                                         "--detail-limit", 1, 5000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-institution-lhb-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    InstitutionLhbService service(load_blocks(root, {}).securities);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("period").as_string()
              << " institution-LHB query -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
