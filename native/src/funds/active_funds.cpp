#include "tdx/active_funds.hpp"
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

constexpr const char* master_resource = "list/func_zdjjzczc101_1.jsn";

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
    if (!object.is_object()) throw Error("active-fund row must be an object");
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
    try {
        return mainland_market_id(text_value(row, "$SC"));
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

std::string change_direction(double current_value, double change) {
    constexpr double epsilon = 0.000001;
    if (std::abs(change) <= epsilon) return "unchanged";
    if (change > 0.0 && std::abs(current_value - change) <= epsilon)
        return "new";
    return change > 0.0 ? "increased" : "decreased";
}

bool direction_matches(const Json& row, const std::string& direction) {
    if (direction == "all") return true;
    const auto actual = text_value(row, "direction");
    if (direction == "increased") return actual == "increased" || actual == "new";
    return actual == direction;
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
                   const std::string& direction, int limit) {
    Json result = Json::array();
    const auto needle = lower_ascii(trim(query));
    for (const auto& row : rows.as_array()) {
        if (!direction_matches(row, direction)) continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        if (static_cast<int>(result.size()) >= limit) break;
        result.push_back(row);
    }
    return result;
}

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
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

Json normalize_active_fund_security_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::map<std::pair<int, std::string>, std::string>& industries) {
    if (!rows.is_array()) throw Error("active-fund security rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = row_market(row);
        const auto code = text_value(row, "$ZQDM");
        if (!market || !digits(code, 6)) continue;
        const auto holding_value = number_value(row, "zcsz");
        const auto value_change = number_value(row, "zcszbd");
        const auto float_market_cap = number_value(row, "jzrsz");
        Json item = Json::object();
        item["security"] = security_document(*market, code, securities);
        item["start_date"] = text_value(row, "ksrq");
        item["end_date"] = text_value(row, "jzrs");
        item["period_end_float_market_cap_yuan"] = number_or_null(float_market_cap);
        item["period_end_total_market_cap_yuan"] =
            number_or_null(number_value(row, "jzrzsz"));
        item["holding_market_value_yuan"] = number_or_null(holding_value);
        item["holding_market_value_change_yuan"] = number_or_null(value_change);
        item["holding_pct_float"] = holding_value && float_market_cap &&
                std::abs(*float_market_cap) > 0.000001
            ? Json(*holding_value * 100.0 / *float_market_cap) : Json(nullptr);
        item["holding_shares"] = number_or_null(number_value(row, "zcsl"));
        item["holding_share_change"] = number_or_null(number_value(row, "zcslbd"));
        item["fund_count"] = number_or_null(number_value(row, "zcjs"));
        item["direction"] = change_direction(
            holding_value.value_or(0.0), value_change.value_or(0.0));
        const auto industry = industries.find({*market, code});
        item["industry"] = industry == industries.end() ? "" : industry->second;
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "holding_market_value_change_yuan").value_or(0.0) >
                number_value(right, "holding_market_value_change_yuan").value_or(0.0);
        });
    return result;
}

Json normalize_active_fund_detail_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("active-fund detail rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        const auto market_text = text_value(row, "$SC");
        if (code.empty() || market_text.empty()) continue;
        int market = -1;
        try {
            std::size_t used = 0;
            market = std::stoi(market_text, &used);
            if (used != market_text.size()) continue;
        } catch (...) {
            continue;
        }
        Json fund = Json::object();
        fund["market_id"] = market;
        fund["code"] = code;
        fund["fund_id"] = "FUND" + market_text + "-" + code;
        fund["name"] = text_value(row, "jjmc");
        Json item = Json::object();
        item["fund"] = std::move(fund);
        item["holding_market_value_yuan"] = number_or_null(number_value(row, "cgsz"));
        item["holding_shares"] = number_or_null(number_value(row, "cgsl"));
        item["nav_pct"] = number_or_null(number_value(row, "zjzb"));
        item["holding_rank"] = number_or_null(number_value(row, "cgwl"));
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return number_value(left, "holding_market_value_yuan").value_or(0.0) >
                number_value(right, "holding_market_value_yuan").value_or(0.0);
        });
    return result;
}

ActiveFundService::ActiveFundService(BlockData data, Fetcher fetcher)
    : securities_(std::move(data.securities)), fetcher_(std::move(fetcher)) {
    std::map<std::string, std::string> names;
    for (const auto& block : data.blocks) {
        if (block.family == "research-industry" && block.level == 1)
            names[block.block_code] = block.name;
    }
    for (const auto& member : data.members) {
        if (member.family != "research-industry") continue;
        const auto found = names.find(member.block_code);
        if (found != names.end())
            security_industries_.emplace(
                std::make_pair(member.market_id, member.code), found->second);
    }
}

