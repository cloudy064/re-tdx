#include "tdx/trades_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <limits>

namespace tdx {
namespace detail {
namespace trades {

std::int64_t consume_varint(const Bytes& payload, std::size_t& offset,
                            std::size_t record_index) {
    if (offset >= payload.size())
        throw Error("trade record " + std::to_string(record_index + 1) +
                    " varint starts past payload end");
    const auto first = payload[offset++];
    std::uint64_t magnitude = first & 0x3F;
    int shift = 6;
    auto current = first;
    while (current & 0x80) {
        if (offset >= payload.size())
            throw Error("trade record " + std::to_string(record_index + 1) +
                        " varint has no terminator");
        current = payload[offset++];
        if (shift > 55) throw Error("trade varint is too long");
        magnitude += static_cast<std::uint64_t>(current & 0x7F) << shift;
        shift += 7;
    }
    if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw Error("trade varint exceeds int64");
    const auto value = static_cast<std::int64_t>(magnitude);
    return first & 0x40 ? -value : value;
}

std::vector<TradeTick> parse_records(const Bytes& payload, std::size_t& offset,
                                     std::uint16_t count, std::uint16_t start,
                                     int price_divisor) {
    std::vector<TradeTick> ticks;
    ticks.reserve(count);
    std::int64_t price_acc = 0;
    for (std::size_t index = 0; index < count; ++index) {
        if (offset + 2 > payload.size())
            throw Error("trade record " + std::to_string(index + 1) + " has no time");
        TradeTick tick;
        tick.index = index;
        tick.absolute_index = static_cast<std::size_t>(start) + index;
        tick.time_minutes = read_u16_le(payload.data() + offset);
        offset += 2;
        if (tick.time_minutes >= 24 * 60)
            throw Error("trade record has an invalid minute-of-day");
        tick.price_delta_raw = consume_varint(payload, offset, index);
        tick.volume_hand = consume_varint(payload, offset, index);
        tick.order_count = consume_varint(payload, offset, index);
        tick.status_raw = consume_varint(payload, offset, index);
        tick.tail_raw = consume_varint(payload, offset, index);
        if (tick.volume_hand < 0 || tick.order_count < 0)
            throw Error("trade volume or order count is negative");
        if ((tick.price_delta_raw > 0 && price_acc >
             std::numeric_limits<std::int64_t>::max() - tick.price_delta_raw) ||
            (tick.price_delta_raw < 0 && price_acc <
             std::numeric_limits<std::int64_t>::min() - tick.price_delta_raw))
            throw Error("trade accumulated price overflows int64");
        price_acc += tick.price_delta_raw;
        tick.price_acc_raw = price_acc;
        tick.price = static_cast<double>(price_acc) / price_divisor;
        if (!std::isfinite(tick.price) || tick.price < 0)
            throw Error("trade price is invalid");
        tick.time_label = time_label(tick.time_minutes);
        tick.side = trade_side(tick.status_raw);
        ticks.push_back(std::move(tick));
    }
    return ticks;
}

}  // namespace trades
}  // namespace detail
}  // namespace tdx
