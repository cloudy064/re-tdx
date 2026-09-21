#include "level2_sdk_snapshot_digest.hpp"

#include "tdx/common.hpp"

namespace tdx::level2_detail {

std::string level2_snapshot_sha256(const Bytes& value) {
    return sha256_bytes(value);
}

}  // namespace tdx::level2_detail
