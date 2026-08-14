#pragma once

#include "server_state_internal.hpp"

#include "tdx/json.hpp"

#include <string>

namespace tdx::server_detail {

HttpResponse json_response(Json value, int status = 200,
                           std::string reason = "OK");
bool upstream_unavailable_error(const std::string& message);
HttpResponse upstream_unavailable_response(const std::string& message);

}  // namespace tdx::server_detail
