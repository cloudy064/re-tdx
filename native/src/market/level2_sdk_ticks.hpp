#pragma once

#include "tdx/common.hpp"
#include "tdx/json.hpp"

namespace tdx::level2_detail {

// Decode records delivered by the authorized Level2 SDK callback.  These are
// post-transport application records; this module never logs in or subscribes.
Json decode_sdk_transactions(const Bytes& payload, int limit);
Json decode_sdk_orders(const Bytes& payload, int limit);
Json decode_sdk_price_queues(const Bytes& payload, int limit);
Json decode_sdk_quote_updates(const Bytes& payload, int limit, int data_type);

}  // namespace tdx::level2_detail
