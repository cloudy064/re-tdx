#include "tdx/valuation.hpp"
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
#include <map>
#include <optional>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr const char* kMasterResource = "list/func_zsgz101_1.jsn";

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
    if (!object.is_object()) throw Error("valuation row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

Json copy_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? *value : Json(nullptr);
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

std::optional<double> numeric_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string() || value->as_string().empty() || value->as_string() == "--")
        return std::nullopt;
    try {
        std::size_t used = 0;
        const double parsed = std::stod(value->as_string(), &used);
        return used == value->as_string().size() && std::isfinite(parsed)
            ? std::optional<double>(parsed) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    if (normalized == "2" || normalized == "bj") return 2;
    throw Error("market must be sz/sh/bj or 0/1/2");
}

std::string market_name(int value) {
    return value == 0 ? "sz" : value == 1 ? "sh" : value == 2 ? "bj"
                                                               : "m" + std::to_string(value);
}

std::string market_prefix(int value) {
    return value == 0 ? "SZ" : value == 1 ? "SH" : value == 2 ? "BJ"
                                                               : "M" + std::to_string(value);
}

bool six_digits(const std::string& value) {
    return value.size() == 6 && std::all_of(value.begin(), value.end(), [](char ch) {
        return ch >= '0' && ch <= '9';
    });
}

bool valid_date(const std::string& value, bool optional = true) {
    if (value.empty()) return optional;
    return value.size() == 8 && std::all_of(value.begin(), value.end(), [](char ch) {
        return ch >= '0' && ch <= '9';
    });
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

Json security_document(const std::string& market, const std::string& code,
                       const std::map<std::pair<int, std::string>, Security>& securities) {
    const int id = market_id(market);
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

Json source_summary(const Json& document) {
    Json result = Json::object();
    for (const auto* key : {"resource", "size", "row_count", "endpoint"})
        result[key] = document.at(key);
    return result;
}

const Json& document_for_resource(const Json& documents, const std::string& resource) {
    if (!documents.is_array()) throw Error("valuation documents must be an array");
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing valuation resource: " + resource);
}

Json range_summary(const Json& history, std::string_view field) {
    std::optional<double> minimum, maximum, latest;
    for (const auto& row : history.as_array()) {
        const auto current = numeric_value(row, field);
        if (!current) continue;
        if (!minimum || *current < *minimum) minimum = current;
        if (!maximum || *current > *maximum) maximum = current;
        latest = current;
    }
    Json result = Json::object();
    result["minimum"] = minimum ? Json(*minimum) : Json(nullptr);
    result["maximum"] = maximum ? Json(*maximum) : Json(nullptr);
    result["latest"] = latest ? Json(*latest) : Json(nullptr);
    return result;
}

Json filter_history(const Json& history, const ValuationQuery& options) {
    Json result = Json::array();
    for (const auto& row : history.as_array()) {
        const auto date = text_value(row, "date");
        if (!options.start_date.empty() && date < options.start_date) continue;
        if (!options.end_date.empty() && date > options.end_date) continue;
        result.push_back(row);
    }
    if (result.size() > static_cast<std::size_t>(options.limit)) {
        Json limited = Json::array();
        const auto begin = result.size() - static_cast<std::size_t>(options.limit);
        for (std::size_t index = begin; index < result.size(); ++index)
            limited.push_back(result.as_array()[index]);
        return limited;
    }
    return result;
}

}  // namespace

Json normalize_valuation_master_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("valuation master rows must be an array");
    Json result = Json::array();
    std::set<std::tuple<int, std::string, std::string>> identities;
    for (const auto& row : rows.as_array()) {
        const auto market = text_value(row, "$SC1");
        const auto code = text_value(row, "$ZQDM1");
        const auto detail_id = text_value(row, "$ZQDM");
        if (!six_digits(code) || !six_digits(detail_id))
            throw Error("valuation master security or detail ID is invalid");
        const int id = market_id(market);
        if (!identities.emplace(id, code, detail_id).second)
            throw Error("valuation master contains a duplicate index key");
        Json value = Json::object();
        value["date"] = copy_value(row, "date");
        value["detail_id"] = detail_id;
        value["security"] = security_document(market, code, securities);
        Json metrics = Json::object();
        metrics["pe"] = copy_value(row, "pe");
        metrics["pe_percentile"] = copy_value(row, "pefws");
        metrics["pb"] = copy_value(row, "pb");
        metrics["pb_percentile"] = copy_value(row, "pbfws");
        metrics["dividend_yield"] = copy_value(row, "gxl");
        metrics["roe"] = copy_value(row, "roe");
        metrics["earnings_yield"] = copy_value(row, "syl");
        metrics["valuation_label"] = copy_value(row, "gzsp");
        value["metrics"] = std::move(metrics);
        Json returns = Json::object();
        returns["days_5"] = copy_value(row, "jwzf");
        returns["days_10"] = copy_value(row, "jszf");
        returns["days_20"] = copy_value(row, "jezf");
        returns["days_30"] = copy_value(row, "jsszf");
        value["returns"] = std::move(returns);
        value["history_start_date"] = copy_value(row, "jsksrq");
        result.push_back(std::move(value));
    }
    std::sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return left.at("security").at("security_id").as_string() <
                   right.at("security").at("security_id").as_string();
        });
    return result;
}

