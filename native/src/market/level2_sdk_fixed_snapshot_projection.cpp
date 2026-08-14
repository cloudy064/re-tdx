#include "tdx/level2.hpp"

#include "tdx/common.hpp"
#include "level2_sdk_snapshot_digest.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace tdx {
namespace {

constexpr std::size_t maximum_body_size = level2_offline_payload_limit;

struct SnapshotContract {
    int data_type;
    const char* callback_kind;
    const char* format;
    const char* schema;
    std::size_t body_size;
};

SnapshotContract snapshot_contract(Level2SdkHostSnapshotDataType data_type) {
    switch (data_type) {
    case Level2SdkHostSnapshotDataType::multi_level_quote:
        return {1803, "multi-level-quote", "sdk-1803-host-projection",
                "tdx-level2-sdk-1803-host-snapshot-replacement-v1", 32016};
    case Level2SdkHostSnapshotDataType::order_queue_at_price:
        return {18031, "order-queue-at-price",
                "sdk-18031-host-projection",
                "tdx-level2-sdk-18031-host-snapshot-replacement-v1", 20012};
    }
    throw Error(
        "SDK fixed host snapshot projection data-type must be 1803 or 18031");
}

void add_offline_boundary(Json& result) {
    result["input_body_retained"] = false;
    result["raw_body_emitted"] = false;
    result["body_transformed"] = false;
    result["callback_executed"] = false;
    result["sdk_callback_invoked"] = false;
    result["sdk_callback_executed"] = false;
    result["sdk_called"] = false;
    result["host_storage_call_attempted"] = false;
    result["host_state_write_performed"] = false;
    result["host_message_dispatch_attempted"] = false;
    result["host_messages_sent"] = 0;
    result["wire_bytes_built"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
}

}  // namespace

Json project_level2_sdk_host_snapshot_replacement(
    const Level2SdkHostSnapshotReplacementRequest& request) {
    const auto contract = snapshot_contract(request.data_type);
    if (request.body.size() > maximum_body_size)
        throw Error(
            "SDK fixed host snapshot projection body exceeds the 384 KiB "
            "offline safety limit");
    if (request.body.size() != contract.body_size)
        throw Error(
            "SDK " + std::to_string(contract.data_type) +
            " host snapshot projection requires exactly " +
            std::to_string(contract.body_size) + " body bytes");

    const auto digest =
        level2_detail::level2_snapshot_sha256(request.body);

    Json prior = Json::object();
    prior["input_required"] = false;
    prior["read"] = false;
    prior["retained"] = false;
    prior["disposition"] = "replaced-in-full";

    Json next = Json::object();
    next["representation"] = "exact-callback-body-byte-snapshot";
    next["byte_size"] = static_cast<std::uint64_t>(request.body.size());
    next["sha256"] = digest;
    next["body_emitted"] = false;

    Json replacement = Json::object();
    replacement["scope"] = "complete-fixed-size-logical-state";
    replacement["source_copy_semantics"] = "byte-for-byte-without-field-transform";
    replacement["projected"] = true;
    replacement["performed_against_host"] = false;
    replacement["previous_logical_state"] = std::move(prior);
    replacement["new_logical_state"] = std::move(next);

    Json result = Json::object();
    result["schema"] = contract.schema;
    result["format"] = contract.format;
    result["normalization_kind"] =
        "offline-exact-fixed-body-state-replacement";
    result["data_type"] = contract.data_type;
    result["callback_kind"] = contract.callback_kind;
    result["source_body_size"] =
        static_cast<std::uint64_t>(request.body.size());
    result["projected_logical_state_size"] =
        static_cast<std::uint64_t>(request.body.size());
    result["projected_logical_state_sha256"] = digest;
    result["maximum_input_body_size"] =
        static_cast<std::uint64_t>(maximum_body_size);
    result["replacement"] = std::move(replacement);
    result["field_projection_performed"] = false;
    result["decoded_summary_included"] = false;
    result["evidence"] =
        "recovered TdxW fixed-body callback branch and exact copy byte count";
    add_offline_boundary(result);
    return result;
}

}  // namespace tdx
