#include "tdx/level2.hpp"

#include "level2_sdk_security_classifier.hpp"

#include "tdx/common.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace tdx {
namespace {

constexpr std::size_t source_body_size = 20012;
constexpr std::size_t source_count_offset = 8;
constexpr std::size_t source_quantity_offset = 12;
constexpr std::size_t maximum_record_count = 5000;
constexpr std::size_t projected_record_size = 6;
constexpr std::size_t maximum_input_body_size = level2_offline_payload_limit;

void put_u32(std::array<std::uint8_t, projected_record_size>& record,
             std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index)
        record[offset + index] =
            static_cast<std::uint8_t>(value >> (8U * index));
}

std::string bytes_hex(
    const std::array<std::uint8_t, projected_record_size>& record) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(record.size() * 2);
    for (const auto value : record) {
        result.push_back(digits[value >> 4U]);
        result.push_back(digits[value & 0x0fU]);
    }
    return result;
}

void add_offline_boundary(Json& result) {
    result["input_body_retained"] = false;
    result["raw_body_emitted"] = false;
    result["previous_state_read"] = false;
    result["sdk_called"] = false;
    result["sdk_callback_invoked"] = false;
    result["callback_executed"] = false;
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

Json project_level2_sdk_18031_queue_record_projection(
    const Level2Sdk18031QueueRecordProjectionRequest& request) {
    if (request.body.size() > maximum_input_body_size)
        throw Error(
            "SDK 18031 queue record projection body exceeds the 384 KiB "
            "offline safety limit");
    if (request.body.size() != source_body_size)
        throw Error(
            "SDK 18031 queue record projection requires exactly 20012 body bytes");

    const auto classification =
        level2_detail::classify_level2_sdk_security(
            request.market_id, request.code,
            "SDK 18031 queue record projection");
    const auto count_raw =
        read_u32_le(request.body.data() + source_count_offset);
    const auto count_effective = std::min<std::size_t>(
        count_raw, maximum_record_count);
    const std::uint32_t quantity_divisor_raw =
        classification.sub_594680_predicate_raw ? 10U : 1U;

    Json records = Json::array();
    for (std::size_t index = 0; index < count_effective; ++index) {
        const auto source_quantity_raw = read_u32_le(
            request.body.data() + source_quantity_offset + 4 * index);
        const auto projected_quantity_raw =
            source_quantity_raw / quantity_divisor_raw;
        std::array<std::uint8_t, projected_record_size> packed{};
        put_u32(packed, 2, projected_quantity_raw);

        Json row = Json::object();
        row["index"] = static_cast<std::uint64_t>(index);
        row["source_quantity_raw"] =
            static_cast<std::uint64_t>(source_quantity_raw);
        row["zero_u16_at_0_raw"] = 0;
        row["projected_quantity_raw"] =
            static_cast<std::uint64_t>(projected_quantity_raw);
        row["packed_hex"] = bytes_hex(packed);
        records.push_back(std::move(row));
    }

    Json first_raw = Json::object();
    first_raw["count_raw"] = static_cast<std::uint64_t>(count_raw);
    first_raw["count_effective"] =
        static_cast<std::uint64_t>(count_effective);
    first_raw["clamped"] = count_raw > maximum_record_count;
    first_raw["source_count_offset"] =
        static_cast<std::uint64_t>(source_count_offset);
    first_raw["source_quantity_offset"] =
        static_cast<std::uint64_t>(source_quantity_offset);
    first_raw["projected_record_count"] =
        static_cast<std::uint64_t>(count_effective);
    first_raw["projected_byte_size"] = static_cast<std::uint64_t>(
        count_effective * projected_record_size);
    first_raw["records"] = std::move(records);

    Json state = Json::object();
    state["schema"] =
        "tdx-level2-sdk-18031-queue-record-state-v1";
    state["storage"] = "contiguous-6-byte-records";
    state["projected_record_size"] =
        static_cast<std::uint64_t>(projected_record_size);
    state["first_raw"] = std::move(first_raw);
    state["second_output_raw"] = 0;
    state["field_naming"] =
        "first_raw/second_output_raw retain evidence-bound output order; "
        "no buy/sell direction is inferred";

    Json result = Json::object();
    result["schema"] =
        "tdx-level2-sdk-18031-queue-record-projection-v1";
    result["format"] = "sdk-18031-queue-record-projection";
    result["normalization_kind"] =
        "offline-sub_69B8E0-sdk-18031-6-byte-record-projection";
    result["data_type"] = 18031;
    result["market_raw"] =
        static_cast<std::uint64_t>(request.market_id);
    result["code_ascii"] = request.code;
    result["security_class_raw"] =
        classification.security_class_raw;
    result["sub_594680_predicate_raw"] =
        classification.sub_594680_predicate_raw;
    result["quantity_divisor_raw"] =
        static_cast<std::uint64_t>(quantity_divisor_raw);
    result["source_body_size"] =
        static_cast<std::uint64_t>(request.body.size());
    result["maximum_input_body_size"] =
        static_cast<std::uint64_t>(maximum_input_body_size);
    result["source_header_u32_0_raw"] = static_cast<std::uint64_t>(
        read_u32_le(request.body.data()));
    result["source_header_u32_1_raw"] = static_cast<std::uint64_t>(
        read_u32_le(request.body.data() + 4));
    result["projected_record_size"] =
        static_cast<std::uint64_t>(projected_record_size);
    result["field_projection_performed"] = true;
    result["projected_state"] = std::move(state);
    result["count_policy"] =
        "u32 count is clamped to 5000 before record projection";
    result["quantity_policy"] =
        "u32 quantity divided by 10 when sub_594680 is true, otherwise "
        "retained; division truncates toward zero";
    result["evidence"] =
        "TdxW sub_69B8E0 SDK branch, sub_5960B0, and sub_594680";
    add_offline_boundary(result);
    return result;
}

}  // namespace tdx
