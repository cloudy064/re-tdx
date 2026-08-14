#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>

namespace tdx {
namespace {

constexpr std::uint16_t command_1369 = 1369;
constexpr std::uint16_t command_1371 = 1371;
constexpr std::size_t request_1369_size = 0x28;
constexpr std::size_t request_1371_size = 0x30;

void validate_security_code(std::string_view code) {
    if (code.size() != 6 ||
        !std::all_of(code.begin(), code.end(), [](unsigned char ch) {
            return ch >= '0' && ch <= '9';
        })) {
        throw Error("TdxW Level2 code must contain exactly six digits");
    }
}

void put_u16(Bytes& result, std::size_t offset, std::uint16_t value) {
    result[offset] = static_cast<std::uint8_t>(value);
    result[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_i32(Bytes& result, std::size_t offset, std::int32_t value) {
    const auto raw = static_cast<std::uint32_t>(value);
    for (int shift = 0; shift < 32; shift += 8)
        result[offset++] = static_cast<std::uint8_t>(raw >> shift);
}

void put_f32(Bytes& result, std::size_t offset, float value) {
    static_assert(sizeof(value) == 4);
    static_assert(std::numeric_limits<float>::is_iec559);
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    for (int shift = 0; shift < 32; shift += 8)
        result[offset++] = static_cast<std::uint8_t>(raw >> shift);
}

void put_security(Bytes& result, std::uint16_t market_id,
                  std::string_view code) {
    put_u16(result, 2, market_id);
    std::copy(code.begin(), code.end(), result.begin() + 4);
}

}  // namespace

Bytes build_level2_tdxw_1369_request(
    const Level2Tdxw1369Request& request) {
    if (request.market_id > 2)
        throw Error("TdxW Level2 market-id must be 0..2");
    validate_security_code(request.code);
    if (!request.request_count || request.request_count > 1000)
        throw Error("TdxW 1369 request-count must be 1..1000");

    Bytes result(request_1369_size, 0);
    put_u16(result, 0, command_1369);
    put_security(result, request.market_id, request.code);
    result[37] = 1;
    put_u16(result, 38, request.request_count);
    return result;
}

Bytes build_level2_tdxw_1371_request(
    const Level2Tdxw1371Request& request) {
    if (request.market_id > 2)
        throw Error("TdxW Level2 market-id must be 0..2");
    validate_security_code(request.code);
    if (!std::isfinite(request.selected_price) || request.selected_price <= 0.0F)
        throw Error("TdxW 1371 selected-price must be finite and positive");
    if (!request.request_count || request.request_count > 5000)
        throw Error("TdxW 1371 request-count must be 1..5000");

    Bytes result(request_1371_size, 0);
    put_u16(result, 0, command_1371);
    put_security(result, request.market_id, request.code);
    result[37] = request.side_mode_raw;
    put_f32(result, 38, request.selected_price);
    put_i32(result, 42, request.cursor);
    put_u16(result, 46, request.request_count);
    return result;
}

}  // namespace tdx
