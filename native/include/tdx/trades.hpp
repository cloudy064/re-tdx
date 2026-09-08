#pragma once

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tdx {

struct TradeTick {
    std::size_t index{};
    std::size_t absolute_index{};
    std::uint16_t time_minutes{};
    std::string time_label;
    double price{};
    std::int64_t volume_hand{};
    std::int64_t order_count{};
    std::int64_t status_raw{};
    std::string side;
    std::int64_t price_delta_raw{};
    std::int64_t price_acc_raw{};
    std::int64_t tail_raw{};
};

struct TradePage {
    int market_id{};
    std::string code;
    std::string trading_date;
    std::uint16_t start{};
    std::uint16_t request_count{};
    int price_divisor{};
    std::optional<float> price_base_raw;
    std::vector<TradeTick> ticks;
};

struct TradeSeries {
    int market_id{};
    std::string code;
    std::string trading_date;
    std::string source_mode;
    std::size_t pages{};
    std::uint16_t page_size{};
    int price_divisor{};
    std::optional<float> price_base_raw;
    std::vector<TradeTick> ticks;
};

Bytes build_today_trades_request_data(int market_id, std::string_view code,
                                      std::uint16_t start = 0,
                                      std::uint16_t count = 1800);
Bytes build_history_trades_request_data(int market_id, std::string_view code,
                                        std::string_view trading_date,
                                        std::uint16_t start = 0,
                                        std::uint16_t count = 2000);
TradePage parse_today_trades_payload(const Bytes& payload, int market_id,
                                     const std::string& code, std::uint16_t start,
                                     std::uint16_t request_count,
                                     std::string trading_date = {});
TradePage parse_history_trades_payload(const Bytes& payload, int market_id,
                                       const std::string& code,
                                       std::string_view trading_date,
                                       std::uint16_t start,
                                       std::uint16_t request_count);
Json aggregate_trade_ticks(const std::vector<TradeTick>& ticks);

Json fetch_market_trades_document(const std::filesystem::path& root,
                                  const std::vector<std::string>& securities,
                                  const std::string& trading_date = {},
                                  int page_size = 0, int max_pages = 100,
                                  int timeout_ms = 10000,
                                  const BlockData* block_data = nullptr,
                                  const std::vector<std::string>& hosts = {},
                                  const std::function<void()>& check = {});
int command_market_trades(const std::vector<std::string>& args);

}  // namespace tdx
