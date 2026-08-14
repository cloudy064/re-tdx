#pragma once

#include "recon_contract_internal.hpp"

namespace tdx::recon_contract_detail {

using MarketResearchContractValidator = bool (*)(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);

bool validate_market_research_catalog_contract(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);

// Catalog-family contracts.  None of them reads the request context, so the
// per-contract validators take only the document and the result accumulator.
using MarketResearchCatalogContractValidator = void (*)(const Json&, Json&);

void validate_forecast_latest_contract(const Json&, Json&);
void validate_economic_indicators_catalog_contract(const Json&, Json&);
void validate_economic_indicator_detail_contract(const Json&, Json&);
void validate_theme_library_catalog_contract(const Json&, Json&);
void validate_theme_library_detail_contract(const Json&, Json&);
void validate_strategic_themes_catalog_contract(const Json&, Json&);
void validate_strategic_theme_detail_contract(const Json&, Json&);
void validate_thematic_opportunities_catalog_contract(const Json&, Json&);
void validate_thematic_opportunity_detail_contract(const Json&, Json&);
void validate_thematic_legacy_client_theme_contract(const Json&, Json&);
void validate_thematic_hype_completed_contract(const Json&, Json&);
void validate_thematic_hype_active_contract(const Json&, Json&);

// The five thematic contracts share one preamble (schema, view, availability,
// population totals, master sources).  It lives in the thematic-groups unit and
// is reused by the hype unit.
void assert_thematic_common(const Json& document, Json& result,
                            std::string_view expected_view);
bool validate_market_research_signal_contract(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);
bool validate_market_research_event_contract(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);
bool validate_market_research_institution_contract(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);
bool validate_market_research_intelligence_contract(
    const std::string& contract_id,
    const Json& document,
    const Json& context,
    Json& result);

}  // namespace tdx::recon_contract_detail
