#include "minute_download_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/session_audit.hpp"
#include "tdx/transport.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {

using namespace minute_download_detail;

bool valid_expansion_code(std::string_view code) {
    if (code.empty() || code.size() > 9 || code.front() == ' ' || code.back() == ' ')
        return false;
    return std::all_of(code.begin(), code.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == ' ';
    });
}

Bytes build_kline_request_data(std::uint16_t market_id, std::string_view code,
                               std::uint16_t start, std::uint16_t count,
                               std::uint16_t period,
                               std::uint16_t period_parameter) {
    if (!valid_kline_code(code, 6))
        throw Error("K-line code must contain 1..6 ASCII letters or digits");
    if (count < 1 || count > maximum_page_size) throw Error("K-line page size must be 1..800");
    Bytes data(42, 0);
    write_u16(data, 0, market_id);
    std::copy(code.begin(), code.end(), data.begin() + 2);
    if (period > 11) throw Error("K-line period ID is outside the known range");
    write_u16(data, 8, period);
    write_u16(data, 10, period_parameter);
    write_u16(data, 12, start);
    write_u16(data, 14, count);
    return data;
}

Bytes build_expansion_kline_request_data(std::uint8_t market_id, std::string_view code,
                                         std::uint32_t start, std::uint16_t count,
                                         std::uint16_t period) {
    if (!valid_expansion_code(code))
        throw Error("expansion K-line code must contain 1..9 ASCII letters, digits, or internal spaces");
    if (count < 1 || count > maximum_page_size)
        throw Error("expansion K-line page size must be 1..800");
    if (period > 11) throw Error("expansion K-line period ID is outside the known range");
    Bytes data(20, 0);
    data[0] = market_id;
    std::copy(code.begin(), code.end(), data.begin() + 1);
    write_u16(data, 10, period);
    write_u16(data, 12, 1);
    for (int shift = 0; shift < 32; shift += 8)
        data[14 + static_cast<std::size_t>(shift / 8)] =
            static_cast<std::uint8_t>(start >> shift);
    write_u16(data, 18, count);
    return data;
}

std::vector<MinuteBar> parse_kline_payload(const Bytes& payload, bool index_mode,
                                           std::uint16_t period) {
    if (payload.size() < 2) throw Error("K-line response is shorter than two bytes");
    const auto count = read_u16_le(payload.data());
    if (payload.size() == 2 && count != 0)
        throw Error("K-line response contains a count but no records");
    if (count > maximum_page_size) throw Error("K-line response count exceeds 800");
    std::size_t offset = 2;
    std::int64_t previous_close_milli = 0;
    std::vector<MinuteBar> result;
    result.reserve(count);
    for (std::uint16_t record = 0; record < count; ++record) {
        if (offset + 4 > payload.size()) throw Error("K-line record has no time fields");
        const bool intraday = period < 4 || period == 7 || period == 8 || period == 13;
        int decoded_date = 0;
        int minute_of_day = 15 * 60;
        if (intraday) {
            const auto encoded_date = read_u16_le(payload.data() + offset);
            decoded_date = decode_lc1_date(encoded_date);
            minute_of_day = read_u16_le(payload.data() + offset + 2);
        } else {
            const auto raw_date = read_u32_le(payload.data() + offset);
            const int year = static_cast<int>(raw_date / 10000);
            const int month = static_cast<int>(raw_date / 100 % 100);
            const int day = static_cast<int>(raw_date % 100);
            if (year < 1990 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31)
                throw Error("invalid daily K-line date: " + std::to_string(raw_date));
            decoded_date = static_cast<int>(raw_date);
        }
        offset += 4;
        if (minute_of_day >= 24 * 60) throw Error("K-line minute-of-day is invalid");

        const auto open_milli = previous_close_milli + consume_varint(payload, offset);
        const auto close_milli = open_milli + consume_varint(payload, offset);
        const auto high_milli = open_milli + consume_varint(payload, offset);
        const auto low_milli = open_milli + consume_varint(payload, offset);
        previous_close_milli = close_milli;
        if (offset + 8 > payload.size()) throw Error("K-line record has no volume/amount fields");
        const double volume_value = decode_wire_number(read_u32_le(payload.data() + offset));
        const double amount_value = decode_wire_number(read_u32_le(payload.data() + offset + 4));
        offset += 8;

        MinuteBar bar;
        bar.date = decoded_date;
        bar.hour = minute_of_day / 60;
        bar.minute = minute_of_day % 60;
        bar.open = static_cast<float>(open_milli / 1000.0);
        bar.close = static_cast<float>(close_milli / 1000.0);
        bar.high = static_cast<float>(high_milli / 1000.0);
        bar.low = static_cast<float>(low_milli / 1000.0);
        bar.amount = static_cast<float>(amount_value);
        // Check before llround: floating wire values may exceed int64 too.
        if (!std::isfinite(volume_value) || volume_value < 0.0 ||
            volume_value >= static_cast<double>(std::numeric_limits<std::int64_t>::max()))
            throw Error("K-line volume exceeds nonnegative int64");
        bar.volume = std::llround(volume_value);
        if (index_mode) {
            if (offset + 4 > payload.size()) throw Error("index K-line has no breadth fields");
            bar.extra_1 = read_u16_le(payload.data() + offset);
            bar.extra_2 = read_u16_le(payload.data() + offset + 2);
            offset += 4;
        }
        for (float value : {bar.open, bar.high, bar.low, bar.close, bar.amount})
            if (!std::isfinite(value)) throw Error("K-line record contains a non-finite value");
        result.push_back(bar);
    }
    if (offset != payload.size())
        throw Error("K-line response has " + std::to_string(payload.size() - offset) +
                    " trailing bytes");
    return result;
}

std::vector<MinuteBar> parse_expansion_kline_payload(const Bytes& payload,
                                                     std::uint16_t period) {
    if (payload.size() < 20)
        throw Error("expansion K-line response is shorter than 20 bytes");
    const auto count = read_u16_le(payload.data() + 18);
    if (count > maximum_page_size)
        throw Error("expansion K-line response count exceeds 800");
    const auto expected = 20 + static_cast<std::size_t>(count) * 32;
    if (payload.size() != expected)
        throw Error("expansion K-line response size mismatch: expected " +
                    std::to_string(expected) + ", got " + std::to_string(payload.size()));
    std::vector<MinuteBar> result;
    result.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto* row = payload.data() + 20 + static_cast<std::size_t>(index) * 32;
        const bool intraday = period < 4 || period == 7 || period == 8 || period == 13;
        int date = 0, minute_of_day = 15 * 60;
        if (intraday) {
            date = decode_lc1_date(read_u16_le(row));
            minute_of_day = read_u16_le(row + 2);
            if (minute_of_day >= 24 * 60)
                throw Error("expansion K-line minute-of-day is invalid");
        } else {
            date = static_cast<int>(read_u32_le(row));
            const int year = date / 10000, month = date / 100 % 100, day = date % 100;
            if (year < 1990 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31)
                throw Error("invalid expansion daily K-line date: " + std::to_string(date));
        }
        MinuteBar bar;
        bar.date = date;
        bar.hour = minute_of_day / 60;
        bar.minute = minute_of_day % 60;
        bar.open = read_f32_le(row + 4);
        bar.high = read_f32_le(row + 8);
        bar.low = read_f32_le(row + 12);
        bar.close = read_f32_le(row + 16);
        bar.amount = 0.0f;
        bar.amount_available = false;
        bar.open_interest = read_u32_le(row + 20);
        const auto volume = read_u32_le(row + 24);
        bar.volume = static_cast<std::int64_t>(volume);
        bar.auxiliary_price = read_f32_le(row + 28);
        bar.has_expansion_fields = true;
        for (float value : {bar.open, bar.high, bar.low, bar.close, bar.auxiliary_price})
            if (!std::isfinite(value))
                throw Error("expansion K-line record contains a non-finite value");
        result.push_back(bar);
    }
    return result;
}

std::uint32_t parse_expansion_instrument_count_payload(const Bytes& payload) {
    if (payload.size() < 23)
        throw Error("expansion instrument-count response is shorter than 23 bytes");
    static constexpr std::array<std::uint8_t, 6> signature{'T', 'D', 'X', '_', 'D', 'S'};
    if (!std::equal(signature.begin(), signature.end(), payload.begin()))
        throw Error("expansion instrument-count response has no TDX_DS signature");
    return read_u32_le(payload.data() + 19);
}

std::vector<ExpansionInstrument> parse_expansion_instrument_info_payload(
    const Bytes& payload) {
    if (payload.size() < 6)
        throw Error("expansion instrument-info response is shorter than six bytes");
    const auto count = read_u16_le(payload.data() + 4);
    if (count > 100)
        throw Error("expansion instrument-info page count exceeds 100");
    const auto expected = 6 + static_cast<std::size_t>(count) * 64;
    if (payload.size() < expected)
        throw Error("expansion instrument-info response is truncated");
    std::vector<ExpansionInstrument> result;
    result.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto* row = payload.data() + 6 + static_cast<std::size_t>(index) * 64;
        ExpansionInstrument instrument;
        instrument.category = row[0];
        instrument.market_id = row[1];
        instrument.code = decode_fixed_gbk(row + 5, 9);
        instrument.name = decode_fixed_gbk(row + 14, 17);
        instrument.description = decode_fixed_gbk(row + 31, 9);
        instrument.contract_multiplier = read_u32_le(row + 56);
        result.push_back(std::move(instrument));
    }
    return result;
}

Json parse_expansion_quote_payload(const Bytes& payload) {
    if (payload.size() < 150)
        throw Error("expansion quote response is shorter than 150 bytes");
    const auto* row = payload.data();
    Json result = Json::object();
    result["schema"] = "tdx-expansion-quote-v1";
    result["market_id"] = static_cast<std::uint64_t>(row[0]);
    result["market"] = std::to_string(row[0]);
    result["code"] = decode_fixed_gbk(row + 1, 9);
    const auto price = [&](std::size_t offset) {
        const double value = read_f32_le(row + offset);
        if (!std::isfinite(value)) throw Error("expansion quote contains a non-finite price");
        return value;
    };
    const double pre_close = price(14);
    const double current = price(30);
    result["pre_close"] = pre_close;
    result["pre_settlement"] = pre_close;
    result["reference_price_semantics"] = "previous settlement for futures; previous close for other markets";
    result["open"] = price(18);
    result["high"] = price(22);
    result["low"] = price(26);
    result["price"] = current;
    result["opening_volume"] = static_cast<std::uint64_t>(read_u32_le(row + 34));
    result["volume"] = static_cast<std::uint64_t>(read_u32_le(row + 42));
    result["last_volume"] = static_cast<std::uint64_t>(read_u32_le(row + 46));
    result["inside_volume"] = static_cast<std::uint64_t>(read_u32_le(row + 54));
    result["outside_volume"] = static_cast<std::uint64_t>(read_u32_le(row + 58));
    result["open_interest"] = static_cast<std::uint64_t>(read_u32_le(row + 66));
    result["change"] = current - pre_close;
    result["change_percent"] = std::abs(pre_close) > 1e-15
        ? (current - pre_close) * 100.0 / pre_close : 0.0;
    Json unknown = Json::array();
    unknown.push_back(static_cast<std::uint64_t>(read_u32_le(row + 38)));
    unknown.push_back(static_cast<std::uint64_t>(read_u32_le(row + 50)));
    unknown.push_back(static_cast<std::uint64_t>(read_u32_le(row + 62)));
    result["unknown_u32"] = std::move(unknown);
    Json bids = Json::array(), asks = Json::array();
    for (std::size_t level = 0; level < 5; ++level) {
        Json bid = Json::object();
        bid["level"] = static_cast<std::uint64_t>(level + 1);
        bid["price"] = price(70 + level * 4);
        bid["volume"] = static_cast<std::uint64_t>(read_u32_le(row + 90 + level * 4));
        bids.push_back(std::move(bid));
        Json ask = Json::object();
        ask["level"] = static_cast<std::uint64_t>(level + 1);
        ask["price"] = price(110 + level * 4);
        ask["volume"] = static_cast<std::uint64_t>(read_u32_le(row + 130 + level * 4));
        asks.push_back(std::move(ask));
    }
    result["bids"] = std::move(bids);
    result["asks"] = std::move(asks);
    result["response_bytes"] = static_cast<std::uint64_t>(payload.size());
    return result;
}

Bytes build_expansion_minute_request_data(std::uint8_t market_id,
                                          std::string_view code) {
    if (!valid_expansion_code(code))
        throw Error("expansion minute code must contain 1..9 ASCII letters, digits, or internal spaces");
    Bytes result(10, 0);
    result[0] = market_id;
    std::copy(code.begin(), code.end(), result.begin() + 1);
    return result;
}

Bytes build_expansion_history_minute_request_data(std::uint32_t date,
                                                  std::uint8_t market_id,
                                                  std::string_view code) {
    if (!date) throw Error("historical expansion minute date cannot be zero");
    auto result = Bytes{};
    result.reserve(14);
    append_u32(result, date);
    const auto security = build_expansion_minute_request_data(market_id, code);
    result.insert(result.end(), security.begin(), security.end());
    return result;
}

Json parse_expansion_minute_payload(const Bytes& payload, int header_bytes) {
    if (header_bytes != 12 && header_bytes != 20)
        throw Error("expansion minute header must be 12 or 20 bytes");
    if (payload.size() < static_cast<std::size_t>(header_bytes))
        throw Error("expansion minute response is shorter than its header");
    const auto count = read_u16_le(payload.data() + header_bytes - 2);
    if (count > 2000) throw Error("expansion minute response count exceeds 2000");
    const auto expected = static_cast<std::size_t>(header_bytes) +
                          static_cast<std::size_t>(count) * 18;
    if (payload.size() != expected)
        throw Error("expansion minute response size mismatch: expected " +
                    std::to_string(expected) + ", got " +
                    std::to_string(payload.size()));
    Json result = Json::object();
    result["schema"] = "tdx-expansion-timeline-v1";
    result["wire_count"] = static_cast<std::uint64_t>(count);
    result["header_bytes"] = static_cast<std::uint64_t>(header_bytes);
    result["response_bytes"] = static_cast<std::uint64_t>(payload.size());
    if (header_bytes == 12) {
        result["response_market_id"] = static_cast<std::uint64_t>(payload[0]);
        result["response_code"] = decode_fixed_gbk(payload.data() + 1, 9);
    }
    Json points = Json::array();
    std::uint64_t invalid_time_count = 0;
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto* row = payload.data() + header_bytes + static_cast<std::size_t>(index) * 18;
        const auto wire_minute = read_u16_le(row);
        const auto minute = static_cast<std::uint16_t>(wire_minute % (24 * 60));
        const auto session_day_offset = wire_minute / (24 * 60);
        const double price = read_f32_le(row + 2);
        const double average = read_f32_le(row + 6);
        if (!std::isfinite(price) || !std::isfinite(average))
            throw Error("expansion minute record contains a non-finite price");
        Json point = Json::object();
        point["index"] = static_cast<std::uint64_t>(index);
        point["wire_minute"] = static_cast<std::uint64_t>(wire_minute);
        point["minute_of_day"] = static_cast<std::uint64_t>(minute);
        point["session_day_offset"] = static_cast<std::uint64_t>(session_day_offset);
        const bool valid_time = session_day_offset <= 1;
        point["valid_time"] = valid_time;
        point["time"] = valid_time ? Json(minute_label(minute)) : Json(nullptr);
        if (!valid_time) ++invalid_time_count;
        point["price"] = price;
        point["average_price"] = average;
        point["volume"] = static_cast<std::uint64_t>(read_u32_le(row + 10));
        point["open_interest"] = static_cast<std::uint64_t>(read_u32_le(row + 14));
        points.push_back(std::move(point));
    }
    result["invalid_time_count"] = invalid_time_count;
    result["points"] = std::move(points);
    return result;
}

Bytes build_expansion_trade_request_data(std::uint8_t market_id,
                                         std::string_view code,
                                         std::uint32_t start,
                                         std::uint16_t count) {
    if (!valid_expansion_code(code))
        throw Error("expansion trade code must contain 1..9 ASCII letters, digits, or internal spaces");
    if (!count || count > 1800)
        throw Error("expansion trade page size must be 1..1800");
    auto result = build_expansion_minute_request_data(market_id, code);
    append_u32(result, start);
    append_u16(result, count);
    return result;
}

