#include "tdx/minute.hpp"
#include "tdx/common.hpp"
#include "tdx/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {
constexpr std::size_t kRecordSize = 32;

std::string two_digits(int value) {
    std::ostringstream output;
    output << std::setw(2) << std::setfill('0') << value;
    return output.str();
}

std::string date_text(int value) {
    const int year = value / 10000;
    const int month = value / 100 % 100;
    const int day = value % 100;
    return std::to_string(year) + "-" + two_digits(month) + "-" + two_digits(day);
}

std::string time_text(const MinuteBar& bar) {
    return two_digits(bar.hour) + ":" + two_digits(bar.minute);
}

}  // namespace

int decode_lc1_date(std::uint16_t value) {
    const int year = value / 2048 + 2004;
    const int remainder = value % 2048;
    const int month = remainder / 100;
    const int day = remainder % 100;
    if (year < 2004 || year > 2200 || month < 1 || month > 12 || day < 1 || day > 31)
        throw Error("invalid LC1 date word: " + std::to_string(value));
    return year * 10000 + month * 100 + day;
}

std::vector<MinuteBar> parse_lc1(const Bytes& data) {
    if (data.size() % kRecordSize != 0)
        throw Error("LC1 size is not a multiple of 32 bytes");
    std::vector<MinuteBar> result;
    result.reserve(data.size() / kRecordSize);
    for (std::size_t offset = 0; offset < data.size(); offset += kRecordSize) {
        const auto* row = data.data() + offset;
        MinuteBar bar;
        bar.date = decode_lc1_date(read_u16_le(row));
        const int minute_word = read_u16_le(row + 2);
        bar.hour = minute_word / 60;
        bar.minute = minute_word % 60;
        bar.open = read_f32_le(row + 4);
        bar.high = read_f32_le(row + 8);
        bar.low = read_f32_le(row + 12);
        bar.close = read_f32_le(row + 16);
        bar.amount = read_f32_le(row + 20);
        bar.volume = read_i32_le(row + 24);
        bar.extra_1 = read_u16_le(row + 28);
        bar.extra_2 = read_u16_le(row + 30);
        if (bar.hour < 0 || bar.hour > 23 || bar.minute < 0 || bar.minute > 59)
            throw Error("invalid LC1 minute at record " + std::to_string(result.size()));
        for (float value : {bar.open, bar.high, bar.low, bar.close, bar.amount})
            if (!std::isfinite(value)) throw Error("non-finite LC1 numeric value");
        if (bar.high + 1e-5f < std::max({bar.open, bar.low, bar.close}) ||
            bar.low - 1e-5f > std::min({bar.open, bar.high, bar.close}))
            throw Error("invalid LC1 OHLC range at record " + std::to_string(result.size() + 1));
        result.push_back(bar);
    }
    return result;
}

fs::path locate_lc1(const fs::path& root, std::string_view market,
                    std::string_view code, bool index_mode) {
    const std::string prefix = market == "0" || market == "sz" ? "sz" :
                               market == "1" || market == "sh" ? "sh" :
                               market == "2" || market == "bj" ? "bj" : "";
    if (prefix.empty()) throw Error("market must be 0/sz, 1/sh, or 2/bj");
    const std::string filename = prefix + std::string(code) + ".lc1";
    std::vector<fs::path> candidates;
    if (index_mode) {
        candidates.push_back(root / "vipdoc" / prefix / "fzline" / filename);
        candidates.push_back(root / "vipdoc" / prefix / "minline" / filename);
    } else {
        candidates.push_back(root / "vipdoc" / prefix / "minline" / filename);
        candidates.push_back(root / "vipdoc" / prefix / "fzline" / filename);
    }
    for (const auto& candidate : candidates) if (fs::is_regular_file(candidate)) return candidate;
    throw Error("LC1 cache not found for " + prefix + std::string(code));
}

std::string render_minute_csv(const MinuteSeries& series) {
    std::ostringstream output;
    output << "date,time,open,high,low,close,amount,volume,extra_1,extra_2,open_interest,auxiliary_price\n";
    output << std::setprecision(9);
    for (const auto& bar : series.bars) {
        output << date_text(bar.date) << ',' << time_text(bar) << ','
               << bar.open << ',' << bar.high << ',' << bar.low << ',' << bar.close << ','
               << (bar.amount_available ? std::to_string(bar.amount) : std::string{}) << ','
               << bar.volume << ',' << bar.extra_1 << ',' << bar.extra_2 << ',';
        if (bar.has_expansion_fields)
            output << bar.open_interest << ',' << bar.auxiliary_price;
        else output << ',';
        output << '\n';
    }
    return output.str();
}

