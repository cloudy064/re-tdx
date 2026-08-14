#pragma once

#include "tdx/json.hpp"

namespace tdx::recon_contract_detail {

using FoundationContractValidator = void (*)(const Json& document, Json& result);

void validate_formula_icons_contract(const Json& document, Json& result);
void validate_formula_coverage_contract(const Json& document, Json& result);
void validate_service_health_contract(const Json& document, Json& result);
void validate_feature_catalog_contract(const Json& document, Json& result);
void validate_openapi_catalog_contract(const Json& document, Json& result);
bool validate_formula_capabilities(const Json& capabilities);

}  // namespace tdx::recon_contract_detail
