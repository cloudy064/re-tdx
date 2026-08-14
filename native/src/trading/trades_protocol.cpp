#include "tdx/trades.hpp"
#include "tdx/trades_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>

namespace tdx {

using detail::trades::append_u16;
using detail::trades::append_u32;
using detail::trades::normalize_date;
using detail::trades::parse_records;
using detail::trades::trade_price_divisor;
using detail::trades::validate_security;

Bytes build_today_trades_request_data(int market_id, std::string_view code,
                                      std::uint16_t start, std::uint16_t count) {
    validate_security(market_id, code);
    if (!count) throw Error("trade request count must be positive");
    Bytes result{static_cast<std::uint8_t>(market_id), 0};
    result.insert(result.end(), code.begin(), code.end());
    append_u16(result, start);
    append_u16(result, count);
    return result;
}

Bytes build_history_trades_request_data(int market_id, std::string_view code,
                                        std::string_view trading_date,
                                        std::uint16_t start, std::uint16_t count) {
    validate_security(market_id, code);
    if (!count) throw Error("trade request count must be positive");
    const auto date = normalize_date(std::string(trading_date));
    Bytes result;
    append_u32(result, static_cast<std::uint32_t>(std::stoul(date)));
    append_u16(result, static_cast<std::uint16_t>(market_id));
    result.insert(result.end(), code.begin(), code.end());
    append_u16(result, start);
    append_u16(result, count);
    return result;
}

TradePage parse_today_trades_payload(const Bytes& payload, int market_id,
                                     const std::string& code, std::uint16_t start,
                                     std::uint16_t request_count,
                                     std::string trading_date) {
    validate_security(market_id, code);
    if (payload.size() < 2) throw Error("today trades response is shorter than two bytes");
    const auto count = read_u16_le(payload.data());
    std::size_t offset = 2;
    auto ticks = parse_records(payload, offset, count, start, trade_price_divisor(code));
    if (offset != payload.size())
        throw Error("today trades response has " + std::to_string(payload.size() - offset) +
                    " trailing bytes");
    return TradePage{market_id, code, std::move(trading_date), start, request_count,
                     trade_price_divisor(code), std::nullopt, std::move(ticks)};
}

TradePage parse_history_trades_payload(const Bytes& payload, int market_id,
                                       const std::string& code,
                                       std::string_view trading_date,
                                       std::uint16_t start,
                                       std::uint16_t request_count) {
    validate_security(market_id, code);
    if (payload.size() < 6) throw Error("history trades response is shorter than six bytes");
    const auto count = read_u16_le(payload.data());
    const float price_base = read_f32_le(payload.data() + 2);
    if (!std::isfinite(price_base)) throw Error("history trades price base is not finite");
    std::size_t offset = 6;
    auto ticks = parse_records(payload, offset, count, start, trade_price_divisor(code));
    if (offset != payload.size())
        throw Error("history trades response has " + std::to_string(payload.size() - offset) +
                    " trailing bytes");
    return TradePage{market_id, code, normalize_date(std::string(trading_date)), start,
                     request_count, trade_price_divisor(code), price_base, std::move(ticks)};
}

}  // namespace tdx
