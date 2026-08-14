// overview: market breadth over the complete current dashboard, reconciled
// against the standard relation matrix. Statistics only, no historical series.
#include "factors_internal.hpp"

#include "tdx/common.hpp"

#include <utility>

namespace tdx {

using namespace factor_detail;

Json FactorService::query_overview(
    const FactorQuery& options, const ViewDefinition& definition,
    const std::string& key, std::time_t now) {
    // Three complete source views feed the breadth document; each goes through
    // the public entry point so their caches and source records are shared.
    FactorQuery source_query = options;
    source_query.query.clear();
    source_query.include_patterns = false;
    source_query.enrich_quotes = false;
    source_query.all_pages = true;
    source_query.limit = 30000;
    source_query.view = "catalog";
    const auto catalog = query(source_query);
    source_query.view = "dashboard";
    const auto dashboard = query(source_query);
    source_query.view = "standard-matrix";
    const auto standard_matrix = query(source_query);

    auto breadth = build_factor_breadth_document(
        catalog.at("records"), dashboard.at("records"));
    breadth = reconcile_factor_breadth_document(
        std::move(breadth), dashboard.at("records"),
        standard_matrix.at("records"));

    Json filtered = filter_by_factor_field(breadth.at("records"), options.query);
    const auto matched = filtered.as_array().size();
    const bool truncated = apply_limit(filtered, options.limit);

    // Co-occurrence pairs are matched whole rather than by nested factor.
    Json cooccurrence = Json::array();
    for (const auto& pair : breadth.at("cooccurrence").as_array())
        if (options.query.empty() || searchable(pair, options.query))
            cooccurrence.push_back(pair);

    Json sources = Json::array();
    append_sources(sources, catalog);
    append_sources(sources, dashboard);
    append_sources(sources, standard_matrix);

    Json counts = breadth.at("counts");
    counts["matched_rows"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(filtered.as_array().size());
    counts["truncated"] = truncated;

    Json parameters = Json::object();
    parameters["view"] = "overview";
    parameters["query"] = options.query;
    parameters["all_pages"] = true;

    Json methodology = Json::object();
    methodology["source"] = "Derived only from complete 200646 current dashboard rows joined to 200626 catalog metadata.";
    methodology["coverage_denominator"] = dashboard.at("counts").at("returned");
    methodology["cooccurrence_metric"] = "Jaccard percentage over current dashboard membership.";
    methodology["membership_reconciliation"] =
        "Each dashboard factor label is compared with the complete corresponding 200636 member list.";
    methodology["historical_series"] = false;
    methodology["investment_signal"] = false;

    Json result = Json::object();
    result["schema"] = "tdx-factors-native-v1";
    result["availability"] = text(standard_matrix, "availability") == "partial"
        ? "partial" : "live";
    result["generated_at"] = now_text();
    result["view"] = "overview";
    result["view_title"] = definition.title;
    result["available_views"] = views_document();
    result["parameters"] = std::move(parameters);
    result["factor_id"] = Json(nullptr);
    result["selected_factor"] = Json(nullptr);
    result["query"] = options.query;
    result["pagination"] = Json(nullptr);
    result["methodology"] = std::move(methodology);
    result["quote_enrichment"] = Json(nullptr);
    result["warnings"] = standard_matrix.at("warnings");
    result["sources"] = std::move(sources);
    result["counts"] = std::move(counts);
    result["records"] = std::move(filtered);
    result["cooccurrence"] = std::move(cooccurrence);
    result["signal_count_distribution"] = breadth.at("signal_count_distribution");
    result["safety_distribution"] = breadth.at("safety_distribution");
    result["unresolved_factor_names"] = breadth.at("unresolved_factor_names");
    result["cache"] = fresh_cache_document(options.cache_ttl_seconds);
    cache_[key] = CachedDocument{result, now};
    return result;
}

}  // namespace tdx
