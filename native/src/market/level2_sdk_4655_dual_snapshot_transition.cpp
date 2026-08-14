#include "tdx/level2.hpp"

#include "tdx/common.hpp"
#include "level2_sdk_snapshot_digest.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace tdx {
namespace {

constexpr std::size_t companion_snapshot_size = 46;
constexpr std::size_t raw_header_size = 39;
constexpr std::size_t raw_record_size = 18;
constexpr std::size_t attach_block_size = 120;
constexpr std::size_t dispatcher_minimum_raw_size = 57;
constexpr std::size_t maximum_snapshot_size = level2_offline_payload_limit;

std::int16_t signed_i16_le(const std::uint8_t* data) {
    const auto raw = static_cast<std::uint16_t>(data[0]) |
                     (static_cast<std::uint16_t>(data[1]) << 8U);
    if (raw <= static_cast<std::uint16_t>(
                   std::numeric_limits<std::int16_t>::max()))
        return static_cast<std::int16_t>(raw);
    return static_cast<std::int16_t>(
        static_cast<std::int32_t>(raw) - 0x10000);
}

int signed_i8(std::uint8_t raw) {
    return raw <= 0x7fU ? static_cast<int>(raw)
                        : static_cast<int>(raw) - 0x100;
}

Json snapshot_metadata(const Bytes& value, const char* representation) {
    Json result = Json::object();
    result["representation"] = representation;
    result["byte_size"] = static_cast<std::uint64_t>(value.size());
    result["sha256"] = level2_detail::level2_snapshot_sha256(value);
    result["body_emitted"] = false;
    return result;
}

Bytes byte_range(const Bytes& value, std::size_t offset, std::size_t size) {
    return Bytes(value.begin() + static_cast<std::ptrdiff_t>(offset),
                 value.begin() + static_cast<std::ptrdiff_t>(offset + size));
}

void add_offline_boundary(Json& result) {
    result["input_body_retained"] = false;
    result["companion_body_emitted"] = false;
    result["raw_body_emitted"] = false;
    result["raw_to_companion_conversion_performed"] = false;
    result["callback_executed"] = false;
    result["sdk_callback_invoked"] = false;
    result["sdk_callback_executed"] = false;
    result["sdk_called"] = false;
    result["host_state_gate_evaluated"] = false;
    result["host_state_write_performed"] = false;
    result["host_date_formatting_executed"] = false;
    result["host_notification_executed"] = false;
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

Json project_level2_sdk_4655_companion_raw_transition(
    const Level2Sdk4655CompanionRawTransitionRequest& request) {
    if (request.companion_snapshot.size() != companion_snapshot_size)
        throw Error(
            "SDK 4655 companion/raw transition companion input must be "
            "exactly 46 bytes");
    if (request.raw.size() > maximum_snapshot_size)
        throw Error(
            "SDK 4655 dual snapshot transition raw input exceeds the "
            "384 KiB offline safety limit");
    if (request.raw.size() < dispatcher_minimum_raw_size)
        throw Error(
            "SDK 4655 dispatcher-qualified raw input must contain at least "
            "57 bytes");

    const int attach_flag_raw = signed_i8(request.raw[28]);
    const int count_raw = signed_i16_le(request.raw.data() + 29);
    const auto expected_size =
        static_cast<std::int64_t>(raw_header_size) +
        static_cast<std::int64_t>(raw_record_size) * count_raw +
        static_cast<std::int64_t>(attach_block_size) * attach_flag_raw;
    if (expected_size < 0 ||
        expected_size != static_cast<std::int64_t>(request.raw.size()))
        throw Error(
            "SDK 4655 raw input size must equal "
            "39 + 18*count_i16 + 120*attach_flag_i8");

    Json attach = Json::object();
    attach["flag_raw_signed_i8"] = attach_flag_raw;
    attach["copy_condition"] = "flag_raw_signed_i8 == 1";
    attach["present"] = attach_flag_raw == 1;
    if (attach_flag_raw == 1) {
        const auto signed_offset =
            static_cast<std::int64_t>(raw_header_size) +
            static_cast<std::int64_t>(raw_record_size) * count_raw;
        if (signed_offset < 0)
            throw Error(
                "SDK 4655 attach offset precedes the explicit raw buffer");
        const auto offset = static_cast<std::size_t>(signed_offset);
        const auto bytes = byte_range(request.raw, offset, attach_block_size);
        attach["offset"] = static_cast<std::uint64_t>(offset);
        attach["byte_size"] = static_cast<std::uint64_t>(attach_block_size);
        attach["sha256"] = level2_detail::level2_snapshot_sha256(bytes);
        attach["body_emitted"] = false;
    } else {
        attach["offset"] = Json(nullptr);
        attach["byte_size"] = 0;
        attach["sha256"] = Json(nullptr);
        attach["body_emitted"] = false;
    }

    Json records = Json::object();
    records["count_raw_signed_i16"] = count_raw;
    records["iteration_count_effective"] = count_raw > 0 ? count_raw : 0;
    records["stride"] = static_cast<std::uint64_t>(raw_record_size);
    records["offset"] = static_cast<std::uint64_t>(raw_header_size);
    records["iteration_order"] = "raw-source-index-ascending";
    records["iteration_scope"] =
        "native diagnostic date formatting only; not a decoded field projection";
    records["field_semantics_interpreted"] = false;
    records["records_emitted"] = false;

    Json candidate = Json::object();
    candidate["schema"] =
        "tdx-level2-sdk-4655-companion-raw-state-candidate-v1";
    candidate["conditional"] = true;
    candidate["companion_snapshot"] = snapshot_metadata(
        request.companion_snapshot,
        "opaque-explicit-caller-cloned-companion-snapshot-exact-46-bytes");
    candidate["raw_snapshot"] = snapshot_metadata(
        request.raw, "opaque-explicit-dispatcher-qualified-raw-snapshot");
    candidate["attach_snapshot"] = std::move(attach);
    candidate["ready_value_if_branch_executes"] = 1;
    candidate["body_emitted"] = false;

    Json gate = Json::object();
    gate["source"] = "sub_10079A02-entry-host-state-predicate";
    gate["inputs_available_in_request"] = false;
    gate["evaluated"] = false;
    gate["replacement_branch_selected"] = Json(nullptr);
    gate["request_does_not_guess_host_flags"] = true;

    Json native_only = Json::object();
    native_only["record_date_formatting_conditionally_present"] = true;
    native_only["record_date_formatting_executed"] = false;
    native_only["mode_2_subscription_or_notification_conditionally_present"] =
        true;
    native_only["subscription_or_notification_executed"] = false;

    Json result = Json::object();
    result["schema"] =
        "tdx-level2-sdk-4655-companion-raw-transition-v1";
    result["format"] = "sdk-4655-companion-raw-transition";
    result["normalization_kind"] =
        "offline-dispatcher-qualified-conditional-replacement-branch-projection";
    result["function_id"] = 4655;
    result["dispatcher_qualified"] = true;
    result["companion_exact_byte_size"] =
        static_cast<std::uint64_t>(companion_snapshot_size);
    result["companion_input_evidence_role"] =
        "caller-cloned prior local this+456 companion snapshot";
    result["companion_input_derived_from_raw"] = false;
    result["maximum_snapshot_byte_size"] =
        static_cast<std::uint64_t>(maximum_snapshot_size);
    result["raw_shape_expression"] =
        "39 + 18*count_i16 + 120*attach_flag_i8";
    result["raw_records"] = std::move(records);
    result["host_state_gate"] = std::move(gate);
    result["projected_state_candidate"] = std::move(candidate);
    result["native_host_only_branches"] = std::move(native_only);
    result["actual_host_replacement_claimed"] = false;
    result["sdk_json_adapter_semantics_claimed"] = false;
    result["evidence"] =
        "tpbus sub_1007CB4E, sub_10068065, and sub_10079A02";
    add_offline_boundary(result);
    return result;
}

}  // namespace tdx
