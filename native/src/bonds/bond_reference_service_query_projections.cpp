#include "bond_reference_internal.hpp"

namespace tdx {
using namespace bond_reference_detail;

namespace {

// A fetched comparison table paired with the source that describes it.  Only the
// document survives the fetch; the cache flags are folded into the caller's
// FetchResult before it is stored.
using ProjectionDocument = std::pair<const BondReferenceSource*, Json>;

// The set of security ids a source document normalizes to.  Rows that fail to
// normalize are skipped so one malformed row cannot void an entire audit.
std::set<std::string> security_ids(const Json& document,
                                   const BondReferenceSource& source) {
    std::set<std::string> ids;
    const auto& rows = document.at("rows");
    for (std::size_t index = 0; index < rows.size(); ++index) {
        try {
            const auto normalized = normalize_row(
                rows.as_array()[index], source, false, index);
            ids.insert(normalized.at("security").at("security_id").as_string());
        } catch (...) {}
    }
    return ids;
}

// Ids present in `left` but not in `right`, in sorted-set order.
Json difference(const std::set<std::string>& left,
                const std::set<std::string>& right) {
    Json result = Json::array();
    for (const auto& id : left)
        if (!right.count(id)) result.push_back(id);
    return result;
}

// Projection buckets are named "<bucket>-sh" / "<bucket>-sz"; anything without
// the Shanghai suffix is reported as Shenzhen, matching the two-market plans.
std::string projection_market(const BondReferenceSource& source) {
    return source.bucket.size() >= 3 &&
        source.bucket.compare(source.bucket.size() - 3, 3, "-sh") == 0
        ? "sh" : "sz";
}

// Compares the selected master against the union of its per-market projections.
// `client_master_comparison` is left for the caller to fill.
Json reconcile_projections(const BondReferenceSource& selected,
                           const Json& master_document,
                           const std::set<std::string>& master_ids,
                           const std::vector<ProjectionDocument>& projections) {
    std::set<std::string> projection_ids;
    Json rows = Json::array();
    for (const auto& [source, document] : projections) {
        const auto ids = security_ids(document, *source);
        projection_ids.insert(ids.begin(), ids.end());
        Json row = Json::object();
        row["resource"] = source->resource;
        row["market"] = projection_market(*source);
        row["count"] = static_cast<std::uint64_t>(ids.size());
        rows.push_back(std::move(row));
    }
    Json reconciliation = Json::object();
    reconciliation["master_resource"] = selected.resource;
    reconciliation["master_row_count"] =
        static_cast<std::uint64_t>(master_document.at("rows").size());
    reconciliation["master_count"] = static_cast<std::uint64_t>(master_ids.size());
    reconciliation["projection_union_count"] =
        static_cast<std::uint64_t>(projection_ids.size());
    reconciliation["counts_match"] = master_ids.size() == projection_ids.size();
    reconciliation["exact_match"] = master_ids == projection_ids;
    reconciliation["master_only"] = difference(master_ids, projection_ids);
    reconciliation["projection_only"] = difference(projection_ids, master_ids);
    reconciliation["projections"] = std::move(rows);
    return reconciliation;
}

// Compares the client master against the selected master, and reports which
// projections it reproduces exactly -- matching exactly one is the signal that it
// is a single-market table rather than a replacement for the legacy source.
Json compare_client_master(const std::set<std::string>& master_ids,
                           const ProjectionDocument& client_master,
                           const std::vector<ProjectionDocument>& projections) {
    const auto& [source, document] = client_master;
    const auto comparison_ids = security_ids(document, *source);
    Json audit = Json::object();
    audit["resource"] = source->resource;
    audit["row_count"] = static_cast<std::uint64_t>(document.at("rows").size());
    audit["security_count"] = static_cast<std::uint64_t>(comparison_ids.size());
    audit["counts_match"] = master_ids.size() == comparison_ids.size();
    audit["exact_match"] = master_ids == comparison_ids;
    audit["selected_only"] = difference(master_ids, comparison_ids);
    audit["client_master_only"] = difference(comparison_ids, master_ids);
    Json matching = Json::array();
    for (const auto& [projection_source, projection_document] : projections)
        if (security_ids(projection_document, *projection_source) == comparison_ids)
            matching.push_back(projection_source->resource);
    audit["matching_projection_resources"] = std::move(matching);
    audit["matches_single_market_projection"] =
        audit.at("matching_projection_resources").size() == 1;
    return audit;
}

}  // namespace

Json BondReferenceService::audit_category_projections(
    const BondReferenceSource& selected, const BondReferenceQuery& options,
    FetchResult& fetched, Json& projection_sources) {
    // Policy-financial audits itself because its ten-row master is only
    // meaningful next to the per-market projections; every other bucket opts in.
    const bool projection_audit = options.group == "category" &&
        (options.include_projections || options.bucket == "policy-financial") &&
        !category_projections(options.bucket).empty();
    if (!projection_audit) return Json(nullptr);

    // Every extra document folds into the caller's cache reporting: refreshed if
    // any fetch refreshed, and the oldest age across all of them.
    const auto fold = [&fetched](const FetchResult& extra) {
        fetched.refreshed = fetched.refreshed || extra.refreshed;
        fetched.age_seconds = std::max(fetched.age_seconds, extra.age_seconds);
    };
    std::vector<ProjectionDocument> projections;
    for (const auto& projection : category_projections(options.bucket)) {
        auto projection_fetched = fetch_source(projection, options);
        fold(projection_fetched);
        projections.emplace_back(&projection,
                                 std::move(projection_fetched.document));
    }
    std::optional<ProjectionDocument> client_master;
    if (options.include_projections) {
        const auto& comparison = category_client_master(options.bucket);
        if (comparison) {
            auto comparison_fetched = fetch_source(*comparison, options);
            fold(comparison_fetched);
            client_master.emplace(&*comparison,
                                  std::move(comparison_fetched.document));
        }
    }

    const auto master_ids = security_ids(fetched.document, selected);
    auto reconciliation = reconcile_projections(
        selected, fetched.document, master_ids, projections);
    reconciliation["client_master_comparison"] = client_master
        ? compare_client_master(master_ids, *client_master, projections)
        : Json(nullptr);

    // Projection metadata first, then the client master, so the caller can splice
    // this straight in after the selected source's own entry.
    for (const auto& [source, document] : projections) {
        (void)source;
        projection_sources.push_back(jsn_source_metadata(document));
    }
    if (client_master)
        projection_sources.push_back(jsn_source_metadata(client_master->second));
    return reconciliation;
}

}  // namespace tdx
