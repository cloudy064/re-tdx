#include "leverage_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tdx {
using namespace leverage_detail;

Json LeverageService::query_margin(const MarginQuery& options) {
    if (options.limit < 1 || options.limit > 10000) throw Error("margin limit is outside 1..10000");
    if (options.view != "market" && options.view != "transfer" &&
        options.view != "ranking" &&
        options.view != "security" && options.view != "classifications" &&
        options.view != "classification-history")
        throw Error("margin view must be market, transfer, ranking, security, classifications, or classification-history");
    Json rows = Json::array(), trend = Json::array(), sources = Json::array(), errors = Json::array();
    bool refreshed = false; int age = 0;
    std::string effective_date = trim(options.date);
    if (options.view == "market") {
        auto source = fetch_resource("list/func_rzrq101_1.jsn", options.refresh,
                                     options.cache_ttl_seconds, options.timeout_ms);
        rows = filter_rows(normalize_margin_market_rows(source.document.at("rows")),
                           options.query, options.limit);
        sources.push_back(source_summary(source.document)); refreshed = source.refreshed; age = source.age_seconds;
    } else if (options.view == "transfer") {
        auto source = fetch_resource("list/func_rzt101_1.jsn", options.refresh,
                                     options.cache_ttl_seconds, options.timeout_ms);
        rows = filter_rows(normalize_margin_transfer_rows(source.document.at("rows")),
                           options.query, options.limit);
        sources.push_back(source_summary(source.document));
        refreshed = source.refreshed; age = source.age_seconds;
    } else if (options.view == "ranking") {
        const auto* found = find_resource(margin_resources, options.category);
        if (!found) throw Error("unknown margin ranking category");
        auto source = fetch_resource(std::string(found->resource), options.refresh,
                                     options.cache_ttl_seconds, options.timeout_ms);
        rows = filter_rows(normalize_margin_security_rows(source.document.at("rows"), securities_),
                           options.query, options.limit);
        sources.push_back(source_summary(source.document)); refreshed = source.refreshed; age = source.age_seconds;
    } else if (options.view == "security") {
        const int id = market_id(options.market, true);
        if (!digits(options.code, 6)) throw Error("margin security code must contain six digits");
        const bool etf = options.category == "etf";
        const auto bases = etf
            ? std::vector<std::pair<std::string, bool>>{{"rzrq3/", false}, {"rzrq4/", true}}
            : std::vector<std::pair<std::string, bool>>{{"rzrq1/", false}, {"rzrq2/", true}};
        for (const auto& [base, is_trend] : bases) {
            const auto resource = base + std::to_string(id) + options.code + ".jsn";
            try {
                auto source = fetch_resource(resource, options.refresh,
                                             options.detail_cache_ttl_seconds, options.timeout_ms);
                sources.push_back(source_summary(source.document)); refreshed = refreshed || source.refreshed;
                age = std::max(age, source.age_seconds);
                if (is_trend) {
                    for (const auto& row : source.document.at("rows").as_array()) {
                        Json item = Json::object(); item["date"] = value_text(row, "date");
                        item["financing_balance_float_market_pct"] = number_json(value_number(row, "rzzltsz"));
                        item["short_balance_float_shares_pct"] = number_json(value_number(row, "zqzltgf"));
                        trend.push_back(std::move(item));
                    }
                    sort_date(trend, false);
                } else rows = normalize_margin_security_rows(source.document.at("rows"), securities_);
            } catch (const std::exception& error) {
                Json failure = Json::object(); failure["resource"] = resource; failure["message"] = error.what();
                errors.push_back(std::move(failure));
            }
        }
    } else if (options.view == "classifications") {
        const auto* found = find_resource(margin_classification_resources, options.category);
        if (!found)
            throw Error("margin classification category must be industry, concept, or style");
        auto selected_date = effective_date;
        if (!selected_date.empty() && !digits(selected_date, 8))
            throw Error("margin classification date must contain eight digits");
        if (selected_date.empty()) {
            auto master = fetch_resource("list/func_rzrq101_1.jsn", options.refresh,
                                         options.cache_ttl_seconds, options.timeout_ms);
            const auto market_rows = normalize_margin_market_rows(master.document.at("rows"));
            if (market_rows.as_array().empty())
                throw Error("margin market series has no date for classification details");
            selected_date = value_text(market_rows.as_array().front(), "date");
            sources.push_back(source_summary(master.document));
            refreshed = master.refreshed; age = master.age_seconds;
        }
        const auto resource = std::string(found->resource) + selected_date + ".jsn";
        effective_date = selected_date;
        auto source = fetch_resource(resource, options.refresh,
                                     options.detail_cache_ttl_seconds, options.timeout_ms);
        rows = filter_rows(normalize_margin_classification_rows(
            source.document.at("rows"), options.category), options.query, options.limit);
        sources.push_back(source_summary(source.document));
        refreshed = refreshed || source.refreshed; age = std::max(age, source.age_seconds);
    } else {
        if (!safe_group_id(trim(options.group_id)))
            throw Error("margin classification-history requires --group-id");
        const auto resource = "rzrq5/" + trim(options.group_id) + ".jsn";
        auto source = fetch_resource(resource, options.refresh,
                                     options.detail_cache_ttl_seconds, options.timeout_ms);
        rows = filter_rows(normalize_margin_classification_history_rows(
            source.document.at("rows")), options.query, options.limit);
        sources.push_back(source_summary(source.document));
        refreshed = source.refreshed; age = source.age_seconds;
    }
    Json result = Json::object();
    result["schema"] = "tdx-market-margin-native-v1"; result["generated_at"] = now_text();
    result["view"] = options.view; result["category"] = options.category;
    result["date"] = effective_date.empty() ? Json(nullptr) : Json(effective_date);
    result["group_id"] = options.group_id.empty() ? Json(nullptr) : Json(options.group_id);
    result["records"] = std::move(rows); result["trend"] = std::move(trend);
    result["sources"] = std::move(sources); result["detail_errors"] = std::move(errors);
    result["cache"] = query_cache(refreshed, age);
    result["count"] = static_cast<std::uint64_t>(result.at("records").size());
    auto health = jsn_sources_health(result.at("sources"));
    const bool stale = health.at("stale").as_bool();
    result["upstream_health"] = std::move(health);
    result["availability"] = !result.at("detail_errors").as_array().empty()
        ? "partial" : stale ? "stale-cache" : "live";
    result["latest"] = result.at("records").as_array().empty()
        ? Json(nullptr) : result.at("records").as_array().front();
    result["semantics"] =
        "Security details use rzrq1/2 for stocks and rzrq3/4 for ETF when category=etf. "
        "Classification snapshots use raw yuan. rzrq5 chart history is explicitly converted "
        "from 亿元 for financing/difference and 千万元 for short balance. "
        "The transfer view keeps func_rzt101 amounts in yuan and quantities in shares; "
        "upstream empty values remain null and are never inferred as zero.";
    return result;
}


} // namespace tdx

