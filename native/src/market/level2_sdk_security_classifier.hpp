#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace tdx::level2_detail {

struct Level2SdkSecurityClassification {
    int security_class_raw{};
    bool sub_594680_predicate_raw{};
};

// Reproduce the already-closed sub_5960B0/sub_594680 classifier pair. The
// caller supplies only an error-label prefix; it cannot alter classification.
Level2SdkSecurityClassification classify_level2_sdk_security(
    std::uint16_t market_id, const std::string& code,
    std::string_view contract_label);

}  // namespace tdx::level2_detail
