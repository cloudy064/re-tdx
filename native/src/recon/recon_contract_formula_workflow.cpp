#include "recon_contract_internal.hpp"
#include "recon_contract_formula_internal.hpp"
#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tdx::recon_contract_detail {

namespace {

using WorkflowContractValidator = void (*)(const Json& document, Json& result);

void validate_formula_inline_scan_post(const Json& document, Json& result) {
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-source-interpreter-v1"),
                  "tdx-source-interpreter-v1",
                  value_or_null(member(document, "engine")));
    add_assertion(result, "inline_source_mode",
                  string_is(member(document, "formula_source_mode"), "inline-post"),
                  "inline-post", value_or_null(member(document, "formula_source_mode")));
    add_assertion(result, "request_not_retained",
                  bool_is(member(document, "request_body_retained"), false), false,
                  value_or_null(member(document, "request_body_retained")));
    const bool selection_shape =
        string_is(member(document, "kind"), "selection") &&
        number_is(member(document, "requested_count"), 2) &&
        number_is(member(document, "evaluated"), 2) &&
        number_is(member(document, "error_count"), 0) &&
        number_is(member(document, "fetch_error_count"), 0) &&
        number_is(member(document, "fetch_workers"), 2) &&
        member(document, "matches") && member(document, "matches")->is_array();
    add_assertion(result, "selection_shape", selection_shape, true,
                  selection_shape);
    const auto* analysis = member(document, "analysis");
    add_assertion(result, "source_analysis",
                  analysis && bool_is(member(*analysis, "syntax_supported"), true) &&
                      bool_is(member(*analysis, "executable"), true),
                  true, value_or_null(analysis));
    const auto* adjustment = member(document, "adjustment_summary");
    const bool qfq = string_is(member(document, "adjustment_mode"), "qfq") &&
        adjustment && string_is(member(*adjustment, "mode"), "qfq") &&
        string_is(member(*adjustment, "security_scope"), "per-security") &&
        string_is(member(*adjustment, "source_command"), "0x000F") &&
        string_is(member(*adjustment, "method"),
                  "local-corporate-action-factor-v1");
    add_assertion(result, "qfq_adjustment", qfq, true, qfq);
    const bool cache = formula_adjustment_cache_is(document, 2);
    add_assertion(result, "bounded_shared_adjustment_cache", cache, true,
                  cache);
}

void validate_formula_inline_backtest_post(const Json& document, Json& result) {
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-source-backtest-v1"),
                  "tdx-source-backtest-v1",
                  value_or_null(member(document, "engine")));
    add_assertion(result, "inline_source_mode",
                  string_is(member(document, "formula_source_mode"), "inline-post"),
                  "inline-post", value_or_null(member(document, "formula_source_mode")));
    add_assertion(result, "request_not_retained",
                  bool_is(member(document, "request_body_retained"), false), false,
                  value_or_null(member(document, "request_body_retained")));
    const auto* trade_count = member(document, "trade_count");
    const bool expert_shape =
        string_is(member(document, "kind"), "expert") &&
        number_is(member(document, "count"), 240) &&
        trade_count && trade_count->is_number() && trade_count->as_number() >= 1.0 &&
        string_is(member(document, "signal_timing"),
                  "signal at close, execute at next open");
    add_assertion(result, "expert_backtest_shape", expert_shape, true,
                  expert_shape);
    const auto* analysis = member(document, "analysis");
    add_assertion(result, "source_analysis",
                  analysis && bool_is(member(*analysis, "syntax_supported"), true) &&
                      bool_is(member(*analysis, "numeric_signal_safe"), true) &&
                      !bool_is(member(*analysis, "has_future_function"), true),
                  true, value_or_null(analysis));
    const auto* adjustment = member(document, "adjustment");
    const bool qfq = string_is(member(document, "adjustment_mode"), "qfq") &&
        adjustment && string_is(member(*adjustment, "mode"), "qfq") &&
        string_is(member(*adjustment, "source_command"), "0x000F") &&
        string_is(member(*adjustment, "method"),
                  "local-corporate-action-factor-v1");
    add_assertion(result, "qfq_adjustment", qfq, true, qfq);
}

