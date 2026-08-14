#include "bond_reference_internal.hpp"

namespace tdx::bond_reference_detail {
namespace {

// Facet tallies as name/count rows, in the map's sorted order.
Json counts_json(const std::map<std::string, std::uint64_t>& values) {
    Json result = Json::array();
    for (const auto& [name, count] : values) {
        Json row = Json::object();
        row["name"] = name;
        row["count"] = count;
        result.push_back(std::move(row));
    }
    return result;
}

// Empty text reports as null rather than "", so absent and blank stay distinct.
Json text_or_null(const std::string& value) {
    return value.empty() ? Json(nullptr) : Json(value);
}

}  // namespace

Json build_summary(const BondReferenceSource& source, std::size_t source_row_count,
                   std::uint64_t matched, const RowFacets& facets) {
    Json summary = Json::object();
    summary["source_group"] = source.group;
    summary["source_bucket"] = source.bucket;
    summary["source_name"] = source.name;
    summary["source_row_count"] = static_cast<std::uint64_t>(source_row_count);
    summary["matched_count"] = matched;
    summary["matched_unique_security_count"] =
        static_cast<std::uint64_t>(facets.matched_security_ids.size());
    summary["credit_ratings"] = counts_json(facets.credit_counts);
    summary["rate_types"] = counts_json(facets.rate_counts);
    summary["bond_types"] = counts_json(facets.type_counts);
    summary["earliest_maturity_date"] = text_or_null(facets.earliest_maturity);
    summary["latest_maturity_date"] = text_or_null(facets.latest_maturity);
    return summary;
}

Json source_option_list() {
    Json source_options = Json::array();
    for (const auto& source : bond_reference_sources()) {
        Json row = Json::object();
        row["group"] = source.group;
        row["bucket"] = source.bucket;
        row["name"] = source.name;
        row["resource"] = source.resource;
        source_options.push_back(std::move(row));
    }
    return source_options;
}

std::string_view query_semantics() {
    return
        "The credit-rating, rate-type and bond-category pages are overlapping reference views. "
        "Their JSN files contain static terms and coupon schedules; quote/depth fields shown "
        "by the desktop host are intentionally not fabricated. Schedule rates stored as "
        "decimal ratios are normalized to percent. Large AAA/fixed-rate resources are fetched "
        "only when their bucket is selected. The all-category bucket exposes the downloaded "
        "42k-row all-bond archive and defers coupon-array expansion until result pagination. "
        "Category projection audits are opt-in because they can load large Shanghai/Shenzhen "
        "tables; the private-bond and asset-backed client masters are compared separately rather "
        "than replacing the more complete legacy category sources. "
        "Policy-financial GM values are explicit 100-million-yuan source units and are converted "
        "to yuan; its ten-row all-market master is reconciled against the seven-row Shanghai and "
        "three-row Shenzhen projections.";
}

Json build_query_filters(const BondReferenceQuery& options) {
    Json filters = Json::object();
    filters["group"] = options.group;
    filters["bucket"] = options.bucket;
    filters["market"] = text_or_null(options.market);
    filters["code"] = text_or_null(options.code);
    filters["query"] = options.query;
    filters["sort"] = options.sort;
    filters["order"] = options.order;
    filters["offset"] = options.offset;
    filters["limit"] = options.limit;
    filters["include_projections"] = options.include_projections;
    return filters;
}

}  // namespace tdx::bond_reference_detail