ActiveFundService::FetchResult ActiveFundService::fetch_master(
    const ActiveFundQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age, false, {}};
    Json source;
    try {
        source = fetcher_
            ? fetcher_(master_resource, "bi", options.timeout_ms)
            : fetch_jsn_resource_rows(master_resource, "bi", options.timeout_ms);
    } catch (const std::exception& error) {
        if (master_cache_.fetched_at)
            return {master_cache_.document, false, age, true, error.what()};
        throw;
    }
    Json document = Json::object();
    document["securities"] = normalize_active_fund_security_rows(
        source.at("rows"), securities_, security_industries_);

    std::uint64_t increased = 0, decreased = 0, newly_held = 0, unchanged = 0;
    double total_value = 0.0, total_change = 0.0, total_shares = 0.0;
    std::uint64_t fund_positions = 0;
    std::set<std::string> periods;
    for (const auto& row : document.at("securities").as_array()) {
        const auto direction = text_value(row, "direction");
        if (direction == "new") { ++newly_held; ++increased; }
        else if (direction == "increased") ++increased;
        else if (direction == "decreased") ++decreased;
        else ++unchanged;
        total_value += number_value(row, "holding_market_value_yuan").value_or(0.0);
        total_change += number_value(row, "holding_market_value_change_yuan").value_or(0.0);
        total_shares += number_value(row, "holding_shares").value_or(0.0);
        fund_positions += static_cast<std::uint64_t>(
            std::max(0.0, number_value(row, "fund_count").value_or(0.0)));
        periods.insert(text_value(row, "start_date") + "-" + text_value(row, "end_date"));
    }
    Json summary = Json::object();
    summary["securities"] = static_cast<std::uint64_t>(document.at("securities").size());
    summary["increased"] = increased;
    summary["decreased"] = decreased;
    summary["newly_held"] = newly_held;
    summary["unchanged"] = unchanged;
    summary["fund_positions"] = fund_positions;
    summary["total_holding_market_value_yuan"] = total_value;
    summary["total_holding_market_value_change_yuan"] = total_change;
    summary["total_holding_shares"] = total_shares;
    summary["period_count"] = static_cast<std::uint64_t>(periods.size());
    Json period_values = Json::array();
    for (const auto& period : periods) period_values.push_back(period);
    summary["periods"] = std::move(period_values);
    document["summary"] = std::move(summary);
    document["source"] = source_summary(source);
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0, false, {}};
}

ActiveFundService::FetchResult ActiveFundService::fetch_resource(
    const std::string& resource, const ActiveFundQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = failure_cache_.begin(); item != failure_cache_.end();) {
        if (now - item->second.second >= options.detail_cache_ttl_seconds)
            item = failure_cache_.erase(item);
        else ++item;
    }
    if (options.refresh) {
        failure_cache_.erase(resource);
    }
    const auto cached = resource_cache_.find(resource);
    const int age = cached != resource_cache_.end()
        ? static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at)) : 0;
    if (!options.refresh && cached != resource_cache_.end() &&
        age < options.detail_cache_ttl_seconds)
        return {cached->second.document, false, age, false, {}};
    const auto failed = failure_cache_.find(resource);
    if (failed != failure_cache_.end()) {
        if (cached != resource_cache_.end())
            return {cached->second.document, false, age, true, failed->second.first};
        throw Error(failed->second.first);
    }
    try {
        const auto document = fetcher_
            ? fetcher_(resource, "bi", options.timeout_ms)
            : fetch_jsn_resource_rows(resource, "bi", options.timeout_ms);
        resource_cache_[resource] = {document, std::time(nullptr)};
        failure_cache_.erase(resource);
        return {document, true, 0, false, {}};
    } catch (const std::exception& error) {
        failure_cache_[resource] = {error.what(), std::time(nullptr)};
        if (cached != resource_cache_.end())
            return {cached->second.document, false, age, true, error.what()};
        throw;
    }
}

