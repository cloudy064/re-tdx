#include "bond_reference_internal.hpp"

namespace tdx {
using namespace bond_reference_detail;

Json BondReferenceService::query(const BondReferenceQuery& input) {
    BondReferenceQuery options = input;
    const auto& selected_source = resolve_query(options);

    auto fetched = fetch_source(selected_source, options);
    // A reference into the fetched copy, not into cache_, so the projection
    // fetches below cannot invalidate it.
    const auto& raw_rows = fetched.document.at("rows");
    Json projection_sources = Json::array();
    auto projection_reconciliation = audit_category_projections(
        selected_source, options, fetched, projection_sources);

    RowFacets facets;
    auto matched_rows = collect_matching_rows(
        raw_rows, selected_source, options, facets);
    sort_matching_rows(matched_rows, options);
    const auto matched = static_cast<std::uint64_t>(matched_rows.size());
    auto records = paginate_rows(matched_rows, raw_rows, selected_source, options);

    Json sources = Json::array();
    sources.push_back(jsn_source_metadata(fetched.document));
    for (auto& metadata : projection_sources.as_array())
        sources.push_back(std::move(metadata));
    const auto health = jsn_sources_health(sources);

    Json result = Json::object();
    result["schema"] = "tdx-market-bond-reference-native-v1";
    result["generated_at"] = now_text();
    result["availability"] = health.at("stale").as_bool()
        ? "stale-cache" : matched ? "live" : "empty";
    result["source_options"] = source_option_list();
    result["summary"] = build_summary(
        selected_source, raw_rows.size(), matched, facets);
    result["match_count"] = matched;
    result["returned"] = static_cast<std::uint64_t>(records.size());
    result["records"] = std::move(records);
    result["filters"] = build_query_filters(options);
    result["sources"] = std::move(sources);
    result["projection_reconciliation"] = std::move(projection_reconciliation);
    result["upstream_health"] = health;
    Json cache = Json::object();
    cache["refreshed"] = fetched.refreshed;
    cache["age_seconds"] = fetched.age_seconds;
    result["cache"] = std::move(cache);
    result["semantics"] = std::string(query_semantics());
    return result;
}

}  // namespace tdx
