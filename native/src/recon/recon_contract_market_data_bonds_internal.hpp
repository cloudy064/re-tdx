#pragma once

#include "tdx/json.hpp"

namespace tdx::recon_contract_detail {

using BondContractValidator = void (*)(const Json& document, Json& result);

void validate_bond_reference_universe(const Json& document, Json& result);
void validate_bond_reference_corporate(const Json& document, Json& result);
void validate_bond_reference_private_boundary(const Json& document, Json& result);
void validate_bond_reference_aa_plus(const Json& document, Json& result);
void validate_bond_reference_government(const Json& document, Json& result);
void validate_bond_reference_policy_financial(const Json& document, Json& result);
void validate_pending_convertible_bonds(const Json& document, Json& result);
void validate_convertible_bond_subscriptions(const Json& document, Json& result);
void validate_exchangeable_bond_supplement(const Json& document, Json& result);
void validate_convertible_bond_pricing(const Json& document, Json& result);

}  // namespace tdx::recon_contract_detail
