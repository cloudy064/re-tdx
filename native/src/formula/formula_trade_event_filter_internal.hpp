#pragma once

#include "formula_language_internal.hpp"

#include "tdx/json.hpp"

#include <cstddef>

namespace tdx::formula_trade_event_filter_detail {

// Apply AUTOFILTER only as an additive, read-only projection over the
// collected top-level trade-event IR.  Raw numeric results and raw candidates
// remain untouched.
Json apply_autofilter_projection(
    const formula_language_detail::Program& program,
    Json& trade_event_primitives, std::size_t bar_count);

}  // namespace tdx::formula_trade_event_filter_detail
