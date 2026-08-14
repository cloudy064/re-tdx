#include "minute_download_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/security_identity.hpp"
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
#include <tuple>

namespace fs = std::filesystem;

namespace tdx::minute_download_detail {

void write_u16(Bytes& data, std::size_t offset, std::uint16_t value) {
    data[offset] = static_cast<std::uint8_t>(value);
    data[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void append_u16(Bytes& data, std::uint16_t value) {
    data.push_back(static_cast<std::uint8_t>(value));
    data.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(Bytes& data, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        data.push_back(static_cast<std::uint8_t>(value >> shift));
}

void append_float(Bytes& data, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(data, bits);
}

std::int64_t consume_varint(const Bytes& payload, std::size_t& offset) {
    if (offset >= payload.size()) throw Error("K-line varint starts past payload end");
    const auto first = payload[offset];
    std::uint64_t magnitude = first & 0x3F;
    int shift = 6;
    std::uint8_t current = first;
    ++offset;
    while (current & 0x80) {
        if (offset >= payload.size()) throw Error("K-line varint has no terminator");
        current = payload[offset++];
        if (shift > 62) throw Error("K-line varint is too long");
        magnitude += static_cast<std::uint64_t>(current & 0x7F) << shift;
        shift += 7;
    }
    if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        throw Error("K-line varint exceeds int64");
    const auto value = static_cast<std::int64_t>(magnitude);
    return first & 0x40 ? -value : value;
}

double decode_wire_number(std::uint32_t value) {
    if (value == 0) return 0.0;
    std::int32_t signed_value = 0;
    std::memcpy(&signed_value, &value, sizeof(value));
    const int exponent = signed_value >> 24;
    const auto high_byte = static_cast<unsigned>((value >> 16) & 0xFF);
    const auto middle_byte = static_cast<unsigned>((value >> 8) & 0xFF);
    const auto low_byte = static_cast<unsigned>(value & 0xFF);
    const double base = std::pow(2.0, static_cast<double>(exponent * 2 - 0x7F));
    const double high = high_byte > 0x80
        ? base * (64.0 + static_cast<double>(high_byte & 0x7F)) / 64.0
        : base * static_cast<double>(high_byte) / 128.0;
    const double scale = high_byte & 0x80 ? 2.0 : 1.0;
    const double middle = base * static_cast<double>(middle_byte) / 32768.0 * scale;
    const double low = base * static_cast<double>(low_byte) / 8388608.0 * scale;
    return base + high + middle + low;
}

std::uint16_t encode_lc1_date(int value) {
    const int year = value / 10000;
    const int month = value / 100 % 100;
    const int day = value % 100;
    const int encoded = (year - 2004) * 2048 + month * 100 + day;
    if (encoded < 0 || encoded > 0xFFFF || decode_lc1_date(static_cast<std::uint16_t>(encoded)) != value)
        throw Error("date cannot be encoded in LC1: " + std::to_string(value));
    return static_cast<std::uint16_t>(encoded);
}

int parse_integer(const std::string& text, std::string_view option, int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(option) + " must be an integer in " +
                    std::to_string(minimum) + ".." + std::to_string(maximum));
    }
}

std::pair<std::string, std::uint16_t> normalize_market(std::string market,
                                                       const std::string& code) {
    market = lower_ascii(trim(std::move(market)));
    if (market.empty()) {
        if (code.empty()) throw Error("market cannot be inferred from an empty code");
        if (is_tdx_block_index_code(code) || code.front() == '6' ||
            code.front() == '9') market = "sh";
        else if (code.front() == '8') market = "bj";
        else market = "sz";
    }
    if (market == "0" || market == "sz") return {"sz", 0};
    if (market == "1" || market == "sh") return {"sh", 1};
    if (market == "2" || market == "bj") return {"bj", 2};
    static const std::map<std::string, std::uint16_t> expansion_markets{
        {"qz", 28}, {"qd", 29}, {"qs", 30}, {"cz", 47}, {"qg", 66}
    };
    if (const auto found = expansion_markets.find(market); found != expansion_markets.end())
        return {market, found->second};
    try {
        std::size_t used = 0;
        const auto id = std::stoul(market, &used);
        if (used == market.size() && id <= 0xFFFF)
            return {market, static_cast<std::uint16_t>(id)};
    } catch (...) {}
    throw Error("market must be sz/sh/bj, qz/qd/qs/cz/qg, or a numeric TDX market ID");
}

bool valid_kline_code(std::string_view code, std::size_t maximum_size) {
    return !code.empty() && code.size() <= maximum_size &&
           std::all_of(code.begin(), code.end(), [](unsigned char ch) {
               return std::isalnum(ch) != 0;
           });
}

std::uint32_t normalize_expansion_date(std::string date) {
    date = lower_ascii(trim(std::move(date)));
    if (date.empty() || date == "latest" || date == "today" || date == "current")
        return 0;
    date.erase(std::remove(date.begin(), date.end(), '-'), date.end());
    const int value = parse_integer(date, "--date", 19900101, 22001231);
    const int month = value / 100 % 100, day = value % 100;
    if (month < 1 || month > 12 || day < 1 || day > 31)
        throw Error("--date must be YYYYMMDD or YYYY-MM-DD");
    return static_cast<std::uint32_t>(value);
}

std::string minute_label(std::uint16_t minute, int second) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << minute / 60 << ':'
           << std::setw(2) << minute % 60;
    if (second >= 0) output << ':' << std::setw(2) << second;
    return output.str();
}

ExpansionTradeNature decode_expansion_trade_nature(std::uint8_t market_id,
                                                    std::uint16_t nature,
                                                    std::uint32_t volume,
                                                    std::int32_t open_interest_change) {
    if (market_id == 31 || market_id == 48) {
        if (nature == 0) return {1, "buy", "B"};
        if (nature == 256) return {-1, "sell", "S"};
        return {};
    }
    const auto volume_signed = volume > static_cast<std::uint32_t>(
        std::numeric_limits<std::int32_t>::max())
        ? std::numeric_limits<std::int32_t>::max()
        : static_cast<std::int32_t>(volume);
    const int mark = nature / 10000;
    if (mark == 0) {
        if (open_interest_change > 0)
            return {1, "buy", volume_signed == open_interest_change ? "双开" : "多开"};
        if (open_interest_change == 0) return {1, "buy", "多换"};
        return {1, "buy", volume_signed == -static_cast<std::int64_t>(open_interest_change)
            ? "双平" : "空平"};
    }
    if (mark == 1) {
        if (open_interest_change > 0)
            return {-1, "sell", volume_signed == open_interest_change ? "双开" : "空开"};
        if (open_interest_change == 0) return {-1, "sell", "空换"};
        return {-1, "sell", volume_signed == -static_cast<std::int64_t>(open_interest_change)
            ? "双平" : "多平"};
    }
    if (open_interest_change > 0)
        return {0, "neutral", volume_signed == open_interest_change ? "双开" : "开仓"};
    if (open_interest_change < 0)
        return {0, "neutral", volume_signed == -static_cast<std::int64_t>(open_interest_change)
            ? "双平" : "平仓"};
    return {0, "neutral", "换手"};
}

std::string decode_fixed_gbk(const std::uint8_t* data, std::size_t size) {
    while (size && data[size - 1] == 0) --size;
    return trim(decode_gbk(Bytes(data, data + size)));
}

KlinePeriod normalize_period(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    if (value == "time" || value == "timeline" || value == "1m") return {value == "1m" ? "1m" : "time", 7, true};
    if (value == "5m") return {"5m", 0, true};
    if (value == "15m") return {"15m", 1, true};
    if (value == "30m") return {"30m", 2, true};
    if (value == "60m" || value == "1h") return {"60m", 3, true};
    if (value == "day" || value == "1d" || value == "daily") return {"day", 4, false};
    if (value == "week" || value == "1w" || value == "weekly") return {"week", 5, false};
    if (value == "month" || value == "1mo" || value == "monthly") return {"month", 6, false};
    throw Error("period must be time, 1m, 5m, 15m, 30m, 60m, day, week, or month");
}

void sort_bars_chronologically(std::vector<MinuteBar>& bars) {
    std::stable_sort(bars.begin(), bars.end(),
                     [](const MinuteBar& left, const MinuteBar& right) {
        return std::tie(left.date, left.hour, left.minute) <
               std::tie(right.date, right.hour, right.minute);
    });
}

std::vector<MinuteBar> merge_bars(const std::vector<MinuteBar>& existing,
                                  const std::vector<MinuteBar>& downloaded) {
    std::map<std::pair<int, int>, MinuteBar> rows;
    for (const auto& bar : existing) rows[{bar.date, bar.hour * 60 + bar.minute}] = bar;
    for (const auto& bar : downloaded) rows[{bar.date, bar.hour * 60 + bar.minute}] = bar;
    std::vector<MinuteBar> result;
    result.reserve(rows.size());
    for (const auto& [key, bar] : rows) {
        (void)key;
        result.push_back(bar);
    }
    return result;
}

std::vector<MinuteBar> select_date(std::vector<MinuteBar> bars, std::string date) {
    date = lower_ascii(trim(std::move(date)));
    if (date == "all") return bars;
    int wanted = 0;
    if (date.empty() || date == "latest") {
        for (const auto& bar : bars) wanted = std::max(wanted, bar.date);
    } else {
        date.erase(std::remove(date.begin(), date.end(), '-'), date.end());
        wanted = parse_integer(date, "--date", 20040101, 21991231);
    }
    bars.erase(std::remove_if(bars.begin(), bars.end(),
                              [&](const MinuteBar& bar) { return bar.date != wanted; }),
               bars.end());
    return bars;
}

fs::path from_utf8(const std::string& value) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(value));
#else
    return fs::path(value);
#endif
}

void print_help() {
    std::cout <<
        "Usage: tdx-tool minute download --code CODE [options]\n\n"
        "Download native 1-minute K-lines with 7709 command 0x052D.\n\n"
        "Options:\n"
        "  --market MARKET         sz/sh/bj or expansion ID/name (qz=28, qd=29, qs=30, cz=47, qg=66)\n"
        "  --kind auto|stock|index 880xxx defaults to index\n"
        "  --host HOST[:PORT]      Repeatable; default connect.cfg HQHOST primary-first\n"
        "  --timeout-ms N          Socket timeout (default 10000)\n"
        "  --pages N               Number of pages (default 1)\n"
        "  --page-size N           1..800 bars per page (default 800)\n"
        "  --start N               History offset (default 0)\n"
        "  --root PATH             TDX root used for optional cache merge\n"
        "  --no-merge-existing     Do not merge TDX/output LC1 caches\n"
        "  --lc1-output PATH       Snapshot path (default output/minute/MARKETCODE.lc1)\n"
        "  --format json|csv|none  Additional export (default json)\n"
        "  --date latest|all|DATE  Export date selection (default latest)\n"
        "  --output PATH           JSON/CSV path\n";
}

}  // namespace tdx::minute_download_detail
