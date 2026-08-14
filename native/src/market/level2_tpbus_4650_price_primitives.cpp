#include "tdx/level2.hpp"

#include "tdx/common.hpp"
#include "level2_sdk_snapshot_digest.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

namespace tdx {
namespace {

constexpr std::size_t raw_header_size = 96;
constexpr std::size_t quote_snapshot_size = 120;
constexpr std::size_t base_offset = 36;
constexpr std::size_t fraction_offset = 54;
constexpr std::size_t auxiliary_offset = 114;

std::int16_t read_i16_le(const std::uint8_t* data) {
    const auto raw = static_cast<std::uint16_t>(data[0]) |
                     (static_cast<std::uint16_t>(data[1]) << 8U);
    if (raw <= static_cast<std::uint16_t>(
                   std::numeric_limits<std::int16_t>::max()))
        return static_cast<std::int16_t>(raw);
    return static_cast<std::int16_t>(
        static_cast<std::int32_t>(raw) - 0x10000);
}

std::uint32_t f32_bits(float value) {
    std::uint32_t raw{};
    std::memcpy(&raw, &value, sizeof(raw));
    return raw;
}

std::uint32_t low_u32(std::int64_t value) {
    return static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(value));
}

std::int64_t native_ftol2(double value) {
    constexpr double lower = -9223372036854775808.0;
    constexpr double upper = 9223372036854775808.0;
    if (!std::isfinite(value) || value < lower || value >= upper)
        return std::numeric_limits<std::int64_t>::min();
    return static_cast<std::int64_t>(value);
}

bool prefix(const Bytes& raw, std::size_t offset, const char* text,
            std::size_t size) {
    if (offset + size > raw.size()) return false;
    for (std::size_t index = 0; index < size; ++index) {
        if (raw[offset + index] !=
            static_cast<std::uint8_t>(text[index]))
            return false;
    }
    return true;
}

Json bounded_code(const Bytes& raw) {
    constexpr std::size_t begin = 10;
    constexpr std::size_t end = raw_header_size;
    std::size_t nul = begin;
    while (nul < end && raw[nul] != 0) ++nul;
    bool printable = nul < end;
    for (std::size_t index = begin; printable && index < nul; ++index)
        printable = raw[index] >= 0x20U && raw[index] <= 0x7eU;
    return printable
        ? Json(std::string(raw.begin() + begin, raw.begin() + nul))
        : Json(nullptr);
}

Json float_projection(float value) {
    Json result = Json::object();
    result["raw_u32"] = static_cast<std::uint64_t>(f32_bits(value));
    result["finite"] = std::isfinite(value);
    result["value"] = std::isfinite(value)
        ? Json(static_cast<double>(value))
        : Json(nullptr);
    return result;
}

void add_offline_boundary(Json& result) {
    result["input_body_retained"] = false;
    result["raw_body_emitted"] = false;
    result["host_identity_accessed"] = false;
    result["host_identity_comparison_executed"] = false;
    result["previous_host_state_accessed"] = false;
    result["host_clock_accessed"] = false;
    result["handler_executed"] = false;
    result["host_state_write_performed"] = false;
    result["sdk_called"] = false;
    result["callback_executed"] = false;
    result["host_message_dispatch_attempted"] = false;
    result["host_messages_sent"] = 0;
    result["wire_bytes_built"] = false;
    result["network_requests"] = 0;
    result["request_sent"] = false;
    result["subscription_sent"] = false;
    result["authorization_attempted"] = false;
    result["credentials_accessed"] = false;
    result["entitlement_bypass"] = false;
    result["offline"] = true;
}

}  // namespace

