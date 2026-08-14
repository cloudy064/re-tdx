#pragma once

#include "formula_context_plan_internal.hpp"

#include "tdx/json.hpp"

#include <filesystem>
#include <string>

namespace tdx::formula_context_detail {

void bind_formula_finance_extensions(
    Json &context, Json &finance_values, Json &professional_finance_values,
    const std::filesystem::path &root, const std::string &market,
    const std::string &code, const Json *kline_document,
    const FormulaContextPlan &plan, bool point_in_time_finance,
    int security_type, int timeout_ms);

} // namespace tdx::formula_context_detail
