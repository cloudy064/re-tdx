#pragma once

#include "server_formula_internal.hpp"
#include "server_state_internal.hpp"

#include <map>
#include <optional>
#include <string>

namespace tdx::server_detail {

struct TqlexHttpQueryPlan {
    int page{};
    int page_size{};
    bool all_pages{};
    int requested_max_pages{};
    int effective_max_pages{};
};

TqlexHttpQueryPlan tqlex_http_query_plan(const RequestTarget& target);
Json query_tqlex_api(const ApiState& state, const RequestTarget& target);
Json query_pbrpc_api(const ApiState& state, const RequestTarget& target);
std::map<std::string, std::string> query_json_string_map(
    const RequestTarget& target, const std::string& prefix);
std::optional<HttpResponse> route_registered_market_api(
    const ApiState& state, const RequestTarget& target);

}  // namespace tdx::server_detail
