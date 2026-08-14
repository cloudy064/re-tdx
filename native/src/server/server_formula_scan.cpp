#include "server_formula_support_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formula_strategy.hpp"
#include "tdx/minute.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tdx::server_detail {

Json execute_formula_scan(const FormulaHttpState& state, const RequestTarget& target,
                          const Json& selected,
                          const std::map<std::string, double>& parameters) {
    const auto& analysis = selected.at("analysis");
    if (!analysis.at("executable_with_context").as_bool())
        throw Error("selected scan formula requires unsupported data or a future function");
    const bool point_in_time_finance = query_bool(target, "point_in_time_finance");
    bool finance_dependency = false;
    for (const auto& value : analysis.at("external_dependencies").as_array())
        if (value.as_string() == "FINANCE" || value.as_string() == "FINVALUE")
            finance_dependency = true;
    if (point_in_time_finance && !finance_dependency)
        throw Error("point_in_time_finance requires FINANCE or FINVALUE dependencies");
    const auto codes_text = trim(query_value(target, "codes"));
    if (codes_text.empty()) throw Error("codes is required");
    std::vector<std::pair<std::string, std::string>> securities;
    for (const auto& item : split(codes_text, ',')) {
        if (trim(item).empty()) continue;
        RequestTarget security; security.query["code"] = trim(item);
        securities.push_back(query_security(security));
    }
    if (securities.empty() || securities.size() > 50) throw Error("codes must contain 1..50 securities");
    const auto period = lower_ascii(trim(query_value(target, "period", "day")));
    const int pages = parse_bounded(query_value(target, "pages", "1"), "pages", 1, 20);
    const int page_size = parse_bounded(query_value(target, "page_size", "800"), "page_size", 1, 800);
    const int lookback = parse_bounded(query_value(target, "lookback", "1"), "lookback", 1, 10000);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "10000"), "timeout_ms", 100, 30000);
    const int workers = parse_bounded(query_value(target, "workers", "4"),
                                      "workers", 1, 16);
    const auto adjustment_mode = requested_kline_adjustment_mode(target);
    std::vector<Json> klines; Json fetch_errors = Json::array();
    auto outcomes = fetch_adjusted_formula_klines(
        state, target, securities, period, pages, page_size, timeout, workers);
    for (std::size_t index = 0; index < securities.size(); ++index) {
        const auto& [market, code] = securities[index];
        try {
            if (!outcomes[index].error.empty())
                throw Error(outcomes[index].error);
            auto kline = std::move(outcomes[index].kline);
            attach_security_metadata(state.block_data, kline, market, code);
            if (analysis.at("has_external_dependency").as_bool())
                kline["formula_context"] = build_formula_market_context_document(
                    state.root, market, code, analysis, timeout, &state.block_data, &kline,
                    point_in_time_finance, &state.formulas, state.jsn_root,
                    nullptr, &parameters);
            klines.push_back(std::move(kline));
        } catch (const std::exception& error) {
            Json row = Json::object(); row["market"] = market; row["code"] = code;
            row["error"] = error.what(); fetch_errors.push_back(std::move(row));
        }
    }
    auto result = scan_formula_documents(klines, selected, parameters, lookback);
    result["requested_count"] = static_cast<std::uint64_t>(securities.size());
    result["fetch_error_count"] = static_cast<std::uint64_t>(fetch_errors.size());
    result["fetch_errors"] = std::move(fetch_errors);
    result["fetch_workers"] = static_cast<std::uint64_t>(
        std::min<std::size_t>(static_cast<std::size_t>(workers), securities.size()));
    result["point_in_time_finance"] = point_in_time_finance;
    attach_multi_security_adjustment_summary(
        result, klines, adjustment_mode);
    return result;
}

Json query_formula_scan(const FormulaHttpState& state, const RequestTarget& target) {
    const auto wanted = lower_ascii(trim(query_value(target, "formula")));
    const auto formula_kind = lower_ascii(trim(query_value(
        target, "formula_kind", "selection")));
    if (wanted.empty()) throw Error("formula is required");
    const Json* selected = nullptr;
    for (const auto& formula : state.formulas.at("formulas").as_array())
        if (lower_ascii(formula.at("code").as_string()) == wanted &&
            formula.at("kind_key").as_string() == formula_kind) {
            selected = &formula;
            break;
        }
    if (!selected)
        throw Error("formula not found for scan: " + wanted +
                    " (" + formula_kind + ")");
    std::map<std::string, double> parameters;
    for (const auto& spec : selected->at("parameters").as_array()) {
        const auto name = spec.at("name").as_string();
        const auto found = target.query.find(lower_ascii(name));
        if (found != target.query.end())
            parameters[name] = parse_formula_parameter(found->second, name);
    }
    return execute_formula_scan(state, target, *selected, parameters);
}

