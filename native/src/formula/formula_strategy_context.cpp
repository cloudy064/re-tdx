#include "formula_strategy_internal.hpp"

namespace tdx {

using namespace formula_strategy_detail;

void attach_formula_strategy_contexts(Json& kline_document,
                                      const Json& strategy,
                                      const std::string& tdx_root,
                                      int timeout_ms,
                                      const BlockData* block_data,
                                      bool point_in_time_finance,
                                      bool historical_backtest,
                                      const Json* formula_library) {
    attach_strategy_contexts(kline_document, strategy, from_utf8(tdx_root),
                             timeout_ms, block_data, point_in_time_finance,
                             historical_backtest, formula_library);
}

}  // namespace tdx
