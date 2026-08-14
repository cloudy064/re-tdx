#pragma once

#include "tdx/common.hpp"
#include "tdx/threshold_stocks.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::threshold_stocks {

enum class UniverseKind { high_price, mega_cap };
enum class ViewKind { history, members, security, catalog };
enum class SortDomain { none, history, members };
enum class SortKind {
    date, count, market_cap, market_share, entered, exited,
    code, day_return, threshold_value, net_increase, status,
};

struct UniverseSpec {
    const char* id;
    UniverseKind kind;
    const char* label;
    const char* master_resource;
    const char* trend_count_field;
};
struct ViewSpec {
    const char* id;
    ViewKind kind;
    SortDomain sort_domain;
    bool requires_security;
    bool loads_members;
};
struct SortSpec {
    const char* id;
    SortDomain domain;
    SortKind kind;
};
struct QueryPlan {
    ThresholdStocksQuery options;
    const UniverseSpec* universe{};
    const ViewSpec* view{};
    const SortSpec* sort{};
    int selected_market{-1};
};

const UniverseSpec& universe_spec(std::string_view raw);
const std::array<UniverseSpec, 2>& all_universes();
const ViewSpec& view_spec(std::string_view raw);
const SortSpec& sort_spec(SortDomain domain, std::string_view raw);
QueryPlan make_query_plan(const ThresholdStocksQuery& input);

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::optional<double> number_value(const Json& value, std::string_view key);
Json number_json(const std::optional<double>& value);
Json count_json(const Json& value, std::string_view key);
bool digits(const std::string& value, std::size_t size);
int market_id(std::string value);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
std::string normalized_status(const std::string& raw);
std::optional<double> json_number(const Json& row, std::string_view key);
std::string json_text(const Json& row, std::string_view key);
bool json_contains(const Json& value, const std::string& needle);
std::string now_text();
int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum);
std::filesystem::path native_path(const std::string& value);

const Json& selected_history(const Json& history, const std::string& requested);
Json history_summary(const Json& history);
Json member_summary(const Json& rows, const Json& period);
std::optional<double> sort_metric(const Json& row, const SortSpec& sort);
void sort_rows(Json& rows, const ViewSpec& view, const SortSpec& sort,
               const std::string& order);

}  // namespace tdx::detail::threshold_stocks
