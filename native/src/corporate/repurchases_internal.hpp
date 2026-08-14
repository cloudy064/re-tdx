#pragma once

#include "tdx/repurchases.hpp"
#include "tdx/common.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace tdx::detail::repurchases {

enum class CoreResourceRole {
    plans,
    monthly,
    hong_kong,
};

struct CoreResourceSpec {
    CoreResourceRole role;
    const char* document_key;
    const char* resource;
};

struct AnnualSpec {
    const char* id;
    const char* label;
    const char* resource;
    const char* detail_namespace;
};

enum class ViewKind {
    plans,
    monthly,
    annual,
    hong_kong,
};

struct ViewSpec {
    const char* id;
    ViewKind kind;
};

const std::array<CoreResourceSpec, 3>& core_resources();
const CoreResourceSpec& core_resource(CoreResourceRole role);
const std::array<AnnualSpec, 3>& annual_specs();
const AnnualSpec& annual_spec(const std::string& id);
const ViewSpec& view_spec(const std::string& id);

std::filesystem::path native_path(const std::string& value);
std::string now_text();
const Json* value_ptr(const Json& object, std::string_view name);
std::string text_value(const Json& object, std::string_view name);
std::optional<double> number_value(const Json& object, std::string_view name);
Json number_or_null(const std::optional<double>& value);
Json scaled_number(const Json& row, std::string_view name, double scale);
bool digits(const std::string& value, std::size_t length);
bool valid_year(const std::string& value);
int market_id(const std::string& value);
std::string market_name(int id);
std::string market_prefix(int id);
Json security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
const Json& document_for_resource(const Json& documents, std::string_view resource);
Json source_summary(const Json& document);
Json load_local_resource_rows(const std::filesystem::path& root,
                              const std::string& resource);
Json limited_filtered(const Json& values, const std::string& query, int limit);
int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum);
Json summarize_plans(const Json& plans);

}  // namespace tdx::detail::repurchases
