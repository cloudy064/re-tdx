#include "tdx/lhb.hpp"
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
#include <tuple>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct EventAggregate {
    std::string event_id;
    std::vector<std::pair<std::string, std::string>> key_candidates;
    std::vector<std::string> dates;
    std::vector<std::string> views;
    std::vector<std::string> event_types;
    Json master_records{Json::array()};
    Json primary_record{Json::object()};
};

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

fs::path native_path(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

const Json* value_ptr(const Json& object, std::string_view name) {
    if (!object.is_object()) throw Error("LHB row must be an object");
    const auto found = object.as_object().find(name);
    return found == object.as_object().end() ? nullptr : &found->second;
}

std::string text_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? trim(jsn_scalar_text(*value)) : std::string{};
}

Json copy_value(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    return value ? *value : Json("");
}

void append_unique(std::vector<std::string>& values, const std::string& value) {
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end())
        values.push_back(value);
}

void append_unique(std::vector<std::pair<std::string, std::string>>& values,
                   const std::pair<std::string, std::string>& value) {
    if (std::find(values.begin(), values.end(), value) == values.end())
        values.push_back(value);
}

int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
    // 通达信部分 JSN 主表仍使用原始市场号 44 表示北交所。
    if (normalized == "2" || normalized == "44" || normalized == "bj") return 2;
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

