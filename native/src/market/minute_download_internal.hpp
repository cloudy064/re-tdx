#pragma once

#include "tdx/minute.hpp"
#include "tdx/transport.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::minute_download_detail {

inline constexpr std::uint16_t type_klines = 0x052D;
inline constexpr std::uint16_t type_expansion_klines = 0x23FF;
inline constexpr std::uint16_t type_expansion_instrument_count = 0x23F0;
inline constexpr std::uint16_t type_expansion_instrument_info = 0x23F5;
inline constexpr std::uint16_t type_expansion_quote = 0x23FA;
inline constexpr std::uint16_t type_expansion_minute = 0x240B;
inline constexpr std::uint16_t type_expansion_history_minute = 0x240C;
inline constexpr std::uint16_t type_expansion_trade = 0x23FC;
inline constexpr std::uint16_t type_expansion_history_trade = 0x2406;
inline constexpr std::uint16_t default_period_parameter = 1;
inline constexpr std::uint16_t maximum_page_size = 800;

struct ExpansionTradeNature {
    int direction{};
    std::string side{"neutral"};
    std::string name;
};

struct KlinePeriod {
    std::string name;
    std::uint16_t id{};
    bool intraday{};
};

void write_u16(Bytes& data, std::size_t offset, std::uint16_t value);
void append_u16(Bytes& data, std::uint16_t value);
void append_u32(Bytes& data, std::uint32_t value);
void append_float(Bytes& data, float value);
std::int64_t consume_varint(const Bytes& payload, std::size_t& offset);
double decode_wire_number(std::uint32_t value);
std::uint16_t encode_lc1_date(int value);
int parse_integer(
    const std::string& text,
    std::string_view option,
    int minimum,
    int maximum);
std::pair<std::string, std::uint16_t> normalize_market(
    std::string market, const std::string& code);
bool valid_kline_code(std::string_view code, std::size_t maximum_size);
std::uint32_t normalize_expansion_date(std::string date);
std::string minute_label(std::uint16_t minute, int second = -1);
ExpansionTradeNature decode_expansion_trade_nature(
    std::uint8_t market_id,
    std::uint16_t nature,
    std::uint32_t volume,
    std::int32_t open_interest_change);
std::string decode_fixed_gbk(const std::uint8_t* data, std::size_t size);
KlinePeriod normalize_period(std::string value);
void sort_bars_chronologically(std::vector<MinuteBar>& bars);
std::vector<MinuteBar> merge_bars(
    const std::vector<MinuteBar>& existing,
    const std::vector<MinuteBar>& downloaded);
std::vector<MinuteBar> select_date(
    std::vector<MinuteBar> bars, std::string date);
std::filesystem::path from_utf8(const std::string& value);
void print_help();

}  // namespace tdx::minute_download_detail
