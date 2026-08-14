#include "market_internal.hpp"

#include "tdx/security_identity.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>

namespace tdx::market_detail {

const QuoteSurfaceDefinition &quote_surface(QuoteSurface surface) {
    const auto found = std::find_if(
        quote_surface_definitions.begin(), quote_surface_definitions.end(),
        [surface](const QuoteSurfaceDefinition &definition) {
            return definition.surface == surface;
        });
    if (found == quote_surface_definitions.end())
        throw Error("unknown market quote surface");
    return *found;
}

std::string QuoteCode::display() const {
    return std::string(market_prefixes.at(static_cast<std::size_t>(market_id))) + code;
}

namespace {

void validate_code(const QuoteCode &value) {
    if (value.market_id < 0 || value.market_id > 2 || value.code.size() != 6 ||
        !std::all_of(value.code.begin(), value.code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) {
        throw Error("invalid quote security: " + std::to_string(value.market_id) +
                    ":" + value.code);
    }
}

std::int64_t consume_varint(const Bytes &payload, std::size_t &offset) {
    if (offset >= payload.size())
        throw Error("quote varint starts past record end");
    const auto first = payload[offset++];
    std::uint64_t value = first & 0x3F;
    int shift = 6;
    auto current = first;
    while (current & 0x80) {
        if (offset >= payload.size())
            throw Error("quote varint has no terminator");
        current = payload[offset++];
        if (shift > 55)
            throw Error("quote varint is too long");
        value += static_cast<std::uint64_t>(current & 0x7F) << shift;
        shift += 7;
    }
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw Error("quote varint exceeds int64");
    const auto signed_value = static_cast<std::int64_t>(value);
    return first & 0x40 ? -signed_value : signed_value;
}

double wire_number(std::uint32_t value) {
    if (value == 0)
        return 0.0;
    std::int32_t signed_value = 0;
    std::memcpy(&signed_value, &value, sizeof(value));
    const int exponent = signed_value >> 24;
    const unsigned high_byte = (value >> 16) & 0xFF;
    const unsigned middle_byte = (value >> 8) & 0xFF;
    const unsigned low_byte = value & 0xFF;
    const double base = std::pow(2.0, static_cast<double>(exponent * 2 - 0x7F));
    const double high = high_byte > 0x80
                            ? base * (64.0 + static_cast<double>(high_byte & 0x7F)) /
                                  64.0
                            : base * static_cast<double>(high_byte) / 128.0;
    const double scale = high_byte & 0x80 ? 2.0 : 1.0;
    return base + high + base * middle_byte / 32768.0 * scale +
           base * low_byte / 8388608.0 * scale;
}

PriceFields decode_prices(const Bytes &record, std::size_t &offset) {
    const auto current_delta = consume_varint(record, offset);
    const auto previous_delta = consume_varint(record, offset);
    const auto open_delta = consume_varint(record, offset);
    const auto high_delta = consume_varint(record, offset);
    const auto low_delta = consume_varint(record, offset);
    const auto current = current_delta * 10;
    return PriceFields{current, (previous_delta + current_delta) * 10,
                       (open_delta + current_delta) * 10,
                       (high_delta + current_delta) * 10,
                       (low_delta + current_delta) * 10};
}

bool is_fund_iopv_security(const QuoteCode &security) {
    // TdxW!sub_598CC0 uses this predicate before exposing the derived
    // auxiliary price to the TBigData $JJJZ host field.
    if (security.market_id == 0) {
        return security.code.rfind("158", 0) == 0 ||
               security.code.rfind("159", 0) == 0;
    }
    if (security.market_id != 1 || security.code.back() != '0')
        return false;
    static constexpr std::string_view prefixes[] = {
        "510", "511", "512", "513", "515", "516", "517", "518",
        "520", "530", "551", "560", "561", "562", "563", "588", "589",
    };
    return std::any_of(std::begin(prefixes), std::end(prefixes),
                       [&](std::string_view prefix) {
                           return security.code.rfind(prefix, 0) == 0;
                       });
}

std::vector<Bytes> split_snapshot_records(const Bytes &data, std::size_t count) {
    if (!count)
        return {};
    std::vector<std::size_t> starts;
    for (std::size_t position = 0; position + 7 <= data.size(); ++position) {
        if (data[position] > 2)
            continue;
        if (std::all_of(data.begin() + static_cast<std::ptrdiff_t>(position + 1),
                        data.begin() + static_cast<std::ptrdiff_t>(position + 7),
                        [](std::uint8_t ch) { return ch >= '0' && ch <= '9'; })) {
            starts.push_back(position);
        }
    }
    if (starts.size() != count || starts.front() != 0)
        throw Error("cannot identify snapshot record boundaries");
    std::vector<Bytes> result;
    for (std::size_t index = 0; index < starts.size(); ++index) {
        const auto end = index + 1 < starts.size() ? starts[index + 1] : data.size();
        result.emplace_back(data.begin() + static_cast<std::ptrdiff_t>(starts[index]),
                            data.begin() + static_cast<std::ptrdiff_t>(end));
    }
    return result;
}

Snapshot parse_snapshot_core(const Bytes &record, std::size_t &offset,
                             PriceFields *decoded_prices = nullptr) {
    if (record.size() < 9)
        throw Error("snapshot record header is incomplete");
    QuoteCode security{record[0], std::string(record.begin() + 1, record.begin() + 7)};
    validate_code(security);
    Snapshot result;
    result.security = security;
    result.active = read_u16_le(record.data() + 7);
    offset = 9;
    const auto prices = decode_prices(record, offset);
    if (decoded_prices)
        *decoded_prices = prices;
    result.time_raw = consume_varint(record, offset);
    result.auxiliary_price_delta_raw = consume_varint(record, offset);
    result.total_hand = consume_varint(record, offset);
    result.current_hand = consume_varint(record, offset);
    if (offset + 4 > record.size())
        throw Error("snapshot record has no amount field");
    result.amount = wire_number(read_u32_le(record.data() + offset));
    offset += 4;
    result.inside = consume_varint(record, offset);
    result.outside = consume_varint(record, offset);
    result.auction_imbalance_hand = consume_varint(record, offset);
    result.open_amount = static_cast<double>(consume_varint(record, offset)) * 100.0;
    const double scale = market_price_divisor_for_code(security.code) * 1000.0;
    result.last = prices.current / scale;
    result.previous = prices.previous / scale;
    result.open = prices.open / scale;
    result.high = prices.high / scale;
    result.low = prices.low / scale;
    if (is_fund_iopv_security(security)) {
        // sub_85DD80: auxiliary = current + wire_delta / 10^decimals;
        // sub_598CC0 stores auxiliary / 10 in the snapshot host object.
        const auto auxiliary =
            result.last + static_cast<double>(result.auxiliary_price_delta_raw) /
                              (scale / 10.0);
        if (auxiliary > 0.00009999999747378752)
            result.fund_iopv = auxiliary / 10.0;
    }
    for (double value : {result.last, result.previous, result.open, result.high,
                         result.low, result.amount}) {
        if (!std::isfinite(value))
            throw Error("snapshot contains non-finite value");
    }
    return result;
}

Snapshot parse_snapshot_record(const Bytes &record) {
    std::size_t offset = 0;
    return parse_snapshot_core(record, offset);
}

Speed parse_speed_record(const Bytes &record) {
    std::size_t offset = 0;
    PriceFields prices;
    Speed result;
    result.quote = parse_snapshot_core(record, offset, &prices);
    const double scale =
        market_price_divisor_for_code(result.quote.security.code) * 1000.0;
    for (std::size_t level = 0; level < 5; ++level) {
        const auto buy_delta = consume_varint(record, offset);
        const auto sell_delta = consume_varint(record, offset);
        result.buys[level] =
            Level{(prices.current + buy_delta * 10) / scale,
                  consume_varint(record, offset)};
        result.sells[level] =
            Level{(prices.current + sell_delta * 10) / scale,
                  consume_varint(record, offset)};
    }
    if (offset + 3 > record.size())
        throw Error("speed record extension header is incomplete");
    result.status = read_u16_le(record.data() + offset);
    offset += 2;
    result.extension_marker = record[offset++];
    for (auto &value : result.extension_values)
        value = consume_varint(record, offset);
    if (offset + 4 > record.size())
        throw Error("speed record value is incomplete");
    std::memcpy(&result.rise_speed_raw, record.data() + offset,
                sizeof(result.rise_speed_raw));
    offset += sizeof(result.rise_speed_raw);
    result.tail_raw = read_u16_le(record.data() + offset);
    offset += sizeof(result.tail_raw);
    if (offset != record.size())
        throw Error("speed record has unexpected trailing bytes");
    return result;
}

std::vector<Bytes> split_depth_records(const Bytes &data,
                                       const std::vector<QuoteCode> &requested,
                                       std::size_t count) {
    if (!count)
        return {};
    if (data.size() < 7)
        throw Error("depth response record data is too short");
    std::vector<std::size_t> starts{0};
    std::size_t search_from = 7;
    while (starts.size() < count) {
        std::size_t best = std::string::npos;
        for (const auto &code : requested) {
            Bytes marker{static_cast<std::uint8_t>(code.market_id)};
            marker.insert(marker.end(), code.code.begin(), code.code.end());
            const auto found =
                std::search(data.begin() + static_cast<std::ptrdiff_t>(search_from),
                            data.end(), marker.begin(), marker.end());
            if (found != data.end())
                best = std::min(best,
                                static_cast<std::size_t>(found - data.begin()));
        }
        if (best == std::string::npos)
            throw Error("cannot identify depth record boundaries");
        starts.push_back(best);
        search_from = best + 7;
    }
    std::vector<Bytes> result;
    for (std::size_t index = 0; index < starts.size(); ++index) {
        const auto end = index + 1 < starts.size() ? starts[index + 1] : data.size();
        result.emplace_back(data.begin() + static_cast<std::ptrdiff_t>(starts[index]),
                            data.begin() + static_cast<std::ptrdiff_t>(end));
    }
    return result;
}

Depth parse_depth_record(const Bytes &record) {
    if (record.size() < 9)
        throw Error("depth record header is incomplete");
    Depth result;
    result.quote.security =
        QuoteCode{record[0], std::string(record.begin() + 1, record.begin() + 7)};
    validate_code(result.quote.security);
    result.quote.active = read_u16_le(record.data() + 7);
    std::size_t offset = 9;
    const auto prices = decode_prices(record, offset);
    if (offset + 4 > record.size())
        throw Error("depth record has no update time");
    result.update_time = read_u32_le(record.data() + offset);
    offset += 4;
    result.status = consume_varint(record, offset);
    result.quote.total_hand = consume_varint(record, offset);
    result.quote.current_hand = consume_varint(record, offset);
    if (offset + 4 > record.size())
        throw Error("depth record has no amount");
    result.quote.amount = wire_number(read_u32_le(record.data() + offset));
    offset += 4;
    result.quote.inside = consume_varint(record, offset);
    result.quote.outside = consume_varint(record, offset);
    result.unknown_after_outer = consume_varint(record, offset);
    result.quote.auction_imbalance_hand = result.unknown_after_outer;
    result.quote.open_amount =
        static_cast<double>(consume_varint(record, offset)) * 10.0;
    const double scale =
        market_price_divisor_for_code(result.quote.security.code) * 1000.0;
    result.quote.last = prices.current / scale;
    result.quote.previous = prices.previous / scale;
    result.quote.open = prices.open / scale;
    result.quote.high = prices.high / scale;
    result.quote.low = prices.low / scale;
    for (std::size_t level = 0; level < 5; ++level) {
        const auto buy_delta = consume_varint(record, offset);
        const auto sell_delta = consume_varint(record, offset);
        result.first_level_raw[level] = buy_delta;
        result.second_level_raw[level] = sell_delta;
        result.buys[level] =
            Level{(prices.current + buy_delta * 10) / scale,
                  consume_varint(record, offset)};
        result.sells[level] =
            Level{(prices.current + sell_delta * 10) / scale,
                  consume_varint(record, offset)};
    }
    std::ostringstream tail;
    tail << std::hex << std::setfill('0');
    for (; offset < record.size(); ++offset)
        tail << std::setw(2) << static_cast<unsigned>(record[offset]);
    result.tail_hex = tail.str();
    return result;
}

} // namespace

QuoteCode parse_security(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    int market = -1;
    std::string code;
    const auto colon = value.find(':');
    if (colon != std::string::npos) {
        const auto prefix = value.substr(0, colon);
        code = value.substr(colon + 1);
        if (prefix == "sz")
            market = 0;
        else if (prefix == "sh")
            market = 1;
        else if (prefix == "bj")
            market = 2;
        else {
            try {
                market = std::stoi(prefix);
            } catch (...) {
                throw Error("invalid market prefix: " + prefix);
            }
        }
    } else if (value.size() == 8 &&
               (value.rfind("sz", 0) == 0 || value.rfind("sh", 0) == 0 ||
                value.rfind("bj", 0) == 0)) {
        market = value.rfind("sz", 0) == 0
                     ? 0
                     : value.rfind("sh", 0) == 0 ? 1 : 2;
        code = value.substr(2);
    } else {
        code = value;
        if (is_tdx_block_index_code(code) ||
            (!code.empty() && (code[0] == '6' || code[0] == '9')))
            market = 1;
        else if (!code.empty() && code[0] == '8')
            market = 2;
        else
            market = 0;
    }
    QuoteCode result{market, code};
    validate_code(result);
    return result;
}

Bytes snapshot_request(const std::vector<QuoteCode> &codes) {
    if (codes.size() > 0xFFFF)
        throw Error("snapshot batch exceeds uint16");
    Bytes result{5, 0, 0, 0, 0, 0, 0, 0,
                 static_cast<std::uint8_t>(codes.size()),
                 static_cast<std::uint8_t>(codes.size() >> 8)};
    for (const auto &item : codes) {
        result.push_back(static_cast<std::uint8_t>(item.market_id));
        result.insert(result.end(), item.code.begin(), item.code.end());
    }
    return result;
}

std::vector<Snapshot> parse_snapshots(const Bytes &payload, std::size_t requested) {
    if (payload.size() < 4)
        throw Error("snapshot response is shorter than four bytes");
    const auto count = read_u16_le(payload.data() + 2);
    if (count > requested)
        throw Error("snapshot response count exceeds request count");
    const Bytes records(payload.begin() + 4, payload.end());
    std::vector<Snapshot> result;
    for (const auto &record : split_snapshot_records(records, count))
        result.push_back(parse_snapshot_record(record));
    return result;
}

std::vector<Speed> parse_speeds(const Bytes &payload, std::size_t requested) {
    if (payload.size() < 4)
        throw Error("speed response is shorter than four bytes");
    const auto count = read_u16_le(payload.data() + 2);
    if (count > requested)
        throw Error("speed response count exceeds request count");
    const Bytes records(payload.begin() + 4, payload.end());
    std::vector<Speed> result;
    for (const auto &record : split_snapshot_records(records, count))
        result.push_back(parse_speed_record(record));
    return result;
}

Bytes depth_request(const std::vector<QuoteCode> &codes) {
    if (codes.size() > 0xFFFF)
        throw Error("depth batch exceeds uint16");
    Bytes result{static_cast<std::uint8_t>(codes.size()),
                 static_cast<std::uint8_t>(codes.size() >> 8)};
    for (const auto &item : codes) {
        result.push_back(static_cast<std::uint8_t>(item.market_id));
        result.insert(result.end(), item.code.begin(), item.code.end());
        result.insert(result.end(), 4, 0);
    }
    return result;
}

std::vector<Depth> parse_depths(Bytes payload,
                                const std::vector<QuoteCode> &requested) {
    for (auto &byte : payload)
        byte ^= depth_xor;
    if (payload.size() < 2)
        throw Error("depth response is shorter than two bytes");
    const auto count = read_u16_le(payload.data());
    if (count > requested.size())
        throw Error("depth response count exceeds request count");
    const Bytes records(payload.begin() + 2, payload.end());
    std::vector<Depth> result;
    std::set<std::pair<int, std::string>> requested_keys;
    for (const auto &item : requested)
        requested_keys.insert(item.key());
    for (const auto &record : split_depth_records(records, requested, count)) {
        auto depth = parse_depth_record(record);
        if (!requested_keys.count(depth.quote.security.key()))
            throw Error("depth response contains unrequested security");
        result.push_back(std::move(depth));
    }
    return result;
}

} // namespace tdx::market_detail

namespace tdx {

int market_price_divisor_for_code(std::string_view code) {
    // Bonds and exchange reverse repos use two extra wire decimal digits;
    // exchange funds use one. The ordered table keeps that protocol policy
    // out of decoder control flow.
    for (const auto &rule : market_detail::price_divisor_rules)
        if (code.rfind(rule.prefix, 0) == 0)
            return rule.divisor;
    return 1;
}

Json decode_market_speed_payload(const Bytes &payload, std::size_t requested_count) {
    using namespace market_detail;
    const auto speeds = parse_speeds(payload, requested_count);
    const std::map<std::pair<int, std::string>, Security> names;
    Json records = Json::array();
    for (const auto &item : speeds)
        records.push_back(speed_json(item, names));
    const auto &surface = quote_surface(QuoteSurface::Speed);
    Json document = Json::object();
    document["schema"] = std::string(surface.schema);
    document["command"] = std::string(surface.command_text);
    document["requested"] = static_cast<std::uint64_t>(requested_count);
    document["received"] = static_cast<std::uint64_t>(speeds.size());
    document["records"] = std::move(records);
    return document;
}

Json decode_market_depth_payload(const Bytes &payload,
                                 const std::vector<std::string> &securities) {
    using namespace market_detail;
    std::vector<QuoteCode> requested;
    std::set<std::pair<int, std::string>> seen;
    for (const auto &value : securities) {
        auto code = parse_security(value);
        if (seen.insert(code.key()).second)
            requested.push_back(std::move(code));
    }
    const auto depths = parse_depths(payload, requested);
    const std::map<std::pair<int, std::string>, Security> names;
    Json records = Json::array();
    for (const auto &item : depths)
        records.push_back(depth_json(item, names));
    const auto &surface = quote_surface(QuoteSurface::Depth);
    Json document = Json::object();
    document["schema"] = std::string(surface.schema);
    document["command"] = std::string(surface.command_text);
    document["requested"] = static_cast<std::uint64_t>(requested.size());
    document["received"] = static_cast<std::uint64_t>(depths.size());
    document["records"] = std::move(records);
    return document;
}

} // namespace tdx
