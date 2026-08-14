#include "minute_download_internal.hpp"

#include "tdx/daily.hpp"
#include "tdx/security_identity.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace tdx {
namespace {

using minute_download_detail::KlinePeriod;

struct LocalBar {
    int date{};
    int hour{};
    int minute{};
    double open{};
    double high{};
    double low{};
    double close{};
    double amount{};
    bool amount_available{true};
    std::int64_t volume{};
    std::uint16_t extra_1{};
    std::uint16_t extra_2{};
};

struct DateParts {
    int year{};
    int month{};
    int day{};
};

bool leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int days_in_month(int year, int month) {
    static constexpr int values[] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
    return values[month - 1] + (month == 2 && leap_year(year) ? 1 : 0);
}

DateParts parse_date(int value) {
    DateParts result{value / 10000, value / 100 % 100, value % 100};
    if (result.year < 1990 || result.year > 2200 ||
        result.month < 1 || result.month > 12 || result.day < 1 ||
        result.day > days_in_month(result.year, result.month))
        throw Error("invalid local K-line date: " + std::to_string(value));
    return result;
}

std::int64_t civil_day_number(const DateParts& value) {
    int year = value.year - (value.month <= 2 ? 1 : 0);
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned adjusted_month = static_cast<unsigned>(
        value.month + (value.month > 2 ? -3 : 9));
    const unsigned day_of_year =
        (153 * adjusted_month + 2) / 5 + static_cast<unsigned>(value.day) - 1;
    const unsigned day_of_era =
        year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return static_cast<std::int64_t>(era) * 146097 + day_of_era - 719468;
}

std::string two_digits(int value) {
    std::string result = std::to_string(value);
    if (result.size() < 2) result.insert(result.begin(), '0');
    return result;
}

std::string date_text(int value) {
    const auto parts = parse_date(value);
    return std::to_string(parts.year) + "-" + two_digits(parts.month) + "-" +
           two_digits(parts.day);
}

std::string time_text(int hour, int minute) {
    return two_digits(hour) + ":" + two_digits(minute);
}

void validate_ohlc(const LocalBar& bar) {
    const auto maximum = std::max({bar.open, bar.high, bar.low, bar.close});
    const auto minimum = std::min({bar.open, bar.high, bar.low, bar.close});
    if (bar.high + 1e-8 < maximum || bar.low - 1e-8 > minimum)
        throw Error("invalid local K-line OHLC range at " +
                    std::to_string(bar.date));
}

LocalBar from_daily(const DailyBar& bar) {
    LocalBar result;
    result.date = bar.date;
    result.hour = 15;
    result.open = bar.open;
    result.high = bar.high;
    result.low = bar.low;
    result.close = bar.close;
    result.amount = bar.amount;
    // Keep DAY volume in its stored share unit while aggregating.  Conversion
    // to public K-line lots happens after week/month grouping so odd-lot
    // remainders are not discarded once per day.
    result.volume = static_cast<std::int64_t>(bar.volume);
    parse_date(result.date);
    validate_ohlc(result);
    return result;
}

LocalBar from_minute(const MinuteBar& bar) {
    LocalBar result;
    result.date = bar.date;
    result.hour = bar.hour;
    result.minute = bar.minute;
    result.open = bar.open;
    result.high = bar.high;
    result.low = bar.low;
    result.close = bar.close;
    result.amount = bar.amount;
    result.amount_available = bar.amount_available;
    result.volume = bar.volume;
    result.extra_1 = bar.extra_1;
    result.extra_2 = bar.extra_2;
    parse_date(result.date);
    validate_ohlc(result);
    return result;
}

void merge_bar(LocalBar& target, const LocalBar& value) {
    target.date = value.date;
    target.high = std::max(target.high, value.high);
    target.low = std::min(target.low, value.low);
    target.close = value.close;
    if (target.amount_available && value.amount_available)
        // TDX aggregates the float32 amount column with float32 accumulation.
        // Keep that boundary instead of silently promoting all sums to double;
        // an independently generated server bar can still differ by a few ULP.
        target.amount = static_cast<double>(
            static_cast<float>(target.amount) + static_cast<float>(value.amount));
    else
        target.amount_available = false;
    if ((value.volume > 0 &&
         target.volume > std::numeric_limits<std::int64_t>::max() - value.volume) ||
        (value.volume < 0 &&
         target.volume < std::numeric_limits<std::int64_t>::min() - value.volume))
        throw Error("local K-line volume aggregation overflow");
    target.volume += value.volume;
    target.extra_1 = value.extra_1;
    target.extra_2 = value.extra_2;
}

int session_index(int hour, int minute) {
    const int value = hour * 60 + minute;
    if (value >= 9 * 60 + 31 && value <= 11 * 60 + 30)
        return value - (9 * 60 + 31);
    if (value >= 13 * 60 + 1 && value <= 15 * 60)
        return 120 + value - (13 * 60 + 1);
    return -1;
}

std::pair<int, int> session_time(int index) {
    index = std::max(0, std::min(index, 239));
    const int value = index < 120
        ? 9 * 60 + 31 + index
        : 13 * 60 + 1 + index - 120;
    return {value / 60, value % 60};
}

std::vector<LocalBar> aggregate_intraday(std::vector<LocalBar> source,
                                         int period_minutes) {
    std::stable_sort(source.begin(), source.end(), [](const auto& left,
                                                       const auto& right) {
        return std::tie(left.date, left.hour, left.minute) <
               std::tie(right.date, right.hour, right.minute);
    });
    std::vector<LocalBar> result;
    std::pair<int, int> previous_key{-1, -1};
    for (const auto& bar : source) {
        const int index = session_index(bar.hour, bar.minute);
        // Standard A-share minutes use a continuous 240-minute session index.
        // Preserve an off-session record as its own bucket instead of folding it
        // into a neighbouring regular-session bar.
        const int bucket = index >= 0 ? index / period_minutes
                                      : 10000 + bar.hour * 60 + bar.minute;
        const std::pair<int, int> key{bar.date, bucket};
        if (result.empty() || key != previous_key) {
            result.push_back(bar);
            if (index >= 0) {
                const auto end_index = std::min(
                    (bucket + 1) * period_minutes - 1, 239);
                std::tie(result.back().hour, result.back().minute) =
                    session_time(end_index);
            }
            result.back().extra_1 = 0;
            result.back().extra_2 = 0;
            previous_key = key;
        } else {
            const int label_hour = result.back().hour;
            const int label_minute = result.back().minute;
            merge_bar(result.back(), bar);
            result.back().hour = label_hour;
            result.back().minute = label_minute;
            result.back().extra_1 = 0;
            result.back().extra_2 = 0;
        }
    }
    return result;
}

std::vector<LocalBar> aggregate_daily(std::vector<LocalBar> source,
                                      const std::string& period) {
    std::stable_sort(source.begin(), source.end(), [](const auto& left,
                                                       const auto& right) {
        return left.date < right.date;
    });
    if (period == "day") return source;
    std::vector<LocalBar> result;
    std::int64_t previous_key = std::numeric_limits<std::int64_t>::min();
    for (const auto& bar : source) {
        const auto parts = parse_date(bar.date);
        std::int64_t key = 0;
        if (period == "month") {
            key = static_cast<std::int64_t>(parts.year) * 12 + parts.month;
        } else {
            const auto days = civil_day_number(parts);
            auto monday_offset = (days + 3) % 7;
            if (monday_offset < 0) monday_offset += 7;
            key = days - monday_offset;
        }
        if (result.empty() || key != previous_key) {
            result.push_back(bar);
            previous_key = key;
        } else {
            merge_bar(result.back(), bar);
        }
    }
    return result;
}

Json bar_json(const LocalBar& bar) {
    Json result = Json::object();
    result["date"] = date_text(bar.date);
    result["time"] = time_text(bar.hour, bar.minute);
    result["open"] = bar.open;
    result["high"] = bar.high;
    result["low"] = bar.low;
    result["close"] = bar.close;
    result["amount"] = bar.amount_available ? Json(bar.amount) : Json(nullptr);
    result["volume"] = bar.volume;
    result["extra_1"] = static_cast<int>(bar.extra_1);
    result["extra_2"] = static_cast<int>(bar.extra_2);
    return result;
}

std::vector<LocalBar> page_from_latest(const std::vector<LocalBar>& bars,
                                       int pages, int page_size, int start) {
    if (start >= static_cast<int>(bars.size())) return {};
    const auto end = bars.size() - static_cast<std::size_t>(start);
    const auto requested = static_cast<std::size_t>(pages) *
                           static_cast<std::size_t>(page_size);
    const auto begin = end > requested ? end - requested : 0;
    return {bars.begin() + static_cast<std::ptrdiff_t>(begin),
            bars.begin() + static_cast<std::ptrdiff_t>(end)};
}

std::string aggregation_name(const KlinePeriod& period) {
    if (period.name == "time" || period.name == "1m") return "native-lc1";
    if (period.intraday) return "lc1-a-share-session-buckets";
    if (period.name == "day") return "native-day";
    return "day-calendar-buckets";
}

}  // namespace

