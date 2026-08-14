#pragma once

namespace tdx::cloud_endpoints {

// Default public TQLEX/PBRPC gateway. Callers may still provide an explicit
// endpoint, but the built-in default has one authoritative definition.
inline constexpr char tqlex[] = "http://static.tdx.com.cn:7615/TQLEX";

}  // namespace tdx::cloud_endpoints
