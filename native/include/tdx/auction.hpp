#pragma once

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tdx {

struct AuctionPoint {
    std::size_t index{};
    std::uint16_t minute_of_day_raw{};
    std::uint8_t second_raw{};
    std::string time_label;
    int time_seconds{};
    double price{};
    std::uint32_t matched_volume_hand{};
    std::int32_t unmatched_signed_hand{};
    std::uint32_t unmatched_volume_hand{};
    int unmatched_direction_raw{};
    std::string unmatched_direction;
    std::uint8_t reserved_zero_0e{};
};

struct AuctionSeries {
    int market_id{};
    std::string code;
    std::uint32_t selector{}, start_raw{}, limit{};
    std::vector<AuctionPoint> points;
};

Bytes build_auction_request_data(int market_id, std::string_view code,
                                 std::uint32_t selector = 3,
                                 std::uint32_t start_raw = 0,
                                 std::uint32_t limit = 500);
AuctionSeries parse_auction_payload(const Bytes& payload, int market_id,
                                    const std::string& code,
                                    std::uint32_t selector = 3,
                                    std::uint32_t start_raw = 0,
                                    std::uint32_t limit = 500);

Json fetch_market_auction_document(const std::filesystem::path& root,
                                   const std::vector<std::string>& securities,
                                   std::uint32_t selector = 3,
                                   std::uint32_t start_raw = 0,
                                   std::uint32_t limit = 500,
                                   int timeout_ms = 10000,
                                   const BlockData* block_data = nullptr,
                                   const std::vector<std::string>& hosts = {});
int command_market_auction(const std::vector<std::string>& args);

}  // namespace tdx
