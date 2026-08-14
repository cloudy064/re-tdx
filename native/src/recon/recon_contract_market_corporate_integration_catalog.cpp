#include "recon_contract_market_corporate_internal.hpp"

#include "tdx/common.hpp"

#include <array>
#include <string_view>

namespace tdx::recon_contract_detail {
namespace {

struct MarketCorporateIntegrationContract {
    std::string_view id;
    MarketCorporateIntegrationContractValidator validate;
};

constexpr std::array<MarketCorporateIntegrationContract, 11>
    integration_contracts{{
        {"benchmark-analysis-live", validate_benchmark_analysis_contract},
        {"consensus-stage-rankings-live",
         validate_consensus_stage_rankings_contract},
        {"calendar-expanded-live", validate_calendar_expanded_contract},
        {"recent-large-unlocks-live", validate_recent_large_unlocks_contract},
        {"unlock-monthly-pressure-live",
         validate_unlock_monthly_pressure_contract},
        {"stock-roadshows-live", validate_stock_roadshows_contract},
        {"jsn-discovery-live", validate_jsn_discovery_contract},
        {"jsn-candidates-live", validate_jsn_candidates_contract},
        {"cloud-variants-fixed", validate_cloud_variants_fixed_contract},
        {"report-cache-default", validate_report_cache_default_contract},
        {"report-cache-explicit", validate_report_cache_explicit_contract},
    }};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < integration_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < integration_contracts.size();
             ++right)
            if (integration_contracts[left].id == integration_contracts[right].id)
                return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_corporate_integration_contract(
    const std::string& contract_id, const Json& document,
    const Json& context, Json& result) {
    for (const auto& contract : integration_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, context, result);
        return true;
    }
    return validate_stock_resilience_contract(contract_id, document, result);
}

}  // namespace tdx::recon_contract_detail
