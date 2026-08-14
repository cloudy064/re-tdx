#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace tdx {
namespace {

constexpr std::size_t source_body_size = 32016;
constexpr std::size_t maximum_side_count = 1000;
constexpr std::size_t projected_record_size = 13;
constexpr std::size_t maximum_input_body_size = level2_offline_payload_limit;

static_assert(sizeof(float) == 4,
              "SDK 1803 depth projection requires IEEE-width f32");
static_assert(sizeof(double) == 8,
              "SDK 1803 depth projection requires IEEE-width f64");

struct SourceLayout {
    std::size_t count_offset;
    std::size_t price_offset;
    std::size_t volume_offset;
    std::size_t auxiliary_offset;
};

struct ProjectedRecord {
    std::size_t index{};
    double source_price{};
    std::uint64_t source_price_f64_bits{};
    float projected_price{};
    std::uint32_t projected_price_f32_bits{};
    std::uint32_t volume_raw{};
    std::uint16_t auxiliary_raw{};
    std::uint8_t top_volume_rank_raw{};
    std::array<std::uint8_t, projected_record_size> packed{};
};

std::uint64_t read_u64_le(const Bytes& body, std::size_t offset) {
    return static_cast<std::uint64_t>(
               read_u32_le(body.data() + offset)) |
           (static_cast<std::uint64_t>(
                read_u32_le(body.data() + offset + 4))
            << 32U);
}

double read_f64_le(const Bytes& body, std::size_t offset,
                   std::uint64_t& bits) {
    bits = read_u64_le(body, offset);
    double result{};
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

std::uint32_t f32_bits(float value) {
    std::uint32_t result{};
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

void put_u16(std::array<std::uint8_t, projected_record_size>& record,
             std::size_t offset, std::uint16_t value) {
    record[offset] = static_cast<std::uint8_t>(value);
    record[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

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

std::string u64_hex(std::uint64_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(16, '0');
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[result.size() - 1 - index] = digits[value & 0x0fU];
        value >>= 4U;
    }
    return result;
}

Json finite_number(double value) {
    return std::isfinite(value) ? Json(value) : Json(nullptr);
}

std::vector<ProjectedRecord> project_records(
    const Bytes& body, const SourceLayout& layout, std::size_t count) {
    std::vector<ProjectedRecord> records;
    records.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        ProjectedRecord record;
        record.index = index;
        record.source_price = read_f64_le(
            body, layout.price_offset + 8 * index,
            record.source_price_f64_bits);
        record.projected_price = static_cast<float>(record.source_price);
        record.projected_price_f32_bits = f32_bits(record.projected_price);
        record.volume_raw = read_u32_le(
            body.data() + layout.volume_offset + 4 * index);
        record.auxiliary_raw = read_u16_le(
            body.data() + layout.auxiliary_offset + 4 * index);

        put_u32(record.packed, 0, record.projected_price_f32_bits);
        put_u32(record.packed, 4, record.volume_raw);
        record.packed[8] = 0;
        put_u16(record.packed, 9, record.auxiliary_raw);
        record.packed[11] = 0;
        record.packed[12] = 0;
        records.push_back(record);
    }

    // sub_69B4D0 enters ranking only when the side count is positive.
    if (records.empty()) return records;

    // The native stack initialization is asymmetric: {-1, 0, 0}. Preserve
    // that observable fallback exactly instead of replacing it with a
    // conventional "no candidate" sentinel for all three ranks.
    std::array<std::ptrdiff_t, 3> selected{-1, 0, 0};
    for (std::size_t rank_index = 0; rank_index < selected.size();
         ++rank_index) {
        std::uint32_t best_volume = 0;
        auto best_index = selected[rank_index];
        for (std::size_t index = 0; index < records.size(); ++index) {
            bool already_selected = false;
            for (std::size_t prior = 0; prior < rank_index; ++prior)
                already_selected = already_selected ||
                    selected[prior] == static_cast<std::ptrdiff_t>(index);
            if (!already_selected &&
                records[index].volume_raw > best_volume) {
                best_volume = records[index].volume_raw;
                best_index = static_cast<std::ptrdiff_t>(index);
            }
        }
        selected[rank_index] = best_index;
        if (best_index >= 0) {
            const auto rank = static_cast<std::uint8_t>(rank_index + 1);
            const auto index = static_cast<std::size_t>(best_index);
            records[index].top_volume_rank_raw = rank;
            records[index].packed[11] = rank;
        }
    }
    return records;
}

Json records_json(const std::vector<ProjectedRecord>& records) {
    Json result = Json::array();
    for (const auto& record : records) {
        Json source = Json::object();
        source["price_f64"] = finite_number(record.source_price);
        source["price_f64_bits_hex"] =
            u64_hex(record.source_price_f64_bits);
        source["volume_raw"] =
            static_cast<std::uint64_t>(record.volume_raw);
        source["auxiliary_raw"] =
            static_cast<std::uint64_t>(record.auxiliary_raw);

        Json row = Json::object();
        row["index"] = static_cast<std::uint64_t>(record.index);
        row["source"] = std::move(source);
        row["price_f32"] = finite_number(record.projected_price);
        row["price_f32_raw_u32"] =
            static_cast<std::uint64_t>(record.projected_price_f32_bits);
        row["volume_raw"] =
            static_cast<std::uint64_t>(record.volume_raw);
        row["zero_u8_at_8_raw"] = 0;
        row["auxiliary_raw"] =
            static_cast<std::uint64_t>(record.auxiliary_raw);
        row["top_volume_rank_raw"] =
            static_cast<std::uint64_t>(record.top_volume_rank_raw);
        row["zero_u8_at_12_raw"] = 0;
        row["packed_hex"] = bytes_hex(record.packed);
        result.push_back(std::move(row));
    }
    return result;
}

Json side_json(const Bytes& body, const SourceLayout& layout,
               std::uint32_t count_raw, std::size_t count_effective) {
    const auto records = project_records(body, layout, count_effective);
    Json result = Json::object();
    result["count_raw"] = static_cast<std::uint64_t>(count_raw);
    result["count_effective"] =
        static_cast<std::uint64_t>(count_effective);
    result["clamped"] = count_raw > maximum_side_count;
    result["source_count_offset"] =
        static_cast<std::uint64_t>(layout.count_offset);
    result["source_price_offset"] =
        static_cast<std::uint64_t>(layout.price_offset);
    result["source_volume_offset"] =
        static_cast<std::uint64_t>(layout.volume_offset);
    result["source_auxiliary_offset"] =
        static_cast<std::uint64_t>(layout.auxiliary_offset);
    result["projected_record_count"] =
        static_cast<std::uint64_t>(records.size());
    result["projected_byte_size"] = static_cast<std::uint64_t>(
        records.size() * projected_record_size);
    result["records"] = records_json(records);
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

Json project_level2_sdk_1803_depth_record_projection(
    const Level2Sdk1803DepthRecordProjectionRequest& request) {
    if (request.body.size() > maximum_input_body_size)
        throw Error(
            "SDK 1803 depth record projection body exceeds the 384 KiB "
            "offline safety limit");
    if (request.body.size() != source_body_size)
        throw Error(
            "SDK 1803 depth record projection requires exactly 32016 body bytes");

    constexpr SourceLayout first_layout{8, 16, 8016, 12016};
    constexpr SourceLayout second_layout{12, 16016, 24016, 28016};
    const auto first_count_raw =
        read_u32_le(request.body.data() + first_layout.count_offset);
    const auto second_count_raw =
        read_u32_le(request.body.data() + second_layout.count_offset);
    const auto first_count_effective = std::min<std::size_t>(
        first_count_raw, maximum_side_count);
    const auto second_count_effective = std::min<std::size_t>(
        second_count_raw, maximum_side_count);

    Json state = Json::object();
    state["schema"] =
        "tdx-level2-sdk-1803-depth-record-state-v1";
    state["storage"] = "independent-first-second-13-byte-record-arrays";
    state["projected_record_size"] =
        static_cast<std::uint64_t>(projected_record_size);
    state["projected_record_count"] = static_cast<std::uint64_t>(
        first_count_effective + second_count_effective);
    state["projected_byte_size"] = static_cast<std::uint64_t>(
        (first_count_effective + second_count_effective) *
        projected_record_size);
    state["first"] = side_json(request.body, first_layout, first_count_raw,
                                first_count_effective);
    state["second"] = side_json(request.body, second_layout, second_count_raw,
                                 second_count_effective);
    state["side_naming"] =
        "first/second are retained raw; buy/sell direction is not inferred";

    Json result = Json::object();
    result["schema"] =
        "tdx-level2-sdk-1803-depth-record-projection-v1";
    result["format"] = "sdk-1803-depth-record-projection";
    result["normalization_kind"] =
        "offline-sub_69B4D0-sdk-1803-13-byte-record-projection";
    result["data_type"] = 1803;
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
        "u32 first/second counts are independently clamped to 1000 before "
        "record projection";
    result["ranking_policy"] =
        "each side independently assigns ranks 1..3 by unsigned volume; "
        "strict greater-than keeps earliest ties; native candidate slots "
        "initialize as -1/0/0, so an absent second or third positive candidate "
        "falls back to record zero";
    result["evidence"] = "TdxW sub_69B4D0 SDK branch";
    add_offline_boundary(result);
    return result;
}

}  // namespace tdx
