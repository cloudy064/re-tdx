#include "tdx/level2.hpp"

#include "tdx/common.hpp"
#include "level2_sdk_snapshot_digest.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace tdx {
namespace {

constexpr std::size_t caller_gate_minimum_decoded_size = 6;
constexpr std::uint32_t caller_gate_value = 0xffffffffU;
constexpr std::size_t maximum_snapshot_size = level2_offline_payload_limit;
constexpr std::uint64_t maximum_previous_raw_logical_size =
    static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());

bool is_hex_digit(unsigned char value) {
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

std::string canonical_sha256(const std::string& value, const char* field) {
    if (value.size() != 64 ||
        !std::all_of(value.begin(), value.end(), is_hex_digit))
        throw Error(
            std::string("SDK 4651 previous ") + field +
            " must contain exactly 64 hexadecimal digits");
    return lower_ascii(value);
}

Json candidate_snapshot(const Bytes& value, const char* representation) {
    Json result = Json::object();
    result["representation"] = representation;
    result["byte_size"] = static_cast<std::uint64_t>(value.size());
    result["sha256"] = level2_detail::level2_snapshot_sha256(value);
    result["body_emitted"] = false;
    return result;
}

Json complete_state(std::uint64_t decoded_byte_size,
                    const std::string& decoded_sha256,
                    std::uint64_t raw_byte_size,
                    const std::string& raw_sha256, bool ready,
                    const char* source) {
    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-4651-dual-snapshot-state-v1";
    result["decoded_byte_size"] = decoded_byte_size;
    result["decoded_sha256"] = decoded_sha256;
    result["raw_byte_size"] = raw_byte_size;
    result["raw_sha256"] = raw_sha256;
    result["ready"] = ready;
    result["source"] = source;
    result["metadata_complete"] = true;
    result["bodies_emitted"] = false;
    return result;
}

void validate_snapshot_size(std::size_t size, const char* kind) {
    if (size == 0)
        throw Error(
            std::string("SDK 4651 dual snapshot transition ") + kind +
            " input must be non-empty");
    if (size > maximum_snapshot_size)
        throw Error(
            std::string("SDK 4651 dual snapshot transition ") + kind +
            " input exceeds the 384 KiB offline safety limit");
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

Json project_level2_sdk_4651_dual_snapshot_transition(
    const Level2Sdk4651DualSnapshotTransitionRequest& request) {
    validate_snapshot_size(request.decoded.size(), "decoded");
    validate_snapshot_size(request.raw.size(), "raw");
    if (request.decoded.size() < caller_gate_minimum_decoded_size)
        throw Error(
            "SDK 4651 explicit decoded input must contain at least 6 bytes "
            "for the recovered caller gate");
    if (read_u32_le(request.decoded.data() + 2) != caller_gate_value)
        throw Error(
            "SDK 4651 explicit decoded input does not satisfy the recovered "
            "caller gate: little-endian u32 at offset 2 must be 0xffffffff");

    std::uint64_t previous_raw_size = 0;
    Json previous = Json::object();
    Json projected_previous = Json::object();
    previous["provided"] = request.previous.has_value();
    if (request.previous) {
        const auto& value = *request.previous;
        if (value.raw_byte_size > maximum_previous_raw_logical_size ||
            value.decoded_byte_size > maximum_previous_raw_logical_size)
            throw Error(
                "SDK 4651 previous decoded and raw logical sizes must be "
                "0..INT32_MAX");
        const auto raw_sha256 = canonical_sha256(
            value.raw_sha256, "raw_sha256");
        const auto decoded_sha256 = canonical_sha256(
            value.decoded_sha256, "decoded_sha256");
        previous_raw_size = value.raw_byte_size;
        projected_previous = complete_state(
            value.decoded_byte_size, decoded_sha256,
            value.raw_byte_size, raw_sha256, value.ready,
            "explicit-previous-metadata");
        previous = projected_previous;
        previous["provided"] = true;
    } else {
        previous["schema"] =
            "tdx-level2-sdk-4651-dual-snapshot-state-v1";
        previous["raw_byte_size"] = 0;
        previous["logical_size_defaulted_to_zero"] = true;
        previous["capacity_read"] = false;
        previous["metadata_complete"] = false;
        previous["ready"] = false;
    }

    const auto decoded_sha256 =
        level2_detail::level2_snapshot_sha256(request.decoded);
    const auto raw_sha256 =
        level2_detail::level2_snapshot_sha256(request.raw);
    const auto new_raw_size = static_cast<std::uint64_t>(request.raw.size());
    const bool replaced = previous_raw_size == 0 ||
                          new_raw_size >= previous_raw_size;

    Json candidate = Json::object();
    candidate["decoded_snapshot"] = candidate_snapshot(
        request.decoded, "opaque-explicit-decoded-snapshot");
    candidate["raw_snapshot"] = candidate_snapshot(
        request.raw, "opaque-explicit-raw-snapshot");
    candidate["ready_if_replaced"] = true;

    Json projected = replaced
        ? complete_state(
              static_cast<std::uint64_t>(request.decoded.size()),
              decoded_sha256, new_raw_size, raw_sha256, true,
              "new-explicit-snapshots")
        : projected_previous;

    Json gate = Json::object();
    gate["source"] = "explicit-decoded-snapshot-only";
    gate["minimum_decoded_byte_size"] =
        static_cast<std::uint64_t>(caller_gate_minimum_decoded_size);
    gate["u32_little_endian_offset"] = 2;
    gate["required_u32"] = static_cast<std::uint64_t>(caller_gate_value);
    gate["passed"] = true;
    gate["inferred_from_raw"] = false;

    Json decision = Json::object();
    decision["predicate"] =
        "previous_raw_logical_size == 0 || new_raw_size >= "
        "previous_raw_logical_size";
    decision["previous_raw_logical_size"] = previous_raw_size;
    decision["new_raw_size"] = new_raw_size;
    decision["previous_raw_capacity_read"] = false;
    decision["replaced"] = replaced;
    decision["retained"] = !replaced;
    decision["result"] = replaced ? "replace" : "retain-previous";

    Json result = Json::object();
    result["schema"] =
        "tdx-level2-sdk-4651-dual-snapshot-transition-v1";
    result["format"] = "sdk-4651-dual-snapshot-transition";
    result["normalization_kind"] =
        "offline-explicit-opaque-dual-snapshot-size-transition";
    result["function_id"] = 4651;
    result["maximum_snapshot_byte_size"] =
        static_cast<std::uint64_t>(maximum_snapshot_size);
    result["maximum_previous_raw_logical_size"] =
        maximum_previous_raw_logical_size;
    result["decoded_snapshot_shape"] =
        "opaque-bounded-nonempty-caller-gated";
    result["decoded_exact_length_proven"] = false;
    result["dispatcher_qualified_input"] = true;
    result["caller_gate"] = std::move(gate);
    result["candidate"] = std::move(candidate);
    result["previous_state"] = std::move(previous);
    result["decision"] = std::move(decision);
    result["projected_state"] = std::move(projected);
    result["evidence"] =
        "tpbus sub_100675A0 raw logical-size compare and conditional dual copy";
    add_offline_boundary(result);
    return result;
}

}  // namespace tdx
