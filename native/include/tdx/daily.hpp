#pragma once

#include "tdx/common.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tdx {

// TDX vipdoc daily files contain fixed 32-byte records.  Prices are stored in
// cents, amount is an IEEE-754 float, and volume is the raw host integer.
struct DailyBar {
    int date{};
    double open{};
    double high{};
    double low{};
    double close{};
    double amount{};
    std::uint32_t volume{};
};

std::vector<DailyBar> parse_daily_bars(const Bytes& data);
std::filesystem::path locate_daily_file(
    const std::filesystem::path& root,
    int market_id,
    const std::string& code);
std::vector<DailyBar> load_daily_bars(
    const std::filesystem::path& root,
    int market_id,
    const std::string& code);

}  // namespace tdx
