#include "tdx/level2.hpp"

#include "level2_sdk_security_classifier.hpp"

#include "tdx/common.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <limits>
#include <string>
#include <utility>

namespace tdx {
namespace {

constexpr std::size_t transaction_record_size = 52;
constexpr std::size_t order_record_size = 40;
constexpr std::size_t host_record_size = 20;
constexpr std::size_t maximum_body_size = level2_offline_payload_limit;
constexpr double price_rounding_bias = 0.503000020980835;

struct SecurityProjection {
    int security_class_raw{};
    std::int64_t lot_divisor_raw{};
};

struct LocalOffsetProjection {
    std::int64_t epoch_seconds_raw{};
    int offset_seconds_raw{};
    std::uint16_t stored_u16_raw{};
    int classifier_minute_raw{};
    bool localtime_resolved{};
};

struct PriceProjection {
    std::int64_t converted_i64_raw{};
    std::int32_t stored_i32_raw{};
    bool x87_integer_indefinite{};
};

std::uint64_t read_u64_le_exact(const Bytes& body, std::size_t offset) {
    return static_cast<std::uint64_t>(
               read_u32_le(body.data() + offset)) |
           (static_cast<std::uint64_t>(
                read_u32_le(body.data() + offset + 4))
            << 32U);
}

std::int64_t read_i64_le_exact(const Bytes& body, std::size_t offset) {
    const auto bits = read_u64_le_exact(body, offset);
    std::int64_t result{};
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

double read_f64_le_exact(const Bytes& body, std::size_t offset) {
    const auto bits = read_u64_le_exact(body, offset);
    double result{};
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

std::int32_t low_i32(std::int64_t value) {
    const auto bits = static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(value));
    std::int32_t result{};
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

void put_u16(std::array<std::uint8_t, host_record_size>& record,
             std::size_t offset, std::uint16_t value) {
    record[offset] = static_cast<std::uint8_t>(value);
    record[offset + 1] = static_cast<std::uint8_t>(value >> 8U);
}

void put_u32(std::array<std::uint8_t, host_record_size>& record,
             std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index)
        record[offset + index] =
            static_cast<std::uint8_t>(value >> (8U * index));
}

void put_i32(std::array<std::uint8_t, host_record_size>& record,
             std::size_t offset, std::int32_t value) {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    put_u32(record, offset, bits);
}

std::string bytes_hex(
    const std::array<std::uint8_t, host_record_size>& record) {
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

SecurityProjection security_projection(std::uint16_t market_id,
                                       const std::string& code) {
    const auto classification =
        level2_detail::classify_level2_sdk_security(
            market_id, code, "SDK 1801/1802 host projection");
    return {classification.security_class_raw,
            classification.sub_594680_predicate_raw ? 10 : 100};
}

void validate_body(const Bytes& body, std::size_t record_size,
                   const char* label) {
    if (body.empty())
        throw Error(std::string(label) +
                    " host projection requires at least one callback record; "
                    "null-source/clear-only callbacks are outside this CLI contract");
    if (body.size() > maximum_body_size)
        throw Error(std::string(label) +
                    " host projection body exceeds the 384 KiB limit");
    if (body.size() % record_size != 0)
        throw Error(std::string(label) +
                    " host projection body must be an exact multiple of " +
                    std::to_string(record_size) + " bytes");
}

LocalOffsetProjection local_offset(std::int64_t epoch_ms_raw) {
    LocalOffsetProjection result;
    result.epoch_seconds_raw = epoch_ms_raw / 1000;

    bool time_representable = true;
    if constexpr (std::numeric_limits<std::time_t>::is_signed) {
        if constexpr (sizeof(std::time_t) < sizeof(std::int64_t)) {
            time_representable =
                result.epoch_seconds_raw >= static_cast<std::int64_t>(
                    std::numeric_limits<std::time_t>::lowest()) &&
                result.epoch_seconds_raw <= static_cast<std::int64_t>(
                    std::numeric_limits<std::time_t>::max());
        }
    } else {
        time_representable = result.epoch_seconds_raw >= 0;
        if constexpr (sizeof(std::time_t) < sizeof(std::uint64_t)) {
            time_representable = time_representable &&
                static_cast<std::uint64_t>(result.epoch_seconds_raw) <=
                    static_cast<std::uint64_t>(
                        std::numeric_limits<std::time_t>::max());
        }
    }

    std::tm local{};
    if (time_representable) {
        const auto time = static_cast<std::time_t>(result.epoch_seconds_raw);
#ifdef _WIN32
        result.localtime_resolved = localtime_s(&local, &time) == 0;
#else
        result.localtime_resolved = localtime_r(&time, &local) != nullptr;
#endif
    }
    if (result.localtime_resolved)
        result.offset_seconds_raw = local.tm_sec +
            60 * (local.tm_min + 60 * (local.tm_hour - 6));
    result.stored_u16_raw =
        static_cast<std::uint16_t>(result.offset_seconds_raw);
    // The 1801 branch reloads AX with movzx before applying these thresholds.
    result.classifier_minute_raw =
        (static_cast<int>(result.stored_u16_raw) + 21600) / 60;
    return result;
}

PriceProjection price_projection(double source) {
    // The recovered x86 sequence keeps the f64 operands on the x87 stack,
    // switches only RC to chop, and FISTPs a signed qword. Masked invalid
    // conversions yield x87's integer-indefinite INT64_MIN; the host record
    // subsequently retains only the low dword.
    const auto adjusted = static_cast<long double>(source) *
                              static_cast<long double>(10000.0) +
                          static_cast<long double>(price_rounding_bias);
    constexpr long double minimum = -9223372036854775808.0L;
    constexpr long double maximum_exclusive = 9223372036854775808.0L;
    PriceProjection result;
    if (!std::isfinite(adjusted) || adjusted < minimum ||
        adjusted >= maximum_exclusive) {
        result.converted_i64_raw = std::numeric_limits<std::int64_t>::min();
        result.x87_integer_indefinite = true;
    } else {
        result.converted_i64_raw = static_cast<std::int64_t>(adjusted);
    }
    result.stored_i32_raw = low_i32(result.converted_i64_raw);
    return result;
}

void add_common_record_fields(Json& row, const LocalOffsetProjection& time,
                              const PriceProjection& price,
                              std::int64_t volume_raw,
                              std::int64_t lot_divisor_raw,
                              const std::array<std::uint8_t,
                                               host_record_size>& packed) {
    const auto quotient = volume_raw / lot_divisor_raw;
    const auto remainder = volume_raw % lot_divisor_raw;
    row["time_offset_seconds_raw"] = time.offset_seconds_raw;
    row["time_offset_u16_raw"] =
        static_cast<std::uint64_t>(time.stored_u16_raw);
    row["localtime_resolved"] = time.localtime_resolved;
    row["local_minute_classifier_raw"] = time.classifier_minute_raw;
    row["price_x10000_i64_raw"] = price.converted_i64_raw;
    row["price_x10000_i64_decimal"] =
        std::to_string(price.converted_i64_raw);
    row["price_x10000_i32_raw"] = price.stored_i32_raw;
    row["price_x87_integer_indefinite"] =
        price.x87_integer_indefinite;
    row["volume_lot_quotient_i64_raw"] = quotient;
    row["volume_lot_quotient_i64_decimal"] = std::to_string(quotient);
    row["volume_lot_quotient_i32_raw"] = low_i32(quotient);
    row["volume_lot_remainder_raw"] = remainder;
    row["volume_lot_remainder_u8_raw"] = static_cast<std::uint64_t>(
        static_cast<std::uint8_t>(remainder));
    row["host_record_size"] =
        static_cast<std::uint64_t>(host_record_size);
    row["host_record_hex"] = bytes_hex(packed);
}

Json source_common(const Bytes& body, std::size_t offset,
                   std::int64_t epoch_ms_raw, double source_price,
                   std::int64_t volume_raw) {
    Json source = Json::object();
    source["epoch_ms_raw_i64"] = epoch_ms_raw;
    source["epoch_ms_raw_i64_decimal"] = std::to_string(epoch_ms_raw);
    source["price"] = finite_number(source_price);
    source["price_f64_raw_u64_hex"] =
        u64_hex(read_u64_le_exact(body, offset + 8));
    source["volume_raw_i64"] = volume_raw;
    source["volume_raw_i64_decimal"] = std::to_string(volume_raw);
    return source;
}

void add_offline_boundary(Json& result) {
    result["input_body_retained"] = false;
    result["sdk_called"] = false;
    result["sdk_callback_invoked"] = false;
    result["callback_executed"] = false;
    result["host_storage_call_attempted"] = false;
    result["host_storage_mutated"] = false;
    result["sub_68C170_called"] = false;
    result["sub_68C200_called"] = false;
    result["host_message_dispatch_attempted"] = false;
    result["host_messages_sent"] = 0;
    result["wire_bytes_built"] = false;
    result["network_accessed"] = false;
    result["network_request_bytes_built"] = false;
    result["network_requests"] = 0;
    result["send_attempted"] = false;
    result["request_sent"] = false;
    result["entitlement_bypass"] = false;
    result["offline"] = true;
}

Json projection_envelope(int data_type, std::uint16_t market_id,
                         const std::string& code, const Bytes& body,
                         std::size_t source_record_size,
                         const SecurityProjection& security,
                         Json records) {
    const auto type = std::to_string(data_type);
    Json state = Json::object();
    state["schema"] = "tdx-level2-sdk-" + type + "-host-state-v1";
    state["storage"] = "contiguous-20-byte-records";
    state["record_size"] =
        static_cast<std::uint64_t>(host_record_size);
    state["record_count"] = static_cast<std::uint64_t>(records.size());
    state["byte_size"] = static_cast<std::uint64_t>(
        records.size() * host_record_size);
    state["replacement_semantics"] =
        "complete callback replacement: TdxW clears the selected host vector "
        "before appending these records in source order";
    state["records"] = std::move(records);

    Json result = Json::object();
    result["schema"] =
        "tdx-level2-sdk-" + type + "-host-projection-v1";
    result["format"] = "sdk-" + type + "-host-projection";
    result["normalization_kind"] =
        "offline-sub_68C750-sdk-" + type + "-host-record-projection";
    result["data_type"] = data_type;
    result["market_raw"] = static_cast<std::uint64_t>(market_id);
    result["code_ascii"] = code;
    result["security_class_raw"] = security.security_class_raw;
    result["lot_divisor_raw"] = security.lot_divisor_raw;
    result["source_record_size"] =
        static_cast<std::uint64_t>(source_record_size);
    result["source_body_size"] =
        static_cast<std::uint64_t>(body.size());
    result["source_record_count"] = static_cast<std::uint64_t>(
        body.size() / source_record_size);
    result["projected_host_record_size"] =
        static_cast<std::uint64_t>(host_record_size);
    result["projected_state"] = std::move(state);
    result["field_naming"] = data_type == 1801
        ? "first_raw/second_raw/qualifier_raw are retained as raw names; no "
          "side or venue meaning is inferred"
        : "order_id_raw, side_or_cancel_qualifier_raw, and action_raw use "
          "the recovered host field roles; their byte values are retained raw";
    result["time_policy"] =
        "signed epoch_ms / 1000 toward zero, localtime, then "
        "second+60*(minute+60*(hour-6)); caller stores low u16";
    result["price_policy"] =
        "x87 f64*10000.0+0.503000020980835, chop to signed i64; "
        "NaN/infinity/out-of-range use INT64_MIN integer-indefinite; low i32 stored";
    result["volume_policy"] =
        "signed i64 quotient and remainder toward zero; low quotient i32 and "
        "low remainder byte stored";
    result["evidence"] =
        "TdxW sub_68C750 SDK 1801/1802 branches, sub_689E20, "
        "sub_5960B0, and sub_594680";
    add_offline_boundary(result);
    return result;
}

}  // namespace

Json project_level2_sdk_1801_host_projection(
    const Level2Sdk1801HostProjectionRequest& request) {
    const auto security = security_projection(request.market_id, request.code);
    validate_body(request.body, transaction_record_size, "SDK 1801");

    Json records = Json::array();
    for (std::size_t index = 0;
         index < request.body.size() / transaction_record_size; ++index) {
        const auto offset = index * transaction_record_size;
        const auto epoch_ms_raw = read_i64_le_exact(request.body, offset);
        const auto source_price = read_f64_le_exact(request.body, offset + 8);
        const auto volume_raw = read_i64_le_exact(request.body, offset + 16);
        const auto first_raw = read_u32_le(request.body.data() + offset + 36);
        const auto second_raw = read_u32_le(request.body.data() + offset + 40);
        const auto source_qualifier_raw =
            read_u32_le(request.body.data() + offset + 48);
        const auto time = local_offset(epoch_ms_raw);
        const auto price = price_projection(source_price);
        const auto quotient = volume_raw / security.lot_divisor_raw;
        const auto remainder = volume_raw % security.lot_divisor_raw;

        std::uint8_t qualifier_raw = 2;
        if (security.security_class_raw == 19 &&
            time.classifier_minute_raw > 902) {
            qualifier_raw = 5;
        } else if (time.classifier_minute_raw >= 567) {
            qualifier_raw = source_qualifier_raw == 2
                ? 1 : source_qualifier_raw == 1 ? 0 : 2;
        }

        std::array<std::uint8_t, host_record_size> packed{};
        put_u16(packed, 0, time.stored_u16_raw);
        put_i32(packed, 2, price.stored_i32_raw);
        put_i32(packed, 6, low_i32(quotient));
        packed[10] = static_cast<std::uint8_t>(remainder);
        packed[11] = qualifier_raw;
        put_u32(packed, 12, first_raw);
        put_u32(packed, 16, second_raw);

        Json source = source_common(request.body, offset, epoch_ms_raw,
                                    source_price, volume_raw);
        source["first_raw"] = static_cast<std::uint64_t>(first_raw);
        source["second_raw"] = static_cast<std::uint64_t>(second_raw);
        source["qualifier_raw"] =
            static_cast<std::uint64_t>(source_qualifier_raw);

        Json row = Json::object();
        row["index"] = static_cast<std::uint64_t>(index);
        row["source"] = std::move(source);
        row["qualifier_raw"] =
            static_cast<std::uint64_t>(qualifier_raw);
        row["first_raw"] = static_cast<std::uint64_t>(first_raw);
        row["second_raw"] = static_cast<std::uint64_t>(second_raw);
        add_common_record_fields(row, time, price, volume_raw,
                                 security.lot_divisor_raw, packed);
        records.push_back(std::move(row));
    }
    return projection_envelope(1801, request.market_id, request.code,
                               request.body, transaction_record_size,
                               security, std::move(records));
}

Json project_level2_sdk_1802_host_projection(
    const Level2Sdk1802HostProjectionRequest& request) {
    const auto security = security_projection(request.market_id, request.code);
    validate_body(request.body, order_record_size, "SDK 1802");

    Json records = Json::array();
    for (std::size_t index = 0;
         index < request.body.size() / order_record_size; ++index) {
        const auto offset = index * order_record_size;
        const auto epoch_ms_raw = read_i64_le_exact(request.body, offset);
        const auto source_price = read_f64_le_exact(request.body, offset + 8);
        const auto volume_raw = read_i64_le_exact(request.body, offset + 16);
        const auto order_id_raw =
            read_u32_le(request.body.data() + offset + 24);
        const auto record_type_raw =
            read_u32_le(request.body.data() + offset + 36);
        const auto time = local_offset(epoch_ms_raw);
        const auto price = price_projection(source_price);
        const auto quotient = volume_raw / security.lot_divisor_raw;
        const auto remainder = volume_raw % security.lot_divisor_raw;

        std::uint8_t side_or_cancel_qualifier_raw = 0;
        std::uint8_t action_raw = 0;
        switch (record_type_raw) {
        case 1: action_raw = 'B'; break;
        case 2: action_raw = 'S'; break;
        case 3:
            side_or_cancel_qualifier_raw = 'B';
            action_raw = 'C';
            break;
        case 4:
            side_or_cancel_qualifier_raw = 'S';
            action_raw = 'C';
            break;
        default: break;
        }

        std::array<std::uint8_t, host_record_size> packed{};
        put_u16(packed, 0, time.stored_u16_raw);
        put_i32(packed, 2, price.stored_i32_raw);
        put_i32(packed, 6, low_i32(quotient));
        packed[10] = static_cast<std::uint8_t>(remainder);
        packed[11] = side_or_cancel_qualifier_raw;
        packed[12] = action_raw;
        put_u32(packed, 16, order_id_raw);

        Json source = source_common(request.body, offset, epoch_ms_raw,
                                    source_price, volume_raw);
        source["order_id_raw"] =
            static_cast<std::uint64_t>(order_id_raw);
        source["record_type_raw"] =
            static_cast<std::uint64_t>(record_type_raw);

        Json row = Json::object();
        row["index"] = static_cast<std::uint64_t>(index);
        row["source"] = std::move(source);
        row["side_or_cancel_qualifier_raw"] =
            static_cast<std::uint64_t>(side_or_cancel_qualifier_raw);
        row["action_raw"] = static_cast<std::uint64_t>(action_raw);
        row["order_id_raw"] = static_cast<std::uint64_t>(order_id_raw);
        add_common_record_fields(row, time, price, volume_raw,
                                 security.lot_divisor_raw, packed);
        records.push_back(std::move(row));
    }
    return projection_envelope(1802, request.market_id, request.code,
                               request.body, order_record_size, security,
                               std::move(records));
}

}  // namespace tdx
