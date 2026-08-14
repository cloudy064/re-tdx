// The direct tabular views: catalog, patterns, members, pattern-members,
// dashboard and intraday-radar. One upstream request, normalized, filtered and
// paged. This is also the only path that can answer from a stale cache entry.
#include "factors_internal.hpp"

#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"

#include <map>
#include <utility>

namespace tdx {

using namespace factor_detail;

namespace {

// members and pattern-members must resolve factor_id against the current
// catalog before requesting members, both to reject stale ids and to attach the
// selected factor's metadata to the response.
Json select_factor_from_catalog(
    const std::filesystem::path& root, const BlockData& blocks,
    const std::string& view, const std::string& factor_id,
    int max_pages, int timeout_ms, Json& catalog_source, std::size_t& catalog_count) {
    const auto* catalog_definition = view_definition(view == "members" ? "catalog" : "patterns");
    const auto catalog_upstream = fetch_factor_rows(
        root, catalog_definition->request_id, {}, false, -1, 0, max_pages, timeout_ms);
    const auto raw_catalog = cloud_result_rows(catalog_upstream.at("response"));
    const auto catalog = normalize_factor_rows(raw_catalog, catalog_definition->view, blocks);
    catalog_count = catalog.size();
    Json selected_factor = Json(nullptr);
    for (const auto& record : catalog.as_array())
        if (text(record, "factor_id") == factor_id) selected_factor = record;
    if (selected_factor.is_null())
        throw Error("selected factor_id is absent from the current " +
                    std::string(catalog_definition->view) + " table");
    catalog_source = source_document(
        catalog_upstream, *catalog_definition, raw_catalog.size(), false);
    return selected_factor;
}

Json table_methodology(const std::string& view) {
    Json methodology = Json::object();
    methodology["percentage_units"] = "percentage points as returned by TDX; values are not divided by 100";
    methodology["catalog_backtest_window_days"] = 120;
    methodology["catalog_metrics_are_upstream"] = true;
    methodology["investment_signal"] = false;
    if (view == "dashboard")
        methodology["client_note"] = "TDX UI shows only the first 200 securities; all_pages can expose later server pages.";
    if (view == "intraday-radar") {
        methodology["server_filter"] = "safety_score > 60";
        methodology["rules"] = "Volume-price rise: intraday volume and price make new highs with gain >3%; trend turn: intraday uptrend forms with gain >3%.";
    }
    return methodology;
}

}  // namespace

Json FactorService::query_table(
    const FactorQuery& options, const ViewDefinition& definition,
    const std::string& key, std::time_t now) {
    const bool member_view = options.view == "members" || options.view == "pattern-members";

    Json selected_factor = Json(nullptr);
    Json catalog_source = Json(nullptr);
    std::size_t catalog_count = 0;
    if (member_view)
        selected_factor = select_factor_from_catalog(
            root_, blocks_, options.view, options.factor_id, options.max_pages,
            options.timeout_ms, catalog_source, catalog_count);

    std::map<std::string, std::string> replacements;
    if (member_view) replacements["ID"] = options.factor_id;
    Json upstream;
    std::string upstream_error;
    if (!try_fetch_factor_rows(
            root_, definition.request_id, replacements, options.all_pages,
            options.all_pages ? 0 : -1, options.all_pages ? definition.page_size : 0,
            options.max_pages, options.timeout_ms, upstream, upstream_error)) {
        // Transient upstream failure with retries exhausted: serve the previous
        // document marked stale, or surface the error when nothing is cached.
        const auto cached = cache_.find(key);
        if (cached == cache_.end()) throw Error(upstream_error);
        auto stale = cached->second.document;
        stale["availability"] = "stale-cache";
        stale["cache"]["hit"] = true;
        stale["cache"]["stale"] = true;
        stale["cache"]["age_seconds"] = static_cast<std::uint64_t>(
            std::max<std::time_t>(0, now - cached->second.fetched_at));
        stale["cache"]["upstream_error"] = upstream_error;
        return stale;
    }

    const auto raw_rows = cloud_result_rows(upstream.at("response"));
    auto normalized = normalize_factor_rows(raw_rows, options.view, blocks_);
    Json filtered = Json::array();
    for (const auto& record : normalized.as_array())
        if (searchable(record, options.query)) filtered.push_back(record);
    const auto matched_count = filtered.as_array().size();
    const bool truncated = apply_limit(filtered, options.limit);

    Json quote_enrichment = Json::object();
    quote_enrichment["requested"] = options.enrich_quotes;
    quote_enrichment["status"] = options.enrich_quotes ? "pending" : "not-requested";
    Json warnings = Json::array();
    if (options.enrich_quotes && options.view != "catalog" && options.view != "patterns") {
        try {
            quote_enrichment = enrich_quotes(root_, filtered, blocks_, options.timeout_ms);
        } catch (const std::exception& error) {
            quote_enrichment["status"] = "unavailable";
            quote_enrichment["error"] = error.what();
            warnings.push_back("Optional public L1 quote enrichment failed; factor source rows remain valid.");
        }
    } else if (options.enrich_quotes) {
        // catalog and patterns are factor tables, not security lists.
        quote_enrichment["status"] = "not-applicable";
        warnings.push_back("Quote enrichment applies only to security-list factor views.");
    }

    Json sources = Json::array();
    if (!catalog_source.is_null()) sources.push_back(catalog_source);
    sources.push_back(source_document(upstream, definition, raw_rows.size(), options.all_pages));

    Json counts = Json::object();
    counts["catalog_rows"] = static_cast<std::uint64_t>(catalog_count);
    counts["upstream_rows"] = static_cast<std::uint64_t>(raw_rows.size());
    counts["normalized_rows"] = static_cast<std::uint64_t>(normalized.size());
    counts["query_filtered_out"] = static_cast<std::uint64_t>(normalized.size() - matched_count);
    counts["matched_rows"] = static_cast<std::uint64_t>(matched_count);
    counts["returned"] = static_cast<std::uint64_t>(filtered.as_array().size());
    counts["truncated"] = truncated;

    Json pagination = Json::object();
    pagination["all_pages"] = options.all_pages;
    pagination["page_size"] = definition.page_size;
    pagination["max_pages"] = options.max_pages;
    pagination["client_default_is_first_page_only"] = true;
    pagination["client_default_limit"] = definition.page_size;

    Json result = Json::object();
    result["schema"] = "tdx-factors-native-v1";
    result["availability"] = "live";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["view_title"] = definition.title;
    result["available_views"] = views_document();
    Json parameters = Json::object();
    parameters["view"] = options.view;
    parameters["factor_id"] = options.factor_id.empty()
        ? Json(nullptr) : Json(options.factor_id);
    parameters["query"] = options.query;
    parameters["all_pages"] = options.all_pages;
    parameters["include_patterns"] = false;
    parameters["quotes"] = options.enrich_quotes;
    result["parameters"] = std::move(parameters);
    result["factor_id"] = options.factor_id.empty() ? Json(nullptr) : Json(options.factor_id);
    result["selected_factor"] = selected_factor;
    result["query"] = options.query;
    result["pagination"] = std::move(pagination);
    result["methodology"] = table_methodology(options.view);
    result["quote_enrichment"] = std::move(quote_enrichment);
    result["warnings"] = std::move(warnings);
    result["sources"] = std::move(sources);
    result["counts"] = std::move(counts);
    result["records"] = std::move(filtered);
    result["cache"] = fresh_cache_document(options.cache_ttl_seconds);
    cache_[key] = CachedDocument{result, now};
    return result;
}

}  // namespace tdx
