#pragma once

#include "tdx/strategic_themes.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::strategic_themes {

enum class ViewKind { catalog, categories, themes, theme, security };
enum class SortKind { name, members, id };

struct ViewSpec {
    const char* id;
    ViewKind kind;
    bool requires_theme;
    bool requires_security;
};

struct SortSpec {
    const char* id;
    SortKind kind;
};

struct QueryPlan {
    StrategicThemeQuery options;
    const ViewSpec* view{};
    const SortSpec* sort{};
    int selected_market{-1};
};

const ViewSpec& view_spec(std::string_view value);
const SortSpec& sort_spec(std::string_view value);
void sort_themes(Json& rows, const SortSpec& sort, std::string_view order);

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::optional<std::uint64_t> unsigned_value(
    const Json& value, std::string_view key);
bool digits(const std::string& value, std::size_t size);
int market_id(std::string value);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
std::vector<std::pair<int, std::string>> parse_members(
    const std::string& value, std::size_t* raw_count);
std::vector<std::string> member_ids(const Json& theme);
const Json& document_for_resource(
    const Json& documents, const std::string& resource);
bool json_contains(const Json& value, const std::string& needle);
Json theme_summary(const Json& theme);
std::string now_text();
int bounded(const std::string& text, const std::string& name,
            int low, int high);
std::filesystem::path native_path(const std::string& value);

QueryPlan make_query_plan(const StrategicThemeQuery& input);

}  // namespace tdx::detail::strategic_themes