Json load_local_kline_document(const fs::path& root,
                               const std::string& market_option,
                               const std::string& code,
                               const std::string& kind_option,
                               const std::string& period_option,
                               int pages, int page_size, int start,
                               const std::string& date) {
    using namespace minute_download_detail;
    const auto [market, market_id] = normalize_market(market_option, code);
    if (market_id > 2)
        throw Error("local K-line source is unavailable for expansion markets");
    if (!valid_kline_code(code, 6))
        throw Error("code must contain 1..6 ASCII letters or digits");
    const auto kind = lower_ascii(trim(kind_option));
    if (kind != "auto" && kind != "stock" && kind != "index")
        throw Error("kind must be auto, stock, or index");
    if (pages < 1 || pages > 20 || page_size < 1 || page_size > 800 ||
        start < 0 || start > 65535)
        throw Error("local K-line request is outside the safe range");

    const auto period = normalize_period(period_option);
    const bool index_mode = kind == "index" ||
        (kind == "auto" && market_id == 1 && is_tdx_block_index_code(code));
    fs::path source_path;
    std::vector<LocalBar> source_bars;
    std::vector<LocalBar> bars;
    if (period.intraday) {
        source_path = locate_lc1(root, market, code, index_mode);
        const auto parsed = parse_lc1(read_bytes(source_path));
        source_bars.reserve(parsed.size());
        for (const auto& bar : parsed) source_bars.push_back(from_minute(bar));
        bars = period.name == "time" || period.name == "1m"
            ? source_bars
            : aggregate_intraday(source_bars, period.name == "5m" ? 5 :
                period.name == "15m" ? 15 : period.name == "30m" ? 30 : 60);
    } else {
        source_path = locate_daily_file(root, market_id, code);
        if (!fs::is_regular_file(source_path))
            throw Error("DAY cache not found for " + market + code);
        const auto parsed = parse_daily_bars(read_bytes(source_path));
        source_bars.reserve(parsed.size());
        for (const auto& bar : parsed) source_bars.push_back(from_daily(bar));
        bars = aggregate_daily(source_bars, period.name);
        // 7709 preserves share volume for period=day, but its week/month
        // payloads expose rounded lots.  Keep this period-specific native
        // boundary instead of applying one unit rule to every K-line.
        if (period.name == "week" || period.name == "month") {
            for (auto& bar : bars) {
                bar.volume /= 100;
            }
        } else {
            for (auto& bar : bars)
                bar.volume = std::llround(static_cast<float>(bar.volume));
        }
    }

    auto page = page_from_latest(bars, pages, page_size, start);
    const auto downloaded_count = page.size();
    if (period.intraday && lower_ascii(trim(date)) != "all") {
        std::vector<MinuteBar> selector;
        selector.reserve(page.size());
        for (const auto& bar : page) {
            MinuteBar value;
            value.date = bar.date;
            value.hour = bar.hour;
            value.minute = bar.minute;
            selector.push_back(value);
        }
        const auto selected = select_date(std::move(selector), date);
        const auto wanted = selected.empty() ? 0 : selected.front().date;
        if (wanted == 0)
            page.clear();
        else
            page.erase(std::remove_if(page.begin(), page.end(),
                [&](const LocalBar& bar) { return bar.date != wanted; }), page.end());
    }

    Json document = Json::object();
    document["schema"] = "tdx-minute-v1";
    document["market"] = market;
    document["code"] = code;
    document["name"] = "";
    document["source"] = path_utf8(source_path);
    document["source_mode"] = "local";
    document["transport"] = "local-vipdoc";
    document["aggregation"] = aggregation_name(period);
    document["source_record_count"] =
        static_cast<std::uint64_t>(source_bars.size());
    document["available_count"] = static_cast<std::uint64_t>(bars.size());
    document["downloaded"] = static_cast<std::uint64_t>(downloaded_count);
    document["count"] = static_cast<std::uint64_t>(page.size());
    document["start"] = static_cast<std::uint64_t>(start);
    document["pages"] = static_cast<std::uint64_t>(pages);
    document["page_size"] = static_cast<std::uint64_t>(page_size);
    document["next_start"] =
        static_cast<std::uint64_t>(start) + downloaded_count;
    document["has_more"] = static_cast<std::size_t>(start) + downloaded_count < bars.size();
    document["index_mode"] = index_mode;
    document["period"] = period.name;
    document["period_id"] = static_cast<std::uint64_t>(period.id);
    document["expansion_market"] = false;
    const bool aggregate_lot_volume =
        period.name == "week" || period.name == "month";
    document["volume_unit"] = aggregate_lot_volume ? "lot" : "share";
    document["source_volume_unit"] = "share";
    document["source_volume_divisor"] = aggregate_lot_volume ? 100 : 1;
    document["first_available_date"] = bars.empty()
        ? Json(nullptr) : Json(date_text(bars.front().date));
    document["last_available_date"] = bars.empty()
        ? Json(nullptr) : Json(date_text(bars.back().date));
    Json rows = Json::array();
    for (const auto& bar : page) rows.push_back(bar_json(bar));
    document["bars"] = std::move(rows);
    return document;
}

}  // namespace tdx
