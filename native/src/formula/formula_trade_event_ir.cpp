#include "formula_trade_event_ir_internal.hpp"

#include "formula_engine_support_internal.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_trade_event_ir_detail {
namespace {

using formula_engine_detail::Series;
using formula_language_detail::NodeKind;
using formula_runtime_detail::evaluate_node;

struct TradeSignalDefinition {
    std::string_view name;
    std::uint32_t wrapper_action;
    std::uint32_t host_action_bits;
};

constexpr std::array<TradeSignalDefinition, 6> trade_signals{{
    {"BUY", 0x1, 0x1},
    {"SELL", 0x10, 0x10},
    {"SELLSHORT", 0x100, 0x100},
    {"BUYSHORT", 0x1000, 0x1000},
    {"BUYSHORT_BUY", 0x10000, 0x1001},
    {"SELL_SELLSHORT", 0x100000, 0x110},
}};

const TradeSignalDefinition* trade_signal(std::string_view name) {
    for (const auto& signal : trade_signals)
        if (signal.name == name) return &signal;
    return nullptr;
}

Json aligned_series(const Series& values) {
    Json result = Json::array();
    for (const double value : values)
        result.push_back(std::isfinite(value) ? Json(value) : Json(nullptr));
    return result;
}

}  // namespace

std::optional<TradeEventEvaluation> evaluate_trade_event_statement(
    const formula_language_detail::Statement& statement,
    formula_engine_detail::Environment& environment,
    const formula_runtime_detail::StringEnvironment& strings,
    std::size_t bar_count, std::size_t statement_index) {
    const auto& expression = *statement.expression;
    if (expression.kind != NodeKind::call) return std::nullopt;
    const auto* signal = trade_signal(expression.text);
    if (!signal) return std::nullopt;
    formula_engine_detail::require_arity(
        expression.text,
        std::vector<Series>(expression.children.size()), 2, 2);

    auto condition = evaluate_node(
        *expression.children[0], environment, strings, bar_count);
    const auto price = evaluate_node(
        *expression.children[1], environment, strings, bar_count);
    for (std::size_t index = 0; index < bar_count; ++index)
        if (!std::isfinite(condition[index]) || !std::isfinite(price[index]))
            condition[index] = 0.0;

    Json primitive = Json::object();
    primitive["statement"] = statement.name;
    primitive["statement_index"] = static_cast<std::uint64_t>(statement_index);
    primitive["function"] = expression.text;
    primitive["argument_count"] = 2;
    primitive["condition_argument"] = 0;
    primitive["price_argument"] = 1;
    primitive["condition_series"] = aligned_series(condition);
    primitive["price_series"] = aligned_series(price);
    primitive["missing_rule"] =
        "condition-or-price-native-missing-clears-condition;price-copy-preserved";
    primitive["wrapper_action"] = static_cast<std::uint64_t>(
        signal->wrapper_action);
    primitive["host_action_bits"] = static_cast<std::uint64_t>(
        signal->host_action_bits);
    primitive["native_host_action_guard"] =
        "latest condition approximately 1 (epsilon 1e-5)";
    primitive["historical_signal_candidates"] = Json::array();
    for (std::size_t index = 0; index < bar_count; ++index) {
        if (!formula_engine_detail::truth(condition[index])) continue;
        Json candidate = Json::object();
        candidate["index"] = static_cast<std::uint64_t>(index);
        candidate["condition"] = condition[index];
        candidate["price"] = price[index];
        candidate["projection"] = "offline-per-bar";
        candidate["native_host_action"] = false;
        primitive["historical_signal_candidates"].push_back(
            std::move(candidate));
    }
    primitive["latest_host_action"] = Json(nullptr);
    if (bar_count &&
        std::fabs(condition.back() - 1.0) < 0.0000099999997) {
        Json event = Json::object();
        event["index"] = static_cast<std::uint64_t>(bar_count - 1);
        event["condition"] = condition.back();
        event["price"] = price.back();
        event["wrapper_action"] = static_cast<std::uint64_t>(
            signal->wrapper_action);
        event["host_action_bits"] = static_cast<std::uint64_t>(
            signal->host_action_bits);
        event["execution_side_effects"] = false;
        event["order_submission"] = false;
        primitive["latest_host_action"] = std::move(event);
    }
    return TradeEventEvaluation{std::move(condition), std::move(primitive)};
}

Json trade_event_ir_document(Json primitives, std::size_t statement_count,
                             Json autofilter_projection) {
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-formula-trade-event-ir-v1";
    result["scope"] = "top-level-trading-signal-statements-only";
    result["bar_reference"] = "points[index]";
    result["source_statement_count"] =
        static_cast<std::uint64_t>(statement_count);
    result["primitive_count"] =
        static_cast<std::uint64_t>(primitives.size());
    result["execution_side_effects"] = false;
    result["order_submission"] = false;
    result["account_access"] = false;
    result["network_access"] = false;
    result["autofilter"] = std::move(autofilter_projection);
    result["primitives"] = std::move(primitives);
    return result;
}

}  // namespace tdx::formula_trade_event_ir_detail
