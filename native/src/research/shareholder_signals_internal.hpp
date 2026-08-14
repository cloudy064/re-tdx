#pragma once

#include "tdx/shareholder_signals.hpp"
#include "tdx/common.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::shareholder_signals {

enum class ResourceKind {
    notable_investors,
    institution_accumulation,
    research_growth,
    small_cap_institution,
    investor_directory,
};
enum class ViewKind {
    all,
    notable_investors,
    institution_accumulation,
    research_growth,
    small_cap_institution,
    investor_directory,
};
enum class SortKind {
    signal,
    holding_value,
    institution_growth,
    holder_change,
    research_6m,
    profit_growth,
    return_6m,
    code,
};

using SignalRowNormalizer = Json (*)(
    const Json&, const std::map<std::pair<int, std::string>, Security>&);

struct ResourceSpec {
    const char* resource;
    ResourceKind kind;
    const char* id;
    const char* label;
    SignalRowNormalizer normalize_row;
};
struct ViewSpec {
    const char* id;
    ViewKind kind;
    const char* signal_kind;
    bool directory;
};
struct SortSpec {
    const char* id;
    SortKind kind;
    const char* field;
};
struct QueryPlan {
    ShareholderSignalsQuery options;
    const ViewSpec* view{};
    const SortSpec* sort{};
    std::string selected_market;
};
struct InvestorProjection {
    Json directory{Json::array()};
    Json selected{Json(nullptr)};
    Json holdings{Json::array()};
    Json detail_source{Json(nullptr)};
    Json reconciliation{Json::object()};
    std::size_t directory_matched{};
};

const std::array<ResourceSpec, 5>& all_resources();
const ResourceSpec& resource_spec(std::string_view resource);
const ResourceSpec& resource_spec(ResourceKind kind);
const ViewSpec& view_spec(std::string_view view);
const SortSpec& sort_spec(std::string_view sort);

const Json* field(const Json& row, std::string_view name);
std::string text_value(const Json& row, std::string_view name);
std::optional<double> number_value(const Json& row, std::string_view name);
Json number(const Json& row, std::string_view name);
Json scaled(const Json& row, std::string_view name, double scale);
Json difference(const Json& row, std::string_view left, std::string_view right);
Json ratio_pct(const Json& row, std::string_view numerator,
               std::string_view denominator);
bool digits(const std::string& value);
int integer_value(const Json& row, std::string_view name);
Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
std::string now_text();
std::filesystem::path native_path(const std::string& value);
int bounded(const std::string& value, std::string_view name, int low, int high);
Json load_local_resource_rows(
    const std::filesystem::path& root, const std::string& resource);
const Json& document_for(const Json& documents, std::string_view resource);
Json source_summary(const Json& document, std::size_t normalized_rows);
void add_number(Json& item, const Json& row,
                const char* output, const char* input);
std::optional<double> normalized_number(const Json& row, std::string_view name);

Json normalize_notable_signal_row(
    const Json& row, const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_institution_signal_row(
    const Json& row, const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_research_signal_row(
    const Json& row, const std::map<std::pair<int, std::string>, Security>& securities);
Json normalize_small_cap_signal_row(
    const Json& row, const std::map<std::pair<int, std::string>, Security>& securities);

QueryPlan make_query_plan(const ShareholderSignalsQuery& input);
void sort_signal_rows(Json& rows, const SortSpec& sort, std::string_view order);
InvestorProjection project_investor(
    const QueryPlan& plan, const Json& master,
    const std::filesystem::path& jsn_root,
    const std::map<std::pair<int, std::string>, Security>& securities);

}  // namespace tdx::detail::shareholder_signals
