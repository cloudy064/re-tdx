#pragma once

#include "tdx/limit_review.hpp"
#include "tdx/common.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::limit_review {

enum class ViewKind { catalog, current, annual, history, daily, security };
enum class NormalizeKind {
    current_limit_up,
    current_limit_down,
    current_surge,
    annual,
    market_history,
    daily_limit_up,
    daily_limit_down,
};

struct ViewSpec {
    const char* id;
    ViewKind kind;
    const char* label;
    const char* categories_json;
    const char* resources_json;
    const char* semantics;
};
struct ResourceSpec {
    const char* resource;
    NormalizeKind normalize;
};
struct ResourceRequest {
    std::string resource;
    NormalizeKind normalize;
    bool allow_missing{};
};
struct QueryPlan {
    LimitReviewQuery options;
    const ViewSpec* view{};
    int selected_market{-1};
    std::vector<ResourceRequest> records;
    std::string history_resource;
};

const std::array<ResourceSpec, 5>& fixed_resources();
const ResourceSpec& resource_spec(NormalizeKind kind);
const ViewSpec& view_spec(std::string_view value);
Json catalog_document();

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::optional<double> number_value(const Json& value, std::string_view key);
Json number_or_null(const Json& value, std::string_view key, double scale = 1.0);
bool digits(const std::string& value, std::size_t size);
int market_id(const std::string& value);
std::string market_name(int value);
std::string market_prefix(int value);
Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
bool missing_resource_error(const std::string& message);
Json missing_document(const std::string& resource, const std::string& message);
Json source_summary(const Json& document);
bool json_contains(const Json& value, const std::string& needle);
Json select_records(const Json& candidates, const QueryPlan& plan,
                    std::uint64_t& matched);
void append_rows(Json& destination, const Json& source);
std::string now_text();
int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum);
std::filesystem::path from_utf8(const std::string& value);

QueryPlan make_query_plan(const LimitReviewQuery& input);
Json normalize_resource_rows(
    NormalizeKind kind, const Json& rows, const QueryPlan& plan,
    const std::map<std::pair<int, std::string>, Security>& securities);

}  // namespace tdx::detail::limit_review
