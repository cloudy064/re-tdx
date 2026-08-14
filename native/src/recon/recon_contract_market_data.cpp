#include "recon_contract_market_data_internal.hpp"

#include <array>

namespace tdx::recon_contract_detail {
namespace {

constexpr std::array<MarketDataContractValidator, 5> market_data_validators{
    validate_market_data_quote_contract,
    validate_market_data_institution_contract,
    validate_market_data_flow_contract,
    validate_market_data_session_contract,
    validate_market_data_bond_contract,
};

}  // namespace

bool validate_market_data_contract(const std::string& contract_id,
                                   const Json& document,
                                   const Json& context,
                                   Json& result) {
    for (const auto validator : market_data_validators)
        if (validator(contract_id, document, context, result)) return true;
    return false;
}

}  // namespace tdx::recon_contract_detail