std::optional<double> numeric(const Json& object, std::string_view name) {
    const auto* value = value_ptr(object, name);
    if (!value || value->is_null()) return std::nullopt;
    if (value->is_number()) return value->as_number();
    if (!value->is_string() || value->as_string().empty()) return std::nullopt;
    try {
        std::size_t used = 0;
        const double result = std::stod(value->as_string(), &used);
        if (used != value->as_string().size() || !std::isfinite(result))
            return std::nullopt;
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

const Json& document_for_resource(const Json& documents, const std::string& resource) {
    if (!documents.is_array()) throw Error("LHB master documents must be an array");
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("missing LHB master resource: " + resource);
}

Json string_array(const std::vector<std::string>& values) {
    Json result = Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

Json metric_document(const Json& row) {
    Json metrics = Json::object();
    metrics["buy_share_pct"] = copy_value(row, "bzb");
    metrics["sell_share_pct"] = copy_value(row, "szb");
    metrics["net_buy"] = copy_value(row, "jmr");
    metrics["buy_amount"] = copy_value(row, "zmr");
    metrics["sell_amount"] = copy_value(row, "zmc");
    metrics["institution_buy_count"] = copy_value(row, "sl1");
    metrics["institution_sell_count"] = copy_value(row, "sl2");
    metrics["category"] = copy_value(row, "lb");
    return metrics;
}

Json security_document(
    const std::string& market, const std::string& code,
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

double metric_number(const Json& event, std::string_view name) {
    const auto& metrics = event.at("metrics");
    const auto value = numeric(metrics, name);
    return value.value_or(0.0);
}

bool missing_jsn_resource_error(const std::string& message) {
    return message.find("zero-length JSN resource") != std::string::npos;
}

}  // namespace

const std::vector<LhbView>& lhb_views() {
    static const std::vector<LhbView> views{
        {"龙虎榜还原", "list/func_lhbfx101_1.jsn"},
        {"机构参与", "list/func_lhbfx103_1.jsn"},
        {"市场风口", "list/func_lhbfx104_1.jsn"},
        {"合力封板", "list/func_lhbfx105_1.jsn"},
        {"一家独大", "list/func_lhbfx106_1.jsn"},
        {"量化席位", "list/func_lhbfx107_1.jsn"},
        {"同城携手", "list/func_lhbfx108_1.jsn"},
        {"游资席位", "list/func_lhbfx110_1.jsn"},
    };
    return views;
}

Json aggregate_lhb_master_documents(
    const Json& documents,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::string& generated_at) {
    std::map<std::string, EventAggregate> aggregates;
    Json sources = Json::array();
    std::uint64_t master_rows = 0;
    for (std::size_t priority = 0; priority < lhb_views().size(); ++priority) {
        const auto& view = lhb_views()[priority];
        const auto& document = document_for_resource(documents, view.resource);
        const auto& rows = document.at("rows");
        if (!rows.is_array()) throw Error("LHB master rows must be an array");
        Json source = jsn_source_metadata(document);
        source["name"] = view.name;
        sources.push_back(std::move(source));
        master_rows += static_cast<std::uint64_t>(rows.size());
        for (const auto& row : rows.as_array()) {
            const auto event_id = text_value(row, "$ZQDM");
            const auto market = text_value(row, "$SC1");
            const auto code = text_value(row, "$ZQDM1");
            if (event_id.empty() || market.empty() || code.empty())
                throw Error(view.resource + ": LHB event or security key is empty");
            auto [found, inserted] = aggregates.emplace(event_id, EventAggregate{});
            auto& event = found->second;
            if (inserted) {
                event.event_id = event_id;
                event.primary_record = row;
            }
            append_unique(event.key_candidates, {market, code});
            append_unique(event.dates, text_value(row, "date").empty()
                                            ? text_value(row, "rq")
                                            : text_value(row, "date"));
            append_unique(event.views, view.name);
            append_unique(event.event_types, text_value(row, "lx").empty()
                                                ? text_value(row, "sblx")
                                                : text_value(row, "lx"));
            Json master = Json::object();
            master["view"] = view.name;
            master["view_priority"] = static_cast<std::uint64_t>(priority);
            master["resource"] = view.resource;
            master["record"] = row;
            event.master_records.push_back(std::move(master));
        }
    }

    Json events = Json::array();
    std::set<std::pair<std::string, std::string>> stocks;
    std::uint64_t conflicts = 0;
    for (auto& [event_id, event] : aggregates) {
        std::sort(event.dates.begin(), event.dates.end(), std::greater<>());
        const auto& primary_key = event.key_candidates.front();
        stocks.insert(event.key_candidates.begin(), event.key_candidates.end());
        if (event.key_candidates.size() > 1) ++conflicts;
        Json value = Json::object();
        value["event_id"] = event_id;
        value["date"] = event.dates.empty() ? "" : event.dates.front();
        value["dates"] = string_array(event.dates);
        value["views"] = string_array(event.views);
        value["event_types"] = string_array(event.event_types);
        Json candidates = Json::array();
        for (const auto& [market, code] : event.key_candidates) {
            auto candidate = security_document(market, code, securities);
            candidates.push_back(std::move(candidate));
        }
        value["key_candidates"] = std::move(candidates);
        value["key_conflict"] = event.key_candidates.size() > 1;
        value["security"] = security_document(primary_key.first, primary_key.second, securities);
        value["metrics"] = metric_document(event.primary_record);
        value["master_records"] = std::move(event.master_records);
        events.push_back(std::move(value));
    }
    std::sort(events.as_array().begin(), events.as_array().end(),
        [](const Json& left, const Json& right) {
            const auto left_key = left.at("date").as_string() + ":" + left.at("event_id").as_string();
            const auto right_key = right.at("date").as_string() + ":" + right.at("event_id").as_string();
            return left_key > right_key;
        });

    Json counts = Json::object();
    counts["master_views"] = static_cast<std::uint64_t>(lhb_views().size());
    counts["master_rows"] = master_rows;
    counts["events"] = static_cast<std::uint64_t>(events.size());
    counts["event_view_memberships"] = master_rows;
    counts["stocks"] = static_cast<std::uint64_t>(stocks.size());
    counts["conflicting_master_keys"] = conflicts;
    Json result = Json::object();
    result["schema"] = "tdx-lhb-master-native-v1";
    result["generated_at"] = generated_at.empty() ? now_text() : generated_at;
    result["counts"] = std::move(counts);
    result["sources"] = std::move(sources);
    result["events"] = std::move(events);
    return result;
}

Json normalize_lhb_detail_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("LHB detail rows must be an array");
    Json result = Json::array();
    std::set<std::tuple<std::string, std::string, std::string>> identities;
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        const auto market = text_value(row, "sc");
        const auto date = text_value(row, "date");
        if (code.empty() || market.empty() || date.empty())
            throw Error("LHB detail identity is incomplete");
        identities.emplace(market, code, date);
        Json value = Json::object();
        value["market"] = market;
        value["code"] = code;
        value["date"] = date;
        value["category"] = copy_value(row, "lb");
        value["broker"] = copy_value(row, "yyb");
        value["side"] = copy_value(row, "yyb1");
        value["rank"] = copy_value(row, "yyb2");
        value["buy_amount"] = copy_value(row, "bje");
        value["sell_amount"] = copy_value(row, "sje");
        value["net_buy"] = copy_value(row, "jmr");
        value["share_percent"] = copy_value(row, "zb");
        value["buy_success_rate_1d"] = copy_value(row, "mrcgl1");
        value["buy_success_rate_3d"] = copy_value(row, "mrcgl3");
        value["buy_success_rate_5d"] = copy_value(row, "mrcgl5");
        value["estimated_cost"] = copy_value(row, "ygcb");
        value["estimated_return"] = copy_value(row, "ygsy");
        value["tag"] = copy_value(row, "yzbq");
        value["operation_url"] = copy_value(row, "czjl");
        value["row_type"] = text_value(row, "yyb2") == "0" ? "total" : "broker";
        result.push_back(std::move(value));
    }
    if (identities.size() > 1) throw Error("LHB detail contains multiple securities or dates");
    return result;
}

LhbService::LhbService(std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

LhbService::FetchResult LhbService::fetch_master(const LhbQuery& options) {
    const auto now = std::time(nullptr);
    const int age = master_cache_.fetched_at
        ? static_cast<int>(std::max<std::time_t>(0, now - master_cache_.fetched_at)) : 0;
    if (!options.refresh && master_cache_.fetched_at &&
        age < options.master_cache_ttl_seconds)
        return {master_cache_.document, false, age};
    std::vector<std::string> resources;
    for (const auto& view : lhb_views()) resources.push_back(view.resource);
    auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
    auto model = aggregate_lhb_master_documents(documents, securities_);
    master_cache_ = {model, std::time(nullptr)};
    return {std::move(model), true, 0};
}

std::map<std::string, LhbService::FetchResult> LhbService::fetch_details(
    const std::vector<std::string>& event_ids, const LhbQuery& options,
    Json& errors) {
    if (!errors.is_array()) throw Error("LHB detail errors must be an array");
    std::map<std::string, FetchResult> result;
    std::vector<std::string> stale;
    const auto now = std::time(nullptr);
    for (const auto& event_id : event_ids) {
        const auto found = detail_cache_.find(event_id);
        const int age = found == detail_cache_.end() ? 0
            : static_cast<int>(std::max<std::time_t>(0, now - found->second.fetched_at));
        if (!options.refresh && found != detail_cache_.end() &&
            age < options.detail_cache_ttl_seconds)
            result[event_id] = {found->second.document, false, age};
        else stale.push_back(event_id);
    }
    if (!stale.empty()) {
        std::vector<std::string> resources;
        for (const auto& event_id : stale) resources.push_back("lhbfx/" + event_id + ".jsn");
        const auto store_document = [&](const std::string& event_id, Json document) {
            document["rows"] = normalize_lhb_detail_rows(document.at("rows"));
            detail_cache_[event_id] = {document, std::time(nullptr)};
            result[event_id] = {std::move(document), true, 0};
        };
        try {
            auto documents = fetch_jsn_resources_rows(resources, "bi", options.timeout_ms);
            if (documents.size() != stale.size())
                throw Error("LHB detail batch size mismatch");
            for (std::size_t index = 0; index < stale.size(); ++index)
                store_document(stale[index], documents.as_array()[index]);
        } catch (const Error& batch_error) {
            if (!missing_jsn_resource_error(batch_error.what())) throw;
            for (const auto& event_id : stale) {
                const auto resource = "lhbfx/" + event_id + ".jsn";
                try {
                    store_document(event_id,
                        fetch_jsn_resource_rows(resource, "bi", options.timeout_ms));
                } catch (const Error& error) {
                    if (!missing_jsn_resource_error(error.what())) throw;
                    Json failure = Json::object();
                    failure["event_id"] = event_id;
                    failure["resource"] = resource;
                    failure["message"] = error.what();
                    errors.push_back(std::move(failure));
                }
            }
        }
    }
    return result;
}

Json LhbService::query(const LhbQuery& options) {
    if (options.limit < 1 || options.limit > 500) throw Error("limit must be in 1..500");
    if (options.master_cache_ttl_seconds < 0 || options.master_cache_ttl_seconds > 3600 ||
        options.detail_cache_ttl_seconds < 0 || options.detail_cache_ttl_seconds > 86400)
        throw Error("LHB cache TTL is outside the supported range");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    const bool security_mode = !options.market.empty() || !options.code.empty();
    if (security_mode && (options.market.empty() || options.code.empty()))
        throw Error("security mode requires both market and code");
    if (security_mode && !options.event_id.empty())
        throw Error("choose security or event mode, not both");
    int selected_market = -1;
    if (security_mode) {
        selected_market = market_id(options.market);
        if (options.code.size() != 6 || !std::all_of(options.code.begin(), options.code.end(),
            [](char ch) { return ch >= '0' && ch <= '9'; }))
            throw Error("code must contain six digits");
    }
    if (!options.event_id.empty() && !std::all_of(options.event_id.begin(), options.event_id.end(),
        [](char ch) { return ch >= '0' && ch <= '9'; }))
        throw Error("event_id must contain digits");

    auto master = fetch_master(options);
    Json selected = Json::array();
    for (const auto& event : master.document.at("events").as_array()) {
        bool matches = !security_mode && options.event_id.empty();
        if (!options.event_id.empty()) matches = event.at("event_id").as_string() == options.event_id;
        if (security_mode) {
            matches = false;
            for (const auto& candidate : event.at("key_candidates").as_array())
                if (static_cast<int>(candidate.at("market_id").as_number()) == selected_market &&
                    candidate.at("code").as_string() == options.code) {
                    matches = true;
                    break;
                }
        }
        if (matches) selected.push_back(event);
        if (selected.size() >= static_cast<std::size_t>(options.limit)) break;
    }
    if (!options.event_id.empty() && selected.size() == 0)
        throw Error("LHB event is not present in the current master views: " + options.event_id);

    std::vector<std::string> event_ids;
    if (options.include_details)
        for (const auto& event : selected.as_array())
            event_ids.push_back(event.at("event_id").as_string());
    Json detail_errors = Json::array();
    const auto details = fetch_details(event_ids, options, detail_errors);
    std::uint64_t detail_rows = 0, refreshed_details = 0;
    int maximum_detail_age = 0;
    Json sources = master.document.at("sources");
    for (auto& event : selected.as_array()) {
        const auto event_id = event.at("event_id").as_string();
        const auto found = details.find(event_id);
        if (found == details.end()) {
            event["detail_available"] = false;
            event["details"] = Json::array();
            continue;
        }
        const auto& document = found->second.document;
        event["detail_available"] = true;
        event["detail_resource"] = document.at("resource");
        event["details"] = document.at("rows");
        event["detail_row_count"] = document.at("row_count");
        sources.push_back(jsn_source_metadata(document));
        detail_rows += static_cast<std::uint64_t>(document.at("row_count").as_number());
        if (found->second.refreshed) ++refreshed_details;
        maximum_detail_age = std::max(maximum_detail_age, found->second.age_seconds);
    }

    double buy_amount = 0.0, sell_amount = 0.0, net_buy = 0.0;
    std::uint64_t view_memberships = 0;
    std::set<std::string> event_dates;
    for (const auto& event : selected.as_array()) {
        buy_amount += metric_number(event, "buy_amount");
        sell_amount += metric_number(event, "sell_amount");
        net_buy += metric_number(event, "net_buy");
        view_memberships += static_cast<std::uint64_t>(event.at("views").size());
        event_dates.insert(event.at("date").as_string());
    }
    Json summary = Json::object();
    summary["event_count"] = static_cast<std::uint64_t>(selected.size());
    summary["view_memberships"] = view_memberships;
    summary["detail_rows"] = detail_rows;
    summary["unique_dates"] = static_cast<std::uint64_t>(event_dates.size());
    summary["event_reported_buy_amount_sum"] = buy_amount;
    summary["event_reported_sell_amount_sum"] = sell_amount;
    summary["event_reported_net_buy_sum"] = net_buy;
    summary["latest_date"] = selected.size() ? selected.as_array().front().at("date") : Json(nullptr);

    Json cache = Json::object();
    cache["master_ttl_seconds"] = options.master_cache_ttl_seconds;
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    cache["detail_ttl_seconds"] = options.detail_cache_ttl_seconds;
    cache["details_refreshed"] = refreshed_details;
    cache["maximum_detail_age_seconds"] = maximum_detail_age;
    const auto upstream_health = jsn_sources_health(sources);
    const bool stale = upstream_health.at("stale").as_bool();
    cache["stale"] = stale;
    cache["upstream"] = upstream_health;
    Json result = Json::object();
    result["schema"] = "tdx-lhb-native-v1";
    result["availability"] = stale ? "stale-cache" :
        detail_errors.size() ? "partial" : "live";
    result["generated_at"] = now_text();
    result["mode"] = security_mode ? "security" : options.event_id.empty() ? "master" : "event";
    if (security_mode)
        result["security"] = security_document(std::to_string(selected_market), options.code, securities_);
    result["master_counts"] = master.document.at("counts");
    result["sources"] = std::move(sources);
    result["detail_errors"] = std::move(detail_errors);
    result["cache"] = std::move(cache);
    result["summary"] = std::move(summary);
    result["events"] = std::move(selected);
    return result;
}

int command_market_lhb(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market lhb [options]\n\n"
            "Native LHB event, view, security, and broker-detail aggregation.\n\n"
            "Options:\n"
            "  --root PATH             TDX installation root\n"
            "  --market sz|sh|bj       Security market (requires --code)\n"
            "  --code CODE             Query current LHB events for one security\n"
            "  --event ID              Query one event ID\n"
            "  --no-details            Do not fetch lhbfx/<event>.jsn broker rows\n"
            "  --limit N               Default 10, maximum 500\n"
            "  --timeout-ms N          Default 10000\n"
            "  --output PATH           Default output/tdx-lhb-native.json\n"
            "  --compact               Write compact JSON\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    LhbQuery query;
    query.market = trim(args.take_option("--market"));
    query.code = trim(args.take_option("--code"));
    query.event_id = trim(args.take_option("--event"));
    query.limit = bounded_integer(args.take_option("--limit", "10"), "--limit", 1, 500);
    query.timeout_ms = bounded_integer(args.take_option("--timeout-ms", "10000"),
                                       "--timeout-ms", 100, 60000);
    const bool no_details = args.take_flag("--no-details");
    query.include_details = !no_details && (!query.event_id.empty() || !query.code.empty());
    query.refresh = true;
    const auto output = native_path(args.take_option("--output", "output/tdx-lhb-native.json"));
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : native_path(root_text));
    auto securities = load_blocks(root, {}).securities;
    LhbService service(std::move(securities));
    const auto document = service.query(query);
    atomic_write_text(output, document.dump(compact ? -1 : 2) + "\n");
    std::cout << "completed " << document.at("mode").as_string() << " LHB query with "
              << document.at("summary").at("event_count").as_number()
              << " events -> " << path_utf8(output) << '\n';
    return 0;
}

}  // namespace tdx
