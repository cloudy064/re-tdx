#pragma once

#include "tdx/forecasts.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace tdx::forecast_detail {

using SecurityMap = std::map<std::pair<int, std::string>, Security>;

enum class ResourceKind {
    industry,
    hong_kong,
    latest,
};

struct ResourceDefinition {
    ResourceKind kind;
    const char* path;
};

const std::array<ResourceDefinition, 3>& resource_definitions();
const char* resource_name(ResourceKind kind);

enum class ViewKind {
    industries,
    securities,
    latest,
    hong_kong,
};

struct ViewDefinition {
    std::string_view name;
    ViewKind kind;
};

const std::array<ViewDefinition, 4>& view_definitions();
const ViewDefinition* find_view(std::string_view name);
bool valid_category(std::string_view category);

std::filesystem::path native_path(const std::string& value);
std::string now_text();

const Json* value_ptr(const Json& object, std::string_view name);
std::string text_value(const Json& object, std::string_view name);
std::optional<double> number_value(const Json& object, std::string_view name);
Json number_or_null(const std::optional<double>& value);
Json scaled_number(const Json& row, std::string_view name, double scale,
                   bool integral = false);

bool digits(const std::string& value, std::size_t size);
bool industry_code(const std::string& value);
bool forecast_group_code(const std::string& value);
bool safe_identifier(const std::string& value);
int mainland_market_id(const std::string& value);
std::optional<int> row_mainland_market(const Json& row);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(int id, const std::string& code,
                       const SecurityMap& securities);

std::string normalized_multiline(std::string value);
std::string sentiment(const std::string& type);
std::pair<std::string, std::string> split_hong_kong_text(std::string value);

const Json& document_for_resource(const Json& documents,
                                  std::string_view resource);
Json source_summary(const Json& document);
Json limited_filtered(const Json& rows, const std::string& query, int limit,
                      const std::string& category = "all");
int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum);

Json build_core_summary(const Json& document);

}  // namespace tdx::forecast_detail
