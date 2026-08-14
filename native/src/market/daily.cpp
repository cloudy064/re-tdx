#include "tdx/daily.hpp"

#include <cmath>

namespace tdx {

std::vector<DailyBar> parse_daily_bars(const Bytes& data) {
    if (data.size() % 32 != 0)
        throw Error("TDX daily file size is not a multiple of 32 bytes");
    std::vector<DailyBar> result;
    result.reserve(data.size() / 32);
    for (std::size_t offset = 0; offset < data.size(); offset += 32) {
        const auto* record = data.data() + offset;
        const auto amount = static_cast<double>(read_f32_le(record + 20));
        result.push_back(DailyBar{
            static_cast<int>(read_u32_le(record)),
            static_cast<double>(read_i32_le(record + 4)) / 100.0,
            static_cast<double>(read_i32_le(record + 8)) / 100.0,
            static_cast<double>(read_i32_le(record + 12)) / 100.0,
            static_cast<double>(read_i32_le(record + 16)) / 100.0,
            std::isfinite(amount) ? amount : 0.0,
            read_u32_le(record + 24),
        });
    }
    return result;
}

std::filesystem::path locate_daily_file(
    const std::filesystem::path& root, int market_id,
    const std::string& code) {
    if (market_id >= 0 && market_id <= 2) {
        const std::string prefix = market_id == 0 ? "sz" :
                                   market_id == 1 ? "sh" : "bj";
        return root / "vipdoc" / prefix / "lday" /
               (prefix + code + ".day");
    }
    return root / "vipdoc" / "ds" / "lday" /
           (std::to_string(market_id) + "#" + code + ".day");
}

std::vector<DailyBar> load_daily_bars(
    const std::filesystem::path& root, int market_id,
    const std::string& code) {
    const auto path = locate_daily_file(root, market_id, code);
    if (!std::filesystem::is_regular_file(path)) return {};
    return parse_daily_bars(read_bytes(path));
}

}  // namespace tdx
