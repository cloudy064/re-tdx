#pragma once

#include "tdx/common.hpp"
#include "tdx/reverse_repo.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::reverse_repo {

enum class ViewKind { rates, security, catalog };
enum class SortKind {
    rate, net_rate, gross_interest, net_interest, term, interest_days,
    available_date, withdrawable_date, turnover, code, source_rank,
};
struct ViewSpec { const char* id; ViewKind kind; bool requires_security; };
struct SortSpec { const char* id; SortKind kind; const char* field; bool text; };
struct QueryPlan {
    ReverseRepoQuery options;
    const ViewSpec* view{};
    const SortSpec* sort{};
    int selected_market{-1};
};

const char* schedule_resource();
const ViewSpec& view_spec(std::string_view raw);
const SortSpec& sort_spec(std::string_view raw);
Json catalog_rows();
QueryPlan make_query_plan(const ReverseRepoQuery& input);

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::optional<double> number_value(const Json& value, std::string_view key);
std::optional<double> json_number(const Json& value, std::string_view key);
std::string json_text(const Json& value, std::string_view key);
bool digits(const std::string& value, std::size_t size);
std::string iso_date(const std::string& value);
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
void sort_rows(Json& rows, const SortSpec& sort, const std::string& order);
Json summary_for(const Json& rows, int principal_yuan);

}  // namespace tdx::detail::reverse_repo
