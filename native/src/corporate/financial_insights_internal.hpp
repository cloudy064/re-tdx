#pragma once

#include "tdx/financial_insights.hpp"
#include "tdx/common.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::detail::financial_insights {

struct ResourceDefinition {
    std::string_view resource;
    std::string_view kind;
    std::string_view label;
};

struct SortDefinition {
    std::string_view name;
    std::string_view field;
};

const std::array<ResourceDefinition, 15>& resource_definitions();
const ResourceDefinition& find_resource(std::string_view resource);
const ResourceDefinition* find_view(std::string_view view);
const std::vector<std::string>& resource_paths();
std::string sort_field(std::string_view sort);

const Json* field(const Json& row, std::string_view name);
std::string text_value(const Json& row, std::string_view name);
std::optional<double> number_value(const Json& row, std::string_view name);
Json number(const Json& row, std::string_view name);
Json scaled(const Json& row, std::string_view name, double scale);
Json difference(const Json& row, std::string_view left,
                std::string_view right);
Json change_pct(const Json& row, std::string_view current,
                std::string_view prior);
Json ratio_pct(const Json& row, std::string_view numerator,
               std::string_view denominator);
Json midpoint(const Json& row, std::string_view lower,
              std::string_view upper);
Json change_pct_absolute_base(const Json& row, std::string_view current,
                              std::string_view prior);
bool digits(const std::string& value);
int integer_value(const Json& row, std::string_view name);
Json security_document(
    int market, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
std::string current_time_text();
std::filesystem::path native_utf8_path(const std::string& value);
Json load_local_resource_rows(const std::filesystem::path& root,
                              const std::string& resource);
const Json& document_for(const Json& documents, std::string_view resource);
Json source_summary(const Json& document, std::size_t normalized_rows);
void add_number(Json& item, const Json& row, const char* output,
                const char* input);
std::optional<double> normalized_number(const Json& row,
                                        const std::string& name);
void set_signal(Json& item, const Json& signal, const Json& amount,
                const Json& ratio);

}  // namespace tdx::detail::financial_insights
