#pragma once

#include "recon_contract_formula_internal.hpp"
#include "recon_contract_internal.hpp"

namespace tdx::recon_contract_detail {

using FormulaRuntimeContractValidator = bool (*)(
    const std::string& contract_id,
    const Json& document,
    Json& result);

bool validate_formula_runtime_math_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_runtime_future_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_runtime_security_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_runtime_data_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_runtime_host_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);
bool validate_formula_runtime_context_contract(
    const std::string& contract_id,
    const Json& document,
    Json& result);

}  // namespace tdx::recon_contract_detail
