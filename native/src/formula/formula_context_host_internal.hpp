#pragma once

#include "formula_context_plan_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"

#include <filesystem>
#include <string>

namespace tdx::formula_context_detail {

int formula_security_type(const std::string &market, const std::string &code,
                          const Json *kline_document);

void bind_host_formula_context(
    Json &context, Json &symbols, const std::filesystem::path &root,
    const std::string &market, const std::string &code,
    const FormulaContextPlan &plan, int status_market_id,
    int formula_security_type, const Json *kline_document,
    const BlockData *block_data, int timeout_ms);

} // namespace tdx::formula_context_detail
