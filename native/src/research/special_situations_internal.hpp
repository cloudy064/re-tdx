#pragma once

#include "tdx/special_situations.hpp"

#include "tdx/common.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/market.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::special_situations_detail {

using SecurityDirectory = std::map<std::pair<int, std::string>, Security>;

enum class ResourceKind {
    merger,
    b_to_h,
    market_cap_risk,
    major_restructuring_plan,
    major_restructuring_review,
    major_restructuring_completed,
    ordinary_merger_plan,
    neeq_transfer_plan,
    neeq_regulation,
    neeq_transfer_completed,
};

struct ResourceDefinition {
    ResourceKind kind;
    std::string_view resource;
    std::string_view event_kind;
    std::string_view label;
};

const std::array<ResourceDefinition, 10>& resource_definitions();
const std::vector<std::string>& resource_names();
const ResourceDefinition* find_resource_definition(std::string_view resource);

const Json* field(const Json& row, std::string_view name);
std::string text_value(const Json& row, std::string_view name);
std::optional<double> number_value(const Json& row, std::string_view name);
Json number(const Json& row, std::string_view name);
bool digits(const std::string& value, std::size_t size = 6);
int parsed_market(const std::string& value);
Json security_from_raw(const Json& raw, std::string_view market_key,
                       std::string_view code_key,
                       const SecurityDirectory& securities);
Json string_array(const std::string& value);
std::string now_text();
std::filesystem::path native_path(const std::string& value);
Json load_local_resource_rows(const std::filesystem::path& root,
                              const std::string& resource);
const Json& document_for(const Json& documents, std::string_view resource);
Json source_summary(const Json& document);
std::optional<double> quote_price(const Json& quote);
Json premium(const std::optional<double>& current,
             const std::optional<double>& reference);
Json quote_for(const Json& security,
               const std::map<std::pair<int, std::string>, Json>& quotes);
int bounded(const std::string& value, std::string_view name, int low, int high);
bool view_matches_kind(const std::string& view, const std::string& kind);

}  // namespace tdx::special_situations_detail
