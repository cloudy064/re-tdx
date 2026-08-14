#include "server_formula_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formula_render_profile.hpp"
#include "tdx/minute.hpp"

#include <map>
#include <set>
#include <string>

namespace tdx::server_detail {

Json query_formula_execution(const FormulaHttpState& state, const RequestTarget& target) {
    const auto [market, code] = query_kline_security(target);
    const auto wanted = lower_ascii(trim(query_value(target, "formula")));
    const auto wanted_formula_kind = lower_ascii(trim(query_value(target, "formula_kind")));
    if (wanted.empty()) throw Error("formula is required");
    const Json* selected = nullptr;
    for (const auto& formula : state.formulas.at("formulas").as_array()) {
        if (lower_ascii(formula.at("code").as_string()) == wanted &&
            (wanted_formula_kind.empty() || formula.at("kind_key").as_string() == wanted_formula_kind)) {
            selected = &formula; break;
        }
    }
    if (!selected) throw Error("formula not found: " + wanted);
    const auto source_entry = selected->as_object().find("source_text");
    if (source_entry == selected->as_object().end() || !source_entry->second.is_string())
        throw Error("selected formula source text is unavailable");
    const auto& analysis = selected->at("analysis");
    const bool allow_future = query_bool(target, "allow_future");
    const bool point_in_time_finance = query_bool(target, "point_in_time_finance");
    const bool future_executable = analysis.at("read_only_future_executable").as_bool();
    if (!analysis.at("executable_with_context").as_bool() &&
        !(allow_future && future_executable)) {
        if (future_executable && !allow_future)
            throw Error("selected formula uses a future function; pass allow_future=1 only for read-only chart rendering");
        std::string reason = "selected formula is not executable by the current interpreter";
        const auto detail = analysis.as_object().find("reason");
        if (detail != analysis.as_object().end() && detail->second.is_string())
            reason += ": " + detail->second.as_string();
        throw Error(reason);
    }
    const auto kind = lower_ascii(trim(query_value(target, "kind", "auto")));
    const auto period = lower_ascii(trim(query_value(target, "period", "day")));
    const auto date = trim(query_value(target, "date", "all"));
    const int pages = parse_bounded(query_value(target, "pages", "1"), "pages", 1, 20);
    const int page_size = parse_bounded(query_value(target, "page_size", "800"), "page_size", 1, 800);
    const int start = parse_bounded(query_value(target, "start", "0"), "start", 0, 65535);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"), "timeout_ms", 100, 30000);
    std::map<std::string, double> parameters;
    const auto specs = selected->as_object().find("parameters");
    if (specs != selected->as_object().end() && specs->second.is_array()) {
        for (const auto& spec : specs->second.as_array()) {
            const auto name = spec.at("name").as_string();
            const auto found = target.query.find(lower_ascii(name));
            const auto exact = target.query.find(name);
            if (found != target.query.end()) parameters[name] = parse_formula_parameter(found->second, name);
            else if (exact != target.query.end()) parameters[name] = parse_formula_parameter(exact->second, name);
        }
    }
    auto kline = fetch_kline_document(market, code, kind, period, pages, page_size,
                                      start, date, timeout, state.root);
    attach_security_metadata(state.block_data, kline, market, code);
    kline = apply_requested_kline_adjustment(
        state.root, target, market, code, kind, timeout, std::move(kline));
    const auto option_name = trim(query_value(target, "option_name"));
    const auto option_expiry = trim(query_value(target, "expiry"));
    if (!option_name.empty()) kline["option_name"] = option_name;
    if (!option_expiry.empty()) kline["option_expiry"] = option_expiry;
    const auto risk_free = trim(query_value(target, "risk_free"));
    if (!risk_free.empty()) {
        const auto parsed = parse_formula_parameter(risk_free, "risk_free");
        if (parsed < -1.0 || parsed > 1.0)
            throw Error("risk_free is outside the supported range");
        kline["option_risk_free"] = parsed;
    }
    Json context;
    const Json* context_pointer = nullptr;
    if (analysis.at("has_external_dependency").as_bool()) {
        if (market != "sz" && market != "sh" && market != "bj") {
            for (const auto& dependency : analysis.at("external_dependencies").as_array())
                if (!formula_dependency_supported_in_expansion(
                        dependency.as_string(), market))
                    throw Error("selected external formula dependency is unavailable for expansion markets");
        }
        context = build_formula_market_context_document(state.root, market, code, analysis,
                                                        timeout, &state.block_data, &kline,
                                                        point_in_time_finance,
                                                        &state.formulas,
                                                        state.jsn_root,
                                                        nullptr, &parameters);
        context_pointer = &context;
    }
    auto result = evaluate_formula_document(std::move(kline), *selected, parameters, context_pointer);
    result["render_environment"] = formula_render_environment_document(state.root);
    if (allow_future && future_executable) {
        result["future_execution_mode"] = "explicit-read-only-lookahead";
        result["future_functions"] = analysis.at("future_functions");
        result["scan_allowed"] = false;
        result["backtest_allowed"] = false;
    }
    return result;
}


} // namespace tdx::server_detail
