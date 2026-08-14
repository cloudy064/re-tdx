#pragma once

#include "tdx/anomaly_risk.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::anomaly_risk {

enum class ViewKind { statistics, suspension_risk };
using RowNormalizer = Json (*)(const Json&, const BlockData&);

struct ViewSpec {
    const char* id;
    ViewKind kind;
    const char* title;
    const char* request_id;
    const char* source_file;
    const char* module;
    bool all_pages;
    int page;
    int page_size;
    int max_pages;
    bool retains_unresolved_fields;
    RowNormalizer normalize;
};
struct WarningSpec {
    int code;
    const char* status;
    const char* label;
    bool active;
};
struct QueryPlan {
    AnomalyRiskQuery options;
    const ViewSpec* view{};
    const WarningSpec* warning_filter{};
    std::map<std::string, std::string> overrides;
    std::string cache_key;
};

const ViewSpec& view_spec(std::string_view raw);
const WarningSpec& warning_spec(int code);
const WarningSpec* warning_filter_spec(std::string_view raw);
Json available_views();
Json warning_catalog();
QueryPlan make_query_plan(const AnomalyRiskQuery& input);

std::filesystem::path native_path(const std::string& value);
std::string now_text();
const Json* field(const Json& row, std::string_view key);
std::string text(const Json& row, std::string_view key);
std::optional<double> number(const Json& row, std::string_view key);
Json number_json(const std::optional<double>& value);
bool digits(const std::string& value);
Json date_json(const std::string& raw);
int row_market_id(const Json& row, std::string_view key);
std::string market_name(int market);
std::string market_prefix(int market);
std::string resolve_name(const BlockData& blocks, int market,
                         const std::string& code, std::string fallback = {});
Json identity_document(const BlockData& blocks, int market,
                       const std::string& code, const std::string& fallback_name,
                       const std::string& entity_type);
int bounded(const std::string& raw, std::string_view name,
            int minimum, int maximum);
bool transient_error(const std::string& message);

std::optional<double> parse_number_text(const std::string& value);
Json warning_evidence(const Json& row);
Json window_document(const Json& row, int days,
                     std::string_view deviation_key,
                     std::string_view start_key,
                     std::string_view periods_key,
                     std::string_view security_return_key,
                     std::string_view index_return_key);
Json result_set_metadata(const Json& upstream, std::size_t decoded_rows);
Json source_document(const Json& upstream, const ViewSpec& view,
                     std::size_t decoded_rows);

Json build_live_document(const Json& upstream, const QueryPlan& plan,
                         const BlockData& blocks);

}  // namespace tdx::detail::anomaly_risk
