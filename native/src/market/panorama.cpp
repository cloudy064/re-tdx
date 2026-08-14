#include "tdx/panorama.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace tdx {
namespace {

struct FieldSpec {
    const char* name;
    const char* source;
};

struct ViewSpec {
    const char* id;
    const char* label;
    const char* resource;
    const char* market_field;
    const char* code_field;
    std::vector<FieldSpec> fields;
};

const std::vector<ViewSpec>& views() {
    static const std::vector<ViewSpec> values{
        {"quality-rating", "量化吸引力评级", "list/func_aqfph101_1.jsn", "$SC", "$ZQDM",
         {{"latest_date", "date"}, {"latest_score", "zxfs"},
          {"previous_date", "date1"}, {"previous_score", "scfs"},
          {"rating_change", "pj"}, {"risk_type", "fxlx"},
          {"leader_count", "lds"}, {"leader_type", "ldlx"},
          {"pe_ttm", "syl"}, {"roe_pct", "roe"}}},
        {"capital-flow", "多周期资金流向", "list/func_gx_zjlx101_1.jsn", "$SC", "$ZQDM",
         {{"statistics_date", "date"}, {"main_net_inflow_1d", "rzljlr1"},
          {"main_net_inflow_5d", "rzljlr5"}, {"main_net_inflow_10d", "rzljlr10"},
          {"main_net_inflow_20d", "rzljlr20"}, {"main_net_inflow_30d", "rzljlr30"},
          {"net_inflow_5d", "rjlr5"}, {"net_inflow_10d", "rjlr10"},
          {"net_inflow_20d", "rjlr20"}, {"net_inflow_30d", "rjlr30"}}},
        {"risk-watch", "质押商誉解禁风险", "list/func_gx_fxgz101_1.jsn", "$SC", "$ZQDM",
         {{"pledged_shares_10k", "ljzy"}, {"pledged_total_share_pct", "zzgb"},
          {"pledge_warning_price", "yjj"}, {"pledge_warning_shares_10k", "yjgs"},
          {"pledge_closeout_price", "pcj"}, {"pledge_closeout_shares_10k", "pcgs"},
          {"goodwill_current", "sy1"}, {"goodwill_previous", "sy2"},
          {"goodwill_change", "syzj"}, {"goodwill_net_asset_pct", "syzgd"},
          {"unlock_date", "jjrq"}, {"unlock_shares", "jjsl"},
          {"unlock_total_share_pct", "jjgzb"}, {"unlock_reason", "jjyy"}}},
        {"financials", "财报与估值摘要", "list/func_gx_cbsj101_1.jsn", "$SC", "$ZQDM",
         {{"report_period", "BGQ"}, {"market_value_100m", "SZ"},
          {"pe_ttm", "PE"}, {"pb_mrq", "PB"}, {"peg", "PS"},
          {"debt_ratio_pct", "ZCFZ"}, {"profit_yoy_pct", "TB1"},
          {"revenue_yoy_pct", "TB2"}, {"roe_pct", "ROE"},
          {"gross_margin_pct", "XSM"}, {"rd_expense_ratio_pct", "fyzb1"},
          {"sales_expense_ratio_pct", "fyzb2"}, {"management_expense_ratio_pct", "fyzb3"},
          {"finance_expense_ratio_pct", "fyzb4"}, {"revenue_turnover_pct", "ZZL1"},
          {"asset_turnover_pct", "ZZL2"}, {"institution_holding_float_pct", "JGCC"},
          {"dividend_year", "FHND"}, {"dividend_yield_pct", "GXL"}}},
        {"earnings-forecast", "业绩预告与预测估值", "list/func_gx_cbyg101_1.jsn", "$SC", "$ZQDM",
         {{"forecast_date", "ygdate"}, {"report_period", "bgq"},
          {"forecast_type", "type"}, {"net_profit_lower", "jlr1"},
          {"net_profit_upper", "jlr2"}, {"net_profit_previous", "jlr3"},
          {"profit_yoy_lower_pct", "zj3"}, {"profit_yoy_upper_pct", "zj4"},
          {"eps", "eps"}, {"forecast_profit_lower", "x"},
          {"forecast_profit_upper", "y"}, {"total_shares_10k", "zgb"}}},
        {"analyst-estimate", "机构评级与盈利预测", "list/func_gx_fxspj101_1.jsn", "$SC", "$ZQDM",
         {{"latest_date", "ZXRQ"}, {"research_count_6m", "JGSL"},
          {"composite_rating", "ZHPJ"}, {"pe", "PE"},
          {"forecast_eps_growth_pct", "YCEPS"}, {"target_price", "MBJ"},
          {"report_period", "BGQ"}, {"current_eps", "MGSY"},
          {"industry", "hy"}, {"forecast_base_year", "TBGQ"},
          {"forecast_eps_t", "MGSY0"}, {"forecast_eps_t1", "MGSY1"},
          {"forecast_eps_t2", "MGSY2"}}},
        {"distribution", "高送转与分红送配", "list/func_gx_zfsp101_1.jsn", "$SC", "$ZQDM",
         {{"capital_reserve_per_share", "zbgj"}, {"undistributed_profit_per_share", "wfplr"},
          {"parent_profit", "jlr1"}, {"parent_profit_yoy_pct", "jlr3"},
          {"announcement_date", "ggdate"}, {"bonus_transfer_per_10", "szbl"},
          {"cash_dividend_per_10", "xjfh"}, {"record_date", "djdate"},
          {"ex_dividend_date", "qxdate"}, {"plan_stage", "fajd"},
          {"rights_announcement_date", "ggrq"}, {"rights_code", "dm"},
          {"rights_ratio", "bl"}, {"rights_start_date", "qsjkr"},
          {"rights_end_date", "jzjkr"}}},
        {"lhb-overview", "龙虎榜成交与席位概览", "list/func_gx_lhbd101_1.jsn", "$SC1", "$ZQDM1",
         {{"event_id", "$ZQDM"}, {"event_date", "date"},
          {"buy_turnover_pct", "bzb"}, {"sell_turnover_pct", "szb"},
          {"net_buy", "jmr"}, {"buy_total", "zmr"}, {"sell_total", "zmc"},
          {"stock_connect_seats", "sl1"}, {"institution_seats", "sl2"},
          {"abnormal_reason", "lx"}}},
        {"margin-overview", "融资融券概览", "list/func_gx_rzrq101_1.jsn", "$SC", "$ZQDM",
         {{"statistics_date", "ygdate"}, {"float_market_value", "J_LTSZ"},
          {"float_shares", "J_LTGB"}, {"financing_balance", "rzye"},
          {"financing_net_buy", "drjme"}, {"financing_market_value_pct", "rzzltsz"},
          {"short_balance_shares", "rqyl"}, {"short_net_sell_shares", "drjml"},
          {"short_float_share_pct", "zqzltgf"}, {"margin_difference", "rzrqce"}}},
        {"ownership-change", "股东增减持区间", "list/func_gx_zcjc101_1.jsn", "$SC", "$ZQDM",
         {{"increase_value_10k", "zcsz"}, {"decrease_value_10k", "jcsz"},
          {"net_change_value_10k", "jzcsz"}, {"increase_price_min", "price1"},
          {"increase_price_max", "price2"}, {"decrease_price_min", "price3"},
          {"decrease_price_max", "price4"}, {"start_date", "date1"},
          {"end_date", "date2"}}},
    };
    return values;
}

const ViewSpec* find_view(const std::string& id) {
    const auto folded = lower_ascii(trim(id));
    const auto found = std::find_if(views().begin(), views().end(), [&](const ViewSpec& value) {
        return value.id == folded;
    });
    return found == views().end() ? nullptr : &*found;
}

const Json* value_ptr(const Json& object, std::string_view name) {
    if (!object.is_object()) return nullptr;
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

int market_id(const std::string& value) {
    const auto normalized = lower_ascii(trim(value));
    if (normalized == "0" || normalized == "sz") return 0;
    if (normalized == "1" || normalized == "sh") return 1;
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

bool six_digits(const std::string& value) {
    return value.size() == 6 && std::all_of(value.begin(), value.end(), [](char ch) {
        return ch >= '0' && ch <= '9';
    });
}

Json catalog_document() {
    Json rows = Json::array();
    for (const auto& view : views()) {
        Json row = Json::object();
        row["id"] = view.id;
        row["label"] = view.label;
        row["resource"] = view.resource;
        row["market_field"] = view.market_field;
        row["code_field"] = view.code_field;
        Json fields = Json::array();
        for (const auto& field : view.fields) {
            Json item = Json::object();
            item["name"] = field.name;
            item["source"] = field.source;
            fields.push_back(std::move(item));
        }
        row["fields"] = std::move(fields);
        rows.push_back(std::move(row));
    }
    Json result = Json::object();
    result["schema"] = "tdx-panorama-catalog-native-v1";
    result["view_count"] = static_cast<std::uint64_t>(views().size());
    result["views"] = std::move(rows);
    result["semantics"] = "Typed projections of TDX GX/AQFPH JSN resources; client-computed quote columns are not fabricated when absent from the resource.";
    return result;
}

const Json& document_for(const Json& documents, std::string_view resource) {
    if (!documents.is_array()) throw Error("panorama source documents must be an array");
    for (const auto& document : documents.as_array())
        if (text_value(document, "resource") == resource) return document;
    throw Error("panorama source document is missing: " + std::string(resource));
}

Json source_summary(const Json& document) {
    return jsn_source_metadata(document);
}

Json normalize_row(const ViewSpec& view, const Json& raw,
                   const std::map<std::pair<int, std::string>, Security>& securities) {
    const int id = market_id(text_value(raw, view.market_field));
    const auto code = text_value(raw, view.code_field);
    const auto found = securities.find({id, code});
    Json data = Json::object();
    for (const auto& field : view.fields) data[field.name] = copy_value(raw, field.source);
    Json result = Json::object();
    result["market"] = market_name(id);
    result["market_id"] = id;
    result["code"] = code;
    result["security_id"] = market_prefix(id) + code;
    result["name"] = found == securities.end() ? "" : found->second.name;
    result["name_resolved"] = found != securities.end();
    result["data"] = std::move(data);
    result["raw"] = raw;
    return result;
}

bool row_matches(const Json& row, const PanoramaQuery& options,
                 int selected_market, const std::string& needle) {
    if (selected_market >= 0 && static_cast<int>(row.at("market_id").as_number()) != selected_market)
        return false;
    if (!options.code.empty() && row.at("code").as_string() != options.code) return false;
    if (needle.empty()) return true;
    return lower_ascii(row.at("code").as_string()).find(needle) != std::string::npos ||
           lower_ascii(row.at("name").as_string()).find(needle) != std::string::npos ||
           lower_ascii(row.at("data").dump(-1)).find(needle) != std::string::npos;
}

Json section_document(const ViewSpec& view, const Json& source,
                      const PanoramaQuery& options,
                      const std::map<std::pair<int, std::string>, Security>& securities) {
    const int selected_market = options.market.empty() ? -1 : market_id(options.market);
    const auto needle = lower_ascii(trim(options.query));
    Json matches = Json::array();
    const auto& rows = source.at("rows");
    if (!rows.is_array()) throw Error("panorama JSN rows must be an array");
    std::uint64_t matched = 0;
    for (const auto& raw : rows.as_array()) {
        Json normalized;
        try { normalized = normalize_row(view, raw, securities); }
        catch (const std::exception&) { continue; }
        if (!row_matches(normalized, options, selected_market, needle)) continue;
        if (matched >= static_cast<std::uint64_t>(options.offset) &&
            matches.size() < static_cast<std::size_t>(options.limit))
            matches.push_back(std::move(normalized));
        ++matched;
    }
    Json result = Json::object();
    result["view"] = view.id;
    result["label"] = view.label;
    result["resource"] = view.resource;
    result["source_rows"] = static_cast<std::uint64_t>(rows.size());
    result["matched"] = matched;
    result["offset"] = options.offset;
    result["limit"] = options.limit;
    result["returned"] = static_cast<std::uint64_t>(matches.size());
    result["truncated"] = matched > static_cast<std::uint64_t>(options.offset) + matches.size();
    result["records"] = std::move(matches);
    result["source"] = source_summary(source);
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

int bounded(const std::string& text, const std::string& name, int minimum, int maximum) {
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

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

}  // namespace

Json compose_panorama_document(
    const PanoramaQuery& options,
    const Json& source_documents,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    const auto view_id = lower_ascii(trim(options.view));
    if (view_id == "catalog") return catalog_document();
    if (options.offset < 0 || options.offset > 1'000'000)
        throw Error("offset must be in 0..1000000");
    if (options.limit < 1 || options.limit > 5000)
        throw Error("limit must be in 1..5000");
    if (options.market.empty() != options.code.empty())
        throw Error("market and code must be supplied together");
    if (!options.code.empty() && !six_digits(options.code))
        throw Error("code must contain six digits");
    if (!options.market.empty()) (void)market_id(options.market);

    Json sections = Json::array();
    if (view_id == "security") {
        if (options.code.empty()) throw Error("security view requires market and code");
        for (const auto& view : views())
            sections.push_back(section_document(
                view, document_for(source_documents, view.resource), options, securities));
    } else {
        const auto* view = find_view(view_id);
        if (!view) throw Error("unknown panorama view: " + options.view);
        sections.push_back(section_document(
            *view, document_for(source_documents, view->resource), options, securities));
    }

    std::uint64_t matched = 0, returned = 0;
    Json sources = Json::array();
    for (const auto& section : sections.as_array()) {
        matched += static_cast<std::uint64_t>(section.at("matched").as_number());
        returned += static_cast<std::uint64_t>(section.at("returned").as_number());
        sources.push_back(section.at("source"));
    }
    Json counts = Json::object();
    counts["sections"] = static_cast<std::uint64_t>(sections.size());
    counts["matched"] = matched;
    counts["returned"] = returned;
    Json result = Json::object();
    result["schema"] = "tdx-market-panorama-native-v1";
    result["generated_at"] = now_text();
    result["view"] = view_id;
    result["market"] = options.market.empty() ? Json(nullptr) : Json(market_name(market_id(options.market)));
    result["code"] = options.code.empty() ? Json(nullptr) : Json(options.code);
    result["query"] = options.query;
    result["counts"] = std::move(counts);
    result["sections"] = std::move(sections);
    result["sources"] = std::move(sources);
    result["upstream_health"] = jsn_sources_health(result.at("sources"));
    result["semantics"] = "Static TDX panorama resources are normalized without fabricating client-side quote columns; raw rows remain available for audit.";
    return result;
}

PanoramaService::PanoramaService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

Json PanoramaService::query(const PanoramaQuery& options) {
    const auto view_id = lower_ascii(trim(options.view));
    if (view_id == "catalog") return catalog_document();
    if (options.cache_ttl_seconds < 0 || options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (options.timeout_ms < 100 || options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");

    std::vector<const ViewSpec*> selected;
    if (view_id == "security") {
        for (const auto& view : views()) selected.push_back(&view);
    } else {
        const auto* view = find_view(view_id);
        if (!view) throw Error("unknown panorama view: " + options.view);
        selected.push_back(view);
    }
    const auto now = std::time(nullptr);
    std::vector<std::string> stale;
    for (const auto* view : selected) {
        const auto found = cache_.find(view->resource);
        if (options.refresh || found == cache_.end() ||
            now - found->second.fetched_at >= options.cache_ttl_seconds)
            stale.push_back(view->resource);
    }
    if (!stale.empty()) {
        const auto fetched = fetch_jsn_resources_rows(stale, "bi", options.timeout_ms);
        if (!fetched.is_array() || fetched.as_array().size() != stale.size())
            throw Error("panorama resource batch is incomplete");
        for (std::size_t index = 0; index < stale.size(); ++index)
            cache_[stale[index]] = {fetched.as_array()[index], std::time(nullptr)};
    }
    Json documents = Json::array();
    for (const auto* view : selected) {
        const auto found = cache_.find(view->resource);
        if (found == cache_.end()) throw Error("panorama cache is incomplete");
        documents.push_back(found->second.document);
    }
    auto result = compose_panorama_document(options, documents, securities_);
    Json cache = Json::object();
    cache["refreshed"] = !stale.empty();
    cache["ttl_seconds"] = options.cache_ttl_seconds;
    cache["resource_count"] = static_cast<std::uint64_t>(selected.size());
    result["cache"] = std::move(cache);
    return result;
}

int command_market_panorama(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) {
        std::cout <<
            "Usage: tdx-tool market panorama [options]\n\n"
            "Typed TDX GX/AQFPH panorama: flows, risks, financials, forecasts,\n"
            "analyst estimates, distributions, LHB, margin and ownership changes.\n\n"
            "  --root PATH           TDX installation root\n"
            "  --view ID             catalog, security, quality-rating, capital-flow,\n"
            "                        risk-watch, financials, earnings-forecast,\n"
            "                        analyst-estimate, distribution, lhb-overview,\n"
            "                        margin-overview, ownership-change\n"
            "  --market sz|sh|bj     Use with --code; required by security view\n"
            "  --code CODE           Six-digit security code\n"
            "  --query TEXT          Filter code, resolved name or normalized values\n"
            "  --offset N            Default 0\n"
            "  --limit N             Default 200; maximum 5000\n"
            "  --refresh             Bypass service cache\n"
            "  --cache-ttl N         Default 300 seconds\n"
            "  --timeout-ms N         Default 15000\n"
            "  --output FILE         Write JSON instead of stdout\n"
            "  --compact\n";
        return 0;
    }
    const auto root_text = args.take_option("--root");
    PanoramaQuery query;
    query.view = args.take_option("--view", "catalog");
    query.market = args.take_option("--market");
    query.code = args.take_option("--code");
    query.query = args.take_option("--query");
    query.offset = bounded(args.take_option("--offset", "0"), "offset", 0, 1'000'000);
    query.limit = bounded(args.take_option("--limit", "200"), "limit", 1, 5000);
    query.refresh = args.take_flag("--refresh");
    query.cache_ttl_seconds = bounded(
        args.take_option("--cache-ttl", "300"), "cache-ttl", 0, 86400);
    query.timeout_ms = bounded(
        args.take_option("--timeout-ms", "15000"), "timeout-ms", 100, 60000);
    const auto output_text = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto root = find_tdx_root(root_text.empty() ? fs::path{} : from_utf8(root_text));
    PanoramaService service(load_blocks(root, {}).securities);
    const auto result = service.query(query);
    const auto text = result.dump(compact ? -1 : 2) + "\n";
    if (output_text.empty()) std::cout << text;
    else atomic_write_text(from_utf8(output_text), text);
    return 0;
}

}  // namespace tdx