Json project_level2_tpbus_4650_price_primitives(
    const Level2Tpbus4650PricePrimitivesRequest& request) {
    Level2Tpbus4650DispatchPreflightRequest preflight_request;
    preflight_request.raw = request.raw;
    const auto preflight =
        level2_tpbus_4650_dispatch_preflight_document(preflight_request);
    if (request.raw[0] != 1)
        throw Error(
            "tpbus 4650 price primitives require raw_u8_at_0 == 1 so "
            "the recovered raw+96 quote-snapshot branch is selected");
    if (request.raw.size() < raw_header_size + quote_snapshot_size)
        throw Error(
            "tpbus 4650 price primitives require at least 120 bytes at "
            "raw+96 after exact shape validation");

    const auto snapshot_begin = request.raw.begin() +
        static_cast<std::ptrdiff_t>(raw_header_size);
    Bytes snapshot(snapshot_begin, snapshot_begin +
        static_cast<std::ptrdiff_t>(quote_snapshot_size));
    const auto base = read_u32_le(snapshot.data() + base_offset);
    const auto fraction = read_u32_le(snapshot.data() + fraction_offset);
    const auto auxiliary = read_f32_le(snapshot.data() + auxiliary_offset);
    const auto market = read_i16_le(request.raw.data() + 8);
    const bool sh_star = market == 1 &&
        (prefix(request.raw, 10, "688", 3) ||
         prefix(request.raw, 10, "689", 3));
    const bool sz_chinext = market == 0 &&
        prefix(request.raw, 10, "30", 2);
    const bool fractional_security = sh_star || sz_chinext;
    const bool fraction_in_range = fraction != 0 && fraction < 100;

    float projected_float{};
    if (fractional_security) {
        const double base_and_fraction =
            static_cast<double>(base) +
            (fraction_in_range ? static_cast<double>(fraction) * 0.01
                               : 0.0);
        projected_float = static_cast<float>(
            base_and_fraction + static_cast<double>(auxiliary) / 100.0);
    } else if (request.target_market_or_mode_raw != 0) {
        projected_float = static_cast<float>(base);
    } else {
        projected_float = static_cast<float>(
            static_cast<double>(base) +
            (fraction_in_range ? static_cast<double>(fraction) * 0.01
                               : 0.0));
    }

    std::uint32_t projected_integer = base;
    if (fractional_security) {
        const auto auxiliary_integer =
            native_ftol2(static_cast<double>(auxiliary) / 100.0);
        projected_integer += low_u32(auxiliary_integer);
    }

    Json identity = Json::object();
    identity["market_raw_signed_i16"] = market;
    identity["code_raw"] = bounded_code(request.raw);
    identity["sh_688_or_689"] = sh_star;
    identity["sz_30"] = sz_chinext;
    identity["fractional_security_branch"] = fractional_security;
    identity["source"] = "raw-header-offline-projection";
    identity["host_match_executed"] = false;

    Json fields = Json::object();
    fields["base_u32_offset"] = static_cast<std::uint64_t>(base_offset);
    fields["base_u32_raw"] = static_cast<std::uint64_t>(base);
    fields["fraction_u32_offset"] =
        static_cast<std::uint64_t>(fraction_offset);
    fields["fraction_u32_raw"] = static_cast<std::uint64_t>(fraction);
    fields["fraction_in_native_range_1_to_99"] = fraction_in_range;
    fields["auxiliary_f32_offset"] =
        static_cast<std::uint64_t>(auxiliary_offset);
    fields["auxiliary_f32_raw_u32"] =
        static_cast<std::uint64_t>(f32_bits(auxiliary));
    fields["auxiliary_f32"] = std::isfinite(auxiliary)
        ? Json(static_cast<double>(auxiliary))
        : Json(nullptr);

    Json result = Json::object();
    result["schema"] = "tdx-level2-tpbus-4650-price-primitives-v1";
    result["format"] = "tpbus-4650-price-primitives";
    result["function_id"] = 4650;
    result["normalization_kind"] =
        "offline-raw-plus-96-quote-price-subprojection";
    result["dispatch_shape_qualified"] =
        preflight.at("raw_validator").at("qualified");
    result["full_dispatcher_qualified"] = false;
    result["full_handler_state_projection_performed"] = false;
    result["target_market_or_mode_raw_plus_72"] =
        static_cast<std::uint64_t>(request.target_market_or_mode_raw);
    result["identity"] = std::move(identity);
    result["quote_snapshot"] = Json::object();
    result["quote_snapshot"]["offset"] =
        static_cast<std::uint64_t>(raw_header_size);
    result["quote_snapshot"]["byte_size"] =
        static_cast<std::uint64_t>(quote_snapshot_size);
    result["quote_snapshot"]["sha256"] =
        level2_detail::level2_snapshot_sha256(snapshot);
    result["quote_snapshot"]["body_emitted"] = false;
    result["source_fields"] = std::move(fields);
    result["projected_float_price"] = float_projection(projected_float);
    result["projected_integer_price_raw_u32"] =
        static_cast<std::uint64_t>(projected_integer);
    result["numeric_scope"] =
        "raw-f32/u32 arithmetic with recovered operation order; non-default "
        "x87 precision-control edge cases are not claimed";
    result["evidence"] =
        "tpbus sub_10066A34, sub_10066B6B, __ftol2; "
        "output/ida-tpbus-4650-price-primitives-targeted-20260814.json";
    add_offline_boundary(result);
    return result;
}

}  // namespace tdx
