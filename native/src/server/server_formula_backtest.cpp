#include "server_formula_support_internal.hpp"

#include "../formula/formula_engine_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formula_strategy.hpp"
#include "tdx/minute.hpp"

#include <map>
#include <string>

namespace tdx::server_detail {

Json execute_formula_backtest(const FormulaHttpState& state, const RequestTarget& target,
                              const Json& selected,
                              const std::map<std::string, double>& parameters) {
    const auto [market, code] = query_security(target);
    const auto& analysis = selected.at("analysis");
    if (!analysis.at("executable_with_context").as_bool())
        throw Error("selected backtest formula requires unsupported data or a future function");
    const bool point_in_time_finance = query_bool(target, "point_in_time_finance");
    std::set<std::string> dependencies;
    for (const auto& value : analysis.at("external_dependencies").as_array())
        dependencies.insert(value.as_string());
    const bool finance_dependency = dependencies.count("FINANCE") ||
                                    dependencies.count("FINVALUE");
    if (point_in_time_finance && !finance_dependency)
        throw Error("point_in_time_finance requires FINANCE or FINVALUE dependencies");
    if (finance_dependency && !point_in_time_finance)
        throw Error("historical backtest finance requires point_in_time_finance=1; "
                    "current report constants are not backtest-safe");
    for (const auto& dependency : dependencies)
        if (dependency != "FINANCE" && dependency != "FINVALUE" &&
            !formula_engine_support::supported_formula_reference_dependency(
                dependency))
            throw Error("historical backtest does not yet admit external dependency " +
                        dependency + "; its point-in-time safety is unproven");
    const auto period = lower_ascii(trim(query_value(target, "period", "day")));
    const auto kind = lower_ascii(trim(query_value(target, "kind", "auto")));
    const int pages = parse_bounded(query_value(target, "pages", "5"), "pages", 1, 20);
    const int page_size = parse_bounded(query_value(target, "page_size", "800"), "page_size", 1, 800);
    const int start = parse_bounded(query_value(target, "start", "0"), "start", 0, 65535);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"), "timeout_ms", 100, 30000);
    const auto initial = parse_formula_parameter(query_value(target, "initial_capital", "100000"), "initial_capital");
    const auto commission = parse_formula_parameter(query_value(target, "commission_bps", "2.5"), "commission_bps");
    const auto slippage = parse_formula_parameter(query_value(target, "slippage_bps", "1"), "slippage_bps");
    auto kline = fetch_kline_document(market, code, kind, period, pages, page_size,
                                      start, "all", timeout, state.root);
    attach_security_metadata(state.block_data, kline, market, code);
    kline = apply_requested_kline_adjustment(
        state.root, target, market, code, kind, timeout, std::move(kline));
    Json context; const Json* context_pointer = nullptr;
    if (analysis.at("has_external_dependency").as_bool()) {
        context = build_formula_market_context_document(
            state.root, market, code, analysis, timeout, &state.block_data, &kline,
            point_in_time_finance, &state.formulas, state.jsn_root, nullptr,
            &parameters);
        context_pointer = &context;
    }
    auto result = backtest_formula_document(std::move(kline), selected, parameters,
                                            initial, commission, slippage,
                                            context_pointer);
    result["point_in_time_finance"] = point_in_time_finance;
    return result;
}

Json query_formula_backtest(const FormulaHttpState& state, const RequestTarget& target,
                            const Json& formulas) {
    const auto wanted = lower_ascii(trim(query_value(target, "formula")));
    if (wanted.empty()) throw Error("formula is required");
    const auto formula_kind = lower_ascii(trim(query_value(
        target, "formula_kind", "expert")));
    const Json* selected = nullptr;
    for (const auto& formula : formulas.at("formulas").as_array())
        if (lower_ascii(formula.at("code").as_string()) == wanted &&
            formula.at("kind_key").as_string() == formula_kind) {
            selected = &formula;
            break;
        }
    if (!selected)
        throw Error("formula not found for backtest: " + wanted +
                    " (" + formula_kind + ")");
    std::map<std::string, double> parameters;
    for (const auto& spec : selected->at("parameters").as_array()) {
        const auto name = spec.at("name").as_string();
        const auto found = target.query.find(lower_ascii(name));
        if (found != target.query.end())
            parameters[name] = parse_formula_parameter(found->second, name);
    }
    return execute_formula_backtest(state, target, *selected, parameters);
}

Json query_inline_formula_backtest(const FormulaHttpState& state,
                                   const RequestTarget& original_target,
                                   const Json& body) {
    RequestTarget target = original_target;
    merge_formula_request_target(target, body);
    merge_formula_adjustment_target(target, body);
    auto request = parse_inline_formula_request(body, "expert");
    auto result = execute_formula_backtest(
        state, target, request.definition, request.parameters);
    result["formula_source_mode"] = "inline-post";
    result["source_bytes"] = static_cast<std::uint64_t>(request.source.size());
    result["request_body_retained"] = false;
    return result;
}


} // namespace tdx::server_detail
