#pragma once

#include "tdx/capital_strength.hpp"
#include "tdx/common.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::capital_strength {

struct PeriodSpec {
    const char* id;
    const char* label;
    const char* resource;
    const char* return_key;
    const char* total_net_key;
    const char* main_net_key;
    int order;
};

enum class ViewKind { ranking, confluence, security, catalog };
enum class SortKind {
    ddx,
    return_pct,
    total_net,
    main_net,
    float_shares,
    source_rank,
    period,
    code,
    period_count,
    average_ddx,
    maximum_ddx,
    minimum_ddx,
    best_rank,
};

struct ViewSpec {
    const char* id;
    ViewKind kind;
    const char* default_sort;
    const char* default_order;
    bool all_periods;
    bool requires_security;
};

struct SortSpec {
    const char* id;
    SortKind kind;
    unsigned allowed_views;
    const char* field;
};

struct QueryPlan {
    CapitalStrengthQuery options;
    const ViewSpec* view{};
    const SortSpec* sort{};
    int selected_market{-1};
    std::vector<const PeriodSpec*> periods;
};

const std::array<PeriodSpec, 5>& all_period_specs();
const PeriodSpec& period_spec(std::string_view value);
const ViewSpec& view_spec(std::string_view value);
const SortSpec& sort_spec(ViewKind view, std::string_view value);
int period_order(std::string_view value);
Json catalog_rows();

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::optional<double> number_value(const Json& value, std::string_view key);
Json number_json(const std::optional<double>& value);
std::optional<double> json_number(const Json& value, std::string_view key);
std::string json_text(const Json& value, std::string_view key);
bool digits(const std::string& value, std::size_t size);
int market_id(std::string value);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
bool json_contains(const Json& value, const std::string& needle);
std::string now_text();
int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum);
std::filesystem::path native_path(const std::string& value);

std::optional<double> row_sort_metric(const Json& row, const SortSpec& sort);
Json ranking_summary(const Json& rows);
QueryPlan make_query_plan(const CapitalStrengthQuery& input);
Json filter_capital_strength_records(const Json& records, const QueryPlan& plan);

}  // namespace tdx::detail::capital_strength
