#include "level2_sdk_ticks.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace tdx::level2_detail {
namespace {

constexpr std::size_t transaction_record_size = 52;
constexpr std::size_t order_record_size = 40;
constexpr std::size_t price_queue_record_size = 432;
constexpr std::size_t quote_update_record_size = 380;

std::uint32_t u32(const Bytes& data, std::size_t offset) {
    return read_u32_le(data.data() + offset);
}

std::uint64_t u64(const Bytes& data, std::size_t offset) {
    std::uint64_t value{};
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

double f64(const Bytes& data, std::size_t offset) {
    double value{};
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

Json finite_number(double value) {
    return std::isfinite(value) ? Json(value) : Json(nullptr);
}

Json quote_levels(const Bytes& payload, std::size_t record_offset,
                  std::size_t price_offset, std::size_t quantity_offset) {
    Json levels = Json::array();
    for (std::size_t index = 0; index < 10; ++index) {
        Json level = Json::object();
        level["level"] = static_cast<std::uint64_t>(index + 1);
        level["price"] = finite_number(
            f64(payload, record_offset + price_offset + index * 8));
        level["quantity_raw"] = finite_number(read_f32_le(
            payload.data() + record_offset + quantity_offset + index * 4));
        levels.push_back(std::move(level));
    }
    return levels;
}

std::string transaction_side(std::uint32_t direction) {
    if (direction == 1) return "buy";
    if (direction == 2) return "sell";
    return direction == 0 ? "neutral" : "unknown";
}

std::string order_type(std::uint32_t type) {
    switch (type) {
    case 1: return "buy";
    case 2: return "sell";
    case 3: return "buy-cancel";
    case 4: return "sell-cancel";
    default: return "unknown";
    }
}

std::size_t checked_record_count(const Bytes& payload,
                                 std::size_t record_size,
                                 const char* name) {
    if (payload.empty() || payload.size() % record_size != 0)
        throw Error(std::string(name) + " payload must be a non-empty exact multiple of " +
                    std::to_string(record_size) + " bytes");
    return payload.size() / record_size;
}

}  // namespace

Json decode_sdk_transactions(const Bytes& payload, int limit) {
    const auto count = checked_record_count(
        payload, transaction_record_size, "SDK 1801 transaction");
    const auto selected = std::min<std::size_t>(
        count, static_cast<std::size_t>(limit));
    Json records = Json::array();
    for (std::size_t index = 0; index < selected; ++index) {
        const auto offset = index * transaction_record_size;
        const auto direction = u32(payload, offset + 48);
        Json row = Json::object();
        row["index"] = static_cast<std::uint64_t>(index);
        row["time_raw"] = u64(payload, offset);
        const double price = f64(payload, offset + 8);
        row["price"] = std::isfinite(price) ? Json(price) : Json(nullptr);
        row["volume_raw"] = u64(payload, offset + 16);
        row["auxiliary_1_raw"] =
            static_cast<std::uint64_t>(u32(payload, offset + 36));
        row["auxiliary_2_raw"] =
            static_cast<std::uint64_t>(u32(payload, offset + 40));
        row["direction_raw"] = static_cast<std::uint64_t>(direction);
        row["side"] = transaction_side(direction);
        records.push_back(std::move(row));
    }
    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-1801-v1";
    result["data_type"] = 1801;
    result["record_size"] =
        static_cast<std::uint64_t>(transaction_record_size);
    result["count"] = static_cast<std::uint64_t>(count);
    result["records"] = std::move(records);
    result["truncated"] = selected < count;
    result["batch_semantics"] = "counted-record-array";
    result["transport_scope"] =
        "authorized post-transport SDK callback records";
    return result;
}

Json decode_sdk_orders(const Bytes& payload, int limit) {
    const auto count = checked_record_count(
        payload, order_record_size, "SDK 1802 order");
    const auto selected = std::min<std::size_t>(
        count, static_cast<std::size_t>(limit));
    Json records = Json::array();
    for (std::size_t index = 0; index < selected; ++index) {
        const auto offset = index * order_record_size;
        const auto type = u32(payload, offset + 36);
        Json row = Json::object();
        row["index"] = static_cast<std::uint64_t>(index);
        row["time_raw"] = u64(payload, offset);
        const double price = f64(payload, offset + 8);
        row["price"] = std::isfinite(price) ? Json(price) : Json(nullptr);
        row["volume_raw"] = u64(payload, offset + 16);
        row["order_id_raw"] =
            static_cast<std::uint64_t>(u32(payload, offset + 24));
        row["record_type_raw"] = static_cast<std::uint64_t>(type);
        row["record_type"] = order_type(type);
        records.push_back(std::move(row));
    }
    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-1802-v1";
    result["data_type"] = 1802;
    result["record_size"] = static_cast<std::uint64_t>(order_record_size);
    result["count"] = static_cast<std::uint64_t>(count);
    result["records"] = std::move(records);
    result["truncated"] = selected < count;
    result["batch_semantics"] = "counted-record-array";
    result["transport_scope"] =
        "authorized post-transport SDK callback records";
    return result;
}

Json decode_sdk_price_queues(const Bytes& payload, int limit) {
    if (payload.size() != price_queue_record_size)
        throw Error("SDK 1804 price-queue callback requires exactly 432 bytes");
    const auto first_count = u32(payload, 424);
    const auto second_count = u32(payload, 428);
    if (first_count > 50 || second_count > 50)
        throw Error("SDK 1804 side count exceeds 50");
    std::size_t first_limit = std::min<std::size_t>(
        first_count, (static_cast<std::size_t>(limit) + 1) / 2);
    std::size_t second_limit = std::min<std::size_t>(
        second_count, static_cast<std::size_t>(limit) - first_limit);
    auto remaining = static_cast<std::size_t>(limit) - first_limit - second_limit;
    const auto add_first = std::min<std::size_t>(
        first_count - first_limit, remaining);
    first_limit += add_first;
    remaining -= add_first;
    second_limit += std::min<std::size_t>(
        second_count - second_limit, remaining);
    const auto quantities = [&](std::size_t count, std::size_t offset) {
        Json result = Json::array();
        for (std::size_t index = 0; index < count; ++index) {
            const double value = read_f32_le(payload.data() + offset + index * 4);
            result.push_back(std::isfinite(value) ? Json(value) : Json(nullptr));
        }
        return result;
    };
    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-1804-v1";
    result["data_type"] = 1804;
    result["record_size"] =
        static_cast<std::uint64_t>(price_queue_record_size);
    result["body_count"] = 1;
    result["batch_semantics"] = "single-fixed-body";
    const double first_price = f64(payload, 8);
    const double second_price = f64(payload, 216);
    result["first_price"] =
        std::isfinite(first_price) ? Json(first_price) : Json(nullptr);
    result["second_price"] =
        std::isfinite(second_price) ? Json(second_price) : Json(nullptr);
    result["first_count"] = static_cast<std::uint64_t>(first_count);
    result["second_count"] = static_cast<std::uint64_t>(second_count);
    result["first_quantities_raw"] = quantities(first_limit, 16);
    result["second_quantities_raw"] = quantities(second_limit, 224);
    result["truncated"] = first_limit < first_count || second_limit < second_count;
    result["side_semantics"] =
        "first/second preserved; buy/sell direction requires an authorized sample";
    result["transport_scope"] =
        "authorized post-transport SDK callback record";
    return result;
}

Json decode_sdk_quote_updates(const Bytes& payload, int limit, int data_type) {
    if (data_type != 1807 && data_type != 18071)
        throw Error("SDK quote update data type must be 1807 or 18071");
    if (payload.size() != quote_update_record_size)
        throw Error("SDK 1807/18071 quote-update callback requires exactly 380 bytes");
    constexpr std::size_t count = 1;
    const auto selected = std::min<std::size_t>(
        count, static_cast<std::size_t>(limit));
    Json records = Json::array();
    for (std::size_t index = 0; index < selected; ++index) {
        const auto offset = index * quote_update_record_size;
        Json row = Json::object();
        row["index"] = static_cast<std::uint64_t>(index);
        row["time_epoch_ms_raw"] = u64(payload, offset);
        row["last_price"] = finite_number(f64(payload, offset + 8));
        row["open_price"] = finite_number(f64(payload, offset + 16));
        row["high_price"] = finite_number(f64(payload, offset + 24));
        row["low_price"] = finite_number(f64(payload, offset + 32));
        row["amount_raw"] = finite_number(f64(payload, offset + 40));
        row["volume_raw"] = u64(payload, offset + 48);
        row["special_volume_raw"] = u64(payload, offset + 56);
        row["quote_kind_raw"] =
            static_cast<std::uint64_t>(u32(payload, offset + 64));
        row["auxiliary_68_raw"] =
            static_cast<std::uint64_t>(u32(payload, offset + 68));
        row["open_interest_or_auxiliary_raw"] =
            static_cast<std::uint64_t>(u32(payload, offset + 72));
        row["pre_close_price"] = finite_number(f64(payload, offset + 76));
        row["ask_levels"] = quote_levels(payload, offset, 108, 188);
        row["bid_levels"] = quote_levels(payload, offset, 228, 308);
        Json aggregate = Json::object();
        aggregate["average_bid_price"] =
            finite_number(f64(payload, offset + 348));
        aggregate["total_bid_quantity_raw"] = finite_number(
            read_f32_le(payload.data() + offset + 356));
        aggregate["average_ask_price"] =
            finite_number(f64(payload, offset + 364));
        aggregate["total_ask_quantity_raw"] = finite_number(
            read_f32_le(payload.data() + offset + 372));
        row["aggregate"] = std::move(aggregate);
        records.push_back(std::move(row));
    }
    Json result = Json::object();
    result["schema"] = data_type == 1807 ?
        "tdx-level2-sdk-1807-v1" : "tdx-level2-sdk-18071-v1";
    result["data_type"] = data_type;
    result["record_size"] =
        static_cast<std::uint64_t>(quote_update_record_size);
    result["count"] = static_cast<std::uint64_t>(count);
    result["records"] = std::move(records);
    result["truncated"] = selected < count;
    result["batch_semantics"] = "single-fixed-body";
    result["security_identity_scope"] =
        "resolved from the SDK request/callback registry, not embedded in this body";
    result["field_boundary"] =
        "+68 and +72 remain raw because this TdxW consumer does not name them";
    result["transport_scope"] =
        "authorized post-transport SDK callback record";
    return result;
}

}  // namespace tdx::level2_detail