Json normalize_valuation_fund_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("valuation fund rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto market = text_value(row, "$SC");
        const auto code = text_value(row, "$ZQDM");
        if (!six_digits(code)) throw Error("valuation related fund code is invalid");
        Json value = security_document(market, code, securities);
        value["fund_type"] = copy_value(row, "jjlx");
        value["net_asset_value"] = copy_value(row, "dwjz");
        value["premium_pct"] = copy_value(row, "yjl");
        value["latest_shares"] = copy_value(row, "zxfe");
        value["minimum_redemption_unit"] = copy_value(row, "zxssdw");
        result.push_back(std::move(value));
    }
    return result;
}

Json merge_valuation_history_rows(const Json& pe_rows, const Json& pb_rows) {
    if (!pe_rows.is_array() || !pb_rows.is_array())
        throw Error("valuation history rows must be arrays");
    std::map<std::string, Json> values;
    const auto ensure = [&](const std::string& date) -> Json& {
        auto [found, inserted] = values.emplace(date, Json::object());
        if (inserted) {
            found->second["date"] = date;
            found->second["pe"] = nullptr;
            found->second["pe_percentile"] = nullptr;
            found->second["pb"] = nullptr;
            found->second["pb_percentile"] = nullptr;
        }
        return found->second;
    };
    for (const auto& row : pe_rows.as_array()) {
        const auto date = text_value(row, "date");
        if (!valid_date(date, false)) throw Error("valuation PE history date is invalid");
        auto& value = ensure(date);
        value["pe"] = copy_value(row, "pe");
        value["pe_percentile"] = copy_value(row, "pebfw");
    }
    for (const auto& row : pb_rows.as_array()) {
        const auto date = text_value(row, "date");
        if (!valid_date(date, false)) throw Error("valuation PB history date is invalid");
        auto& value = ensure(date);
        value["pb"] = copy_value(row, "pb");
        value["pb_percentile"] = copy_value(row, "pbbfw");
    }
    Json result = Json::array();
    for (auto& [date, value] : values) result.push_back(std::move(value));
    return result;
}

ValuationService::ValuationService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

ValuationService::FetchResult ValuationService::fetch_master(
    const ValuationQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};
    auto resource = fetch_jsn_resource_rows(kMasterResource, "bi", options.timeout_ms);
    Json document = Json::object();
    document["indices"] = normalize_valuation_master_rows(resource.at("rows"), securities_);
    Json sources = Json::array();
    sources.push_back(source_summary(resource));
    document["sources"] = std::move(sources);
    master_cache_ = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

ValuationService::FetchResult ValuationService::fetch_detail(
    const std::string& detail_id, const ValuationQuery& options) {
    const auto now = std::time(nullptr);
    for (auto item = detail_cache_.begin(); item != detail_cache_.end();) {
        if (now - item->second.fetched_at >= options.detail_cache_ttl_seconds)
            item = detail_cache_.erase(item);
        else ++item;
    }
    const auto cached = detail_cache_.find(detail_id);
    const int age = cached == detail_cache_.end() ? 0
        : static_cast<int>(std::max<std::time_t>(0, now - cached->second.fetched_at));
    if (!options.refresh && cached != detail_cache_.end())
        return {cached->second.document, false, age};
    const std::vector<std::string> resources{
        "zsgz1/" + detail_id + ".jsn",
        "zsgz3/" + detail_id + ".jsn",
        "zsgz4/" + detail_id + ".jsn"};
    const auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    const auto& funds = document_for_resource(documents, resources[0]);
    const auto& pe = document_for_resource(documents, resources[1]);
    const auto& pb = document_for_resource(documents, resources[2]);
    Json document = Json::object();
    document["funds"] = normalize_valuation_fund_rows(funds.at("rows"), securities_);
    document["history"] = merge_valuation_history_rows(pe.at("rows"), pb.at("rows"));
    Json sources = Json::array();
    for (const auto& resource : documents.as_array()) sources.push_back(source_summary(resource));
    document["sources"] = std::move(sources);
    detail_cache_[detail_id] = {document, std::time(nullptr)};
    return {std::move(document), true, 0};
}

