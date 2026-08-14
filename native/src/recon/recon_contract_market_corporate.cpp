#include "recon_contract_market_corporate_internal.hpp"

#include <array>

namespace tdx::recon_contract_detail {
namespace {

constexpr std::array<MarketCorporateContractValidator, 6>
    market_corporate_validators{
        validate_market_corporate_offerings_contract,
        validate_market_corporate_catalog_contract,
        validate_market_corporate_company_contract,
        validate_market_corporate_ipo_contract,
        validate_market_corporate_insights_contract,
        validate_market_corporate_integration_contract,
    };

}  // namespace

bool validate_market_corporate_contract(const std::string& contract_id,
                                        const Json& document,
                                        const Json& context,
                                        Json& result) {
    for (const auto validator : market_corporate_validators)
        if (validator(contract_id, document, context, result)) return true;
    return false;
}

}  // namespace tdx::recon_contract_detail
