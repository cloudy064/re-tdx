#pragma once

#include "tdx/json.hpp"

namespace tdx::server_detail {

// Browser-safe wrappers around the existing offline Level2 domain functions.
// Both accept JSON values only; neither accepts filesystem paths or opens a
// network/session boundary.
Json query_level2_build(const Json& body);
Json query_level2_decode(const Json& body);
Json query_level2_project(const Json& body);

}  // namespace tdx::server_detail