Json ActiveFundService::query(const ActiveFundQuery& options) {
    const std::set<std::string> views{"securities", "funds"};
    if (!views.count(options.view)) throw Error("view must be securities or funds");
    const std::set<std::string> directions{
        "all", "increased", "decreased", "new", "unchanged"};
    if (!directions.count(options.direction))
        throw Error("direction must be all, increased, decreased, new, or unchanged");
    if (options.limit < 1 || options.limit > 5000 ||
        options.detail_limit < 1 || options.detail_limit > 5000)
        throw Error("active-fund limits are outside the supported range");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be provided together");
    if (options.view == "funds" && options.code.empty())
        throw Error("funds view requires market and code");

    const bool security_mode = !options.code.empty();
    const int market = security_mode ? mainland_market_id(options.market) : -1;
    if (security_mode && !digits(options.code, 6))
        throw Error("code must contain six digits");
    auto master = fetch_master(options);
    Json selected = Json(nullptr);
    for (const auto& row : master.document.at("securities").as_array()) {
        if (!security_mode) continue;
        const auto& security = row.at("security");
        if (static_cast<int>(security.at("market_id").as_number()) == market &&
            text_value(security, "code") == options.code) {
            selected = row;
            break;
        }
    }

    Json securities = Json::array(), funds = Json::array(), errors = Json::array();
    Json sources = Json::array();
    sources.push_back(master.document.at("source"));
    bool detail_refreshed = false;
    bool detail_stale = false;
    int detail_age = 0;
    std::string detail_upstream_error;
    if (options.view == "securities") {
        Json candidates = security_mode ? Json::array() : master.document.at("securities");
        if (security_mode && !selected.is_null()) candidates.push_back(selected);
        securities = filtered_rows(
            candidates, options.query, options.direction, options.limit);
    } else if (!selected.is_null()) {
        const auto resource = "zdjjzczc/" + std::to_string(market) +
            options.code + ".jsn";
        try {
            auto detail = fetch_resource(resource, options);
            detail_refreshed = detail.refreshed;
            detail_stale = detail.stale;
            detail_age = detail.age_seconds;
            detail_upstream_error = detail.upstream_error;
            sources.push_back(source_summary(detail.document));
            const auto normalized = normalize_active_fund_detail_rows(
                detail.document.at("rows"));
            funds = filtered_rows(normalized, options.query, "all", options.detail_limit);
        } catch (const std::exception& error) {
            Json failure = Json::object();
            failure["resource"] = resource;
            failure["message"] = error.what();
            errors.push_back(std::move(failure));
        }
    }

    const auto upstream_health = jsn_sources_health(sources);
    const bool upstream_stale = upstream_health.at("stale").as_bool();
    Json result = Json::object();
    result["schema"] = "tdx-market-active-funds-native-v1";
    result["availability"] = master.stale || detail_stale || upstream_stale
        ? "stale-cache" : "live";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["mode"] = security_mode ? "security" : "catalog";
    result["securities"] = std::move(securities);
    result["funds"] = std::move(funds);
    result["selected_holding"] = std::move(selected);
    result["summary"] = master.document.at("summary");
    result["detail_errors"] = std::move(errors);
    Json filters = Json::object();
    filters["direction"] = options.direction;
    filters["query"] = options.query;
    result["filters"] = std::move(filters);
    result["sources"] = std::move(sources);
    Json cache = Json::object();
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    cache["master_stale"] = master.stale;
    cache["master_upstream_error"] = master.upstream_error.empty()
        ? Json(nullptr) : Json(master.upstream_error);
    cache["detail_refreshed"] = detail_refreshed;
    cache["detail_age_seconds"] = detail_age;
    cache["detail_stale"] = detail_stale;
    cache["detail_upstream_error"] = detail_upstream_error.empty()
        ? Json(nullptr) : Json(detail_upstream_error);
    cache["upstream"] = upstream_health;
    cache["stale"] = master.stale || detail_stale || upstream_stale;
    result["cache"] = std::move(cache);
    Json counts = Json::object();
    counts["securities"] = static_cast<std::uint64_t>(result.at("securities").size());
    counts["funds"] = static_cast<std::uint64_t>(result.at("funds").size());
    counts["detail_errors"] =
        static_cast<std::uint64_t>(result.at("detail_errors").size());
    result["counts"] = std::move(counts);
    return result;
}

int command_market_active_funds(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market active-funds [options]\n\n"
            "Native active-fund quarterly holdings and stock-to-fund details.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --view NAME             securities|funds\n"
            "  --direction NAME        all|increased|decreased|new|unchanged\n"
            "  --query TEXT            Filter returned stocks or funds\n"
            "  --market sz|sh|bj       Select a security with --code\n"
            "  --code CODE             Six-digit security code\n"
            "  --limit N               Stock rows, default 2000\n"
            "  --detail-limit N        Fund rows, default 2000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-active-funds-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    ActiveFundQuery query;
    query.view = lower_ascii(trim(args.take_option("--view", "securities")));
    query.direction = lower_ascii(trim(args.take_option("--direction", "all")));
    query.query = trim(args.take_option("--query"));
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.limit = bounded_integer(args.take_option("--limit", "2000"),
                                  "--limit", 1, 5000);
    query.detail_limit = bounded_integer(args.take_option("--detail-limit", "2000"),
                                         "--detail-limit", 1, 5000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-active-funds-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(
        root_text.empty() ? fs::path{} : native_path(root_text));
    ActiveFundService service(load_blocks(root, {"research-industry"}));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("view").as_string()
              << " active-fund query -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
