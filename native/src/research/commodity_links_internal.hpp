#pragma once

#include "tdx/commodity_links.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::commodity_links_detail {

using SecurityDirectory = std::map<std::pair<int, std::string>, Security>;

struct ViewDefinition {
    std::string_view view;
    std::string_view label;
    std::string_view resources;
    std::string_view selection;
    std::string_view default_sort;
    std::string_view allowed_sorts;
};

const std::array<ViewDefinition, 7>& view_definitions();
const ViewDefinition* find_view_definition(std::string_view view);
bool view_allows_sort(const ViewDefinition& definition, std::string_view sort);
const std::string& commodity_master_resource();
const std::string& theme_master_resource();
Json catalog_rows();

const Json* value_ptr(const Json& value, std::string_view key);
std::string text_value(const Json& value, std::string_view key);
std::string text_value_any(
    const Json& value, std::initializer_list<std::string_view> keys);
std::optional<double> number_value(const Json& value, std::string_view key);
std::optional<double> number_value_any(
    const Json& value, std::initializer_list<std::string_view> keys);
Json number_json(const std::optional<double>& value);
bool digits(const std::string& value, std::size_t size);
std::string iso_date(const std::string& value);
int market_id(std::string value);
std::string market_name(int id);
Json security_document(int id, const std::string& code,
                       const SecurityDirectory& securities);
Json parse_security_set(const std::string& text,
                        const SecurityDirectory& securities);
bool security_matches(const Json& security, int selected_market,
                      const std::string& code);
bool security_set_contains(const Json& set, int selected_market,
                           const std::string& code);
std::optional<double> pct_change(const std::optional<double>& latest,
                                 const std::optional<double>& base,
                                 bool absolute_denominator = false);
bool json_contains(const Json& value, const std::string& needle);
std::string json_text(const Json& value, std::string_view key);
std::optional<double> json_number(const Json& value, std::string_view key);
std::string now_text();
int bounded(const std::string& text, const std::string& name,
            int minimum, int maximum);
std::filesystem::path native_path(const std::string& value);
const Json* find_by_id(const Json& rows, std::string_view key,
                       const std::string& id);
Json find_all_by_id(const Json& rows, std::string_view key,
                    const std::string& id);
void sort_rows(Json& rows, const std::string& view,
               const std::string& sort, const std::string& order);

}  // namespace tdx::commodity_links_detail
