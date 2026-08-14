#include "industry_profile_internal.hpp"

namespace tdx::detail::industry_profile {
namespace {

constexpr std::array<IndustryHoldingPeriod, 4> kHoldingPeriods{{
    {"q1", "一季度", "list/func_cgfxhy101_1.jsn"},
    {"half-year", "半年度", "list/func_cgfxhy103_1.jsn"},
    {"q3", "三季度", "list/func_cgfxhy104_1.jsn"},
    {"annual", "年度", "list/func_cgfxhy105_1.jsn"},
}};

constexpr std::string_view kShareholderResource =
    "list/func_hygdrs101_1.jsn";

constexpr bool unique_periods() {
    for (std::size_t left = 0; left < kHoldingPeriods.size(); ++left)
        for (std::size_t right = left + 1; right < kHoldingPeriods.size(); ++right)
            if (kHoldingPeriods[left].key == kHoldingPeriods[right].key ||
                kHoldingPeriods[left].resource == kHoldingPeriods[right].resource)
                return false;
    return true;
}

static_assert(unique_periods(),
              "industry holding period keys and resources must be unique");

}  // namespace

const std::array<IndustryHoldingPeriod, 4>& industry_holding_periods() {
    return kHoldingPeriods;
}

std::string_view industry_shareholder_resource() {
    return kShareholderResource;
}

const std::vector<std::string>& industry_master_resource_paths() {
    static const std::vector<std::string> resources = [] {
        std::vector<std::string> result;
        result.reserve(kHoldingPeriods.size() + 1);
        for (const auto& period : kHoldingPeriods)
            result.emplace_back(period.resource);
        result.emplace_back(kShareholderResource);
        return result;
    }();
    return resources;
}

}  // namespace tdx::detail::industry_profile