std::string render_minute_json(const MinuteSeries& series, bool pretty) {
    Json root = Json::object();
    root["schema"] = "tdx-minute-v1";
    root["market"] = series.market;
    root["code"] = series.code;
    root["name"] = series.name;
    root["source"] = path_utf8(series.source);
    root["count"] = static_cast<std::uint64_t>(series.bars.size());
    Json rows = Json::array();
    for (const auto& bar : series.bars) {
        Json row = Json::object();
        row["date"] = date_text(bar.date);
        row["time"] = time_text(bar);
        row["open"] = bar.open;
        row["high"] = bar.high;
        row["low"] = bar.low;
        row["close"] = bar.close;
        row["amount"] = bar.amount_available ? Json(bar.amount) : Json(nullptr);
        row["volume"] = bar.volume;
        row["extra_1"] = static_cast<int>(bar.extra_1);
        row["extra_2"] = static_cast<int>(bar.extra_2);
        if (bar.has_expansion_fields) {
            row["open_interest"] = static_cast<std::uint64_t>(bar.open_interest);
            row["auxiliary_price"] = bar.auxiliary_price;
            const auto market = lower_ascii(series.market);
            if (market == "31" || market == "48" || market == "kh" || market == "kg")
                row["hk_short_volume"] = bar.auxiliary_price;
            else row["settlement_price"] = bar.auxiliary_price;
        }
        rows.push_back(std::move(row));
    }
    root["bars"] = std::move(rows);
    return root.dump(pretty ? 2 : -1) + '\n';
}

Json load_minute_document(const fs::path& root, const std::string& market,
                          const std::string& code, bool index_mode,
                          const std::string& selected_date) {
    const auto source = locate_lc1(root, market, code, index_mode);
    MinuteSeries series{market, code, "", source, parse_lc1(read_bytes(source))};
    auto date = lower_ascii(trim(selected_date));
    if (date.empty()) date = "latest";
    if (date != "all") {
        int wanted = 0;
        if (date == "latest") {
            for (const auto& bar : series.bars) wanted = std::max(wanted, bar.date);
        } else {
            date.erase(std::remove(date.begin(), date.end(), '-'), date.end());
            if (date.size() != 8 || !std::all_of(date.begin(), date.end(), [](char ch) {
                    return ch >= '0' && ch <= '9';
                })) throw Error("date must be latest, all, or YYYY-MM-DD");
            wanted = std::stoi(date);
        }
        series.bars.erase(std::remove_if(series.bars.begin(), series.bars.end(),
            [&](const MinuteBar& bar) { return bar.date != wanted; }), series.bars.end());
    }
    return Json::parse(render_minute_json(series, false));
}

int command_minute_extract(const std::vector<std::string>& values) {
    Args args(values);
    if (args.take_flag("--help")) {
        std::cout << "Usage: tdx-tool minute extract --code CODE [--market 0|1|2]"
                     " [--root PATH] [--input FILE] [--date YYYYMMDD]"
                     " [--format json|csv] [--output FILE] [--index]\n";
        return 0;
    }
    const std::string code = args.take_option("--code");
    if (code.empty()) throw Error("minute extract requires --code");
    const std::string market = args.take_option("--market", "0");
    const std::string format = lower_ascii(args.take_option("--format", "json"));
    const std::string selected_date = args.take_option("--date");
    const std::string output_name = args.take_option("--output");
    const std::string input_name = args.take_option("--input");
    const std::string root_name = args.take_option("--root");
    const std::string name = args.take_option("--name");
    const bool index_mode = args.take_flag("--index");
    const bool compact = args.take_flag("--compact");
    args.require_empty();
    if (format != "json" && format != "csv") throw Error("format must be json or csv");

    const fs::path root = find_tdx_root(root_name.empty() ? fs::path{} : fs::u8path(root_name));
    const fs::path source = input_name.empty()
        ? locate_lc1(root, market, code, index_mode)
        : fs::u8path(input_name);
    MinuteSeries series{market, code, name, source, parse_lc1(read_bytes(source))};
    if (!selected_date.empty()) {
        const std::string normalized = selected_date.find('-') == std::string::npos
            ? selected_date
            : selected_date.substr(0, 4) + selected_date.substr(5, 2) + selected_date.substr(8, 2);
        const int date = std::stoi(normalized);
        series.bars.erase(std::remove_if(series.bars.begin(), series.bars.end(),
                                         [&](const MinuteBar& bar) { return bar.date != date; }),
                          series.bars.end());
    }
    const std::string rendered = format == "csv" ? render_minute_csv(series)
                                                   : render_minute_json(series, !compact);
    if (output_name.empty()) std::cout << rendered;
    else {
        const fs::path output = fs::u8path(output_name);
        atomic_write_text(output, rendered);
        std::cout << "wrote " << series.bars.size() << " bars to " << path_utf8(output) << '\n';
    }
    return 0;
}

}  // namespace tdx