Json query_inline_formula_scan(const FormulaHttpState& state,
                               const RequestTarget& original_target,
                               const Json& body) {
    RequestTarget target = original_target;
    merge_formula_request_target(target, body);
    merge_formula_adjustment_target(target, body);
    auto request = parse_inline_formula_request(body, "selection");
    auto result = execute_formula_scan(
        state, target, request.definition, request.parameters);
    result["formula_source_mode"] = "inline-post";
    result["source_bytes"] = static_cast<std::uint64_t>(request.source.size());
    result["request_body_retained"] = false;
    return result;
}

Json query_formula_strategy(const FormulaHttpState& state,
                            const RequestTarget& original_target,
                            const Json& body,
                            bool historical_backtest) {
    if (!body.is_object()) throw Error("formula strategy body must be an object");
    const auto* manifest = json_member(body, "strategy");
    if (!manifest || !manifest->is_object())
        throw Error("formula strategy body requires a strategy object");
    RequestTarget target = original_target;
    merge_formula_request_target(target, body);
    merge_formula_adjustment_target(target, body);
    const auto strategy = normalize_formula_strategy_document(*manifest, &state.formulas);
    const auto codes_text = trim(query_value(target, "codes"));
    if (codes_text.empty()) throw Error("codes is required");
    std::vector<std::pair<std::string, std::string>> securities;
    std::set<std::string> unique;
    for (const auto& item : split(codes_text, ',')) {
        if (trim(item).empty()) continue;
        RequestTarget security_target;
        security_target.query["code"] = trim(item);
        const auto security = query_security(security_target);
        const auto id = security.first + security.second;
        if (!unique.insert(id).second)
            throw Error("codes contains duplicate security: " + id);
        securities.push_back(security);
    }
    if (securities.empty() || securities.size() > 50)
        throw Error("codes must contain 1..50 unique securities");
    const auto period = lower_ascii(trim(query_value(target, "period", "day")));
    const int pages = parse_bounded(
        query_value(target, "pages", historical_backtest ? "5" : "1"),
        "pages", 1, 20);
    const int page_size = parse_bounded(
        query_value(target, "page_size", "800"), "page_size", 1, 800);
    const int lookback = parse_bounded(
        query_value(target, "lookback", "1"), "lookback", 1, 10000);
    const int timeout = parse_bounded(
        query_value(target, "timeout_ms", "10000"), "timeout_ms", 100, 30000);
    const int workers = parse_bounded(query_value(target, "workers", "4"),
                                      "workers", 1, 16);
    const bool point_in_time_finance = query_bool(target, "point_in_time_finance");
    const auto adjustment_mode = requested_kline_adjustment_mode(target);
    std::vector<Json> klines;
    Json fetch_errors = Json::array();
    auto outcomes = fetch_adjusted_formula_klines(
        state, target, securities, period, pages, page_size, timeout, workers);
    for (std::size_t index = 0; index < securities.size(); ++index) {
        const auto& [market, code] = securities[index];
        try {
            if (!outcomes[index].error.empty())
                throw Error(outcomes[index].error);
            auto kline = std::move(outcomes[index].kline);
            attach_security_metadata(state.block_data, kline, market, code);
            attach_formula_strategy_contexts(
                kline, strategy, path_utf8(state.root), timeout,
                &state.block_data, point_in_time_finance, historical_backtest);
            klines.push_back(std::move(kline));
        } catch (const std::exception& error) {
            Json row = Json::object();
            row["market"] = market;
            row["code"] = code;
            row["error"] = error.what();
            fetch_errors.push_back(std::move(row));
        }
    }
    if (historical_backtest && fetch_errors.size())
        throw Error("strategy portfolio backtest requires a complete fixed universe");
    Json result;
    if (historical_backtest) {
        const auto initial = parse_formula_parameter(
            query_value(target, "initial_capital", "100000"), "initial_capital");
        const auto commission = parse_formula_parameter(
            query_value(target, "commission_bps", "2.5"), "commission_bps");
        const auto slippage = parse_formula_parameter(
            query_value(target, "slippage_bps", "1"), "slippage_bps");
        result = backtest_formula_strategy_documents(
            klines, strategy, initial, commission, slippage);
    } else {
        result = scan_formula_strategy_documents(klines, strategy, lookback);
    }
    result["strategy_source_mode"] = "manifest-post";
    result["request_body_retained"] = false;
    result["requested_count"] = static_cast<std::uint64_t>(securities.size());
    result["fetch_error_count"] = static_cast<std::uint64_t>(fetch_errors.size());
    result["fetch_errors"] = std::move(fetch_errors);
    result["fetch_workers"] = static_cast<std::uint64_t>(
        std::min<std::size_t>(static_cast<std::size_t>(workers), securities.size()));
    result["point_in_time_finance"] = point_in_time_finance;
    attach_multi_security_adjustment_summary(
        result, klines, adjustment_mode);
    return result;
}



} // namespace tdx::server_detail
