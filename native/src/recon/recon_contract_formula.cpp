#include "recon_contract_internal.hpp"
#include "recon_contract_formula_internal.hpp"

#include <array>

namespace tdx::recon_contract_detail {
namespace {

constexpr std::array<FormulaContractValidator, 5> formula_contract_validators{
    validate_formula_foundation_contract,
    validate_formula_calculation_contract,
    validate_formula_render_contract,
    validate_formula_runtime_contract,
    validate_formula_workflow_contract,
};

}  // namespace

bool validate_formula_contract(const std::string& contract_id,
                               const Json& document, Json& result) {
    for (const auto validator : formula_contract_validators)
        if (validator(contract_id, document, result)) return true;
    return false;
}

}  // namespace tdx::recon_contract_detail

