#pragma once

#include "server_formula_internal.hpp"
#include "server_state_internal.hpp"

namespace tdx::server_detail {

Json query_market_margin(const ApiState&, const RequestTarget&);
Json query_market_stock_connect(const ApiState&, const RequestTarget&);
Json query_market_intelligence(const ApiState&, const RequestTarget&);
Json query_market_technical_signals(const ApiState&, const RequestTarget&);
Json query_market_factors(const ApiState&, const RequestTarget&);
Json query_market_block_backtest(const ApiState&, const RequestTarget&);
Json query_market_equity_valuation(const ApiState&, const RequestTarget&);
Json query_market_relative_valuation(const ApiState&, const RequestTarget&);
Json query_market_flow_followup(const ApiState&, const RequestTarget&);
Json query_market_abnormal_moves(const ApiState&, const RequestTarget&);
Json query_market_abnormal_details(const ApiState&, const RequestTarget&);
Json query_market_anomaly_risk(const ApiState&, const RequestTarget&);
Json query_market_profit_gaps(const ApiState&, const RequestTarget&);
Json query_market_index_volatility(const ApiState&, const RequestTarget&);
Json query_market_total_return_gap(const ApiState&, const RequestTarget&);
Json query_market_fund_analytics(const ApiState&, const RequestTarget&);

}  // namespace tdx::server_detail
