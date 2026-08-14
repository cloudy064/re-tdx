#pragma once

#include "recon_contract_internal.hpp"

namespace tdx::recon_contract_detail {

using MarketCorporateContractValidator = bool (*)(
    const std::string&, const Json&, const Json&, Json&);
using MarketCorporateCatalogContractValidator = void (*)(const Json&, Json&);
using MarketCorporateIntegrationContractValidator = void (*)(
    const Json&, const Json&, Json&);

bool validate_market_corporate_offerings_contract(
    const std::string&, const Json&, const Json&, Json&);
bool validate_market_corporate_catalog_contract(
    const std::string&, const Json&, const Json&, Json&);
bool validate_market_corporate_company_contract(
    const std::string&, const Json&, const Json&, Json&);
bool validate_market_corporate_ipo_contract(
    const std::string&, const Json&, const Json&, Json&);
bool validate_market_corporate_insights_contract(
    const std::string&, const Json&, const Json&, Json&);
bool validate_market_corporate_integration_contract(
    const std::string&, const Json&, const Json&, Json&);

// Integration contracts.  Each validator receives the response document and
// the request context; only report-cache-explicit reads the context.
void validate_benchmark_analysis_contract(const Json&, const Json&, Json&);
void validate_consensus_stage_rankings_contract(const Json&, const Json&, Json&);
void validate_calendar_expanded_contract(const Json&, const Json&, Json&);
void validate_recent_large_unlocks_contract(const Json&, const Json&, Json&);
void validate_unlock_monthly_pressure_contract(const Json&, const Json&, Json&);
void validate_stock_roadshows_contract(const Json&, const Json&, Json&);
void validate_jsn_discovery_contract(const Json&, const Json&, Json&);
void validate_jsn_candidates_contract(const Json&, const Json&, Json&);
void validate_cloud_variants_fixed_contract(const Json&, const Json&, Json&);
void validate_report_cache_default_contract(const Json&, const Json&, Json&);
void validate_report_cache_explicit_contract(const Json&, const Json&, Json&);

// Eleven per-security contracts share one resilience shape and differ only by
// expected schema and mode; the securities unit owns that typed table and
// reports whether the id belongs to it.
bool validate_stock_resilience_contract(const std::string& contract_id,
                                        const Json& document, Json& result);

void validate_market_special_situations_contract(const Json&, Json&);
void validate_market_corporate_transitions_contract(const Json&, Json&);
void validate_market_exchange_funds_contract(const Json&, Json&);
void validate_market_etf_share_ranking_contract(const Json&, Json&);
void validate_market_curated_data_contract(const Json&, Json&);
void validate_market_special_attention_contract(const Json&, Json&);
void validate_market_fund_statistics_contract(const Json&, Json&);

}  // namespace tdx::recon_contract_detail
