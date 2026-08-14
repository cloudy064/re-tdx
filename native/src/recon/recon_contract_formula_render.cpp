#include "recon_contract_formula_internal.hpp"

#include <algorithm>

namespace tdx::recon_contract_detail {

bool validate_formula_render_contract(const std::string& contract_id,
                                      const Json& document,
                                      Json& result) {
    const auto strategy = std::find_if(
        formula_render_contract_strategies.begin(),
        formula_render_contract_strategies.end(),
        [&](const FormulaRenderContractStrategy& candidate) {
            return candidate.contract_id == contract_id;
        });
    if (strategy == formula_render_contract_strategies.end()) return false;
    return strategy->validator(contract_id, document, result);
}

}  // namespace tdx::recon_contract_detail
