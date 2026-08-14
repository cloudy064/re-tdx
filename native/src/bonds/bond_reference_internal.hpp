#pragma once

#include "tdx/bond_reference.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::bond_reference_detail {

enum class ScaleSemantics {
    issue_yuan,
    issue_100m_yuan,
    outstanding_100m_yuan,
    client_master_hidden_unit,
};

struct ResourceProfile {
    std::string_view resource;
    bool reference_master;
    ScaleSemantics scale;
};

ResourceProfile resource_profile(const BondReferenceSource& source);
const std::vector<BondReferenceSource>& category_projections(
    const std::string& bucket);
const std::optional<BondReferenceSource>& category_client_master(
    const std::string& bucket);

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::string first_text_value(
    const Json& value, std::initializer_list<std::string_view> keys);
std::optional<double> number_value(const Json& value, std::string_view key);
std::optional<double> first_number_value(
    const Json& value, std::initializer_list<std::string_view> keys);
Json normalize_row(const Json& raw, const BondReferenceSource& source,
                   bool retain_raw, std::size_t raw_index);
bool contains_text(const Json& row, const std::string& needle);

// Facets accumulated while scanning rows, so the summary does not need a second
// pass over the matched set.
struct RowFacets {
    std::map<std::string, std::uint64_t> credit_counts, rate_counts, type_counts;
    std::set<std::string> matched_security_ids;
    std::string earliest_maturity, latest_maturity;
};

// Normalizes `options` in place and resolves the source it selects.  Validation
// order is observable through the error message a bad request reports, so the
// checks stay in their original sequence: group, bucket, sort, order, paging,
// then cache/timeout.
const BondReferenceSource& resolve_query(BondReferenceQuery& options);

// Filters and normalizes the source rows, collecting facets as it goes.  Rows
// keep their `_raw_index` so pagination can re-normalize the originals with
// their raw evidence retained.
Json collect_matching_rows(const Json& raw_rows,
                           const BondReferenceSource& source,
                           const BondReferenceQuery& options,
                           RowFacets& facets);
// Orders the matched rows, breaking ties on security_id for a stable result.
void sort_matching_rows(Json& matched_rows, const BondReferenceQuery& options);
// Re-normalizes the requested page from the raw rows, this time retaining raw
// evidence -- coupon-array expansion is deferred to here so the 42k-row archive
// only pays for the page it returns.
Json paginate_rows(const Json& matched_rows, const Json& raw_rows,
                   const BondReferenceSource& source,
                   const BondReferenceQuery& options);
Json build_summary(const BondReferenceSource& source, std::size_t source_row_count,
                   std::uint64_t matched, const RowFacets& facets);
Json source_option_list();
Json build_query_filters(const BondReferenceQuery& options);
// The verbatim `semantics` note published with every response, explaining what
// these overlapping reference views do and do not contain.
std::string_view query_semantics();
std::string now_text();
int bounded(const std::string& text, const std::string& name, int low, int high);
std::filesystem::path native_path(const std::string& value);
Json load_local_resource_rows(const std::filesystem::path& root,
                              const std::string& resource);

}  // namespace tdx::bond_reference_detail
