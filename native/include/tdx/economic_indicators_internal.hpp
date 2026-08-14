#pragma once

#include "tdx/economic_indicators.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tdx::economic_indicator_detail {

// The 72-row JJZB master. Per-indicator detail resources are derived from the
// indicator id: jjzb1/<id>.jsn is the historical series, jjzb2/<id>.jsn is the
// related-security set.
inline const std::string master_resource{"list/func_jjzb101_1.jsn"};

// ---- field access ----
const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::optional<double> number_value(const Json& value, std::string_view key);
std::optional<double> json_number(const Json& value, std::string_view key);
Json number_json(const std::optional<double>& value);

// ---- scalar shapes ----
bool digits(const std::string& value, std::size_t size);
std::string iso_date(const std::string& value);
bool valid_indicator_id(const std::string& value);

// ---- security identity ----
int market_id(std::string value);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);

// ---- quote lookup ----
const Json* find_quote(const Json& rows, int market, const std::string& code);
Json quote_metric(const Json* quote, std::string_view key);
Json quote_source_metadata(const Json& document, bool refreshed);

// ---- free-text search ----
bool json_contains(const Json& value, const std::string& needle);

// ---- misc ----
std::string now_text();
int bounded(const std::string& text, const std::string& name, int low, int high);
std::filesystem::path native_path(const std::string& value);

// ---- query pipeline ----

// Trims and lower-cases the routable fields in place and rejects illegal
// view/indicator_id/paging/TTL combinations. Throws Error with the original
// messages.
void validate_indicator_query(EconomicIndicatorQuery& options);

// Type and frequency histograms plus the update-date span over the whole
// catalog, computed before any filtering so the totals describe the master.
Json build_indicator_summary(const Json& indicators);

// Free-text filter, sort, then offset/limit window. Reports the pre-paging
// match count through matched.
Json filter_sort_page_catalog(const Json& indicators,
                              const EconomicIndicatorQuery& options,
                              std::uint64_t& matched);

// Applies offset/limit to source, returning at most limit entries.
Json page_array(const Json& source, int offset, int limit);

// Records an upstream failure as {resource, message}.
void record_failure(Json& errors, const Json& resource, const std::string& message);

}  // namespace tdx::economic_indicator_detail
