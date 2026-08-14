#include "tdx/level2.hpp"

#include "level2_sdk_snapshot_digest.hpp"

#include "tdx/common.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace tdx {
namespace {

constexpr std::size_t maximum_payload_size = level2_offline_payload_limit;

std::uint16_t read_u16_at(const Bytes& value, std::size_t offset) {
    return read_u16_le(value.data() + offset);
}

std::uint32_t read_u32_at(const Bytes& value, std::size_t offset) {
    return read_u32_le(value.data() + offset);
}

Json evidence_contract() {
    Json result = Json::object();
    result["batch_dispatcher"] = "sub_1007BFBE@0x1007BFBE";
    result["sequence_initializer"] = "sub_101A5AD0@0x101A5AD0";
    result["u16_length_reader"] = "sub_101A6060@0x101A6060";
    result["u32_reader"] = "sub_10066392@0x10066392";
    return result;
}

Json offline_boundary() {
    Json result = Json::object();
    result["input_body_retained"] = false;
    result["body_bytes_emitted"] = false;
    result["event_bus_accessed"] = false;
    result["host_state_read"] = false;
    result["host_state_write_performed"] = false;
    result["host_message_dispatch_attempted"] = false;
    result["host_messages_sent"] = 0;
    result["sdk_called"] = false;
    result["callback_executed"] = false;
    result["wire_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["authorization_accessed"] = false;
    result["entitlement_bypass"] = false;
    result["offline"] = true;
    return result;
}

Json safe_existing_summary(int mapped_type, const Bytes& body, int limit,
                           bool& available) {
    try {
        available = true;
        return decode_level2_document(
            mapped_type == 111 ? "tpbus-111" : "tpbus-112", body, limit);
    } catch (const Error&) {
        available = false;
        return Json(nullptr);
    }
}

}  // namespace

Json decode_level2_tpbus_115_batch(
    const Level2Tpbus115BatchDecodeRequest& request) {
    if (request.payload.size() > maximum_payload_size)
        throw Error("tpbus-115 input exceeds the 384 KiB offline safety limit");
    if (request.summary_limit < 0 || request.summary_limit > 10000)
        throw Error("tpbus-115 summary limit must be 0..10000");

    Json result = Json::object();
    result["schema"] = "tdx-level2-tpbus-115-batch-v1";
    result["format"] = "tpbus-115";
    result["push_type"] = 115;
    result["input_byte_size"] =
        static_cast<std::uint64_t>(request.payload.size());
    result["maximum_input_byte_size"] =
        static_cast<std::uint64_t>(maximum_payload_size);
    result["evidence"] = evidence_contract();
    result["framing"] =
        "first outer LE-u16-length segment; inner LE-u32 discriminator then "
        "LE-u16-length body";
    result["discriminator_mapping"] =
        "raw 0 or 1 maps to 111; every other u32 maps to 112";

    Json outer = Json::object();
    outer["first_segment_only"] = true;
    outer["declared_segment_byte_size"] = Json(nullptr);
    outer["segment_byte_size"] = 0;
    outer["consumed_byte_size"] = 0;
    outer["trailing_byte_size"] =
        static_cast<std::uint64_t>(request.payload.size());
    outer["trailing_bytes_ignored"] = false;

    Json inner = Json::object();
    inner["status"] = "not-entered";
    inner["stop_offset"] = 0;
    inner["consumed_byte_size"] = 0;
    inner["remaining_byte_size"] = 0;
    inner["complete_tuple_count"] = 0;
    inner["zero_body_skipped_count"] = 0;
    inner["dispatched_body_count"] = 0;
    Json entries = Json::array();

    if (request.payload.size() < 2) {
        outer["status"] = "length-prefix-truncated";
    } else {
        const auto declared = static_cast<std::size_t>(
            read_u16_at(request.payload, 0));
        outer["declared_segment_byte_size"] =
            static_cast<std::uint64_t>(declared);
        if (declared > request.payload.size() - 2) {
            outer["status"] = "first-segment-truncated";
        } else {
            const auto outer_consumed = 2U + declared;
            outer["segment_byte_size"] =
                static_cast<std::uint64_t>(declared);
            outer["consumed_byte_size"] =
                static_cast<std::uint64_t>(outer_consumed);
            outer["trailing_byte_size"] = static_cast<std::uint64_t>(
                request.payload.size() - outer_consumed);
            outer["trailing_bytes_ignored"] =
                request.payload.size() != outer_consumed;
            if (declared == 0) {
                outer["status"] = "zero-length-first-segment";
            } else {
                outer["status"] = "first-segment-ready";
                std::size_t cursor = 2;
                const auto end = outer_consumed;
                std::uint64_t complete_tuples = 0;
                std::uint64_t skipped = 0;
                std::uint64_t dispatched = 0;
                std::string status = "complete";
                while (cursor < end) {
                    const auto tuple_start = cursor;
                    if (end - cursor < 4) {
                        status = "discriminator-truncated";
                        break;
                    }
                    const auto discriminator =
                        read_u32_at(request.payload, cursor);
                    cursor += 4;
                    if (end - cursor < 2) {
                        status = "body-length-prefix-truncated";
                        break;
                    }
                    const auto body_size = static_cast<std::size_t>(
                        read_u16_at(request.payload, cursor));
                    if (body_size > end - cursor - 2) {
                        status = "body-truncated";
                        inner["pending_discriminator_raw"] =
                            static_cast<std::uint64_t>(discriminator);
                        inner["pending_declared_body_byte_size"] =
                            static_cast<std::uint64_t>(body_size);
                        break;
                    }
                    cursor += 2;
                    ++complete_tuples;
                    if (body_size == 0) {
                        ++skipped;
                        continue;
                    }

                    Bytes body(request.payload.begin() +
                                   static_cast<std::ptrdiff_t>(cursor),
                               request.payload.begin() +
                                   static_cast<std::ptrdiff_t>(cursor + body_size));
                    const int mapped_type =
                        discriminator == 0 || discriminator == 1 ? 111 : 112;
                    Json entry = Json::object();
                    entry["order"] = dispatched;
                    entry["tuple_index"] = complete_tuples - 1;
                    entry["tuple_offset"] = static_cast<std::uint64_t>(
                        tuple_start - 2);
                    entry["raw_discriminator"] =
                        static_cast<std::uint64_t>(discriminator);
                    entry["mapped_push_type"] = mapped_type;
                    entry["body_byte_size"] =
                        static_cast<std::uint64_t>(body_size);
                    entry["body_sha256"] =
                        level2_detail::level2_snapshot_sha256(body);
                    bool summary_available = false;
                    auto summary = safe_existing_summary(
                        mapped_type, body, request.summary_limit,
                        summary_available);
                    entry["summary_available"] = summary_available;
                    entry["summary"] = std::move(summary);
                    entries.push_back(std::move(entry));
                    ++dispatched;
                    cursor += body_size;
                }
                inner["status"] = status;
                inner["stop_offset"] =
                    static_cast<std::uint64_t>(cursor - 2);
                inner["consumed_byte_size"] =
                    static_cast<std::uint64_t>(cursor - 2);
                inner["remaining_byte_size"] =
                    static_cast<std::uint64_t>(end - cursor);
                inner["complete_tuple_count"] = complete_tuples;
                inner["zero_body_skipped_count"] = skipped;
                inner["dispatched_body_count"] = dispatched;
            }
        }
    }

    result["outer"] = std::move(outer);
    result["inner"] = std::move(inner);
    result["entries"] = std::move(entries);
    const auto boundary = offline_boundary();
    for (const auto& [key, value] : boundary.as_object()) result[key] = value;
    return result;
}

}  // namespace tdx
