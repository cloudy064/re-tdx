#include "tdx/trades_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace tdx {
namespace detail {
namespace trades {

void append_u16(Bytes& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(Bytes& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<std::uint8_t>(value >> shift));
}

int parse_integer(const std::string& text, std::string_view name,
                  int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used, 0);
        if (used != text.size() || value < minimum || value > maximum)
            throw std::invalid_argument("range");
        return value;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

void validate_security(int market_id, std::string_view code) {
    if (market_id < 0 || market_id > 2 || code.size() != 6 ||
        !std::all_of(code.begin(), code.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("invalid trade security identifier");
}

SecurityCode parse_security(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    int market = -1;
    std::string code;
    const auto colon = value.find(':');
    if (colon != std::string::npos) {
        const auto prefix = value.substr(0, colon);
        code = value.substr(colon + 1);
        if (prefix == "sz" || prefix == "0") market = 0;
        else if (prefix == "sh" || prefix == "1") market = 1;
        else if (prefix == "bj" || prefix == "2") market = 2;
    } else if (value.size() == 8 &&
               (value.rfind("sz", 0) == 0 || value.rfind("sh", 0) == 0 ||
                value.rfind("bj", 0) == 0)) {
        market = value.rfind("sz", 0) == 0 ? 0 : value.rfind("sh", 0) == 0 ? 1 : 2;
        code = value.substr(2);
    } else {
        code = value;
        if (!code.empty() && (code.front() == '6' || code.front() == '9')) market = 1;
        else if (!code.empty() && code.front() == '8') market = 2;
        else market = 0;
    }
    validate_security(market, code);
    return {market, code};
}

std::string security_id(int market_id, const std::string& code) {
    return std::array<std::string, 3>{"SZ", "SH", "BJ"}.at(
        static_cast<std::size_t>(market_id)) + code;
}

std::string normalize_date(std::string value) {
    value = trim(std::move(value));
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
    if (value.size() != 8 || !std::all_of(value.begin(), value.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("trading date must use YYYYMMDD or YYYY-MM-DD");
    const int year = std::stoi(value.substr(0, 4));
    const int month = std::stoi(value.substr(4, 2));
    const int day = std::stoi(value.substr(6, 2));
    static const std::array<int, 12> month_days{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1990 || year > 2200 || month < 1 || month > 12)
        throw Error("trading date is invalid: " + value);
    int maximum = month_days[static_cast<std::size_t>(month - 1)];
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
        ++maximum;
    if (day < 1 || day > maximum) throw Error("trading date is invalid: " + value);
    return value;
}

std::string now_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
    return output.str();
}

std::string time_label(int minute) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << minute / 60 << ':'
           << std::setw(2) << minute % 60;
    return output.str();
}

std::string trade_side(std::int64_t status) {
    if (status == 0) return "buy";
    if (status == 1) return "sell";
    if (status == 2) return "neutral";
    return "status_" + std::to_string(status);
}

int trade_price_divisor(const std::string& code) {
    for (const auto prefix : {"10", "11", "12", "15", "16", "50", "51",
                              "52", "53", "56", "58"})
        if (code.rfind(prefix, 0) == 0) return 1000;
    return 100;
}

}  // namespace trades
}  // namespace detail
}  // namespace tdx