void validate_formula_strategy_scan_post(const Json& document, Json& result) {
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-formula-strategy-v1"),
                  "tdx-formula-strategy-v1",
                  value_or_null(member(document, "engine")));
    add_assertion(result, "manifest_source_mode",
                  string_is(member(document, "strategy_source_mode"),
                            "manifest-post"),
                  "manifest-post",
                  value_or_null(member(document, "strategy_source_mode")));
    add_assertion(result, "request_not_retained",
                  bool_is(member(document, "request_body_retained"), false),
                  false, value_or_null(member(document, "request_body_retained")));
    const auto* strategy = member(document, "strategy");
    const auto* rules = strategy ? member(*strategy, "rules") : nullptr;
    bool sanitized = rules && rules->is_array() && rules->size() == 2;
    if (sanitized) {
        for (const auto& rule : rules->as_array())
            sanitized = sanitized && rule.is_object() &&
                !object_has(&rule, "source") &&
                !object_has(&rule, "source_text") &&
                !object_has(&rule, "formula_definition");
    }
    const bool combination = strategy &&
        string_is(member(*strategy, "operator"), "all") &&
        number_is(member(*strategy, "minimum_matches"), 2) && sanitized;
    add_assertion(result, "same_bar_strategy", combination, true, combination);
    const bool scan_shape =
        number_is(member(document, "requested_count"), 2) &&
        number_is(member(document, "evaluated"), 2) &&
        number_is(member(document, "error_count"), 0) &&
        number_is(member(document, "fetch_error_count"), 0) &&
        number_is(member(document, "fetch_workers"), 2) &&
        member(document, "matches") && member(document, "matches")->is_array();
    add_assertion(result, "complete_scan", scan_shape, true, scan_shape);
    const auto* adjustment = member(document, "adjustment_summary");
    const bool qfq = string_is(member(document, "adjustment_mode"), "qfq") &&
        adjustment && string_is(member(*adjustment, "mode"), "qfq") &&
        string_is(member(*adjustment, "security_scope"), "per-security") &&
        string_is(member(*adjustment, "source_command"), "0x000F") &&
        string_is(member(*adjustment, "method"),
                  "local-corporate-action-factor-v1");
    add_assertion(result, "qfq_adjustment", qfq, true, qfq);
    const bool cache = formula_adjustment_cache_is(document, 2);
    add_assertion(result, "bounded_shared_adjustment_cache", cache, true,
                  cache);
}

void validate_formula_strategy_backtest_post(const Json& document, Json& result) {
    add_assertion(result, "engine",
                  string_is(member(document, "engine"),
                            "tdx-formula-strategy-portfolio-v1"),
                  "tdx-formula-strategy-portfolio-v1",
                  value_or_null(member(document, "engine")));
    const bool timing = string_is(
        member(document, "signal_timing"),
        "shared bar close signal; rebalance at next shared bar open");
    add_assertion(result, "next_open_timing", timing, true, timing);
    const auto* attribution = member(document, "attribution");
    const auto* initial = member(document, "initial_capital");
    const auto* final = member(document, "final_equity");
    double attributed = 0.0;
    bool attribution_shape = attribution && attribution->is_array() &&
                             attribution->size() == 2;
    if (attribution_shape) {
        for (const auto& row : attribution->as_array()) {
            const auto* net = member(row, "net_contribution");
            const auto* id = member(row, "security_id");
            if (!net || !net->is_number() || !id || !id->is_string()) {
                attribution_shape = false;
                break;
            }
            attributed += net->as_number();
        }
    }
    const bool reconciled = attribution_shape && initial && initial->is_number() &&
        final && final->is_number() &&
        std::abs(initial->as_number() + attributed - final->as_number()) < 0.01;
    add_assertion(result, "attribution_reconciles", reconciled, true, reconciled);
    const bool portfolio_shape =
        string_is(member(document, "execution_mode"), "native-cpp") &&
        number_is(member(document, "security_count"), 2) &&
        member(document, "aligned_bar_count") &&
        member(document, "aligned_bar_count")->is_number() &&
        member(document, "aligned_bar_count")->as_number() >= 3 &&
        number_is(member(document, "fetch_error_count"), 0) &&
        number_is(member(document, "fetch_workers"), 2) &&
        bool_is(member(document, "request_body_retained"), false);
    add_assertion(result, "portfolio_shape", portfolio_shape, true,
                  portfolio_shape);
    const auto* adjustment = member(document, "adjustment_summary");
    const bool qfq = string_is(member(document, "adjustment_mode"), "qfq") &&
        adjustment && string_is(member(*adjustment, "mode"), "qfq") &&
        string_is(member(*adjustment, "security_scope"), "per-security") &&
        string_is(member(*adjustment, "source_command"), "0x000F") &&
        string_is(member(*adjustment, "method"),
                  "local-corporate-action-factor-v1");
    add_assertion(result, "qfq_adjustment", qfq, true, qfq);
    const bool cache = formula_adjustment_cache_is(document, 2);
    add_assertion(result, "bounded_shared_adjustment_cache", cache, true,
                  cache);
}

struct WorkflowContract {
    std::string_view id;
    WorkflowContractValidator validate;
};

constexpr std::array<WorkflowContract, 4> workflow_contracts{{
    {"formula-inline-scan-post", validate_formula_inline_scan_post},
    {"formula-inline-backtest-post", validate_formula_inline_backtest_post},
    {"formula-strategy-scan-post", validate_formula_strategy_scan_post},
    {"formula-strategy-backtest-post", validate_formula_strategy_backtest_post},
}};

constexpr bool unique_contract_ids() {
    for (std::size_t left = 0; left < workflow_contracts.size(); ++left)
        for (std::size_t right = left + 1; right < workflow_contracts.size(); ++right)
            if (workflow_contracts[left].id == workflow_contracts[right].id) return false;
    return true;
}

static_assert(unique_contract_ids());

}  // namespace

bool validate_formula_workflow_contract(const std::string& contract_id,
                                        const Json& document,
                                        Json& result) {
    for (const auto& contract : workflow_contracts) {
        if (contract.id != contract_id) continue;
        contract.validate(document, result);
        return true;
    }
    return false;
}

}  // namespace tdx::recon_contract_detail

