#pragma once

#include "formula_context_current_finance_internal.hpp"

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace tdx::formula_context_detail {

bool is_tcalc_hk_finance_market(int market_id);
bool is_tcalc_hk_finance_selector(int market_id, int selector);
std::optional<int> tcalc_finance_binding_selector(std::string_view binding);
std::set<int> tcalc_finance_selectors(const Json& analysis);
bool supports_tcalc_hk_finance_analysis(const Json& analysis, int market_id);

Json project_tcalc_hk_finance_values(
    const Json& record,
    int market_id,
    int security_type,
    const std::set<int>& selectors);

CurrentFinanceContext build_tcalc_hk_finance_context(
    const std::filesystem::path& root,
    const std::string& code,
    const CurrentFinanceRequirements& requirements,
    int market_id,
    int security_type);

}  // namespace tdx::formula_context_detail
