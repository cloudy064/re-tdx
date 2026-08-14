#pragma once

#include "tdx/common.hpp"
#include "tdx/funds.hpp"

#include <array>
#include <filesystem>
#include <optional>

namespace tdx::detail::funds {

struct PeriodSpec {
    const char* key;
    const char* name;
    int suffix;
    int minutes;
};

const std::array<PeriodSpec, 7>& periods();
Json source_document();
std::string now_text();
std::filesystem::path native_path(const std::string& value);
const Json* find_value(const Json& object, const std::string& name);
std::string scalar_text(const Json* value);
Json scalar_copy(const Json& object, const std::string& name);
std::optional<double> number_value(const Json& object, const std::string& name);
int market_id_from_text(const std::string& value);
std::string market_name(int market_id);
std::string market_prefix(int market_id);
int bounded_integer(const std::string& text, const std::string& name,
                    int minimum, int maximum);
const Json& response_from_document(const Json& document);
Json response_rows(const Json& document);
std::vector<std::pair<std::string, std::string>> industry_keys(const Json& rows);
const Json* find_row(const Json& rows, const std::string& market,
                     const std::string& code);
bool transient_funds_error(const std::string& message);
Json normalized_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities,
    bool detail, const std::map<std::string, std::string>& industry_names);
Json security_reference(
    int market_id, const std::string& code,
    const std::map<std::pair<int, std::string>, Security>& securities);

}  // namespace tdx::detail::funds
