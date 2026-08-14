// pattern-matrix and standard-matrix: expand every factor in the catalog into
// its complete member list, so a reverse index can be derived from complete
// forward relations. There is no direct reverse endpoint upstream.
#include "factors_internal.hpp"

#include "tdx/cloud_workflow.hpp"
#include "tdx/common.hpp"

#include <utility>

namespace tdx {

using namespace factor_detail;

Json FactorService::query_relation_matrix(
    const FactorQuery& options, const ViewDefinition& definition,
    const std::string& key, std::time_t now) {
    const bool pattern_matrix_view = options.view == "pattern-matrix";
    const auto catalog_view = pattern_matrix_view ? "patterns" : "catalog";
    const auto member_view_name = pattern_matrix_view ? "pattern-members" : "members";

    // The catalog is fetched through the public entry point so it reuses the
    // same cache and source bookkeeping as a direct catalog query.
    FactorQuery catalog_query = options;
    catalog_query.view = catalog_view;
    catalog_query.query.clear();
    catalog_query.include_patterns = false;
    catalog_query.enrich_quotes = false;
    catalog_query.all_pages = true;
    catalog_query.limit = 10000;
    const auto catalog = query(catalog_query);

    const auto* member_definition = view_definition(member_view_name);
    Json matrices = Json::array();
    Json sources = Json::array();
    append_sources(sources, catalog);
    Json warnings = Json::array();
    std::size_t successful = 0, failed = 0, total_members = 0;

    for (const auto& factor : catalog.at("records").as_array()) {
        const auto factor_id = text(factor, "factor_id");
        Json matrix = Json::object();
        matrix["factor"] = factor;
        matrix["status"] = "pending";
        matrix["members"] = Json::array();
        matrix["member_count"] = 0;
        try {
            const auto upstream = fetch_factor_rows(
                root_, member_definition->request_id, {{"ID", factor_id}},
                true, 0, member_definition->page_size, options.max_pages,
                options.timeout_ms);
            const auto raw = cloud_result_rows(upstream.at("response"));
            auto members = normalize_factor_rows(raw, member_view_name, blocks_);
            matrix["status"] = "live";
            matrix["member_count"] = static_cast<std::uint64_t>(members.size());
            matrix["members"] = std::move(members);
            total_members += raw.size();
            ++successful;
            sources.push_back(source_document(
                upstream, *member_definition, raw.size(), true));
        } catch (const std::exception& error) {
            // One failed expansion degrades the matrix to partial rather than
            // failing the whole query.
            matrix["status"] = "unavailable";
            matrix["error"] = error.what();
            ++failed;
            warnings.push_back(std::string(pattern_matrix_view ? "Pattern" : "Standard") +
                               " factor " + factor_id +
                               " could not be expanded: " + error.what());
        }
        matrices.push_back(std::move(matrix));
    }

    Json filtered = filter_by_factor_field(matrices, options.query);
    const auto matched = filtered.as_array().size();
    const bool truncated = apply_limit(filtered, options.limit);

    Json counts = Json::object();
    counts["catalog_factors"] = catalog.at("counts").at("returned");
    counts["factors_successful"] = static_cast<std::uint64_t>(successful);
    counts["factors_failed"] = static_cast<std::uint64_t>(failed);
    counts["membership_rows"] = static_cast<std::uint64_t>(total_members);
    counts["matched_rows"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(filtered.as_array().size());
    counts["truncated"] = truncated;

    Json parameters = Json::object();
    parameters["view"] = options.view;
    parameters["query"] = options.query;
    parameters["all_pages"] = true;
    parameters["max_pages"] = options.max_pages;

    Json methodology = Json::object();
    methodology["relation"] = pattern_matrix_view
        ? "pattern-factor-to-security" : "standard-factor-to-security";
    methodology["construction"] = pattern_matrix_view
        ? "One complete 200651 expansion for every factor returned by 200650."
        : "One complete 200636 expansion for every factor returned by 200626.";
    methodology["direct_security_reverse_endpoint"] = false;
    methodology["reverse_index_derived_from_complete_forward_relations"] = true;
    methodology["investment_signal"] = false;

    Json result = Json::object();
    result["schema"] = "tdx-factors-native-v1";
    result["availability"] = failed ? "partial" : "live";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["view_title"] = definition.title;
    result["available_views"] = views_document();
    result["parameters"] = std::move(parameters);
    result["factor_id"] = Json(nullptr);
    result["selected_factor"] = Json(nullptr);
    result["query"] = options.query;
    result["pagination"] = Json(nullptr);
    result["methodology"] = std::move(methodology);
    result["quote_enrichment"] = Json(nullptr);
    result["warnings"] = std::move(warnings);
    result["sources"] = std::move(sources);
    result["counts"] = std::move(counts);
    result["records"] = std::move(filtered);
    result["cache"] = fresh_cache_document(options.cache_ttl_seconds);
    cache_[key] = CachedDocument{result, now};
    return result;
}

}  // namespace tdx
