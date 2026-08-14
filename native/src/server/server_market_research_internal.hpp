#pragma once

#include "server_formula_internal.hpp"
#include "server_state_internal.hpp"

namespace tdx::server_detail {

Json query_intraday_funds(const ApiState&, const RequestTarget&);
Json query_lhb(const ApiState&, const RequestTarget&);
Json query_market_valuation(const ApiState&, const RequestTarget&);
Json query_market_futures_issuance(const ApiState&, const RequestTarget&);
Json query_market_consensus(const ApiState&, const RequestTarget&);
Json query_market_research(const ApiState&, const RequestTarget&);
Json query_market_roadshows(const ApiState&, const RequestTarget&);
Json query_market_industry_profile(const ApiState&, const RequestTarget&);
Json query_market_unlocks(const ApiState&, const RequestTarget&);
Json query_market_block_trades(const ApiState&, const RequestTarget&);

}  // namespace tdx::server_detail
