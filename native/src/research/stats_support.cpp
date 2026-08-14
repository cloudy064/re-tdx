#include "stats_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace tdx::stats_detail {

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
        })) throw Error("invalid statistics security identifier");
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


}  // namespace tdx::stats_detail