Json ValuationService::query(const ValuationQuery& options) {
    if (options.limit < 1 || options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (!valid_date(options.start_date) || !valid_date(options.end_date))
        throw Error("valuation dates must use YYYYMMDD");
    if (!options.start_date.empty() && !options.end_date.empty() &&
        options.start_date > options.end_date)
        throw Error("start_date must not be after end_date");
    if (options.master_cache_ttl_seconds < 0 || options.master_cache_ttl_seconds > 86400 ||
        options.detail_cache_ttl_seconds < 0 || options.detail_cache_ttl_seconds > 86400)
        throw Error("valuation cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const bool security_mode = !options.market.empty() || !options.code.empty();
    if (security_mode && (options.market.empty() || options.code.empty()))
        throw Error("security selection requires both market and code");
    if (security_mode && !six_digits(options.code))
        throw Error("code must contain six digits");
    if (!options.detail_id.empty() && !six_digits(options.detail_id))
        throw Error("detail_id must contain six digits");
    if (security_mode && !options.detail_id.empty())
        throw Error("choose market/code or detail_id, not both");

    auto master = fetch_master(options);
    const Json* selected = nullptr;
    int selected_market = -1;
    if (security_mode) selected_market = market_id(options.market);
    for (const auto& index : master.document.at("indices").as_array()) {
        if (security_mode &&
            static_cast<int>(index.at("security").at("market_id").as_number()) == selected_market &&
            index.at("security").at("code").as_string() == options.code)
            selected = &index;
        if (!options.detail_id.empty() && index.at("detail_id").as_string() == options.detail_id)
            selected = &index;
    }
    const bool detail_mode = security_mode || !options.detail_id.empty() || options.include_details;
    if (detail_mode && !selected) {
        if (options.include_details && !security_mode && options.detail_id.empty() &&
            master.document.at("indices").size())
            selected = &master.document.at("indices").as_array().front();
        else throw Error("selected valuation index is not present in the current master table");
    }

    Json cache = Json::object();
    cache["master_ttl_seconds"] = options.master_cache_ttl_seconds;
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    Json result = Json::object();
    result["schema"] = "tdx-market-valuation-native-v1";
    result["generated_at"] = now_text();
    result["mode"] = selected ? "index" : "master";
    result["indices"] = master.document.at("indices");
    Json counts = Json::object();
    counts["indices"] = static_cast<std::uint64_t>(result.at("indices").size());
    counts["related_funds"] = 0;
    counts["history_points"] = 0;
    result["selected"] = selected ? *selected : Json(nullptr);
    result["related_funds"] = Json::array();
    result["history"] = Json::array();
    result["summary"] = Json::object();
    result["sources"] = master.document.at("sources");
    if (selected) {
        auto detail = fetch_detail(selected->at("detail_id").as_string(), options);
        const auto history = filter_history(detail.document.at("history"), options);
        result["related_funds"] = detail.document.at("funds");
        result["history"] = history;
        for (const auto& source : detail.document.at("sources").as_array())
            result["sources"].push_back(source);
        counts["related_funds"] = static_cast<std::uint64_t>(result.at("related_funds").size());
        counts["history_points"] = static_cast<std::uint64_t>(history.size());
        counts["full_history_points"] = static_cast<std::uint64_t>(detail.document.at("history").size());
        Json summary = Json::object();
        summary["first_date"] = history.size() ? history.as_array().front().at("date") : Json(nullptr);
        summary["last_date"] = history.size() ? history.as_array().back().at("date") : Json(nullptr);
        summary["pe"] = range_summary(history, "pe");
        summary["pb"] = range_summary(history, "pb");
        result["summary"] = std::move(summary);
        cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
        cache["detail_refreshed"] = detail.refreshed;
        cache["detail_age_seconds"] = detail.age_seconds;
    }
    result["counts"] = std::move(counts);
    result["cache"] = std::move(cache);
    return result;
}

int command_market_valuation(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market valuation [options]\n\n"
            "Native market-index PE/PB percentile, history, and related ETF query.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --market sz|sh|bj       Select one index with --code\n"
            "  --code CODE             Index code from the valuation master\n"
            "  --detail-id ID          Select hidden valuation detail ID\n"
            "  --details               Expand the first master row when no key is given\n"
            "  --start-date YYYYMMDD    Filter history\n"
            "  --end-date YYYYMMDD      Filter history\n"
            "  --limit N               Most recent history points, default 4000\n"
            "  --timeout-ms N          Default 15000\n"
            "  --output PATH           Default output/tdx-market-valuation-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    ValuationQuery query;
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.detail_id = trim(args.take_option("--detail-id"));
    query.include_details = args.take_flag("--details");
    query.start_date = trim(args.take_option("--start-date"));
    query.end_date = trim(args.take_option("--end-date"));
    query.limit = bounded_integer(args.take_option("--limit", "4000"),
                                  "--limit", 1, 10000);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "15000"),
                                       "--timeout-ms", 100, 60000);
    query.refresh = true;
    const auto output = native_path(args.take_option(
        "--output", "output/tdx-market-valuation-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    ValuationService service(load_blocks(root, {}).securities);
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("mode").as_string()
              << " market valuation query with "
              << document.at("counts").at("history_points").as_number()
              << " history points -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
