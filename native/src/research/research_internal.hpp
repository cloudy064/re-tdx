#pragma once

#include "tdx/research.hpp"
#include "tdx/common.hpp"

#include <filesystem>
#include <initializer_list>
#include <string_view>

namespace tdx::detail::research {

enum class DetailKind {
    activity,
    regulatory,
    related_security,
};

const ResearchCategorySpec& category_spec(const std::string& id);
DetailKind detail_kind(std::string_view name_space);

std::filesystem::path native_path(const std::string& value);
std::string now_text();
const Json* value_ptr(const Json& object, std::string_view name);
Json copy_value(const Json& object, std::string_view name);
std::string text_value(const Json& object, std::string_view name);
Json first_value(const Json& object, std::initializer_list<const char*> names);
int canonical_market_id(const std::string& value);
std::string market_name(int value);
std::string market_prefix(int value);
bool six_digits(const std::string& value);
int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum);
Json security_document(
    int market_id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
const Json& document_for_resource(const Json& documents,
                                  const std::string& resource);
const Json& category_document(const Json& master, const std::string& id);
Json source_summary(const Json& document);
void map_fields(
    Json& output, const Json& input,
    std::initializer_list<std::pair<const char*, const char*>> fields);
std::pair<std::string, std::string> split_embedded_text(const std::string& value);
Json urls_json(const std::string& value);
bool json_contains(const Json& value, const std::string& needle);
Json limited_rows(const Json& rows, int limit);
bool same_security(const Json& record, int market_id, const std::string& code);
std::string derived_detail_id(const std::string& category, const std::string& code);
std::map<std::string, std::string> block_names(const BlockData& data);

}  // namespace tdx::detail::research
