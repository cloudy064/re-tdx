#pragma once

#include "tdx/json.hpp"

#include <map>
#include <string>
#include <vector>

namespace tdx {

Json formula_calculation_catalog_document();
Json calculate_formula_document(
    Json kline_document,
    const std::string& formula,
    const std::map<std::string, int>& parameters = {});
int command_formulas_calculate(const std::vector<std::string>& args);

}  // namespace tdx
