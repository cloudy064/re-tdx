#pragma once

#include "tdx/thematic_opportunities.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::thematic_opportunities {

enum class ResourceRole {
    industry_groups,
    region_groups,
    completed_hype,
    active_hype,
    legacy_themes,
};

struct ResourceDefinition {
    ResourceRole role;
    std::string_view resource;
    std::string_view group_type;
    std::string_view type_name;
    std::string_view name_field;
    std::string_view category_field;
    std::string_view detail_prefix;
    bool legacy{};
};

const std::array<ResourceDefinition, 5>& resource_definitions();
const ResourceDefinition& resource_for_role(ResourceRole role);
const ResourceDefinition& resource_for_group_type(std::string_view type);
const std::vector<std::string>& resource_paths();
std::string_view legacy_declared_page_name();
bool valid_view(std::string_view value);
bool valid_type(std::string_view value);
bool valid_group_sort(std::string_view value);
bool valid_order(std::string_view value);

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::optional<double> number_value(const Json& value, std::string_view key);
Json number_or_null(const std::optional<double>& value);
bool digits(const std::string& value, std::size_t count);
int market_id(std::string value);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities,
    const std::string& source_name = {});
std::vector<std::pair<int, std::string>> parse_members(
    const std::string& value, std::size_t* raw_count);
const Json& document_for_resource(const Json& documents,
                                  std::string_view resource);
Json group_summary(const Json& group);
bool json_contains(const Json& value, const std::string& needle);
void sort_groups(Json& groups, const std::string& sort,
                 const std::string& order);
void sort_hype(Json& rows, const std::string& order);
Json page(const Json& rows, int offset, int limit);
std::string current_time_text();
std::filesystem::path native_utf8_path(const std::string& value);

}  // namespace tdx::detail::thematic_opportunities
