#include "recon_contract_market_research_internal.hpp"

#include "tdx/common.hpp"

#include <array>
#include <string_view>

namespace tdx::recon_contract_detail {
namespace {

struct MarketResearchCatalogContract {
    std::string_view id;
    MarketResearchCatalogContractValidator validate;
};

constexpr std::array<MarketResearchCatalogContract, 12> catalog_contracts{{
    {"forecast-latest-live", validate_forecast_latest_contract},
    {"economic-indicators-catalog-live",
     validate_economic_indicators_catalog_contract},
    {"economic-indicator-detail-live",
     validate_economic_indicator_detail_contract},
    {"theme-library-catalog-live", validate_theme_library_catalog_contract},
    {"theme-library-detail-live", validate_theme_library_detail_contract},
    {"strategic-themes-catalog-live",
     validate_strategic_themes_catalog_contract},
    {"strategic-theme-detail-live", validate_strategic_theme_detail_contract},
    {"thematic-opportunities-catalog-live",
     validate_thematic_opportunities_catalog_contract},
    {"thematic-opportunity-detail-live",
     validate_thematic_opportunity_detail_contract},
    {"thematic-legacy-client-theme-live",
     validate_thematic_legacy_client_theme_contract},
    {"thematic-hype-completed-live", validate_thematic_hype_completed_contract},
    {"thematic-hype-active-live", validate_thematic_hype_active_contract},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < catalog_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < catalog_contracts.size();
             ++right)
            if (catalog_contracts[left].id == catalog_contracts[right].id)
                return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_research_catalog_contract(const std::string& contract_id,
    const Json& document, const Json& context, Json& result) {
    (void)context;
    for (const auto& contract : catalog_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
