#pragma once

#include "tdx/ratings.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::ratings_detail {

struct RatingResourceDefinition {
    std::string_view view;
    std::string_view resource;
};

inline constexpr std::array<RatingResourceDefinition, 3> master_resources{{
    {"hong-kong", "list/func_ggpj101_1.jsn"},
    {"us", "list/func_mgpj101_1.jsn"},
    {"industries", "list/func_hypj101_1.jsn"},
}};

inline constexpr auto hong_kong_master_resource = master_resources[0].resource;
inline constexpr auto united_states_master_resource = master_resources[1].resource;
inline constexpr auto industry_master_resource = master_resources[2].resource;

std::filesystem::path native_path(const std::string& value);
std::string now_text();
const Json* value_ptr(const Json& object, std::string_view name);
std::string text_value(const Json& object, std::string_view name);
std::optional<double> number_value(const Json& object, std::string_view name);
Json number_or_null(const std::optional<double>& value);
bool digits(const std::string& value, std::size_t size);
std::string upper_ascii(std::string value);
bool united_states_symbol(const std::string& value);
int mainland_market_id(const std::string& value);
std::string market_name(int id);
std::string market_prefix(int id);
Json mainland_security_document(
    int id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json hong_kong_security_document(
    const std::string& code,
    const std::map<std::string, std::string>& names);
Json united_states_security_document(const std::string& code,
                                     const std::string& name);
Json industry_document(const std::string& code,
                       const std::map<std::string, std::string>& names);

std::vector<std::string> nonempty_null_tokens(const std::string& text);
void load_hk_ag_names(const std::filesystem::path& path,
                      std::map<std::string, std::string>& result);
void load_hk_target_names(const std::filesystem::path& path,
                          std::map<std::string, std::string>& result);
void load_relation_names(const std::filesystem::path& path,
                         std::map<std::string, std::string>& result);

bool json_contains(const Json& value, const std::string& needle);
Json filtered_rows(const Json& rows, const std::string& query,
                   const std::string& stance, int limit);
const Json& document_for_resource(const Json& documents,
                                  std::string_view resource);
Json source_summary(const Json& document);
Json hong_kong_summary(const Json& rows);
Json industry_summary(const Json& rows);
Json united_states_summary(const Json& rows);
int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum);

}  // namespace tdx::ratings_detail
