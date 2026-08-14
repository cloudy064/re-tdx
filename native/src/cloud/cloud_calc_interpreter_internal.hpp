#pragma once

#include "cloud_calc_builtins_internal.hpp"
#include "cloud_calc_config_internal.hpp"

#include "tdx/json.hpp"

#include <string>
#include <vector>

namespace tdx::cloud_calc_detail {

struct FormulaCheck {
    bool valid{};
    bool executable{};
    std::string kind;
    std::string reason;
    const Builtin* builtin{};
};

FormulaCheck check_formula(const Column& column);
std::vector<std::string> dependency_cycles(const Unit& unit);
Json evaluate_unit(const Unit& unit, const Json& row, int as_of_yyyymmdd);

}  // namespace tdx::cloud_calc_detail
