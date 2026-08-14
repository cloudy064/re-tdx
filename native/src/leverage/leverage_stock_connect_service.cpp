#include "leverage_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

namespace tdx {
using namespace leverage_detail;

Json LeverageService::query_stock_connect(const StockConnectQuery& options) {
    if (options.limit < 1 || options.limit > 10000) throw Error("stock-connect limit is outside 1..10000");
    if (options.view != "flows" && options.view != "holdings" &&
        options.view != "security" && options.view != "activity" &&
        options.view != "industry" && options.view != "industry-detail" &&
        options.view != "active-stocks")
        throw Error("stock-connect view must be flows, holdings, security, activity, industry, industry-detail, or active-stocks");
    Json rows = Json::array(), trend = Json::array(), sources = Json::array(),
         errors = Json::array(), reconciliation(nullptr);
    bool refreshed = false, missing = false; int age = 0;
    if (options.view == "flows") {
        const auto* found = find_resource(flow_resources, options.category);
        if (!found) throw Error("unknown stock-connect flow category");
        auto source = fetch_resource(std::string(found->resource), options.refresh,
                                     options.cache_ttl_seconds, options.timeout_ms);
        rows = filter_rows(normalize_stock_connect_flow_rows(source.document.at("rows")),
                           options.query, options.limit);
        sources.push_back(source_summary(source.document)); refreshed = source.refreshed; age = source.age_seconds;
    } else if (options.view == "holdings") {
        const auto* found = find_resource(holding_resources, options.category);
        if (!found) throw Error("unknown stock-connect holding category");
        try {
            auto source = fetch_resource(std::string(found->resource), options.refresh,
                                         options.cache_ttl_seconds, options.timeout_ms);
            rows = filter_rows(normalize_stock_connect_holding_rows(
                source.document.at("rows"), securities_), options.query, options.limit);
            sources.push_back(source_summary(source.document));
            refreshed = source.refreshed; age = source.age_seconds;
        } catch (const std::exception& error) {
            if (!zero_length_resource(error)) throw;
            missing = true;
            sources.push_back(missing_source(std::string(found->resource), error.what()));
        }
    } else if (options.view == "security") {
        const int id = market_id(options.market);
        if (!digits(options.code, id == 31 || id == 48 ? 5 : 6))
            throw Error("stock-connect security code length does not match the market");
        const auto key = id == 0 || id == 1 || id == 2 ? "jd" + options.code
                                                        : std::to_string(id) + options.code;
        const auto resource = "hsgtcg1/" + key + ".jsn";
        try {
            auto source = fetch_resource(resource, options.refresh,
                                         options.detail_cache_ttl_seconds, options.timeout_ms);
            rows = normalize_stock_connect_history_rows(source.document.at("rows"));
            sources.push_back(source_summary(source.document)); refreshed = source.refreshed; age = source.age_seconds;
        } catch (const std::exception& error) {
            Json failure = Json::object(); failure["resource"] = resource; failure["message"] = error.what();
            errors.push_back(std::move(failure));
        }
        const auto chart_resource = "hsgtcg2/" + key + ".jsn";
        try {
            auto chart = fetch_resource(chart_resource, options.refresh,
                                         options.detail_cache_ttl_seconds, options.timeout_ms);
            trend = normalize_stock_connect_chart_rows(chart.document.at("rows"));
            sources.push_back(source_summary(chart.document));
            refreshed = refreshed || chart.refreshed; age = std::max(age, chart.age_seconds);
            std::map<std::string, const Json*> primary_by_date;
            for (const auto& row : rows.as_array()) primary_by_date[value_text(row, "date")] = &row;
            std::uint64_t matched = 0, mismatch = 0, chart_only = 0;
            for (const auto& row : trend.as_array()) {
                const auto found = primary_by_date.find(value_text(row, "date"));
                if (found == primary_by_date.end()) { ++chart_only; continue; }
                const auto primary_change = value_number(*found->second, "market_value_change_yuan");
                const auto chart_change = value_number(row, "market_value_change_yuan");
                const auto primary_ratio = value_number(*found->second, "holding_ratio_pct");
                const auto chart_ratio = value_number(row, "holding_ratio_pct");
                const bool change_ok = (!primary_change && !chart_change) ||
                    (primary_change && chart_change && std::abs(*primary_change - *chart_change) < 0.01);
                const bool ratio_ok = (!primary_ratio && !chart_ratio) ||
                    (primary_ratio && chart_ratio && std::abs(*primary_ratio - *chart_ratio) < 1e-9);
                change_ok && ratio_ok ? ++matched : ++mismatch;
            }
            reconciliation = Json::object();
            reconciliation["primary_rows"] = static_cast<std::uint64_t>(rows.size());
            reconciliation["chart_rows"] = static_cast<std::uint64_t>(trend.size());
            reconciliation["matched_rows"] = matched;
            reconciliation["mismatch_rows"] = mismatch;
            reconciliation["chart_only_rows"] = chart_only;
            reconciliation["all_matched"] = mismatch == 0 && chart_only == 0 && rows.size() == trend.size();
        } catch (const std::exception& error) {
            Json failure = Json::object(); failure["resource"] = chart_resource;
            failure["message"] = error.what(); errors.push_back(std::move(failure));
        }
    } else if (options.view == "activity") {
        const auto* found = find_resource(activity_resources, options.category);
        if (!found)
            throw Error("unknown stock-connect activity category");
        try {
            auto source = fetch_resource(std::string(found->resource), options.refresh,
                                         options.cache_ttl_seconds, options.timeout_ms);
            rows = filter_rows(normalize_stock_connect_activity_rows(
                source.document.at("rows"), options.category, securities_),
                options.query, options.limit);
            sources.push_back(source_summary(source.document));
            refreshed = source.refreshed; age = source.age_seconds;
        } catch (const std::exception& error) {
            if (!zero_length_resource(error)) throw;
            missing = true;
            sources.push_back(missing_source(std::string(found->resource), error.what()));
        }
    } else if (options.view == "industry") {
        const auto* found = find_resource(industry_resources, options.category);
        if (!found)
            throw Error("unknown stock-connect industry category");
        try {
            auto source = fetch_resource(std::string(found->resource), options.refresh,
                                         options.cache_ttl_seconds, options.timeout_ms);
            rows = filter_rows(normalize_stock_connect_industry_rows(
                source.document.at("rows"), options.category), options.query, options.limit);
            sources.push_back(source_summary(source.document));
            refreshed = source.refreshed; age = source.age_seconds;
        } catch (const std::exception& error) {
            if (!zero_length_resource(error)) throw;
            missing = true;
            sources.push_back(missing_source(std::string(found->resource), error.what()));
        }
    } else if (options.view == "industry-detail") {
        if (!safe_group_id(trim(options.group_id)))
            throw Error("stock-connect industry-detail requires --group-id");
        const auto member_resource = "ggthy/" + trim(options.group_id) + ".jsn";
        const auto trend_resource = "ggthy1/" + trim(options.group_id) + ".jsn";
        auto members = fetch_resource(member_resource, options.refresh,
                                      options.detail_cache_ttl_seconds, options.timeout_ms);
        auto history = fetch_resource(trend_resource, options.refresh,
                                      options.detail_cache_ttl_seconds, options.timeout_ms);
        rows = filter_rows(normalize_stock_connect_southbound_member_rows(
            members.document.at("rows"), securities_), options.query, options.limit);
        trend = normalize_stock_connect_southbound_trend_rows(history.document.at("rows"));
        sources.push_back(source_summary(members.document));
        sources.push_back(source_summary(history.document));
        refreshed = members.refreshed || history.refreshed;
        age = std::max(members.age_seconds, history.age_seconds);
    } else {
        if (!digits(options.date, 8))
            throw Error("active-stocks view requires an eight-digit date");
        const auto suffix = active_channel_suffix(options.channel);
        const auto channel = suffix == "2" ? "sh-northbound" : "sz-northbound";
        const auto resource = "hsgt/" + options.date + suffix + ".jsn";
        try {
            auto source = fetch_resource(resource, options.refresh,
                                         options.detail_cache_ttl_seconds,
                                         options.timeout_ms);
            rows = filter_rows(normalize_stock_connect_active_rows(
                source.document.at("rows"), channel, securities_),
                options.query, options.limit);
            sources.push_back(source_summary(source.document));
            refreshed = source.refreshed; age = source.age_seconds;
        } catch (const std::exception& error) {
            if (!zero_length_resource(error)) throw;
            missing = true;
            sources.push_back(missing_source(resource, error.what()));
        }
    }
    Json result = Json::object();
    result["schema"] = "tdx-market-stock-connect-native-v1"; result["generated_at"] = now_text();
    result["view"] = options.view; result["category"] = options.category;
    result["date"] = options.date.empty() ? Json(nullptr) : Json(options.date);
    result["channel"] = options.channel.empty() ? Json(nullptr) : Json(options.channel);
    result["group_id"] = options.group_id.empty() ? Json(nullptr) : Json(options.group_id);
    result["records"] = std::move(rows); result["trend"] = std::move(trend);
    result["reconciliation"] = std::move(reconciliation);
    result["sources"] = std::move(sources);
    result["detail_errors"] = std::move(errors); result["cache"] = query_cache(refreshed, age);
    result["count"] = static_cast<std::uint64_t>(result.at("records").size());
    auto health = jsn_sources_health(result.at("sources"));
    std::uint64_t missing_sources = 0;
    for (const auto& source : result.at("sources").as_array()) {
        const auto* field = ptr(source, "missing");
        if (field && field->is_bool() && field->as_bool()) ++missing_sources;
    }
    health["missing_sources"] = missing_sources;
    const bool stale = health.at("stale").as_bool();
    result["upstream_health"] = std::move(health);
    result["availability"] = missing ? "empty"
        : !result.at("detail_errors").as_array().empty() ? "partial"
        : stale ? "stale-cache" : "live";
    result["latest"] = result.at("records").as_array().empty()
        ? Json(nullptr) : result.at("records").as_array().front();
    result["semantics"] =
        "Zero-length configured master resources are normal current-empty relations; 2024 activity tables remain explicit historical snapshots, not current rankings. hsgtcg2 is a 万元 chart projection reconciled to hsgtcg1 yuan history. ggthy1 daily industry inflow is expressed in 亿元 and converted to yuan.";
    return result;
}


} // namespace tdx

