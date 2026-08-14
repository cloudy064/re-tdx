#include "professional_data_internal.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::professional_data_detail {

std::string Security::id() const {
    return market_id == 0 ? "SZ" + code : market_id == 1 ? "SH" + code
                                                      : "BJ" + code;
}

fs::path default_cache_directory() {
    return fs::u8path("output/tdx-professional-cache");
}

std::uint32_t current_local_date() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &now)) throw Error("cannot read local date");
#else
    if (!localtime_r(&now, &local)) throw Error("cannot read local date");
#endif
    return static_cast<std::uint32_t>((local.tm_year + 1900) * 10000 +
        (local.tm_mon + 1) * 100 + local.tm_mday);
}

bool valid_code(std::string_view code) {
    return code.size() == 6 && std::all_of(code.begin(), code.end(), [](char ch) {
        return ch >= '0' && ch <= '9';
    });
}

int inferred_market_id(std::string_view code) {
    if (code.rfind("92", 0) == 0 || (!code.empty() && (code.front() == '4' || code.front() == '8')))
        return 2;
    if (!code.empty() && (code.front() == '6' || code.front() == '9')) return 1;
    return 0;
}

Security parse_security(std::string value) {
    value = lower_ascii(trim(std::move(value)));
    std::string market, code;
    const auto colon = value.find(':');
    if (colon != std::string::npos) {
        market = value.substr(0, colon); code = value.substr(colon + 1);
    } else if (value.size() == 8 &&
               (value.rfind("sz", 0) == 0 || value.rfind("sh", 0) == 0 ||
                value.rfind("bj", 0) == 0)) {
        market = value.substr(0, 2); code = value.substr(2);
    } else {
        code = value;
        market = code.rfind("92", 0) == 0 || (!code.empty() && code.front() == '8')
            ? "bj" : (!code.empty() && (code.front() == '6' || code.front() == '9'))
            ? "sh" : "sz";
    }
    if (market == "0") market = "sz";
    else if (market == "1") market = "sh";
    else if (market == "2") market = "bj";
    const int market_id = market == "sz" ? 0 : market == "sh" ? 1 : market == "bj" ? 2 : -1;
    if (market_id < 0 || !valid_code(code)) throw Error("invalid security: " + value);
    return {market_id, market, code};
}

int parse_integer(const std::string& text, std::string_view name, int minimum, int maximum) {
    try {
        std::size_t used = 0;
        const int result = std::stoi(text, &used);
        if (used != text.size() || result < minimum || result > maximum)
            throw std::invalid_argument("range");
        return result;
    } catch (...) {
        throw Error(std::string(name) + " must be in " + std::to_string(minimum) +
                    ".." + std::to_string(maximum));
    }
}

std::uint32_t parse_date(const std::string& text, std::string_view name,
                         std::uint32_t fallback) {
    if (text.empty()) return fallback;
    std::string digits;
    for (const char ch : text) if (ch >= '0' && ch <= '9') digits.push_back(ch);
    if (digits.size() != 8) throw Error(std::string(name) + " must be YYYYMMDD or YYYY-MM-DD");
    const auto value = static_cast<std::uint32_t>(std::stoul(digits));
    const auto month = value / 100 % 100, day = value % 100;
    if (value / 10000 < 1900 || value / 10000 > 2200 || month < 1 || month > 12 ||
        day < 1 || day > 31) throw Error(std::string(name) + " is not a valid date");
    return value;
}

std::string date_text(std::uint32_t value) {
    if (!value) return {};
    const auto raw = std::to_string(value);
    if (raw.size() != 8) return raw;
    return raw.substr(0, 4) + "-" + raw.substr(4, 2) + "-" + raw.substr(6, 2);
}

std::uint16_t u16_at(const Bytes& data, std::size_t offset, std::string_view context) {
    if (offset > data.size() || data.size() - offset < 2)
        throw Error(std::string(context) + " exceeds bounds");
    return read_u16_le(data.data() + offset);
}

std::uint32_t u32_at(const Bytes& data, std::size_t offset, std::string_view context) {
    if (offset > data.size() || data.size() - offset < 4)
        throw Error(std::string(context) + " exceeds bounds");
    return read_u32_le(data.data() + offset);
}

std::optional<double> finite_float(const std::uint8_t* data) {
    const double value = read_f32_le(data);
    return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
}

Json number_json(const std::optional<double>& value) {
    return value ? Json(*value) : Json(nullptr);
}

}  // namespace tdx::professional_data_detail
