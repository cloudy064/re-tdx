#pragma once

#include "tdx/tqlex.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::detail {

std::string read_tqlex_config_text(const std::filesystem::path& path);
std::string remove_xml_comments(const std::string& text);
std::map<std::string, std::string> parse_xml_attributes(std::string_view text);
std::vector<std::string> tqlex_placeholders(std::string_view body);
std::string tqlex_request_id(std::string_view body);
std::string single_quotes_to_json(const std::string& text);
void replace_all(std::string& text, const std::string& needle,
                 const std::string& replacement);

Json::Object& tqlex_request_mapping(Json& request);
const Json::Object& tqlex_request_mapping(const Json& request);
int tqlex_request_integer(const Json& request, const std::string& name,
                          int fallback);
const Json::Object& tqlex_response_object(const Json& value);
const Json* tqlex_object_field(const Json::Object& object,
                               std::string_view name);
bool json_truthy(const Json& value);
int json_integer(const Json& value, std::string_view name);
const Json::Array& tqlex_result_sets(const Json& response);
const Json::Array& tqlex_result_content(const Json& result_set);
std::size_t tqlex_result_set_rows(const Json& response);
Json merge_tqlex_pages(const std::vector<Json>& pages,
                       const std::vector<bool>& paged_sets);

}  // namespace tdx::detail
