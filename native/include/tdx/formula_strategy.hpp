#pragma once

#include "tdx/json.hpp"

#include <string>
#include <vector>

namespace tdx {

struct BlockData;

// Resolve a caller strategy manifest into exact selection-formula definitions.
// Each rule contains either inline `source` or a `formula` code from `library`.
Json normalize_formula_strategy_document(const Json& manifest,
                                         const Json* library = nullptr);

// Attach a separate market context for every rule that needs external TDX
// data. Historical mode rejects dependencies whose point-in-time safety has
// not been demonstrated.
void attach_formula_strategy_contexts(Json& kline_document,
                                      const Json& strategy,
                                      const std::string& tdx_root,
                                      int timeout_ms,
                                      const BlockData* block_data = nullptr,
                                      bool point_in_time_finance = false,
                                      bool historical_backtest = false,
                                      const Json* formula_library = nullptr);

// Evaluate all rules against one K-line document and combine their boolean
// outputs on the exact same date/time bar.
Json evaluate_formula_strategy_document(Json kline_document,
                                        const Json& strategy);

// Scan multiple securities for combined signals in the most recent N bars.
Json scan_formula_strategy_documents(const std::vector<Json>& kline_documents,
                                     const Json& strategy,
                                     int lookback = 1);

// Equal-weight multi-security portfolio backtest. Signals are confirmed at a
// shared bar close and portfolio weights change at the next shared bar open.
Json backtest_formula_strategy_documents(
    const std::vector<Json>& kline_documents,
    const Json& strategy,
    double initial_capital = 100000.0,
    double commission_bps = 2.5,
    double slippage_bps = 1.0);

int command_formulas_strategy(const std::vector<std::string>& args);

}  // namespace tdx
