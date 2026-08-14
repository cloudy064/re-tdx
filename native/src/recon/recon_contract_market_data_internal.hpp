#pragma once

#include "recon_contract_internal.hpp"

namespace tdx::recon_contract_detail {

using MarketDataContractValidator = bool (*)(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);

bool validate_market_data_quote_contract(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);
bool validate_market_data_institution_contract(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);
bool validate_market_data_flow_contract(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);
bool validate_market_data_session_contract(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);
bool validate_market_data_bond_contract(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);

}  // namespace tdx::recon_contract_detail