Bytes build_expansion_history_trade_request_data(std::uint32_t date,
                                                 std::uint8_t market_id,
                                                 std::string_view code,
                                                 std::uint32_t start,
                                                 std::uint16_t count) {
    if (!date) throw Error("historical expansion trade date cannot be zero");
    Bytes result;
    result.reserve(20);
    append_u32(result, date);
    const auto request = build_expansion_trade_request_data(market_id, code, start, count);
    result.insert(result.end(), request.begin(), request.end());
    return result;
}

Json parse_expansion_trade_payload(const Bytes& payload, std::uint8_t market_id,
                                   std::uint32_t start) {
    constexpr std::size_t header_bytes = 16;
    constexpr double price_divisor = 1000.0;
    if (payload.size() < header_bytes)
        throw Error("expansion trade response is shorter than 16 bytes");
    const auto count = read_u16_le(payload.data() + 14);
    const auto expected = header_bytes + static_cast<std::size_t>(count) * 16;
    if (payload.size() != expected)
        throw Error("expansion trade response size mismatch: expected " +
                    std::to_string(expected) + ", got " +
                    std::to_string(payload.size()));
    Json result = Json::object();
    result["schema"] = "tdx-expansion-trades-v1";
    result["wire_count"] = static_cast<std::uint64_t>(count);
    result["response_market_id"] = static_cast<std::uint64_t>(payload[0]);
    result["response_code"] = decode_fixed_gbk(payload.data() + 1, 9);
    result["response_bytes"] = static_cast<std::uint64_t>(payload.size());
    result["price_divisor"] = price_divisor;
    Json trades = Json::array();
    for (std::uint16_t index = 0; index < count; ++index) {
        const auto* row = payload.data() + header_bytes + static_cast<std::size_t>(index) * 16;
        const auto minute = read_u16_le(row);
        if (minute >= 24 * 60)
            throw Error("expansion trade record contains an invalid minute-of-day");
        const auto price_raw = read_u32_le(row + 2);
        const auto volume = read_u32_le(row + 6);
        const auto open_interest_change = read_i32_le(row + 10);
        const auto nature_raw = read_u16_le(row + 14);
        int second = nature_raw % 10000;
        if (second > 59) second = 0;
        const auto nature = decode_expansion_trade_nature(
            market_id, nature_raw, volume, open_interest_change);
        Json trade = Json::object();
        trade["index"] = static_cast<std::uint64_t>(index);
        trade["absolute_index"] = static_cast<std::uint64_t>(start) + index;
        trade["minute_of_day"] = static_cast<std::uint64_t>(minute);
        trade["second"] = second;
        trade["time"] = minute_label(minute, second);
        trade["price_raw"] = static_cast<std::uint64_t>(price_raw);
        trade["price"] = price_raw / price_divisor;
        trade["volume"] = static_cast<std::uint64_t>(volume);
        trade["open_interest_change"] = static_cast<std::int64_t>(open_interest_change);
        trade["nature_raw"] = static_cast<std::uint64_t>(nature_raw);
        trade["nature_mark"] = static_cast<std::uint64_t>(nature_raw / 10000);
        trade["direction"] = nature.direction;
        trade["side"] = nature.side;
        trade["nature"] = nature.name;
        trades.push_back(std::move(trade));
    }
    result["trades"] = std::move(trades);
    return result;
}

Bytes pack_lc1(const std::vector<MinuteBar>& bars) {
    Bytes result;
    result.reserve(bars.size() * 32);
    for (const auto& bar : bars) {
        if (bar.has_expansion_fields)
            throw Error("LC1 cannot preserve expansion-market open-interest/auxiliary fields");
        if (bar.volume < 0 || bar.volume > std::numeric_limits<std::uint32_t>::max())
            throw Error("LC1 volume exceeds uint32 storage range");
        append_u16(result, encode_lc1_date(bar.date));
        append_u16(result, static_cast<std::uint16_t>(bar.hour * 60 + bar.minute));
        append_float(result, bar.open);
        append_float(result, bar.high);
        append_float(result, bar.low);
        append_float(result, bar.close);
        append_float(result, bar.amount);
        append_u32(result, static_cast<std::uint32_t>(bar.volume));
        append_u16(result, bar.extra_1);
        append_u16(result, bar.extra_2);
    }
    return result;
}

}  // namespace tdx
