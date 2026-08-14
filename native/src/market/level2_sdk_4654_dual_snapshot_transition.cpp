#include "tdx/level2.hpp"

#include "tdx/common.hpp"
#include "level2_sdk_snapshot_digest.hpp"

#include <cstddef>
#include <cstdint>

namespace tdx {
namespace {

constexpr std::size_t decoded_snapshot_size = 48;
constexpr std::size_t maximum_snapshot_size = level2_offline_payload_limit;

Json snapshot_metadata(const Bytes& value, const char* representation) {
    Json result = Json::object();
    result["representation"] = representation;
    result["byte_size"] = static_cast<std::uint64_t>(value.size());
    result["sha256"] = level2_detail::level2_snapshot_sha256(value);
    result["body_emitted"] = false;
    return result;
}

void add_offline_boundary(Json& result) {
    result["input_body_retained"] = false;
    result["decoded_body_emitted"] = false;
    result["raw_body_emitted"] = false;
    result["raw_to_decoded_conversion_performed"] = false;
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
    result["authorization_attempted"] = false;
    result["authorization_bypass_attempted"] = false;
    result["credentials_accessed"] = false;
    result["implicit_root_accessed"] = false;
    result["config_accessed"] = false;
    result["entitlement_bypass"] = false;
    result["offline"] = true;
}

}  // namespace

Json project_level2_sdk_4654_dual_snapshot_transition(
    const Level2Sdk4654DualSnapshotTransitionRequest& request) {
    if (request.decoded.size() != decoded_snapshot_size)
        throw Error(
            "SDK 4654 dual snapshot transition decoded input must be "
            "exactly 48 bytes");
    if (request.raw.empty())
        throw Error(
            "SDK 4654 dual snapshot transition raw input must be non-empty");
    if (request.raw.size() > maximum_snapshot_size)
        throw Error(
            "SDK 4654 dual snapshot transition raw input exceeds the "
            "384 KiB offline safety limit");

    Json previous = Json::object();
    previous["input_required"] = false;
    previous["read"] = false;
    previous["retained"] = false;
    previous["disposition"] = "unconditionally-replaced";

    Json projected = Json::object();
    projected["schema"] =
        "tdx-level2-sdk-4654-dual-snapshot-state-v1";
    projected["decoded_snapshot"] = snapshot_metadata(
        request.decoded, "opaque-upstream-decoder-output-exact-48-bytes");
    projected["raw_snapshot"] = snapshot_metadata(
        request.raw, "opaque-authorized-sdk-response-bytes");
    projected["ready"] = true;

    Json replacement = Json::object();
    replacement["decision"] = "replace";
    replacement["unconditional"] = true;
    replacement["replaced"] = true;
    replacement["retained"] = false;
    replacement["performed_against_host"] = false;
    replacement["previous_logical_state"] = std::move(previous);
    replacement["projected_logical_state"] = projected;

    Json result = Json::object();
    result["schema"] =
        "tdx-level2-sdk-4654-dual-snapshot-transition-v1";
    result["format"] = "sdk-4654-dual-snapshot-transition";
    result["normalization_kind"] =
        "offline-explicit-decoded-and-raw-dual-snapshot-replacement";
    result["function_id"] = 4654;
    result["decoded_exact_byte_size"] =
        static_cast<std::uint64_t>(decoded_snapshot_size);
    result["maximum_raw_byte_size"] =
        static_cast<std::uint64_t>(maximum_snapshot_size);
    result["decoded_snapshot_semantics"] =
        "explicit opaque upstream decoder output; never derived from raw";
    result["raw_snapshot_semantics"] =
        "explicit nonempty opaque authorized SDK response bytes from the "
        "qualified dispatcher call";
    result["dispatcher_qualified_input"] = true;
    result["replacement"] = std::move(replacement);
    result["projected_state"] = std::move(projected);
    result["evidence"] =
        "tpbus sub_1006743C exact decoded copy and unconditional raw copy";
    add_offline_boundary(result);
    return result;
}

}  // namespace tdx
