#pragma once

#include "server_formula_internal.hpp"
#include "server_state_internal.hpp"

#include <filesystem>

namespace tdx::server_detail {

Json query_market_snapshot(const ApiState&, const RequestTarget&);
Json query_market_speed(const ApiState&, const RequestTarget&);
Json query_market_instruments_route(const ApiState&, const RequestTarget&);
Json query_market_options_route(const ApiState&, const RequestTarget&);
Json query_market_option_expiry(const ApiState&, const RequestTarget&);
Json query_market_option_chain(const ApiState&, const RequestTarget&);
Json query_market_option_volatility(const ApiState&, const RequestTarget&);
void project_option_resource_paths(Json&, const std::filesystem::path& root);
Json query_market_expansion_timeline_route(const ApiState&, const RequestTarget&);
Json query_market_expansion_trades_route(const ApiState&, const RequestTarget&);
Json query_market_depth(const ApiState&, const RequestTarget&);
Json query_market_finance(const ApiState&, const RequestTarget&);
Json query_market_capital(const ApiState&, const RequestTarget&);
Json query_market_limits(const ApiState&, const RequestTarget&);
Json query_market_auction(const ApiState&, const RequestTarget&);
Json query_market_ranking(const ApiState&, const RequestTarget&);
Json query_market_limit_quality(const ApiState&, const RequestTarget&);
Json query_market_stats(const ApiState&, const RequestTarget&);
Json query_market_professional_route(const ApiState&, const RequestTarget&);
Json query_market_trades(const ApiState&, const RequestTarget&);

}  // namespace tdx::server_detail
