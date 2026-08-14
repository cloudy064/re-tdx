#pragma once

#include "tdx/json.hpp"

#include <filesystem>
#include <set>
#include <string>

namespace tdx::formula_context_detail {

struct CurrentFinanceRequirements {
    bool current_selectors{};
    bool dynamic_pe{};
    bool capital{};
    bool capital_history{};
    bool region{};
    bool turnover{};
    bool total_capital{};
    std::set<int> selectors;

    bool any() const {
        return current_selectors || dynamic_pe || capital || capital_history ||
               region || turnover || total_capital;
    }
};

struct CurrentFinanceContext {
    Json document;
    Json values{Json::object()};
    Json metadata{Json::object()};
    double circulating_shares{};
    int province_id{};

    const Json* record() const;
};

CurrentFinanceRequirements current_finance_requirements(
    const std::set<std::string>& dependencies,
    const std::set<int>& finance_bindings,
    bool point_in_time_finance,
    bool dynamic_pe,
    bool capital_history);

CurrentFinanceContext build_current_finance_context(
    Json& symbols,
    const std::filesystem::path& root,
    const std::string& market,
    const std::string& code,
    const CurrentFinanceRequirements& requirements,
    int market_id,
    int security_type,
    int timeout_ms);

}  // namespace tdx::formula_context_detail
