// security: reverse lookup from one stock to the factors it currently hits.
// Standard factors come from an exact identity match in the complete 200646
// dashboard; pattern factors, which have no reverse endpoint, are derived from
// the complete 200651 matrix only when include_patterns is set.
#include "factors_internal.hpp"

#include "tdx/common.hpp"

#include <utility>

namespace tdx {

using namespace factor_detail;

namespace {

// The single output row. Falls back to the pattern evidence when the security
// is absent from the standard dashboard but still appears in a pattern list.
Json build_security_record(
    const Json* selected, const Json& pattern_evidence,
    const Json& factor_hits, const Json& unresolved, const Json& pattern_factors) {
    Json record = Json::object();
    if (selected) {
        record["security"] = selected->at("security");
        record["source_signal_count"] = selected->at("signal_count");
        record["source_selected_factors_text"] = selected->at("selected_factors_text");
        record["change_pct"] = selected->at("change_pct");
        record["price"] = selected->at("price");
        record["ten_day_change_pct"] = selected->at("ten_day_change_pct");
        record["safety_score"] = selected->at("safety_score");
    } else {
        const auto& evidence = pattern_evidence.as_array().front();
        record["security"] = evidence.at("member_record").at("security");
        record["source_signal_count"] = 0;
        record["source_selected_factors_text"] = "";
        record["change_pct"] = evidence.at("member_record").at("change_pct");
        record["price"] = evidence.at("member_record").at("price");
        record["ten_day_change_pct"] = evidence.at("member_record").at("ten_day_change_pct");
        record["safety_score"] = Json(nullptr);
    }
    record["factors"] = factor_hits;
    record["unresolved_factor_names"] = unresolved;
    record["pattern_factors"] = pattern_factors;
    record["pattern_evidence"] = pattern_evidence;
    return record;
}

Json security_methodology(bool include_patterns) {
    Json methodology = Json::object();
    methodology["relation"] = "security-to-standard-factor";
    methodology["source"] = "Exact security identity in the complete 200646 dashboard, joined to 200626 by factor name.";
    methodology["pattern_factor_reverse_available"] = include_patterns;
    methodology["pattern_factor_boundary"] = include_patterns
        ? "No direct reverse endpoint exists; results are derived from all complete 200651 forward lists."
        : "Set include_patterns=true to build the complete 57-factor 200651 reverse index.";
    methodology["tcalc_formula_equivalence"] = false;
    methodology["tcalc_boundary"] = "mod_Factor.dll family names are not TCalc formula codes; similarly named formulas can have different predicates.";
    methodology["investment_signal"] = false;
    return methodology;
}

}  // namespace

Json FactorService::query_security(
    const FactorQuery& options, const ViewDefinition& definition,
    const std::string& key, std::time_t now) {
    FactorQuery catalog_query = options;
    catalog_query.view = "catalog";
    catalog_query.market.clear();
    catalog_query.code.clear();
    catalog_query.include_patterns = false;
    catalog_query.enrich_quotes = false;
    catalog_query.all_pages = true;
    catalog_query.limit = 10000;
    const auto catalog = query(catalog_query);

    FactorQuery dashboard_query = catalog_query;
    dashboard_query.view = "dashboard";
    dashboard_query.limit = 30000;
    const auto dashboard = query(dashboard_query);

    const auto reverse = factor_hits_for_security(
        catalog.at("records"), dashboard.at("records"),
        options.market, options.code);
    const auto& selected_record = reverse.at("dashboard_record");
    const Json* selected = selected_record.is_null() ? nullptr : &selected_record;
    const auto& factor_hits = reverse.at("factors");
    const auto& unresolved = reverse.at("unresolved_factor_names");

    Json pattern_factors = Json::array();
    Json pattern_evidence = Json::array();
    Json pattern_matrix = Json(nullptr);
    if (options.include_patterns) {
        FactorQuery pattern_query = catalog_query;
        pattern_query.view = "pattern-matrix";
        pattern_query.limit = 10000;
        pattern_matrix = query(pattern_query);
        const auto pattern_reverse = pattern_hits_for_security(
            pattern_matrix.at("records"), options.market, options.code);
        pattern_factors = pattern_reverse.at("factors");
        pattern_evidence = pattern_reverse.at("evidence");
    }

    const bool relation_found = selected || !pattern_factors.as_array().empty();
    Json records = Json::array();
    if (relation_found)
        records.push_back(build_security_record(
            selected, pattern_evidence, factor_hits, unresolved, pattern_factors));

    Json quote_enrichment = Json::object();
    quote_enrichment["requested"] = options.enrich_quotes;
    quote_enrichment["status"] = options.enrich_quotes ? "pending" : "not-requested";
    Json warnings = Json::array();
    if (!pattern_matrix.is_null()) {
        for (const auto& warning : pattern_matrix.at("warnings").as_array())
            warnings.push_back(warning);
        if (text(pattern_matrix, "availability") == "partial")
            warnings.push_back("Pattern reverse coverage is partial because at least one 200651 expansion failed.");
    }
    if (options.enrich_quotes && relation_found) {
        try {
            quote_enrichment = enrich_quotes(root_, records, blocks_, options.timeout_ms);
        } catch (const std::exception& error) {
            // Quotes are decoration; the factor relations stand without them.
            quote_enrichment["status"] = "unavailable";
            quote_enrichment["error"] = error.what();
            warnings.push_back("Optional public L1 quote enrichment failed; factor relations remain valid.");
        }
    } else if (options.enrich_quotes) {
        quote_enrichment["status"] = "empty";
        quote_enrichment["requested_securities"] = 0;
        quote_enrichment["received_securities"] = 0;
        quote_enrichment["matched_records"] = 0;
    }

    Json sources = Json::array();
    append_sources(sources, catalog);
    append_sources(sources, dashboard);
    if (!pattern_matrix.is_null()) append_sources(sources, pattern_matrix);

    Json counts = Json::object();
    counts["catalog_rows"] = catalog.at("counts").at("returned");
    counts["dashboard_rows"] = dashboard.at("counts").at("returned");
    counts["security_found"] = relation_found;
    counts["standard_dashboard_found"] = selected != nullptr;
    counts["factor_hits"] = static_cast<std::uint64_t>(factor_hits.size());
    counts["pattern_factor_hits"] = static_cast<std::uint64_t>(pattern_factors.size());
    counts["pattern_factors_successful"] = pattern_matrix.is_null()
        ? Json(nullptr) : pattern_matrix.at("counts").at("factors_successful");
    counts["pattern_factors_failed"] = pattern_matrix.is_null()
        ? Json(nullptr) : pattern_matrix.at("counts").at("factors_failed");
    counts["unresolved_factor_names"] = static_cast<std::uint64_t>(unresolved.size());
    counts["returned"] = static_cast<std::uint64_t>(records.as_array().size());
    counts["truncated"] = false;

    Json parameters = Json::object();
    parameters["market"] = options.market;
    parameters["code"] = options.code;
    parameters["include_patterns"] = options.include_patterns;
    parameters["coverage"] = options.include_patterns
        ? "standard-factors-from-200646-and-patterns-from-complete-200651-matrix"
        : "standard-factors-from-200646";

    Json result = Json::object();
    result["schema"] = "tdx-factors-native-v1";
    result["availability"] = !pattern_matrix.is_null() &&
        text(pattern_matrix, "availability") == "partial" ? "partial" : "live";
    result["generated_at"] = now_text();
    result["view"] = "security";
    result["view_title"] = definition.title;
    result["available_views"] = views_document();
    result["parameters"] = std::move(parameters);
    result["factor_id"] = Json(nullptr);
    result["selected_factor"] = Json(nullptr);
    result["query"] = "";
    result["pagination"] = Json(nullptr);
    result["methodology"] = security_methodology(options.include_patterns);
    result["quote_enrichment"] = std::move(quote_enrichment);
    result["warnings"] = std::move(warnings);
    result["sources"] = std::move(sources);
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["cache"] = fresh_cache_document(options.cache_ttl_seconds);
    cache_[key] = CachedDocument{result, now};
    return result;
}

}  // namespace tdx
