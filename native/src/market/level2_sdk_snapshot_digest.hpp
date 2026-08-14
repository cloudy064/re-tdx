#pragma once

#include "tdx/common.hpp"

#include <string>

namespace tdx::level2_detail {

// Internal-only digest support for offline snapshot metadata. It is not a
// common public hashing API and performs no storage or network operation.
std::string level2_snapshot_sha256(const Bytes& value);

}  // namespace tdx::level2_detail
