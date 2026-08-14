#include "recon_contract_formula_internal.hpp"
#include "recon_contract_formula_foundation_internal.hpp"

#include <array>
#include <string_view>

namespace tdx::recon_contract_detail {
namespace {

struct FoundationContract {
    std::string_view id;
    FoundationContractValidator validate;
};

constexpr std::array<FoundationContract, 5> kFoundationContracts{{
    {"formula-icons", validate_formula_icons_contract},
    {"formula-coverage-render-fidelity-live", validate_formula_coverage_contract},
    {"health", validate_service_health_contract},
    {"features", validate_feature_catalog_contract},
    {"openapi", validate_openapi_catalog_contract},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < kFoundationContracts.size(); ++left)
        for (std::size_t right = left + 1; right < kFoundationContracts.size(); ++right)
            if (kFoundationContracts[left].id == kFoundationContracts[right].id)
                return false;
    return true;
}

static_assert(unique_contract_ids(), "formula foundation contract ids must be unique");

}  // namespace

bool validate_formula_foundation_contract(const std::string& contract_id,
                                          const Json& document,
                                          Json& result) {
    for (const auto& contract : kFoundationContracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
