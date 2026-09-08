#pragma once

#include "tdx/blocks.hpp"
#include "tdx/json.hpp"
#include "tdx/trades.hpp"
#include "tdx/transport.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx {
namespace detail {
namespace trades {

constexpr std::uint16_t type_heartbeat = 0x0004;
constexpr std::uint16_t type_today_trades = 0x0FC5;
constexpr std::uint16_t type_history_trades = 0x0FC6;
constexpr int maximum_pages = 100;

struct SecurityCode {
    int market_id{};
    std::string code;
    std::pair<int, std::string> key() const { return {market_id, code}; }
};

// Byte assembly
void append_u16(Bytes& output, std::uint16_t value);
void append_u32(Bytes& output, std::uint32_t value);

// Argument and identifier parsing
int parse_integer(const std::string& text, std::string_view name,
                  int minimum, int maximum);
void validate_security(int market_id, std::string_view code);
SecurityCode parse_security(std::string value);
std::string security_id(int market_id, const std::string& code);
std::string normalize_date(std::string value);

// Field formatting
std::string now_text();
std::string time_label(int minute);
std::string trade_side(std::int64_t status);
int trade_price_divisor(const std::string& code);

// Record decoding
std::int64_t consume_varint(const Bytes& payload, std::size_t& offset,
                            std::size_t record_index);
std::vector<TradeTick> parse_records(const Bytes& payload, std::size_t& offset,
                                     std::uint16_t count, std::uint16_t start,
                                     int price_divisor);

// Aggregation
double amount_yuan(const TradeTick& tick);
Json aggregate_group(const std::vector<const TradeTick*>& values);

// Projection
Json tick_json(const TradeTick& tick);
Json series_json(const TradeSeries& series,
                 const std::map<std::pair<int, std::string>, Security>& names);

// Download
TradeSeries download_one(QuoteConnection& connection, const SecurityCode& code,
                         const std::string& trading_date,
                         const std::string& server_trade_date,
                         std::uint16_t page_size, int max_page_count,
                         const std::function<void()>& check = {});

}  // namespace trades
}  // namespace detail
}  // namespace tdx
