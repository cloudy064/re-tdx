#include "recon_contract_market_data_internal.hpp"
#include "recon_contract_market_data_bonds_internal.hpp"

#include <array>
#include <string_view>

namespace tdx::recon_contract_detail {
namespace {

struct BondContract {
    std::string_view id;
    BondContractValidator validate;
};

constexpr std::array<BondContract, 10> kBondContracts{{
    {"bond-reference-universe-live", validate_bond_reference_universe},
    {"bond-reference-corporate-projections-live", validate_bond_reference_corporate},
    {"bond-reference-private-boundary-live", validate_bond_reference_private_boundary},
    {"bond-reference-aa-plus-live", validate_bond_reference_aa_plus},
    {"bond-reference-government-live", validate_bond_reference_government},
    {"bond-reference-policy-financial-live", validate_bond_reference_policy_financial},
    {"pending-convertible-bonds-live", validate_pending_convertible_bonds},
    {"convertible-bond-subscriptions-live", validate_convertible_bond_subscriptions},
    {"exchangeable-bond-supplement-live", validate_exchangeable_bond_supplement},
    {"convertible-bond-pricing-live", validate_convertible_bond_pricing},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < kBondContracts.size(); ++left)
        for (std::size_t right = left + 1; right < kBondContracts.size(); ++right)
            if (kBondContracts[left].id == kBondContracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids(), "bond contract ids must be unique");

}  // namespace

bool validate_market_data_bond_contract(const std::string& contract_id,
    const Json& document, const Json& context, Json& result) {
    (void)context;
    for (const auto& contract : kBondContracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
