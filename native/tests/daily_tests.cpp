#include "tdx/daily.hpp"
#include "tdx/minute.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void append_u32(tdx::Bytes& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffu));
}

void append_float(tdx::Bytes& output, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(output, bits);
}

void append_daily(tdx::Bytes& output, int date, int open, int high, int low,
                  int close, float amount, std::uint32_t volume) {
    append_u32(output, static_cast<std::uint32_t>(date));
    append_u32(output, static_cast<std::uint32_t>(open));
    append_u32(output, static_cast<std::uint32_t>(high));
    append_u32(output, static_cast<std::uint32_t>(low));
    append_u32(output, static_cast<std::uint32_t>(close));
    append_float(output, amount);
    append_u32(output, volume);
    append_u32(output, 0);
}

tdx::MinuteBar minute_bar(int hour, int minute, float price) {
    tdx::MinuteBar result;
    result.date = 20260810;
    result.hour = hour;
    result.minute = minute;
    result.open = price;
    result.high = price + 0.5f;
    result.low = price - 0.25f;
    result.close = price + 0.25f;
    result.amount = price * 1000.0f;
    result.volume = static_cast<std::int32_t>(price * 100.0f);
    return result;
}

}  // namespace

int main() {
    try {
        tdx::Bytes data;
        append_u32(data, 20260810);
        append_u32(data, 1234);
        append_u32(data, 1301);
        append_u32(data, 1202);
        append_u32(data, 1288);
        append_float(data, 987654.5f);
        append_u32(data, 456789);
        append_u32(data, 0);
        const auto bars = tdx::parse_daily_bars(data);
        require(bars.size() == 1 && bars[0].date == 20260810,
                "daily date decoding");
        require(std::abs(bars[0].open - 12.34) < 1e-9 &&
                    std::abs(bars[0].high - 13.01) < 1e-9 &&
                    std::abs(bars[0].low - 12.02) < 1e-9 &&
                    std::abs(bars[0].close - 12.88) < 1e-9,
                "daily OHLC cents decoding");
        require(std::abs(bars[0].amount - 987654.5) < 1e-6 &&
                    bars[0].volume == 456789,
                "daily amount and volume decoding");
        bool rejected = false;
        try { (void)tdx::parse_daily_bars(tdx::Bytes(31)); }
        catch (const tdx::Error&) { rejected = true; }
        require(rejected, "daily record-size validation");

        namespace fs = std::filesystem;
        const auto suffix = std::chrono::high_resolution_clock::now()
                                .time_since_epoch().count();
        const auto root = fs::temp_directory_path() /
                          ("tdx-local-kline-" + std::to_string(suffix));
        fs::create_directories(root / "vipdoc" / "sz" / "lday");
        fs::create_directories(root / "vipdoc" / "sz" / "minline");

        tdx::Bytes daily;
        const int dates[] = {20260730, 20260731, 20260803, 20260804,
                             20260805, 20260806, 20260807, 20260810};
        for (int index = 0; index < 8; ++index) {
            const int open = 1000 + index * 10;
            append_daily(daily, dates[index], open, open + 20, open - 10,
                         open + 5, 100000.0f + index, 155 + index);
        }
        tdx::atomic_write_bytes(
            root / "vipdoc" / "sz" / "lday" / "sz000001.day", daily);

        const auto local_day = tdx::load_local_kline_document(
            root, "sz", "000001", "stock", "day", 1, 3, 1, "all");
        require(local_day.at("source_mode").as_string() == "local" &&
                    local_day.at("transport").as_string() == "local-vipdoc" &&
                    local_day.at("source_record_count").as_number() == 8 &&
                    local_day.at("downloaded").as_number() == 3 &&
                    local_day.at("count").as_number() == 3 &&
                    local_day.at("next_start").as_number() == 4 &&
                    local_day.at("has_more").as_bool(),
                "local DAY pagination metadata");
        require(local_day.at("bars").as_array().front().at("date").as_string() ==
                    "2026-08-05" &&
                    local_day.at("bars").as_array().back().at("date").as_string() ==
                    "2026-08-07",
                "local DAY pages must be selected from latest and returned ascending");

        const auto local_week = tdx::load_local_kline_document(
            root, "0", "000001", "auto", "week", 1, 20, 0, "all");
        require(local_week.at("count").as_number() == 3 &&
                    local_week.at("aggregation").as_string() ==
                        "day-calendar-buckets" &&
                    local_week.at("bars").as_array()[0].at("date").as_string() ==
                        "2026-07-31" &&
                    local_week.at("bars").as_array()[1].at("date").as_string() ==
                        "2026-08-07" &&
                    local_week.at("bars").as_array()[2].at("date").as_string() ==
                        "2026-08-10" &&
                    local_week.at("bars").as_array()[0].at("volume").as_number() == 3 &&
                    local_week.at("bars").as_array()[1].at("volume").as_number() == 7 &&
                    local_week.at("volume_unit").as_string() == "lot" &&
                    local_week.at("source_volume_divisor").as_number() == 100,
                "local weekly calendar aggregation");
        const auto local_month = tdx::load_local_kline_document(
            root, "sz", "000001", "stock", "month", 1, 20, 0, "all");
        require(local_month.at("count").as_number() == 2 &&
                    local_month.at("bars").as_array()[0].at("date").as_string() ==
                        "2026-07-31" &&
                    local_month.at("bars").as_array()[1].at("date").as_string() ==
                        "2026-08-10",
                "local monthly calendar aggregation");

        std::vector<tdx::MinuteBar> minutes;
        for (int minute = 31; minute <= 36; ++minute)
            minutes.push_back(minute_bar(9, minute,
                                         10.0f + static_cast<float>(minute - 31)));
        minutes.push_back(minute_bar(13, 1, 20.0f));
        tdx::atomic_write_bytes(
            root / "vipdoc" / "sz" / "minline" / "sz000001.lc1",
            tdx::pack_lc1(minutes));

        const auto local_five = tdx::load_local_kline_document(
            root, "sz", "000001", "stock", "5m", 1, 20, 0, "all");
        require(local_five.at("source_record_count").as_number() == 7 &&
                    local_five.at("available_count").as_number() == 3 &&
                    local_five.at("count").as_number() == 3 &&
                    local_five.at("bars").as_array()[0].at("time").as_string() ==
                        "09:35" &&
                    local_five.at("bars").as_array()[1].at("time").as_string() ==
                        "09:40" &&
                    local_five.at("bars").as_array()[2].at("time").as_string() ==
                        "13:05",
                "local LC1 five-minute session buckets");
        require(std::abs(local_five.at("bars").as_array()[0]
                             .at("open").as_number() - 10.0) < 1e-6 &&
                    std::abs(local_five.at("bars").as_array()[0]
                             .at("close").as_number() - 14.25) < 1e-6,
                "local LC1 aggregate OHLC");

        const auto local_hour = tdx::load_local_kline_document(
            root, "sz", "000001", "stock", "60m", 1, 20, 0, "all");
        require(local_hour.at("count").as_number() == 2 &&
                    local_hour.at("bars").as_array()[0].at("time").as_string() ==
                        "10:30" &&
                    local_hour.at("bars").as_array()[1].at("time").as_string() ==
                        "14:00",
                "local LC1 sixty-minute lunch-boundary aggregation");

        const auto local_page = tdx::load_local_kline_document(
            root, "sz", "000001", "stock", "1m", 1, 2, 1, "all");
        require(local_page.at("count").as_number() == 2 &&
                    local_page.at("bars").as_array()[0].at("time").as_string() ==
                        "09:35" &&
                    local_page.at("bars").as_array()[1].at("time").as_string() ==
                        "09:36" &&
                    local_page.at("next_start").as_number() == 3 &&
                    local_page.at("has_more").as_bool(),
                "local LC1 history-offset pagination");
        std::error_code cleanup_error;
        fs::remove_all(root, cleanup_error);
        std::cout << "daily tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
