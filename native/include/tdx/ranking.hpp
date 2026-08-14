#pragma once

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

struct CategoryQuote {
    int market_id{};
    std::string code;
    std::uint16_t active1{};
    std::uint16_t active2{};
    double last_price{}, pre_close_price{}, open_price{}, high_price{}, low_price{};
    std::int64_t server_time_raw{}, neg_price_raw{}, total_hand{}, current_hand{};
    double amount{};
    std::uint32_t amount_raw{};
    std::int64_t inside_dish{}, outer_disc{}, after_outer_raw{};
    double open_amount_yuan{}, bid1_price{}, ask1_price{};
    std::int64_t bid1_volume_hand{}, ask1_volume_hand{};
    std::uint16_t status_or_sort_raw{};
    double rise_speed{}, short_turnover{}, two_minute_amount{}, opening_rush{};
    double volume_rise_speed{}, depth{};
    std::string extra_pair_hex, extra_meta_hex;
};

struct CategoryQuotePage {
    std::uint16_t header{};
    std::vector<CategoryQuote> records;
};

std::uint16_t normalize_ranking_category(const std::string& value);
std::uint16_t normalize_ranking_sort(const std::string& value);
Bytes build_category_request_data(std::uint16_t category, std::uint16_t sort_type,
                                  std::uint16_t start, std::uint16_t count,
                                  bool ascending = false, std::uint16_t filter_raw = 0);
CategoryQuotePage parse_category_payload(const Bytes& payload);

Json fetch_market_ranking_document(const std::filesystem::path& root,
                                   const std::string& category,
                                   const std::string& sort,
                                   int start, int count, bool ascending,
                                   int filter_raw, bool all_sealed,
                                   int timeout_ms = 10000,
                                   const BlockData* block_data = nullptr,
                                   const std::vector<std::string>& hosts = {});
int command_market_ranking(const std::vector<std::string>& args);

}  // namespace tdx
