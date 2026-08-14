#include "recon_contract_market_corporate_internal.hpp"

#include <array>
#include <string_view>

namespace tdx::recon_contract_detail {
namespace {

struct MarketCorporateCatalogContract {
    std::string_view id;
    MarketCorporateCatalogContractValidator validate;
};

constexpr std::array<MarketCorporateCatalogContract, 7> catalog_contracts{{
    {"special-situations-live", validate_market_special_situations_contract},
    {"corporate-transitions-live", validate_market_corporate_transitions_contract},
    {"exchange-funds-live", validate_market_exchange_funds_contract},
    {"etf-share-ranking-live", validate_market_etf_share_ranking_contract},
    {"curated-data-live", validate_market_curated_data_contract},
    {"special-attention-live", validate_market_special_attention_contract},
    {"fund-statistics-live", validate_market_fund_statistics_contract},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < catalog_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < catalog_contracts.size(); ++right)
            if (catalog_contracts[left].id == catalog_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_market_corporate_catalog_contract(
    const std::string& contract_id, const Json& document,
    const Json&, Json& result) {
    for (const auto& contract : catalog_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail
