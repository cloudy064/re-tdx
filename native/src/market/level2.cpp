#include "tdx/level2.hpp"

#include "level2_tcalc_order_flow.hpp"
#include "level2_sdk_ticks.hpp"
#include "level2_tpbus_115.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr std::uint16_t transaction_initial_command = 1363;
constexpr std::uint16_t transaction_command = 1364;
constexpr std::uint16_t order_initial_command = 1373;
constexpr std::uint16_t order_command = 1374;
constexpr std::size_t direct_request_size = 26;
constexpr std::size_t multi_level_size = 0x7D10;
constexpr std::size_t order_queue_size = 0x4E2C;
constexpr std::size_t quote_header_size = 99;
constexpr std::size_t quote_level_size = 20;
constexpr std::size_t best_queue_header_size = 54;
constexpr std::uintmax_t sdk_json_4653_file_size_limit = level2_offline_payload_limit;

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

bool valid_utf8(std::string_view text) {
    std::size_t index = 0;
    while (index < text.size()) {
        const auto first = static_cast<unsigned char>(text[index++]);
        if (first <= 0x7f) continue;

        std::size_t continuation_count{};
        std::uint32_t codepoint{};
        std::uint32_t minimum{};
        if (first >= 0xc2 && first <= 0xdf) {
            continuation_count = 1;
            codepoint = first & 0x1fU;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuation_count = 2;
            codepoint = first & 0x0fU;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuation_count = 3;
            codepoint = first & 0x07U;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (continuation_count > text.size() - index) return false;
        for (std::size_t count = 0; count < continuation_count; ++count) {
            const auto next = static_cast<unsigned char>(text[index++]);
            if ((next & 0xc0U) != 0x80U) return false;
            codepoint = (codepoint << 6U) | (next & 0x3fU);
        }
        if (codepoint < minimum || codepoint > 0x10ffffU ||
            (codepoint >= 0xd800U && codepoint <= 0xdfffU))
            return false;
    }
    return true;
}

Json read_sdk_json_4653_input(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error)
        throw Error("cannot inspect SDK 4653 JSON input file size: " +
                    error.message());
    if (size == 0 || size > sdk_json_4653_file_size_limit)
        throw Error(
            "SDK 4653 JSON input must be a non-empty file no larger than 384 KiB");
    const auto bytes = read_bytes(path);
    if (bytes.size() > sdk_json_4653_file_size_limit)
        throw Error("SDK 4653 JSON input exceeds 384 KiB");
    const std::string text(reinterpret_cast<const char*>(bytes.data()),
                           bytes.size());
    if (!valid_utf8(text))
        throw Error("SDK 4653 JSON input must be valid UTF-8");
    return Json::parse(text);
}

void ensure(const Bytes& data, std::size_t offset, std::size_t size, std::string_view field) {
    if (offset > data.size() || size > data.size() - offset)
        throw Error(std::string(field) + " is truncated at offset " + std::to_string(offset));
}

std::uint32_t u32(const Bytes& data, std::size_t offset, std::string_view field) {
    ensure(data, offset, 4, field);
    return read_u32_le(data.data() + offset);
}

std::uint64_t u64(const Bytes& data, std::size_t offset, std::string_view field) {
    ensure(data, offset, 8, field);
    std::uint64_t value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

float f32(const Bytes& data, std::size_t offset, std::string_view field) {
    ensure(data, offset, 4, field);
    return read_f32_le(data.data() + offset);
}

double f64(const Bytes& data, std::size_t offset, std::string_view field) {
    ensure(data, offset, 8, field);
    double value = 0.0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

void append_u16(Bytes& result, std::uint16_t value) {
    result.push_back(static_cast<std::uint8_t>(value));
    result.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(Bytes& result, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        result.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::string hex_bytes(const Bytes& data, std::size_t maximum = std::numeric_limits<std::size_t>::max()) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    const auto size = std::min(data.size(), maximum);
    for (std::size_t index = 0; index < size; ++index)
        output << std::setw(2) << static_cast<unsigned>(data[index]);
    return output.str();
}

Bytes parse_hex(std::string text) {
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }), text.end());
    if (text.size() % 2) throw Error("hex input must contain an even number of digits");
    Bytes result;
    result.reserve(text.size() / 2);
    auto digit = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    for (std::size_t index = 0; index < text.size(); index += 2) {
        const int high = digit(text[index]);
        const int low = digit(text[index + 1]);
        if (high < 0 || low < 0) throw Error("hex input contains a non-hex character");
        result.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return result;
}

std::string fixed_ascii(const Bytes& data, std::size_t offset, std::size_t width,
                        std::string_view field) {
    ensure(data, offset, width, field);
    std::string result;
    for (std::size_t index = 0; index < width && data[offset + index]; ++index) {
        const auto ch = data[offset + index];
        if (ch > 0x7F) throw Error(std::string(field) + " is not ASCII");
        result.push_back(static_cast<char>(ch));
    }
    return result;
}

std::int64_t tdx_varint(const Bytes& data, std::size_t& offset, std::string_view field) {
    ensure(data, offset, 1, field);
    const auto first = data[offset++];
    std::uint64_t magnitude = first & 0x3F;
    int shift = 6;
    auto current = first;
    while (current & 0x80) {
        ensure(data, offset, 1, field);
        current = data[offset++];
        const auto chunk = static_cast<std::uint64_t>(current & 0x7F);
        if (shift > 62 || (shift == 62 && chunk > 1))
            throw Error(std::string(field) + " exceeds int64");
        magnitude |= chunk << shift;
        shift += 7;
    }
    if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw Error(std::string(field) + " exceeds int64");
    const auto value = static_cast<std::int64_t>(magnitude);
    return first & 0x40 ? -value : value;
}

std::pair<std::uint64_t, std::string> protobuf_varint(const Bytes& data, std::size_t& offset) {
    const auto start = offset;
    std::uint64_t value = 0;
    for (int shift = 0; shift < 70; shift += 7) {
        ensure(data, offset, 1, "protobuf varint");
        const auto byte = data[offset++];
        if (shift == 63 && byte > 1) throw Error("protobuf varint exceeds uint64");
        value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        if (!(byte & 0x80)) {
            return {value, hex_bytes(Bytes(data.begin() + static_cast<std::ptrdiff_t>(start),
                                           data.begin() + static_cast<std::ptrdiff_t>(offset)))};
        }
    }
    throw Error("protobuf varint exceeds 10 bytes");
}

std::string time_label(std::int64_t seconds) {
    if (seconds < 0 || seconds >= 86400) return "+" + std::to_string(seconds) + "s";
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << seconds / 3600 << ':'
           << std::setw(2) << seconds / 60 % 60 << ':' << std::setw(2) << seconds % 60;
    return output.str();
}

std::string transaction_side(std::int64_t status) {
    if (status == 0) return "buy";
    if (status == 1) return "sell";
    if (status == 2) return "neutral";
    if (status == -1) return "unknown";
    return "status_" + std::to_string(status);
}

std::string ascii_field(std::uint8_t value) {
    if (!value) return "";
    if (value >= 32 && value <= 126) return std::string(1, static_cast<char>(value));
    std::ostringstream output;
    output << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value);
    return output.str();
}

std::uint16_t direct_command(
    std::string kind,
    Level2DirectRequestVariant variant = Level2DirectRequestVariant::standard) {
    kind = lower_ascii(trim(std::move(kind)));
    if (kind == "transaction" || kind == "trade" || kind == "tick")
        return variant == Level2DirectRequestVariant::initial
            ? transaction_initial_command : transaction_command;
    if (kind == "order" || kind == "entrust")
        return variant == Level2DirectRequestVariant::initial
            ? order_initial_command : order_command;
    throw Error("Level2 kind must be transaction or order");
}

Json parse_direct(Bytes payload, const std::string& kind, int xor_key) {
    if (xor_key >= 0) {
        if (xor_key > 255) throw Error("xor key must be 0..255");
        for (auto& value : payload) value ^= static_cast<std::uint8_t>(xor_key);
    }
    ensure(payload, 0, 6, "direct response header");
    const auto count = read_u16_le(payload.data());
    const auto next_cursor = read_u32_le(payload.data() + 2);
    if (count & 0x8000) throw Error("server returned an error count");
    if (count > 1500) throw Error("direct response count exceeds 1500");
    const auto command = direct_command(kind);
    std::size_t offset = 6;
    std::int64_t price = 0;
    Json records = Json::array();
    for (std::uint16_t index = 0; index < count; ++index) {
        ensure(payload, offset, 2, "record time");
        const auto time_offset = read_u16_le(payload.data() + offset);
        offset += 2;
        const auto price_delta = tdx_varint(payload, offset, "price_delta");
        if ((price_delta > 0 &&
             price > std::numeric_limits<std::int64_t>::max() - price_delta) ||
            (price_delta < 0 &&
             price < std::numeric_limits<std::int64_t>::min() - price_delta))
            throw Error("cumulative Level2 price overflows int64");
        price += price_delta;
        if (price < 0) throw Error("cumulative Level2 price became negative");
        const auto volume = tdx_varint(payload, offset, "volume");
        Json row = Json::object();
        row["index"] = index;
        row["time_offset_seconds"] = time_offset;
        row["time_seconds"] = static_cast<std::uint64_t>(time_offset) + 21600;
        row["time"] = time_label(static_cast<std::int64_t>(time_offset) + 21600);
        row["price_raw"] = price;
        row["price"] = price / 10000.0;
        row["volume_raw"] = volume;
        if (command == transaction_command) {
            row["order_count_raw"] = tdx_varint(payload, offset, "order_count");
            const auto status = tdx_varint(payload, offset, "status");
            row["status_raw"] = status;
            row["side"] = transaction_side(status);
            row["sequence_raw"] = tdx_varint(payload, offset, "sequence");
        } else {
            ensure(payload, offset, 3, "order flags");
            row["order_type_raw"] = payload[offset];
            row["order_type"] = ascii_field(payload[offset]);
            row["side_raw"] = payload[offset + 1];
            row["side"] = ascii_field(payload[offset + 1]);
            row["action_raw"] = payload[offset + 2];
            row["action"] = ascii_field(payload[offset + 2]);
            offset += 3;
            row["order_id_raw"] = tdx_varint(payload, offset, "order_id");
        }
        records.push_back(std::move(row));
    }
    if (offset != payload.size()) throw Error("direct response has " +
        std::to_string(payload.size() - offset) + " trailing bytes");
    Json result = Json::object();
    result["schema"] = "tdx-level2-direct-v1";
    result["kind"] = command == transaction_command ? "transaction" : "order";
    result["count"] = count;
    result["next_cursor"] = static_cast<std::uint64_t>(next_cursor);
    result["consumed_bytes"] = static_cast<std::uint64_t>(offset);
    result["records"] = std::move(records);
    return result;
}

std::pair<std::size_t, std::size_t> side_limits(std::size_t first, std::size_t second,
                                                std::size_t limit) {
    auto first_limit = std::min(first, (limit + 1) / 2);
    auto second_limit = std::min(second, limit - first_limit);
    auto remaining = limit - first_limit - second_limit;
    const auto add_first = std::min(first - first_limit, remaining);
    first_limit += add_first;
    remaining -= add_first;
    second_limit += std::min(second - second_limit, remaining);
    return {first_limit, second_limit};
}

Json parse_sdk_1803(const Bytes& payload, int limit) {
    if (payload.size() != multi_level_size)
        throw Error("1803 callback requires exactly 32016 bytes");
    const auto first_count = u32(payload, 8, "first count");
    const auto second_count = u32(payload, 12, "second count");
    if (first_count > 1000 || second_count > 1000) throw Error("1803 side count exceeds 1000");
    const auto [first_limit, second_limit] = side_limits(first_count, second_count,
                                                         static_cast<std::size_t>(limit));
    auto levels = [&](std::size_t count, std::size_t price_offset,
                      std::size_t volume_offset, std::size_t auxiliary_offset) {
        Json rows = Json::array();
        for (std::size_t index = 0; index < count; ++index) {
            Json row = Json::object();
            row["index"] = static_cast<std::uint64_t>(index);
            row["price"] = f64(payload, price_offset + index * 8, "level price");
            row["volume_raw"] = static_cast<std::uint64_t>(
                u32(payload, volume_offset + index * 4, "level volume"));
            ensure(payload, auxiliary_offset + index * 4, 2, "level auxiliary");
            row["auxiliary_raw"] = read_u16_le(payload.data() + auxiliary_offset + index * 4);
            rows.push_back(std::move(row));
        }
        return rows;
    };
    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-1803-v1";
    result["data_type"] = 1803;
    result["record_size"] = static_cast<std::uint64_t>(multi_level_size);
    result["body_count"] = 1;
    result["batch_semantics"] = "single-fixed-body";
    result["header_u32_0"] = static_cast<std::uint64_t>(u32(payload, 0, "header 0"));
    result["header_u32_1"] = static_cast<std::uint64_t>(u32(payload, 4, "header 1"));
    result["first_side_count"] = static_cast<std::uint64_t>(first_count);
    result["second_side_count"] = static_cast<std::uint64_t>(second_count);
    result["first_side_levels"] = levels(first_limit, 16, 8016, 12016);
    result["second_side_levels"] = levels(second_limit, 16016, 24016, 28016);
    result["truncated"] = first_limit < first_count || second_limit < second_count;
    return result;
}

Json parse_sdk_18031(const Bytes& payload, int limit) {
    if (payload.size() != order_queue_size)
        throw Error("18031 callback requires exactly 20012 bytes");
    const auto count = u32(payload, 8, "queue count");
    if (count > 5000) throw Error("18031 queue count exceeds 5000");
    const auto selected = std::min<std::size_t>(count, static_cast<std::size_t>(limit));
    Json quantities = Json::array();
    for (std::size_t index = 0; index < selected; ++index)
        quantities.push_back(static_cast<std::uint64_t>(
            u32(payload, 12 + index * 4, "queue quantity")));
    Json result = Json::object();
    result["schema"] = "tdx-level2-sdk-18031-v1";
    result["data_type"] = 18031;
    result["record_size"] = static_cast<std::uint64_t>(order_queue_size);
    result["body_count"] = 1;
    result["batch_semantics"] = "single-fixed-body";
    result["header_u32_0"] = static_cast<std::uint64_t>(u32(payload, 0, "header 0"));
    result["header_u32_1"] = static_cast<std::uint64_t>(u32(payload, 4, "header 1"));
    result["count"] = static_cast<std::uint64_t>(count);
    result["quantities_raw"] = std::move(quantities);
    result["truncated"] = selected < count;
    return result;
}

Json parse_tpbus_111(const Bytes& payload, int limit) {
    if (payload.size() < quote_header_size) throw Error("tpbus 111 push requires at least 99 bytes");
    const auto signed_count = static_cast<std::int8_t>(payload[24]);
    if (signed_count < 0) throw Error("tpbus 111 depth count is negative");
    const auto count = static_cast<std::size_t>(signed_count);
    ensure(payload, quote_header_size, count * quote_level_size, "tpbus 111 levels");
    const auto selected = std::min<std::size_t>(count, static_cast<std::size_t>(limit));
    Json levels = Json::array();
    for (std::size_t index = 0; index < selected; ++index) {
        const auto offset = quote_header_size + index * quote_level_size;
        Json row = Json::object();
        row["index"] = static_cast<std::uint64_t>(index);
        row["buy_price"] = f32(payload, offset, "buy price");
        row["buy_volume_raw"] = static_cast<std::uint64_t>(u32(payload, offset + 4, "buy volume"));
        ensure(payload, offset + 8, 2, "buy seats");
        row["buy_seat_count"] = read_u16_le(payload.data() + offset + 8);
        row["sell_price"] = f32(payload, offset + 10, "sell price");
        row["sell_volume_raw"] = static_cast<std::uint64_t>(u32(payload, offset + 14, "sell volume"));
        ensure(payload, offset + 18, 2, "sell seats");
        row["sell_seat_count"] = read_u16_le(payload.data() + offset + 18);
        levels.push_back(std::move(row));
    }
    Json result = Json::object();
    result["schema"] = "tdx-level2-tpbus-111-v1";
    result["push_type"] = 111;
    ensure(payload, 0, 2, "market");
    result["market_id"] = read_u16_le(payload.data());
    result["code"] = fixed_ascii(payload, 2, 22, "security code");
    result["depth_count"] = static_cast<std::uint64_t>(count);
    result["hq_time_raw"] = static_cast<std::uint64_t>(u32(payload, 35, "hq time"));
    result["item_number"] = static_cast<std::uint64_t>(u32(payload, 39, "item number"));
    result["close"] = f32(payload, 43, "close");
    result["open"] = f32(payload, 47, "open");
    result["high"] = f32(payload, 51, "high");
    result["low"] = f32(payload, 55, "low");
    result["last"] = f32(payload, 59, "last");
    result["lead"] = f32(payload, 63, "lead");
    result["volume_raw"] = static_cast<std::uint64_t>(u32(payload, 67, "volume"));
    result["rest_volume_raw"] = static_cast<std::uint64_t>(u32(payload, 71, "rest volume"));
    result["amount_raw"] = f32(payload, 75, "amount");
    result["volume_in_stock_raw"] = static_cast<std::uint64_t>(u32(payload, 79, "volume in stock"));
    result["jjjz"] = f32(payload, 83, "jjjz");
    result["in_out_flag"] = payload[87];
    result["hktt_flag"] = payload[88];
    result["volume_unit"] = payload[89];
    result["ph_volume"] = f32(payload, 90, "ph volume");
    result["levels"] = std::move(levels);
    result["truncated"] = selected < count;
    return result;
}

Json parse_tpbus_112(const Bytes& payload, int limit) {
    if (payload.size() < best_queue_header_size) throw Error("tpbus 112 push requires at least 54 bytes");
    const auto buy_count = u32(payload, 36, "buy count");
    const auto sell_count = u32(payload, 40, "sell count");
    const auto total = static_cast<std::uint64_t>(buy_count) + sell_count;
    if (total > 1000000) throw Error("tpbus 112 queue count exceeds safety limit");
    ensure(payload, best_queue_header_size, static_cast<std::size_t>(total) * 4, "tpbus 112 quantities");
    const auto [buy_limit, sell_limit] = side_limits(buy_count, sell_count,
                                                     static_cast<std::size_t>(limit));
    Json buys = Json::array(), sells = Json::array();
    for (std::size_t index = 0; index < buy_limit; ++index)
        buys.push_back(static_cast<std::uint64_t>(
            u32(payload, best_queue_header_size + index * 4, "buy quantity")));
    const auto sell_offset = best_queue_header_size + static_cast<std::size_t>(buy_count) * 4;
    for (std::size_t index = 0; index < sell_limit; ++index)
        sells.push_back(static_cast<std::uint64_t>(
            u32(payload, sell_offset + index * 4, "sell quantity")));
    Json result = Json::object();
    result["schema"] = "tdx-level2-tpbus-112-v1";
    result["push_type"] = 112;
    ensure(payload, 0, 2, "market");
    result["market_id"] = read_u16_le(payload.data());
    result["code"] = fixed_ascii(payload, 2, 22, "security code");
    result["refresh_number"] = static_cast<std::uint64_t>(u32(payload, 24, "refresh number"));
    result["buy_price"] = f32(payload, 28, "buy price");
    result["sell_price"] = f32(payload, 32, "sell price");
    result["buy_count"] = static_cast<std::uint64_t>(buy_count);
    result["sell_count"] = static_cast<std::uint64_t>(sell_count);
    result["buy_quantities_raw"] = std::move(buys);
    result["sell_quantities_raw"] = std::move(sells);
    result["truncated"] = buy_limit < buy_count || sell_limit < sell_count;
    return result;
}

bool printable_utf8_ascii(const Bytes& value) {
    if (value.empty()) return false;
    return std::all_of(value.begin(), value.end(), [](std::uint8_t ch) {
        return (ch >= 32 && ch <= 126) || ch == '\r' || ch == '\n' || ch == '\t';
    });
}

Json inspect_protobuf(const Bytes& payload, int max_fields, int sample_bytes) {
    std::size_t offset = 0;
    Json fields = Json::array();
    static const std::map<int, std::string> names{{0, "varint"}, {1, "fixed64"},
                                                  {2, "length-delimited"}, {5, "fixed32"}};
    while (offset < payload.size() && fields.size() < static_cast<std::size_t>(max_fields)) {
        const auto field_offset = offset;
        const auto [key, key_hex] = protobuf_varint(payload, offset);
        const auto number = key >> 3;
        const auto wire = static_cast<int>(key & 7);
        if (!number) throw Error("protobuf field number 0");
        if (!names.count(wire)) throw Error("unsupported protobuf wire type " + std::to_string(wire));
        Json field = Json::object();
        field["index"] = static_cast<std::uint64_t>(fields.size());
        field["offset"] = static_cast<std::uint64_t>(field_offset);
        field["field_number"] = number;
        field["wire_type"] = wire;
        field["wire_type_name"] = names.at(wire);
        field["key_hex"] = key_hex;
        if (wire == 0) {
            const auto [value, value_hex] = protobuf_varint(payload, offset);
            field["value_unsigned"] = value;
            field["value_hex"] = value_hex;
        } else if (wire == 1) {
            field["value_unsigned"] = u64(payload, offset, "protobuf fixed64");
            field["value_hex"] = hex_bytes(Bytes(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                                                   payload.begin() + static_cast<std::ptrdiff_t>(offset + 8)));
            offset += 8;
        } else if (wire == 5) {
            field["value_unsigned"] = static_cast<std::uint64_t>(
                u32(payload, offset, "protobuf fixed32"));
            field["value_hex"] = hex_bytes(Bytes(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                                                   payload.begin() + static_cast<std::ptrdiff_t>(offset + 4)));
            offset += 4;
        } else {
            const auto [length, length_hex] = protobuf_varint(payload, offset);
            if (length > payload.size() - offset) throw Error("protobuf length-delimited value is truncated");
            Bytes value(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                        payload.begin() + static_cast<std::ptrdiff_t>(offset + length));
            field["length"] = length;
            field["length_hex"] = length_hex;
            field["sample_hex"] = hex_bytes(value, static_cast<std::size_t>(sample_bytes));
            field["sample_truncated"] = value.size() > static_cast<std::size_t>(sample_bytes);
            if (value.size() <= static_cast<std::size_t>(sample_bytes) && printable_utf8_ascii(value))
                field["text_candidate"] = std::string(value.begin(), value.end());
            offset += static_cast<std::size_t>(length);
        }
        fields.push_back(std::move(field));
    }
    Json result = Json::object();
    result["schema"] = "tdx-level2-protobuf-wire-v1";
    result["encoding"] = "protobuf-wire-unknown-schema";
    result["size"] = static_cast<std::uint64_t>(payload.size());
    result["field_count_reported"] = static_cast<std::uint64_t>(fields.size());
    result["field_limit_reached"] = offset < payload.size();
    result["next_offset"] = static_cast<std::uint64_t>(offset);
    result["fields"] = std::move(fields);
    result["interpretation_note"] = "field numbers and wire types only; business schema is unknown";
    return result;
}

int option_integer(Args& args, std::string_view name, int fallback, int minimum, int maximum) {
    const auto text = args.take_option(name, std::to_string(fallback));
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(text, &consumed, 0);
        if (consumed != text.size() || value < minimum || value > maximum) throw std::invalid_argument("range");
        return value;
    } catch (...) { throw Error(std::string(name) + " is outside the safe range"); }
}

std::uint32_t option_u32(Args& args, std::string_view name,
                         std::uint32_t fallback) {
    const auto text = args.take_option(name, std::to_string(fallback));
    try {
        if (text.empty() || text.front() == '-') throw std::invalid_argument("range");
        std::size_t consumed = 0;
        const auto value = std::stoull(text, &consumed, 0);
        if (consumed != text.size() ||
            value > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("range");
        return static_cast<std::uint32_t>(value);
    } catch (...) {
        throw Error(std::string(name) + " must be an unsigned 32-bit integer");
    }
}

float option_float(Args& args, std::string_view name, float fallback) {
    const auto text = args.take_option(name, std::to_string(fallback));
    try {
        std::size_t consumed = 0;
        const float value = std::stof(text, &consumed);
        if (consumed != text.size() || !std::isfinite(value))
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be a finite number");
    }
}

bool strict_utf8(std::string_view text) {
    for (std::size_t index = 0; index < text.size();) {
        const auto lead = static_cast<unsigned char>(text[index++]);
        if (lead <= 0x7f) continue;
        auto continuation = [&](unsigned char minimum, unsigned char maximum) {
            if (index >= text.size()) return false;
            const auto byte = static_cast<unsigned char>(text[index++]);
            return byte >= minimum && byte <= maximum;
        };
        if (lead >= 0xc2 && lead <= 0xdf) {
            if (!continuation(0x80, 0xbf)) return false;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            const auto second_minimum = lead == 0xe0 ? 0xa0 : 0x80;
            const auto second_maximum = lead == 0xed ? 0x9f : 0xbf;
            if (!continuation(second_minimum, second_maximum) ||
                !continuation(0x80, 0xbf))
                return false;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            const auto second_minimum = lead == 0xf0 ? 0x90 : 0x80;
            const auto second_maximum = lead == 0xf4 ? 0x8f : 0xbf;
            if (!continuation(second_minimum, second_maximum) ||
                !continuation(0x80, 0xbf) || !continuation(0x80, 0xbf))
                return false;
        } else {
            return false;
        }
    }
    return true;
}

Level2SdkFnSubscribeBatchCallPlanRequest parse_fnsubscribe_batch_input(
    const Json& document) {
    if (!document.is_object())
        throw Error("sdk-fnsubscribe-batch-plan input must be a JSON object");
    const auto& object = document.as_object();
    for (const auto& [name, value] : object) {
        (void)value;
        if (name != "data_type" && name != "symbols")
            throw Error("sdk-fnsubscribe-batch-plan input contains an unsupported field: " +
                        name);
    }
    const auto type = object.find("data_type");
    const auto symbols = object.find("symbols");
    if (type == object.end() || symbols == object.end())
        throw Error("sdk-fnsubscribe-batch-plan input requires data_type and symbols");
    if (!type->second.is_number())
        throw Error("sdk-fnsubscribe-batch-plan data_type must be an integer");
    const double type_number = type->second.as_number();
    if (!std::isfinite(type_number) || std::trunc(type_number) != type_number ||
        type_number < 0 ||
        type_number > static_cast<double>(std::numeric_limits<int>::max()))
        throw Error("sdk-fnsubscribe-batch-plan data_type must be an integer");
    if (!symbols->second.is_array())
        throw Error("sdk-fnsubscribe-batch-plan symbols must be an array");

    Level2SdkFnSubscribeBatchCallPlanRequest request;
    request.data_type = static_cast<Level2SdkFnSubscribeDataType>(
        static_cast<int>(type_number));
    request.symbols.reserve(symbols->second.as_array().size());
    for (const auto& symbol : symbols->second.as_array()) {
        if (!symbol.is_string())
            throw Error("sdk-fnsubscribe-batch-plan symbols must contain only strings");
        request.symbols.push_back(symbol.as_string());
    }
    return request;
}

void build_help() {
    std::cout <<
        "Usage: tdx-tool level2 build --format direct|sdk-4653|sdk-4655|sdk-4680\n"
        "       --market N --code CODE [options]\n"
        "       tdx-tool level2 build --format tdxw-1369 --market N --code CODE\n"
        "       [--count 1..1000] [options]\n"
        "       tdx-tool level2 build --format tdxw-1371 --market N --code CODE\n"
        "       --side-mode-raw N --selected-price P [--cursor N]\n"
        "       [--count 1..5000] [options]\n"
        "       tdx-tool level2 build --format sdk-1807-plan --input FILE\n"
        "       [--encoding raw|hex] [options]\n\n"
        "       tdx-tool level2 build --format sdk-fnreqdata-plan\n"
        "       --data-type 1801|1802|1803|1804|18071 --market N --code CODE\n"
        "       [--cursor-raw N --count 1..1500] [options]\n\n"
        "       tdx-tool level2 build --format sdk-fnreqdata-18031-plan\n"
        "       --market N --code CODE --side-mode-raw N --selected-price P\n"
        "       [options]\n\n"
        "       tdx-tool level2 build --format sdk-fnsubscribe-batch-plan\n"
        "       --input FILE [--output FILE] [--compact]\n\n"
        "       tdx-tool level2 build --format sdk-callback-route-plan\n"
        "       --data-type 1801|1802|1803|1804|1807|18031|18071\n"
        "       [--registry-mode-raw N] [--host-time-advanced true|false]\n"
        "       [--output FILE] [--compact]\n\n"
        "       tdx-tool level2 build --format fasthq-subscribe-plan\n"
        "       --market N --code CODE --lx-raw N [--unsubscribe] [options]\n\n"
        "Build a recovered request body offline. No session is opened.\n\n"
        "Direct: --kind transaction|order --cursor N --count 1..1500 [--initial]\n"
        "        default commands are 1364/1374; --initial selects 1363/1373.\n"
        "SDK 4653: --attach-info --repurchase-time\n"
        "SDK 4655: --want-number 1..65535 --attach-info (values >500 become 80)\n"
        "SDK 4680: --depth 5|10\n"
        "TdxW 1369: fixed 40-byte internal IPC body for data type 1803.\n"
        "TdxW 1371: fixed 48-byte internal IPC body for data type 18031;\n"
        "           side-mode-raw is not interpreted as buy/sell.\n"
        "TdxW bodies are not SDK calls or standalone network frames.\n"
        "SDK 1807 plan: 1..100 packed 7-byte security records; emits typed JSON,\n"
        "               not network request bytes.\n"
        "SDK fnReqData plan: 1801/1802 require explicit --cursor-raw and --count;\n"
        "                    1803/1804/18071 fix them to 0/1 and reject overrides.\n"
        "                    Emits unresolved ABI/callback metadata; never calls SDK.\n"
        "SDK fnReqData 18031: preserves raw side mode and finite float price;\n"
        "                     callback correlation requires live host context.\n"
        "                     It does not create a 25-byte local registry record.\n"
        "SDK fnSubscribe batch: strict UTF-8 JSON object with data_type 1801,\n"
        "                       1802, or 1803 and symbols [\"SZ000001\",...].\n"
        "                       Accepts 1..100 pre-resolved symbols; never calls SDK.\n"
        "SDK callback route: 18031 rejects --registry-mode-raw; every other type\n"
        "                    requires its full u32 value. The optional 1807 host\n"
        "                    time state selects a branch; omission preserves both.\n"
        "                    Emits host dispatch metadata and sends no message.\n"
        "FastHQ plan: exact logical Name/CODE/SC/LX/PkgType/OperType/PushType/\n"
        "             BatchPush fields; LX remains an uninterpreted signed raw value.\n"
        "             No TQL body, job enqueue, network bytes, or subscription.\n"
        "Common: --binary-output FILE --output FILE --compact\n";
}

void decode_help() {
    std::cout <<
        "Usage: tdx-tool level2 decode --format FORMAT --input FILE [options]\n\n"
        "Formats: direct-transaction, direct-order, sdk-1801, sdk-1802,\n"
        "         sdk-1803, sdk-18031, sdk-1804, sdk-1807, sdk-18071,\n"
        "         sdk-json-4653, sdk-json-4655, sdk-json-4671, sdk-json-4680\n"
        "         (UTF-8 JSON; sdk-json-4653 input is capped at 384 KiB),\n"
        "         sdk-correlation (25-byte local registry records),\n"
        "         sdk-callback-invocation (sub_68C750 offline envelope),\n"
        "         tpbus-111, tpbus-112, tpbus-115, tcalc-order-flow, tcalc-order-side,\n"
        "         protobuf.\n\n"
        "Options: --encoding raw|hex --xor-key N --limit N --max-fields N\n"
        "         --sample-bytes N --depth 5|10 (sdk-json-4680 only)\n"
        "Callback invocation: --data-type 1801|1802|1803|1804|1807|18031|18071\n"
        "         --arg5-raw N --arg6-raw U32 [--registry-mode-raw U32]\n"
        "         (registry mode is required except for 18031, which rejects it)\n"
        "         --output FILE --compact\n";
}

void session_help() {
    std::cout <<
        "Usage: tdx-tool level2 session [--process TdxW.exe] [--pid N] [--compact]\n\n"
        "Read-only process/module/hash preflight. It does not attach, subscribe, or read tokens.\n";
}

}  // namespace

Bytes build_level2_direct_request(const std::string& kind, int market_id,
                                  const std::string& code, std::uint32_t cursor,
                                  std::uint16_t count,
                                  Level2DirectRequestVariant variant) {
    if (market_id < 0 || market_id > 2) throw Error("market must be 0, 1, or 2");
    if (code.size() != 6 || !std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("code must contain exactly six digits");
    if (!count || count > 1500) throw Error("count must be 1..1500");
    Bytes result;
    result.reserve(direct_request_size);
    append_u32(result, 0);
    append_u32(result, 0x00100000);
    append_u16(result, 16);
    append_u16(result, direct_command(kind, variant));
    append_u16(result, static_cast<std::uint16_t>(market_id));
    result.insert(result.end(), code.begin(), code.end());
    append_u32(result, cursor);
    append_u16(result, count);
    return result;
}

Bytes build_level2_sdk_redirect_request(
    int function_id, int market_id, const std::string& code,
    std::uint16_t want_number, int depth, bool attach_info,
    bool repurchase_time) {
    if (function_id != 4653 && function_id != 4655 && function_id != 4680)
        throw Error("SDK redirect function must be 4653, 4655, or 4680");
    if (market_id < 0 || market_id > 65535)
        throw Error("SDK redirect market must be 0..65535");
    if (code.empty() || code.size() >= 22 ||
        !std::all_of(code.begin(), code.end(), [](unsigned char ch) {
            return ch > 0 && ch <= 0x7f;
        }))
        throw Error("SDK redirect code must contain 1..21 ASCII bytes");
    if (function_id == 4655 && !want_number)
        throw Error("SDK 4655 want-number must be 1..65535");
    if (function_id == 4680 && depth != 5 && depth != 10)
        throw Error("SDK 4680 depth must be 5 or 10");
    const std::size_t size = function_id == 4653 ? 40 :
                             function_id == 4655 ? 46 : 37;
    Bytes result(size, 0);
    result[0] = static_cast<std::uint8_t>(function_id);
    result[1] = static_cast<std::uint8_t>(function_id >> 8);
    result[2] = static_cast<std::uint8_t>(market_id);
    result[3] = static_cast<std::uint8_t>(market_id >> 8);
    std::copy(code.begin(), code.end(), result.begin() + 4);
    if (function_id == 4653) {
        result[30] = attach_info ? 1 : 0;
        result[31] = repurchase_time ? 1 : 0;
    } else if (function_id == 4655) {
        if (want_number > 500) want_number = 80;
        result[34] = static_cast<std::uint8_t>(want_number);
        result[35] = static_cast<std::uint8_t>(want_number >> 8);
        result[36] = attach_info ? 1 : 0;
    } else {
        result[26] = static_cast<std::uint8_t>(depth);
    }
    return result;
}

Json decode_level2_document(const std::string& selected_format, Bytes payload,
                            int limit, int xor_key, int max_fields, int sample_bytes) {
    if (limit < 0 || limit > 10000 || max_fields < 1 || max_fields > 10000 ||
        sample_bytes < 0 || sample_bytes > 4096)
        throw Error("Level2 decode options are outside the safe range");
    const auto format = lower_ascii(trim(selected_format));
    Json result;
    if (format == "direct-transaction") result = parse_direct(std::move(payload), "transaction", xor_key);
    else if (format == "direct-order") result = parse_direct(std::move(payload), "order", xor_key);
    else if (format == "sdk-1801")
        result = level2_detail::decode_sdk_transactions(payload, limit);
    else if (format == "sdk-1802")
        result = level2_detail::decode_sdk_orders(payload, limit);
    else if (format == "sdk-1803") result = parse_sdk_1803(payload, limit);
    else if (format == "sdk-18031") result = parse_sdk_18031(payload, limit);
    else if (format == "sdk-1804")
        result = level2_detail::decode_sdk_price_queues(payload, limit);
    else if (format == "sdk-1807")
        result = level2_detail::decode_sdk_quote_updates(payload, limit, 1807);
    else if (format == "sdk-18071")
        result = level2_detail::decode_sdk_quote_updates(payload, limit, 18071);
    else if (format == "sdk-correlation")
        result = decode_level2_sdk_correlation_registry(payload, limit);
    else if (format == "tpbus-111") result = parse_tpbus_111(payload, limit);
    else if (format == "tpbus-112") result = parse_tpbus_112(payload, limit);
    else if (format == "tpbus-115") {
        Level2Tpbus115BatchDecodeRequest request;
        request.payload = std::move(payload);
        request.summary_limit = limit;
        result = decode_level2_tpbus_115_batch(request);
    }
    else if (format == "tcalc-order-flow")
        result = level2_detail::decode_tcalc_order_flow_document(payload, limit);
    else if (format == "tcalc-order-side")
        result = level2_detail::decode_tcalc_order_side_document(payload);
    else if (format == "protobuf") result = inspect_protobuf(payload, max_fields, sample_bytes);
    else throw Error("unsupported Level2 decode format");
    result["offline"] = true;
    result["entitlement_bypass"] = false;
    return result;
}

Json level2_session_preflight_document(const std::string& process_name, std::uint32_t process_id) {
    Json result = Json::object();
    result["schema"] = "tdx-level2-session-preflight-v1";
    result["read_only"] = true;
    result["attached"] = false;
    result["subscription_sent"] = false;
    result["token_accessed"] = false;
    result["entitlement_bypass"] = false;
    result["process"] = process_name;
#ifdef _WIN32
    DWORD selected = process_id;
    if (!selected) {
        const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) throw Error("cannot enumerate processes");
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        std::vector<DWORD> matches;
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (lower_ascii(wide_to_utf8(entry.szExeFile)) == lower_ascii(process_name))
                    matches.push_back(entry.th32ProcessID);
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        if (matches.size() > 1) {
            Json candidates = Json::array();
            for (const auto match : matches) candidates.push_back(static_cast<std::uint64_t>(match));
            result["running"] = true;
            result["ambiguous"] = true;
            result["candidate_pids"] = std::move(candidates);
            result["ready_for_passive_capture"] = false;
            result["reason"] = "multiple matching processes; select a candidate pid for module verification";
            return result;
        }
        if (!matches.empty()) selected = matches.front();
    }
    result["running"] = selected != 0;
    result["ambiguous"] = false;
    result["pid"] = static_cast<std::uint64_t>(selected);
    if (!selected) {
        result["ready_for_passive_capture"] = false;
        result["reason"] = "process is not running";
        return result;
    }
    const auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, selected);
    if (!handle) throw Error("cannot query selected process");
    std::wstring path_buffer(32768, L'\0');
    DWORD path_size = static_cast<DWORD>(path_buffer.size());
    if (!QueryFullProcessImageNameW(handle, 0, path_buffer.data(), &path_size)) {
        CloseHandle(handle);
        throw Error("cannot query process image path");
    }
    CloseHandle(handle);
    path_buffer.resize(path_size);
    const fs::path image(path_buffer);
    const auto executable_hash = lower_ascii(sha256_file(image));
    constexpr const char* known_executable = "f5f2e6025a4d80bb1afbcb2a51c753d3c1909e09aa9d1bc8f7c3b701b081f74c";
    constexpr const char* known_tpbus = "0b6576270baf5b8421df7c282820306dcaee382b90f8b04ffd7e1075ebbcb481";
    result["image_path"] = path_utf8(image);
    result["tdxw_sha256"] = executable_hash;
    result["tdxw_recognized"] = executable_hash == known_executable;
    Json modules = Json::array();
    bool tpbus_loaded = false;
    bool tpbus_recognized = false;
    const auto module_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, selected);
    if (module_snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W module{};
        module.dwSize = sizeof(module);
        if (Module32FirstW(module_snapshot, &module)) {
            do {
                const auto name = lower_ascii(wide_to_utf8(module.szModule));
                if (name == "tpbus.dll" || name == "taapi.dll" || name == "tdxasiocomm.dll" ||
                    name == "tcalc.dll" || name == "tpool.dll") {
                    Json item = Json::object();
                    item["name"] = wide_to_utf8(module.szModule);
                    item["path"] = wide_to_utf8(module.szExePath);
                    if (name == "tpbus.dll") {
                        tpbus_loaded = true;
                        const auto hash = lower_ascii(sha256_file(fs::path(module.szExePath)));
                        item["sha256"] = hash;
                        item["recognized"] = hash == known_tpbus;
                        tpbus_recognized = hash == known_tpbus;
                    }
                    modules.push_back(std::move(item));
                }
            } while (Module32NextW(module_snapshot, &module));
        }
        CloseHandle(module_snapshot);
    }
    result["modules"] = std::move(modules);
    result["tpbus_loaded"] = tpbus_loaded;
    result["tpbus_recognized"] = tpbus_recognized;
    result["ready_for_passive_capture"] = executable_hash == known_executable &&
                                             tpbus_loaded && tpbus_recognized;
    result["reason"] = result.at("ready_for_passive_capture").as_bool()
        ? "known process and tpbus versions are loaded; an authorized business event is still required"
        : "known process/module prerequisites are incomplete";
#else
    result["running"] = false;
    result["ready_for_passive_capture"] = false;
    result["reason"] = "session preflight is only available on Windows";
#endif
    return result;
}

int command_level2_build(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { build_help(); return 0; }
    const auto format = lower_ascii(args.take_option("--format", "direct"));
    const bool is_tdxw_1369 = format == "tdxw-1369";
    const bool is_tdxw_1371 = format == "tdxw-1371";
    const bool is_sdk_fnreqdata_plan = format == "sdk-fnreqdata-plan";
    const bool is_sdk_fnreqdata_18031_plan =
        format == "sdk-fnreqdata-18031-plan";
    const bool is_sdk_fnsubscribe_batch_plan =
        format == "sdk-fnsubscribe-batch-plan";
    const bool is_sdk_callback_route_plan =
        format == "sdk-callback-route-plan";
    const bool is_fasthq_plan = format == "fasthq-subscribe-plan";
    const bool has_market = args.has("--market");
    const bool has_code = args.has("--code");
    const bool has_kind = args.has("--kind");
    const bool has_cursor = args.has("--cursor");
    const bool has_count = args.has("--count");
    const bool has_want_number = args.has("--want-number");
    const bool has_depth = args.has("--depth");
    const bool has_attach_info = args.has("--attach-info");
    const bool has_repurchase_time = args.has("--repurchase-time");
    const bool has_input = args.has("--input");
    const bool has_encoding = args.has("--encoding");
    const bool has_initial = args.has("--initial");
    const bool has_lx_raw = args.has("--lx-raw");
    const bool has_unsubscribe = args.has("--unsubscribe");
    const bool has_data_type = args.has("--data-type");
    const bool has_cursor_raw = args.has("--cursor-raw");
    const bool has_registry_mode_raw = args.has("--registry-mode-raw");
    const bool has_host_time_advanced = args.has("--host-time-advanced");
    const auto kind = args.take_option("--kind", "transaction");
    const int market = option_integer(
        args, "--market", 0, 0,
        (format == "direct" || is_tdxw_1369 || is_tdxw_1371 ||
         is_sdk_fnreqdata_plan || is_sdk_fnreqdata_18031_plan) ? 2 : 65535);
    const auto code = args.take_option("--code");
    const int cursor = option_integer(
        args, "--cursor", is_tdxw_1371 ? -1 : 0,
        is_tdxw_1371 ? std::numeric_limits<int>::min() : 0,
        std::numeric_limits<int>::max());
    const int count = option_integer(
        args, "--count", is_tdxw_1369 ? 11 : (is_tdxw_1371 ? 5000 : 1500),
        1, is_tdxw_1369 ? 1000 : (is_tdxw_1371 ? 5000 : 1500));
    const int want_number = option_integer(
        args, "--want-number", 80, 1, 65535);
    const int depth = option_integer(args, "--depth", 10, 5, 10);
    const bool attach_info = args.take_flag("--attach-info");
    const bool repurchase_time = args.take_flag("--repurchase-time");
    const bool initial = args.take_flag("--initial");
    const int lx_raw = option_integer(
        args, "--lx-raw", 0, std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max());
    const bool unsubscribe = args.take_flag("--unsubscribe");
    const int data_type = option_integer(
        args, "--data-type", 0, 0, std::numeric_limits<int>::max());
    const auto cursor_raw = option_u32(args, "--cursor-raw", 0);
    const auto registry_mode_raw =
        option_u32(args, "--registry-mode-raw", 0);
    const auto host_time_advanced_text =
        lower_ascii(args.take_option("--host-time-advanced"));
    std::optional<bool> host_time_advanced;
    if (has_host_time_advanced) {
        if (host_time_advanced_text == "true") host_time_advanced = true;
        else if (host_time_advanced_text == "false")
            host_time_advanced = false;
        else
            throw Error("--host-time-advanced must be true or false");
    }
    const bool has_side_mode_raw = args.has("--side-mode-raw");
    const bool has_selected_price = args.has("--selected-price");
    const int side_mode_raw = option_integer(
        args, "--side-mode-raw", 0, 0, 255);
    const float selected_price = option_float(args, "--selected-price", 0.0F);
    const auto input = args.take_option("--input");
    const auto encoding = lower_ascii(args.take_option("--encoding", "raw"));
    const auto binary_output = args.take_option("--binary-output");
    const auto output = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (has_initial && format != "direct")
        throw Error("--initial is only valid for direct Level2 requests");
    if ((has_lx_raw || has_unsubscribe) && !is_fasthq_plan)
        throw Error("--lx-raw and --unsubscribe are only valid for fasthq-subscribe-plan");
    if (has_cursor_raw && !is_sdk_fnreqdata_plan)
        throw Error("--cursor-raw is only valid for sdk-fnreqdata-plan");
    if (has_data_type && !is_sdk_fnreqdata_plan &&
        !is_sdk_callback_route_plan)
        throw Error("--data-type is only valid for SDK typed plan formats");
    if ((has_registry_mode_raw || has_host_time_advanced) &&
        !is_sdk_callback_route_plan)
        throw Error("--registry-mode-raw and --host-time-advanced are only valid for sdk-callback-route-plan");
    if (is_sdk_fnsubscribe_batch_plan) {
        if (!has_input || input.empty())
            throw Error("sdk-fnsubscribe-batch-plan requires --input FILE");
        if (has_market || has_code || has_kind || has_cursor || has_count ||
            has_want_number || has_depth || has_attach_info ||
            has_repurchase_time || has_encoding || has_side_mode_raw ||
            has_selected_price)
            throw Error("sdk-fnsubscribe-batch-plan accepts only --input and JSON output options");
        if (!binary_output.empty())
            throw Error("sdk-fnsubscribe-batch-plan does not produce --binary-output bytes");
        constexpr std::size_t input_limit = 16 * 1024;
        const auto input_text = read_text_utf8(from_utf8(input));
        if (input_text.empty() || input_text.size() > input_limit)
            throw Error("sdk-fnsubscribe-batch-plan input must be 1..16384 UTF-8 bytes");
        if (!strict_utf8(input_text))
            throw Error("sdk-fnsubscribe-batch-plan input must be valid UTF-8");
        const auto request =
            parse_fnsubscribe_batch_input(Json::parse(input_text));
        const auto report = build_level2_sdk_fnsubscribe_batch_call_plan(request)
                                .dump(compact ? -1 : 2) + "\n";
        if (output.empty()) std::cout << report;
        else atomic_write_text(from_utf8(output), report);
        return 0;
    }
    if (format == "sdk-1807-plan") {
        if (input.empty())
            throw Error("level2 build sdk-1807-plan requires --input");
        if (!binary_output.empty())
            throw Error("sdk-1807-plan does not produce --binary-output network bytes");
        Bytes records;
        if (encoding == "raw") records = read_bytes(from_utf8(input));
        else if (encoding == "hex")
            records = parse_hex(read_text_utf8(from_utf8(input)));
        else throw Error("encoding must be raw or hex");
        const auto report = build_level2_sdk_1807_request_plan(records)
                                .dump(compact ? -1 : 2) + "\n";
        if (output.empty()) std::cout << report;
        else atomic_write_text(from_utf8(output), report);
        return 0;
    }
    if (is_sdk_callback_route_plan) {
        if (!has_data_type)
            throw Error("sdk-callback-route-plan requires --data-type");
        if (has_market || has_code || has_kind || has_cursor || has_count ||
            has_want_number || has_depth || has_attach_info ||
            has_repurchase_time || has_input || has_encoding || has_initial ||
            has_lx_raw || has_unsubscribe || has_cursor_raw ||
            has_side_mode_raw || has_selected_price)
            throw Error("sdk-callback-route-plan accepts only --data-type, --registry-mode-raw, --host-time-advanced, and JSON output options");
        if (!binary_output.empty())
            throw Error("sdk-callback-route-plan does not produce --binary-output bytes");

        Level2SdkCallbackRoutePlanRequest request;
        request.data_type =
            static_cast<Level2SdkCallbackDataType>(data_type);
        if (has_registry_mode_raw)
            request.registry_mode_raw = registry_mode_raw;
        request.host_time_advanced = host_time_advanced;
        const auto report = build_level2_sdk_callback_route_plan(request)
                                .dump(compact ? -1 : 2) + "\n";
        if (output.empty()) std::cout << report;
        else atomic_write_text(from_utf8(output), report);
        return 0;
    }
    if (is_sdk_fnreqdata_plan) {
        if (!has_market || !has_code || !has_data_type)
            throw Error("sdk-fnreqdata-plan requires --data-type, --market, and --code");
        const bool variable_window = data_type == 1801 || data_type == 1802;
        const bool fixed_window = data_type == 1803 || data_type == 1804 ||
                                  data_type == 18071;
        if (!variable_window && !fixed_window)
            throw Error("sdk-fnreqdata-plan data-type must be 1801, 1802, 1803, 1804, or 18071");
        if (variable_window && (!has_cursor_raw || !has_count))
            throw Error("sdk-fnreqdata-plan 1801/1802 require --cursor-raw and --count");
        if (fixed_window && (has_cursor_raw || has_count))
            throw Error("sdk-fnreqdata-plan 1803/1804/18071 use fixed cursor 0 and count 1");
        if (has_kind || has_cursor || has_want_number || has_depth ||
            has_attach_info || has_repurchase_time || has_input ||
            has_encoding || has_side_mode_raw || has_selected_price)
            throw Error("sdk-fnreqdata-plan accepts only its typed fields and JSON output options");
        if (!binary_output.empty())
            throw Error("sdk-fnreqdata-plan does not produce --binary-output bytes");

        Level2SdkFnReqDataCallPlanRequest request;
        request.market_id = static_cast<std::uint16_t>(market);
        request.code = code;
        request.data_type = static_cast<Level2SdkFnReqDataType>(data_type);
        if (has_cursor_raw) request.cursor_raw = cursor_raw;
        if (has_count)
            request.request_count = static_cast<std::uint16_t>(count);
        const auto report = build_level2_sdk_fnreqdata_call_plan(request)
                                .dump(compact ? -1 : 2) + "\n";
        if (output.empty()) std::cout << report;
        else atomic_write_text(from_utf8(output), report);
        return 0;
    }
    if (is_sdk_fnreqdata_18031_plan) {
        if (!has_market || !has_code || !has_side_mode_raw ||
            !has_selected_price)
            throw Error("sdk-fnreqdata-18031-plan requires --market, --code, --side-mode-raw, and --selected-price");
        if (has_kind || has_cursor || has_count || has_want_number ||
            has_depth || has_attach_info || has_repurchase_time || has_input ||
            has_encoding)
            throw Error("sdk-fnreqdata-18031-plan accepts only its typed fields and JSON output options");
        if (!binary_output.empty())
            throw Error("sdk-fnreqdata-18031-plan does not produce --binary-output bytes");
        const auto report = build_level2_sdk_fnreqdata_18031_call_plan(
            {static_cast<std::uint16_t>(market), code,
             static_cast<std::uint8_t>(side_mode_raw), selected_price})
                                .dump(compact ? -1 : 2) + "\n";
        if (output.empty()) std::cout << report;
        else atomic_write_text(from_utf8(output), report);
        return 0;
    }
    if (is_fasthq_plan) {
        if (!has_market || !has_code || !has_lx_raw)
            throw Error("fasthq-subscribe-plan requires --market, --code, and --lx-raw");
        if (has_kind || has_cursor || has_count || has_want_number ||
            has_depth || has_attach_info || has_repurchase_time || has_input ||
            has_encoding || has_side_mode_raw || has_selected_price)
            throw Error("fasthq-subscribe-plan accepts only --market, --code, --lx-raw, --unsubscribe, and JSON output options");
        if (!binary_output.empty())
            throw Error("fasthq-subscribe-plan does not produce --binary-output bytes");
        const auto report = build_level2_fasthq_subscribe_job_plan(
            {static_cast<std::uint16_t>(market), code,
             static_cast<std::int32_t>(lx_raw),
             unsubscribe ? Level2FastHqOperation::unsubscribe
                         : Level2FastHqOperation::subscribe})
                                .dump(compact ? -1 : 2) + "\n";
        if (output.empty()) std::cout << report;
        else atomic_write_text(from_utf8(output), report);
        return 0;
    }
    Bytes payload;
    int function_id = 0;
    int tdxw_command = 0;
    int tdxw_data_type = 0;
    if (is_tdxw_1369) {
        if (has_kind || has_cursor || has_want_number || has_depth ||
            has_attach_info || has_repurchase_time || has_side_mode_raw ||
            has_selected_price || has_input || has_encoding)
            throw Error("tdxw-1369 accepts only --market, --code, --count and common output options");
        payload = build_level2_tdxw_1369_request(
            {static_cast<std::uint16_t>(market), code,
             static_cast<std::uint16_t>(count)});
        tdxw_command = 1369;
        tdxw_data_type = 1803;
    } else if (is_tdxw_1371) {
        if (!has_side_mode_raw || !has_selected_price)
            throw Error("tdxw-1371 requires --side-mode-raw and --selected-price");
        if (has_kind || has_want_number || has_depth || has_attach_info ||
            has_repurchase_time || has_input || has_encoding)
            throw Error("tdxw-1371 accepts only its typed fields and common output options");
        payload = build_level2_tdxw_1371_request(
            {static_cast<std::uint16_t>(market), code,
             static_cast<std::uint8_t>(side_mode_raw), selected_price,
             static_cast<std::int32_t>(cursor),
             static_cast<std::uint16_t>(count)});
        tdxw_command = 1371;
        tdxw_data_type = 18031;
    } else if (format == "direct") {
        payload = build_level2_direct_request(
            kind, market, code, static_cast<std::uint32_t>(cursor),
            static_cast<std::uint16_t>(count),
            initial ? Level2DirectRequestVariant::initial
                    : Level2DirectRequestVariant::standard);
    } else if (format.rfind("sdk-", 0) == 0) {
        try {
            function_id = std::stoi(format.substr(4));
        } catch (...) {
            throw Error("Level2 build format must be direct, sdk-4653, sdk-4655, or sdk-4680");
        }
        payload = build_level2_sdk_redirect_request(
            function_id, market, code,
            static_cast<std::uint16_t>(want_number), depth, attach_info,
            repurchase_time);
    } else {
        throw Error("Level2 build format must be direct, sdk-4653, sdk-4655, or sdk-4680");
    }
    if (!binary_output.empty()) atomic_write_bytes(from_utf8(binary_output), payload);
    Json result = Json::object();
    result["schema"] = tdxw_command
        ? "tdx-level2-tdxw-ipc-request-v1"
        : (function_id ? "tdx-level2-sdk-redirect-request-v1"
                       : "tdx-level2-direct-request-v1");
    result["format"] = format;
    if (tdxw_command) {
        result["request_kind"] = "tdxw-internal-ipc-fixed-body";
        result["command"] = tdxw_command;
        result["data_type"] = tdxw_data_type;
        result["request_count"] = count;
        if (is_tdxw_1371) {
            result["side_mode_raw"] = side_mode_raw;
            result["selected_price"] = static_cast<double>(selected_price);
            result["cursor"] = cursor;
        }
        result["sdk_request"] = false;
        result["standalone_network_request"] = false;
        result["transport"] = "TdxW internal IPC";
        result["network_request_bytes"] = false;
        result["network_requests"] = 0;
        result["request_sent"] = false;
    } else if (!function_id) {
        const auto variant = initial ? Level2DirectRequestVariant::initial
                                     : Level2DirectRequestVariant::standard;
        const auto command = direct_command(kind, variant);
        result["kind"] = command == transaction_command ||
                                  command == transaction_initial_command
                              ? "transaction" : "order";
        result["variant"] = initial ? "initial" : "standard";
        result["initial"] = initial;
        result["command"] = command;
        result["cursor"] = cursor;
        result["count"] = count;
    } else {
        result["function_id"] = function_id;
        if (function_id == 4655)
            result["want_number_effective"] = want_number > 500 ? 80 : want_number;
        if (function_id == 4680) result["depth"] = depth;
        result["attach_info"] = attach_info;
        result["repurchase_time"] = repurchase_time;
    }
    result["market_id"] = market;
    result["code"] = code;
    result["size"] = static_cast<std::uint64_t>(payload.size());
    result["payload_hex"] = hex_bytes(payload);
    result["offline"] = true;
    result["subscription_sent"] = false;
    result["entitlement_bypass"] = false;
    const auto report = result.dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << report;
    else atomic_write_text(from_utf8(output), report);
    return 0;
}

int command_level2_decode(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { decode_help(); return 0; }
    const auto format = args.take_option("--format");
    const auto input = args.take_option("--input");
    if (format.empty() || input.empty()) throw Error("level2 decode requires --format and --input");
    const bool has_encoding = args.has("--encoding");
    const bool has_xor_key = args.has("--xor-key");
    const bool has_max_fields = args.has("--max-fields");
    const bool has_sample_bytes = args.has("--sample-bytes");
    const bool has_depth = args.has("--depth");
    const bool has_data_type = args.has("--data-type");
    const bool has_arg5_raw = args.has("--arg5-raw");
    const bool has_arg6_raw = args.has("--arg6-raw");
    const bool has_registry_mode_raw = args.has("--registry-mode-raw");
    const bool is_sdk_callback_invocation =
        format == "sdk-callback-invocation";
    const auto encoding = lower_ascii(args.take_option("--encoding", "raw"));
    const int xor_key = option_integer(args, "--xor-key", -1, -1, 255);
    const int limit = option_integer(args, "--limit", 20, 0, 10000);
    const int max_fields = option_integer(args, "--max-fields", 100, 1, 10000);
    const int sample_bytes = option_integer(args, "--sample-bytes", 64, 0, 4096);
    const int depth = option_integer(args, "--depth", 10, 5, 10);
    const int data_type = option_integer(
        args, "--data-type", 0, 0, std::numeric_limits<int>::max());
    const int callback_arg5_raw = option_integer(
        args, "--arg5-raw", 0, std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max());
    const auto callback_arg6_raw = option_u32(args, "--arg6-raw", 0);
    const auto registry_mode_raw =
        option_u32(args, "--registry-mode-raw", 0);
    const auto output = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const bool sdk_json = format == "sdk-json-4653" ||
                          format == "sdk-json-4655" ||
                          format == "sdk-json-4671" ||
                          format == "sdk-json-4680";
    if (!is_sdk_callback_invocation &&
        (has_data_type || has_arg5_raw || has_arg6_raw ||
         has_registry_mode_raw))
        throw Error("callback ABI options are only valid for sdk-callback-invocation");
    if (sdk_json) {
        if (has_encoding || has_xor_key || has_max_fields || has_sample_bytes)
            throw Error("SDK JSON decode accepts only --limit, sdk-json-4680 --depth, and output options");
        if (has_depth && format != "sdk-json-4680")
            throw Error("--depth is only valid for sdk-json-4680");
        const int function_id = format == "sdk-json-4653" ? 4653 :
                                format == "sdk-json-4655" ? 4655 :
                                format == "sdk-json-4671" ? 4671 : 4680;
        const auto document = function_id == 4653
            ? read_sdk_json_4653_input(from_utf8(input))
            : Json::parse(read_text_utf8(from_utf8(input)));
        const auto report = normalize_level2_sdk_json(
            function_id, document, limit, depth).dump(compact ? -1 : 2) + "\n";
        if (output.empty()) std::cout << report;
        else atomic_write_text(from_utf8(output), report);
        return 0;
    }
    if (is_sdk_callback_invocation) {
        if (!has_data_type || !has_arg5_raw || !has_arg6_raw)
            throw Error("sdk-callback-invocation requires --data-type, --arg5-raw, and --arg6-raw");
        if (has_xor_key || has_max_fields || has_sample_bytes || has_depth)
            throw Error("sdk-callback-invocation accepts only its callback ABI fields, --encoding, --limit, and output options");
        constexpr std::uintmax_t callback_body_limit = level2_offline_payload_limit;
        constexpr std::uintmax_t callback_hex_text_limit =
            callback_body_limit * 2 + 64 * 1024;
        if (encoding != "raw" && encoding != "hex")
            throw Error("encoding must be raw or hex");
        std::error_code size_error;
        const auto input_size = fs::file_size(from_utf8(input), size_error);
        if (size_error)
            throw Error("cannot inspect sdk-callback-invocation input file size");
        if ((encoding == "raw" && input_size > callback_body_limit) ||
            (encoding == "hex" && input_size > callback_hex_text_limit))
            throw Error("sdk-callback-invocation input exceeds its offline safety limit");
        Bytes payload;
        if (encoding == "raw") payload = read_bytes(from_utf8(input));
        else if (encoding == "hex")
            payload = parse_hex(read_text_utf8(from_utf8(input)));
        if (payload.size() > callback_body_limit)
            throw Error("sdk-callback-invocation decoded body exceeds 384 KiB");

        Level2SdkCallbackInvocationRequest request;
        request.data_type =
            static_cast<Level2SdkCallbackDataType>(data_type);
        request.callback_arg5_raw = callback_arg5_raw;
        request.callback_arg6_raw = callback_arg6_raw;
        if (has_registry_mode_raw)
            request.registry_mode_raw = registry_mode_raw;
        request.body = std::move(payload);
        const auto report = decode_level2_sdk_callback_invocation(
                                request, limit)
                                .dump(compact ? -1 : 2) + "\n";
        if (output.empty()) std::cout << report;
        else atomic_write_text(from_utf8(output), report);
        return 0;
    }
    if (has_depth) throw Error("--depth is only valid for sdk-json-4680");
    if (format == "tpbus-115") {
        if (has_xor_key || has_max_fields || has_sample_bytes)
            throw Error(
                "tpbus-115 decode accepts only --encoding, --limit, and output options");
        const auto payload = level2_detail::read_level2_tpbus_115_cli_input(
            from_utf8(input), encoding);
        const auto report = decode_level2_document(
            format, payload, limit, xor_key, max_fields, sample_bytes)
                                .dump(compact ? -1 : 2) + "\n";
        if (output.empty()) std::cout << report;
        else atomic_write_text(from_utf8(output), report);
        return 0;
    }
    Bytes payload;
    if (encoding == "raw") payload = read_bytes(from_utf8(input));
    else if (encoding == "hex") payload = parse_hex(read_text_utf8(from_utf8(input)));
    else throw Error("encoding must be raw or hex");
    const auto report = decode_level2_document(format, std::move(payload), limit, xor_key,
                                                max_fields, sample_bytes).dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << report;
    else atomic_write_text(from_utf8(output), report);
    return 0;
}

int command_level2_session(const std::vector<std::string>& raw_args) {
    Args args(raw_args);
    if (args.take_flag("--help") || args.take_flag("-h")) { session_help(); return 0; }
    const auto process = args.take_option("--process", "TdxW.exe");
    const int pid = option_integer(args, "--pid", 0, 0, std::numeric_limits<int>::max());
    const auto output = args.take_option("--output");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    const auto report = level2_session_preflight_document(process, static_cast<std::uint32_t>(pid))
                            .dump(compact ? -1 : 2) + "\n";
    if (output.empty()) std::cout << report;
    else atomic_write_text(from_utf8(output), report);
    return 0;
}

}  // namespace tdx
