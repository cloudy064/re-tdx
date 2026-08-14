#pragma once

#include "tdx/corporate.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tdx {

// Rules loaded by TdxW from T0002/hq_cache/hqrule.dat.  Defaults mirror the
// constants in TdxW::sub_5AA080 when a key/file is absent.
struct LimitRuleConfig {
    int sz_st_10_date{};
    int sh_st_10_date{};
    double chinext_rate{0.20};
    double star_rate{0.20};
    double beijing_rate{0.30};
    std::string source;
};

LimitRuleConfig load_limit_rule_config(const std::filesystem::path& tdx_root);

struct LimitPrices {
    bool available{};
    double upper{};
    double lower{};
    double rate{};
    int security_class{-1};
    std::string source;
};

// Reconstruct the A-share branches of TdxW::sub_5AA080.  A supplied 0x0452
// record takes precedence because it carries the server's per-security prices.
LimitPrices calculate_limit_prices(int market_id, std::string_view code,
                                   std::string_view name, double previous_close,
                                   int as_of_yyyymmdd, std::uint64_t quote_flags,
                                   const LimitRuleConfig& rules,
                                   const std::optional<SpecialLimitPrice>& special = std::nullopt);

struct SealLevel {
    double price{};
    std::uint64_t volume_hand{};
};

struct SealOrderInput {
    double last_price{};
    std::uint64_t total_hand{};
    double trade_unit{};
    std::uint64_t quote_flags{};
    std::int64_t auction_imbalance_hand{};
    std::array<SealLevel, 2> buys{};
    std::array<SealLevel, 2> sells{};
};

struct SealOrderResult {
    bool available{};
    bool amount_available{};
    bool ratio_available{};
    int direction{};  // 1 limit-up, -1 limit-down, 0 not sealed
    double amount_yuan{};
    double ratio{};
    std::uint64_t queue_hand{};
    std::uint64_t denominator_hand{};
    std::string mode;
};

// Reconstruct TdxW::sub_9B37A0 for the two TBigData projections used by
// $FCAMO (amount) and $FCB (queue/volume ratio), including auction branches.
SealOrderResult calculate_seal_order(const SealOrderInput& quote,
                                     const LimitPrices& limits);

}  // namespace tdx
