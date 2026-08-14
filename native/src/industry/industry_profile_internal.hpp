#pragma once

#include "tdx/industry_profile.hpp"
#include "tdx/common.hpp"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::detail::industry_profile {

struct IndustryHoldingPeriod {
    std::string_view key;
    std::string_view label;
    std::string_view resource;
};

const std::array<IndustryHoldingPeriod, 4>& industry_holding_periods();
std::string_view industry_shareholder_resource();
const std::vector<std::string>& industry_master_resource_paths();

std::filesystem::path native_utf8_path(const std::string& value);
std::string current_time_text();
const Json* json_field(const Json& object, std::string_view name);
Json json_copy(const Json& object, std::string_view name);
std::string json_text(const Json& object, std::string_view name);
std::optional<double> json_number_value(const Json& object,
                                        std::string_view name);
Json numeric_difference(const std::optional<double>& current,
                        const std::optional<double>& previous);
Json numeric_percentage_change(const std::optional<double>& current,
                               const std::optional<double>& previous);
Json numeric_percentage_of(const std::optional<double>& numerator,
                           const std::optional<double>& denominator);
bool valid_research_industry_code(const std::string& value);
bool valid_security_code(const std::string& value);
int canonical_market_id(const std::string& value);
std::string market_name(int value);
std::string market_prefix(int value);
Json make_security_document(
    int market_id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);
Json make_basic_industry(const Block& block,
                         const std::string& parent_code = {});
const Json& find_resource_document(const Json& documents,
                                   std::string_view resource);
Json resource_source_summary(const Json& document);
const Json* find_industry_record(const Json& master,
                                 const std::string& code);
bool industry_has_data(const Json& industry, const char* name);

}  // namespace tdx::detail::industry_profile
