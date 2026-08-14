#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"

namespace tdx::level2_detail {

Json decode_tcalc_order_flow_document(const Bytes& payload, int limit);
Json decode_tcalc_order_side_document(const Bytes& payload);

}  // namespace tdx::level2_detail
