#include "recon_contract_formula_runtime_internal.hpp"

#include <array>

namespace tdx::recon_contract_detail {
namespace {

constexpr std::array<FormulaRuntimeContractValidator, 6>
    formula_runtime_validators{
        validate_formula_runtime_math_contract,
        validate_formula_runtime_future_contract,
        validate_formula_runtime_security_contract,
        validate_formula_runtime_data_contract,
        validate_formula_runtime_host_contract,
        validate_formula_runtime_context_contract,
    };

}  // namespace

bool validate_formula_runtime_contract(const std::string& contract_id,
                                       const Json& document,
                                       Json& result) {
    for (const auto validator : formula_runtime_validators)
        if (validator(contract_id, document, result)) return true;
    return false;
}

}  // namespace tdx::recon_contract_detail