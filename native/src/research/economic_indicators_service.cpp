// EconomicIndicatorService::query: validate, fetch the master, then either page
// the catalog or expand one indicator's optional detail resources. Response keys
// are inserted in output order, so the composition sequence here is load-bearing.
#include "tdx/economic_indicators_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace tdx {

using namespace tdx::economic_indicator_detail;

void EconomicIndicatorService::load_history(
    const EconomicIndicatorQuery& options, QueryState& state) {
    try {
        auto detail = fetch_resource(state.selected.at("history_resource").as_string(),
            options.refresh, options.detail_cache_ttl_seconds, options.timeout_ms);
        state.detail_refreshed = state.detail_refreshed || detail.refreshed;
        state.detail_age = std::max(state.detail_age, detail.age_seconds);
        state.sources.push_back(jsn_source_metadata(detail.document));
        state.history = normalize_economic_indicator_history(detail.document.at("rows"));
        // The series is sorted ascending, so trimming from the front keeps the
        // most recent history_limit points.
        if (state.history.size() > static_cast<std::size_t>(options.history_limit))
            state.history.as_array().erase(
                state.history.as_array().begin(),
                state.history.as_array().end() - options.history_limit);
    } catch (const std::exception& error) {
        record_failure(state.errors, state.selected.at("history_resource"), error.what());
    }
}

void EconomicIndicatorService::attach_quotes(
    const EconomicIndicatorQuery& options, const Json& paged_raw, QueryState& state) {
    std::vector<std::string> requested;
    for (const auto& row : state.related.as_array()) {
        const auto& security = row.at("security");
        requested.push_back(security.at("market").as_string() + ":" +
                            security.at("code").as_string());
    }
    try {
        auto quotes = fetch_quotes(requested, options);
        state.quote_refreshed = quotes.refreshed;
        state.quote_age = quotes.age_seconds;
        state.quote_source = quote_source_metadata(quotes.document, quotes.refreshed);
        // Re-normalize the same page with quote rows attached rather than
        // patching the rows already produced.
        state.related = normalize_economic_indicator_relations(
            paged_raw, quotes.document.at("records"), blocks_.securities);
    } catch (const std::exception& error) {
        record_failure(state.errors, Json("public-l1-snapshot"), error.what());
    }
}

void EconomicIndicatorService::load_related(
    const EconomicIndicatorQuery& options, QueryState& state) {
    try {
        auto detail = fetch_resource(state.selected.at("related_resource").as_string(),
            options.refresh, options.detail_cache_ttl_seconds, options.timeout_ms);
        state.detail_refreshed = state.detail_refreshed || detail.refreshed;
        state.detail_age = std::max(state.detail_age, detail.age_seconds);
        state.sources.push_back(jsn_source_metadata(detail.document));
        auto all = normalize_economic_indicator_relations(
            detail.document.at("rows"), Json::array(), blocks_.securities);
        state.summary["related_security_count"] = static_cast<std::uint64_t>(all.size());
        Json filtered = Json::array();
        const auto needle = lower_ascii(options.query);
        for (const auto& row : all.as_array())
            if (needle.empty() || json_contains(row, needle)) filtered.push_back(row);
        state.matched = filtered.size();
        // Page the normalized rows, then hand the untouched upstream payloads to
        // the normalizer. The page must be a named local: iterating
        // page_array(...).as_array() directly would dangle, because a range-for
        // does not extend the lifetime of the Json temporary behind the reference.
        const Json paged = page_array(filtered, options.offset, options.limit);
        Json paged_raw = Json::array();
        for (const auto& row : paged.as_array()) paged_raw.push_back(row.at("raw"));
        state.related = normalize_economic_indicator_relations(
            paged_raw, Json::array(), blocks_.securities);
        if (options.include_quotes && state.related.size())
            attach_quotes(options, paged_raw, state);
    } catch (const std::exception& error) {
        record_failure(state.errors, state.selected.at("related_resource"), error.what());
    }
}

Json EconomicIndicatorService::query(const EconomicIndicatorQuery& input) {
    EconomicIndicatorQuery options = input;
    validate_indicator_query(options);

    const auto master = fetch_resource(master_resource, options.refresh,
                                       options.master_cache_ttl_seconds,
                                       options.timeout_ms);
    auto indicators = normalize_economic_indicator_rows(master.document.at("rows"));

    QueryState state;
    state.sources.push_back(jsn_source_metadata(master.document));
    // Built from the unfiltered master so the histograms describe the whole
    // catalog rather than the returned page.
    state.summary = build_indicator_summary(indicators);

    if (options.view == "catalog") {
        indicators = filter_sort_page_catalog(indicators, options, state.matched);
    } else {
        for (const auto& row : indicators.as_array())
            if (row.at("indicator_id").as_string() == options.indicator_id) {
                state.selected = row;
                break;
            }
        if (state.selected.is_null())
            throw Error("indicator_id is absent from the active economic indicator catalog");
        indicators = Json::array();
        state.matched = 1;
        if (options.include_history) load_history(options, state);
        if (options.include_related) load_related(options, state);
        // Inserted after the related block so the summary key order matches the
        // original: related_security_count precedes history_points.
        state.summary["history_points"] = static_cast<std::uint64_t>(state.history.size());
    }

    const auto health = jsn_sources_health(state.sources);
    Json result = Json::object();
    result["schema"] = "tdx-market-economic-indicators-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    // Read before selected is moved out below.
    result["availability"] = health.at("stale").as_bool() ? "stale-cache"
        : state.errors.size() ? "partial" : options.view == "catalog" || !state.selected.is_null()
            ? "live" : "empty";
    result["indicators"] = std::move(indicators);
    result["selected_indicator"] = std::move(state.selected);
    result["history"] = std::move(state.history);
    result["related_securities"] = std::move(state.related);
    result["summary"] = std::move(state.summary);
    result["errors"] = std::move(state.errors);
    Json counts = Json::object();
    counts["matched"] = state.matched;
    counts["returned_indicators"] = static_cast<std::uint64_t>(result.at("indicators").size());
    counts["history_points"] = static_cast<std::uint64_t>(result.at("history").size());
    counts["returned_related_securities"] =
        static_cast<std::uint64_t>(result.at("related_securities").size());
    result["counts"] = std::move(counts);
    Json filters = Json::object();
    filters["indicator_id"] = options.indicator_id.empty()
        ? Json(nullptr) : Json(options.indicator_id);
    filters["query"] = options.query;
    filters["sort"] = options.sort;
    filters["order"] = options.order;
    filters["offset"] = options.offset;
    filters["limit"] = options.limit;
    result["filters"] = std::move(filters);
    result["sources"] = std::move(state.sources);
    result["upstream_health"] = health;
    result["quote_source"] = std::move(state.quote_source);
    Json cache = Json::object();
    cache["master_refreshed"] = master.refreshed;
    cache["master_age_seconds"] = master.age_seconds;
    cache["detail_refreshed"] = state.detail_refreshed;
    cache["detail_age_seconds"] = state.detail_age;
    cache["quote_refreshed"] = state.quote_refreshed;
    cache["quote_age_seconds"] = state.quote_age;
    result["cache"] = std::move(cache);
    result["semantics"] =
        "TDX JJZB is an economic/commodity indicator surface. The 72-row master contains current value, unit, month-on-month/year-on-year changes and frequency; jjzb1/<id> is the historical series and jjzb2/<id> is the related-security set. Quote columns in the client are host fields and are only emitted when a public 0x054C snapshot succeeds.";
    return result;
}

}  // namespace tdx
