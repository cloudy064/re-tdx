#include "recon_contract_internal.hpp"

#include <array>

namespace tdx::recon_contract_detail {

bool validate_market_contract(const std::string& contract_id,
                              const Json& document, const Json& context,
                              Json& result) {
    using Validator = bool (*)(const std::string&, const Json&, const Json&, Json&);
    static constexpr std::array<Validator, 3> validators{
        validate_market_data_contract,
        validate_market_research_contract,
        validate_market_corporate_contract,
    };
    for (const auto validator : validators)
        if (validator(contract_id, document, context, result)) return true;
    return false;
}

}  // namespace tdx::recon_contract_detail
