#pragma once

#include "server_formula_internal.hpp"
#include "server_state_internal.hpp"

namespace tdx::server_detail {

Json query_market_institution_lhb(const ApiState&, const RequestTarget&);
Json query_market_active_lhb(const ApiState&, const RequestTarget&);
Json query_market_state_owned_reform(const ApiState&, const RequestTarget&);
Json query_market_ratings(const ApiState&, const RequestTarget&);
Json query_market_foreign_alerts(const ApiState&, const RequestTarget&);
Json query_holder(const ApiState&, const RequestTarget&);

}  // namespace tdx::server_detail
