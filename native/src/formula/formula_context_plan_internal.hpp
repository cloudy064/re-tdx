#pragma once

#include "formula_context_aggregate_internal.hpp"
#include "formula_context_current_finance_internal.hpp"
#include "formula_context_dynamic_internal.hpp"
#include "formula_context_finance_internal.hpp"
#include "formula_context_professional_internal.hpp"
#include "formula_context_relations_internal.hpp"
#include "formula_reference_internal.hpp"

#include "tdx/json.hpp"

#include <filesystem>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>

namespace tdx::formula_context_detail {

struct FormulaContextPlan {
    std::set<std::string> dependencies;
    std::set<int> finance_bindings;
    std::set<int> professional_finance_bindings;
    DynamicQuoteRequirements dynamic_quote;
    std::vector<TradingBinding> stock_trading_bindings;
    std::vector<TradingBinding> board_trading_bindings;
    std::vector<TradingBinding> market_trading_bindings;
    std::vector<OnePointBinding> finone_bindings;
    std::vector<OnePointBinding> stock_one_bindings;
    std::vector<OnePointBinding> board_one_bindings;
    std::vector<OnePointBinding> market_one_bindings;
    std::vector<OnePointBinding> local_one_bindings;
    std::vector<std::string> blocksetnum_bindings;
    std::vector<HorcalcBinding> horizontal_bindings;
    std::vector<IndicatorAggregateBinding> aggregate_bindings;
    std::vector<std::string> calcstockindex_bindings;
    std::vector<FormulaReferenceRequest> formula_reference_bindings;
    std::vector<SplitBinding> split_context_bindings;
    std::vector<std::string> external_series_bindings;
    std::vector<std::string> local_signal_bindings;
    std::vector<std::string> main_quote_bindings;
    bool chip_context{};
    bool capital_history_context{};
    bool security_status_context{};
    bool contract_multiplier_context{};
    bool host_calendar_context{};
    CurrentFinanceRequirements current_finance;
    RelationBlockRequirements relation_blocks;

    bool needs_block_data(int status_market_id) const;
    bool needs_formula_library() const;
    bool needs_professional_kline() const;
    std::set<std::string> block_families(
        const std::filesystem::path &root) const;
};

FormulaContextPlan build_formula_context_plan(const Json &analysis,
                                              bool point_in_time_finance);
void validate_formula_context_plan(const FormulaContextPlan &plan,
                                   const Json *kline_document,
                                   bool point_in_time_finance);
bool includes_any(const std::set<int> &values,
                  std::initializer_list<int> wanted);

} // namespace tdx::formula_context_detail
