#include "financial_insights_internal.hpp"

#include <algorithm>

namespace tdx::detail::financial_insights {
namespace {

constexpr std::array<ResourceDefinition, 15> kResources{{
    {"list/func_cbpl101_8.jsn", "buffett-quality", "巴菲特质量筛选"},
    {"list/func_cbpl106_1.jsn", "high-bonus-potential", "高送转潜力"},
    {"list/func_cbpl107_1.jsn", "investment-property", "投资性房地产"},
    {"list/func_dpsgp_1.jsn", "low-price-sales", "低市销率"},
    {"list/func_fhbdb101.jsn", "dividend-shortfall", "分红不足"},
    {"list/func_gqtz101_1.jsn", "equity-investment", "股权投资"},
    {"list/func_gxjcb101.jsn", "cash-above-market-cap", "现金高于市值"},
    {"list/func_gyszk101_1.jsn", "high-receivables", "高应收账款"},
    {"list/func_knzx101_1.jsn", "profit-warning", "实际与预告利润"},
    {"list/func_xjl101_1.jsn", "cash-flow-quality", "现金流质量"},
    {"list/func_yjfz101_1.jsn", "earnings-reversal", "业绩反转"},
    {"list/func_wjcg101_1.jsn", "steady-growth", "稳健成长"},
    {"list/func_lxsnzz101_1.jsn", "quality-growth", "连续质量增长"},
    {"list/func_tqwclr101.jsn", "profit-breakout", "利润突破"},
    {"list/func_qxfa101_1.jsn", "dividend-plan", "当前分红方案"},
}};

constexpr std::array<SortDefinition, 6> kSorts{{
    {"signal", "signal_value"},
    {"amount", "amount_value_yuan"},
    {"ratio", "ratio_value_pct"},
    {"pe", "pe"},
    {"roe", "roe_pct"},
    {"code", ""},
}};

template <typename Definitions, typename First, typename Second>
constexpr bool unique_fields(const Definitions& definitions,
                             First first, Second second) {
    for (std::size_t left = 0; left < definitions.size(); ++left)
        for (std::size_t right = left + 1; right < definitions.size(); ++right)
            if (first(definitions[left]) == first(definitions[right]) ||
                second(definitions[left]) == second(definitions[right]))
                return false;
    return true;
}

static_assert(unique_fields(
    kResources,
    [](const auto& value) { return value.resource; },
    [](const auto& value) { return value.kind; }),
    "financial insight resources and kinds must be unique");

static_assert(unique_fields(
    kSorts,
    [](const auto& value) { return value.name; },
    [](const auto& value) { return value.name; }),
    "financial insight sort names must be unique");

}  // namespace

const std::array<ResourceDefinition, 15>& resource_definitions() {
    return kResources;
}

const ResourceDefinition& find_resource(std::string_view resource) {
    const auto found = std::find_if(kResources.begin(), kResources.end(),
        [&](const auto& definition) {
            return definition.resource == resource;
        });
    if (found == kResources.end())
        throw Error("unknown financial-insights resource: " +
                    std::string(resource));
    return *found;
}

const ResourceDefinition* find_view(std::string_view view) {
    const auto found = std::find_if(kResources.begin(), kResources.end(),
        [&](const auto& definition) {
            return definition.kind == view;
        });
    return found == kResources.end() ? nullptr : &*found;
}

const std::vector<std::string>& resource_paths() {
    static const std::vector<std::string> paths = [] {
        std::vector<std::string> result;
        result.reserve(kResources.size());
        for (const auto& definition : kResources)
            result.emplace_back(definition.resource);
        return result;
    }();
    return paths;
}

std::string sort_field(std::string_view sort) {
    const auto found = std::find_if(kSorts.begin(), kSorts.end(),
        [&](const auto& definition) {
            return definition.name == sort;
        });
    if (found == kSorts.end())
        throw Error("unsupported financial-insights sort");
    return std::string(found->field);
}

}  // namespace tdx::detail::financial_insights
