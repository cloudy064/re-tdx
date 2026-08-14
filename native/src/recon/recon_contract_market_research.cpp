#include "recon_contract_market_research_internal.hpp"

#include <array>

namespace tdx::recon_contract_detail {
namespace {

constexpr std::array<MarketResearchContractValidator, 5> market_research_validators{
    validate_market_research_catalog_contract,
    validate_market_research_signal_contract,
    validate_market_research_event_contract,
    validate_market_research_institution_contract,
    validate_market_research_intelligence_contract,
};

}  // namespace

bool validate_market_research_contract(const std::string& contract_id,
                                       const Json& document,
                                       const Json& context,
                                       Json& result) {
    for (const auto validator : market_research_validators)
        if (validator(contract_id, document, context, result)) return true;
    return false;
}

}  // namespace tdx::recon_contract_detail