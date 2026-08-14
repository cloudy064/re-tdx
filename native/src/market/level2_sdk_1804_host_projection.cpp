#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

namespace tdx {
namespace {

constexpr std::size_t source_body_size = 432;
constexpr std::size_t host_slot_count = 105;
constexpr std::size_t host_byte_size = host_slot_count * sizeof(float);
constexpr std::size_t maximum_side_quantity_count = 50;
static_assert(sizeof(float) == 4, "SDK 1804 host projection requires f32");
static_assert(sizeof(double) == 8, "SDK 1804 source projection requires f64");

double read_f64_le(const Bytes& body, std::size_t offset) {
    const auto bits = static_cast<std::uint64_t>(
                          read_u32_le(body.data() + offset)) |
                      (static_cast<std::uint64_t>(
                           read_u32_le(body.data() + offset + 4))
                       << 32U);
    double result{};
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

std::uint32_t f32_bits(float value) {
    std::uint32_t result{};
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

float f32_from_bits(std::uint32_t value) {
    float result{};
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

Json finite_f32(std::uint32_t bits) {
    const auto value = f32_from_bits(bits);
    return std::isfinite(value) ? Json(static_cast<double>(value))
                                : Json(nullptr);
}

Json side_projection(const Bytes& body,
                     const std::array<std::uint32_t, host_slot_count>& slots,
                     std::size_t source_price_offset,
                     std::size_t source_quantity_offset,
                     std::size_t source_count_offset,
                     std::size_t destination_price_slot,
                     std::size_t destination_count_slot,
                     std::size_t destination_quantity_slot) {
    const auto count_raw = read_u32_le(body.data() + source_count_offset);
    const auto copied_count = std::min<std::size_t>(
        count_raw, maximum_side_quantity_count);

    Json quantities = Json::array();
    Json quantity_bits = Json::array();
    for (std::size_t index = 0; index < copied_count; ++index) {
        const auto bits = slots[destination_quantity_slot + index];
        quantities.push_back(finite_f32(bits));
        quantity_bits.push_back(static_cast<std::uint64_t>(bits));
    }

    Json result = Json::object();
    result["source_price_offset"] =
        static_cast<std::uint64_t>(source_price_offset);
    result["source_quantity_offset"] =
        static_cast<std::uint64_t>(source_quantity_offset);
    result["source_count_offset"] =
        static_cast<std::uint64_t>(source_count_offset);
    result["destination_price_slot"] =
        static_cast<std::uint64_t>(destination_price_slot);
    result["destination_count_slot"] =
        static_cast<std::uint64_t>(destination_count_slot);
    result["destination_quantity_slot_start"] =
        static_cast<std::uint64_t>(destination_quantity_slot);
    result["price"] = finite_f32(slots[destination_price_slot]);
    result["price_f32_raw_u32"] =
        static_cast<std::uint64_t>(slots[destination_price_slot]);
    result["count_raw"] = static_cast<std::uint64_t>(count_raw);
    result["copied_count"] = static_cast<std::uint64_t>(copied_count);
    result["count_clamped_to_50"] =
        count_raw > maximum_side_quantity_count;
    result["quantity_f32"] = std::move(quantities);
    result["quantity_f32_raw_u32"] = std::move(quantity_bits);
    return result;
}

void add_offline_boundary(Json& result) {
    result["input_body_retained"] = false;
    result["sdk_called"] = false;
    result["sdk_callback_invoked"] = false;
    result["callback_executed"] = false;
    result["host_storage_call_attempted"] = false;
    result["sub_525600_called"] = false;
    result["host_message_dispatch_attempted"] = false;
    result["host_messages_sent"] = 0;
    result["wire_bytes_built"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_sent"] = false;
    result["offline"] = true;
    result["entitlement_bypass"] = false;
}

}  // namespace

Json project_level2_sdk_1804_host_projection(
    const Level2Sdk1804HostProjectionRequest& request) {
    if (request.body.size() != source_body_size)
        throw Error(
            "SDK 1804 host projection requires exactly 432 body bytes");

    std::array<std::uint32_t, host_slot_count> slots{};
    const auto first_price = static_cast<float>(read_f64_le(request.body, 8));
    const auto second_price =
        static_cast<float>(read_f64_le(request.body, 216));
    slots[1] = f32_bits(first_price);
    slots[2] = f32_bits(second_price);

    const auto first_count_raw = read_u32_le(request.body.data() + 424);
    const auto second_count_raw = read_u32_le(request.body.data() + 428);
    slots[3] = first_count_raw;
    slots[4] = second_count_raw;
    const auto first_copied_count = std::min<std::size_t>(
        first_count_raw, maximum_side_quantity_count);
    const auto second_copied_count = std::min<std::size_t>(
        second_count_raw, maximum_side_quantity_count);
    for (std::size_t index = 0; index < first_copied_count; ++index)
        slots[5 + index] = read_u32_le(request.body.data() + 16 + 4 * index);
    for (std::size_t index = 0; index < second_copied_count; ++index)
        slots[55 + index] = read_u32_le(request.body.data() + 224 + 4 * index);

    Json raw_slots = Json::array();
    for (const auto slot : slots)
        raw_slots.push_back(static_cast<std::uint64_t>(slot));

    Json state = Json::object();
    state["schema"] = "tdx-level2-sdk-1804-host-state-v1";
    state["storage"] = "105-f32-slots";
    state["initialization"] =
        "all-105-f32-slots-zeroed-before-field-copy";
    state["slot_encoding"] = "raw-u32-bits-of-each-f32-slot";
    state["slot_count"] = static_cast<std::uint64_t>(host_slot_count);
    state["byte_size"] = static_cast<std::uint64_t>(host_byte_size);
    state["slots_raw_u32"] = std::move(raw_slots);
    state["first"] = side_projection(
        request.body, slots, 8, 16, 424, 1, 3, 5);
    state["second"] = side_projection(
        request.body, slots, 216, 224, 428, 2, 4, 55);
    state["side_semantics"] =
        "first/second preserved; buy/sell direction is not inferred";

    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-1804-host-projection-v1";
    result["format"] = "sdk-1804-host-projection";
    result["normalization_kind"] =
        "offline-sub_68C750-sdk-1804-host-slot-projection";
    result["data_type"] = 1804;
    result["source_body_size"] =
        static_cast<std::uint64_t>(source_body_size);
    result["projected_host_slot_count"] =
        static_cast<std::uint64_t>(host_slot_count);
    result["projected_host_byte_size"] =
        static_cast<std::uint64_t>(host_byte_size);
    result["projected_state"] = std::move(state);
    result["count_policy"] =
        "raw u32 bits retained in slots 3/4; copied quantities clamp to 50";
    result["evidence"] =
        "TdxW sub_68C750 SDK 1804 branch before sub_525600";
    add_offline_boundary(result);
    return result;
}

}  // namespace tdx
