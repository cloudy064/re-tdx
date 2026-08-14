#pragma once

#include "formula_engine_internal.hpp"
#include "formula_language_internal.hpp"
#include "formula_runtime_internal.hpp"

#include "tdx/json.hpp"

#include <cstddef>
#include <optional>

namespace tdx::formula_trade_event_ir_detail {

struct TradeEventEvaluation {
    formula_engine_detail::Series value;
    Json primitive;
};

// Materialize only a top-level trading-signal statement. Nested signal calls
// are deliberately outside this v1 collector's scope.
std::optional<TradeEventEvaluation> evaluate_trade_event_statement(
    const formula_language_detail::Statement& statement,
    formula_engine_detail::Environment& environment,
    const formula_runtime_detail::StringEnvironment& strings,
    std::size_t bar_count, std::size_t statement_index);

Json trade_event_ir_document(Json primitives, std::size_t statement_count,
                             Json autofilter_projection);

}  // namespace tdx::formula_trade_event_ir_detail
