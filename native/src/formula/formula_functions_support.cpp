#include "formula_engine_internal.hpp"

#include "formula_context_support_internal.hpp"
#include "formula_function_dispatch_internal.hpp"

#include "tdx/blocks.hpp"
#include "tdx/common.hpp"
#include "tdx/corporate.hpp"
#include "tdx/options.hpp"
#include "tdx/security_status.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <set>
#include <string_view>
#include <utility>

namespace tdx::formula_engine_detail {

Series constant(double value, std::size_t size) { return Series(size, value); }

bool truth(double value) { return std::isfinite(value) && std::abs(value) > 1e-12; }

bool tcalc_if_select_true(double value) {
    // TCalc IF/IFF/IFN compare the raw-f32 condition with exact zero.
    // The caller owns the leading-sentinel gate; a later interpreter missing
    // value narrows to NaN and therefore follows the native non-zero branch.
    return static_cast<float>(value) != 0.0F;
}

int tcalc_external_integer(double value) {
    const float narrowed = static_cast<float>(value);
    if (!std::isfinite(narrowed) ||
        narrowed >= 2147483648.0f || narrowed < -2147483648.0f)
        return std::numeric_limits<int>::min();
    return static_cast<int>(std::trunc(narrowed));
}

std::string tcalc_external_binding_key(double namespace_value,
                                       double external_id) {
    const auto selector = static_cast<unsigned char>(
        tcalc_external_integer(namespace_value));
    return std::string(selector ? "1#" : "0#") +
           std::to_string(tcalc_external_integer(external_id));
}

double native_float(double value) {
    return static_cast<double>(static_cast<float>(value));
}

std::uint32_t tcalc_msvc_rand(std::uint32_t& state) {
    state = state * 214013u + 2531011u;
    return (state >> 16u) & 0x7fffu;
}

std::string tcalc_number_text(double value, double decimals) {
    if (!std::isfinite(value)) return "-";
    const int precision = std::isfinite(decimals)
        ? std::clamp(static_cast<int>(decimals), 0, 4) : 0;
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision,
                  static_cast<double>(static_cast<float>(value)));
    return buffer;
}

std::size_t tcalc_string_byte_length(std::string_view value) {
    return encode_gbk(value).size();
}

std::string tcalc_substring(std::string_view value, double position,
                            double length) {
    auto bytes = encode_gbk(value);
    if (bytes.empty()) return {};
    auto start = std::isfinite(position)
        ? static_cast<long long>(position) - 1 : 0;
    if (start < 0) start = 0;
    if (start >= static_cast<long long>(bytes.size()))
        start = static_cast<long long>(bytes.size()) - 1;
    auto count = std::isfinite(length) ? static_cast<long long>(length) : 0;
    if (count <= 0) return {};
    count = std::min(count, static_cast<long long>(bytes.size()) - start);
    const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(start);
    return decode_gbk(Bytes(begin, begin + static_cast<std::ptrdiff_t>(count)));
}

int tcalc_market_id(std::string market) {
    return formula_context_detail::formula_market_id(std::move(market));
}

bool tcalc_lfs_security_supported(const std::string& market,
                                  const std::string& code) {
    const int market_id = tcalc_market_id(market);
    const long numeric_code = std::atol(code.c_str());
    if (market_id == 0)
        return code.size() < 2 || code[0] != '3' || code[1] != '9';
    if (market_id == 1)
        return (code.empty() || code.front() != '8') &&
               numeric_code < 990000 && numeric_code > 999;
    if (market_id == 2)
        return numeric_code < 899000 || numeric_code > 899999;
    return true;
}

int tcalc_security_class(int market, const std::string& code) {
    const auto ch = [&](std::size_t index) {
        return index < code.size() ? code[index] : '\0';
    };
    if (market == 0) {
        switch (ch(0)) {
            case '0':
                if (ch(1) == '0')
                    return ch(2) == '2' || ch(2) == '3' || ch(2) == '4' ? 8 : 0;
                if (ch(1) == '3' || ch(1) == '8') return 1;
                return 10;
            case '1':
                if (ch(1) == '0' || ch(1) == '9') return 2;
                if (ch(1) == '1' || ch(1) == '4') return 3;
                if (ch(1) == '2') return 4;
                if (ch(1) == '3') return ch(2) == '1' ? 5 : 3;
                if (ch(1) >= '5' && ch(1) <= '8') return 6;
                return 10;
            case '2':
                if (ch(1) == '3' || ch(1) == '8') return 1;
                return ch(1) == '0' ? 7 : 10;
            case '3':
                if (ch(1) == '0') return 9;
                return ch(1) == '8' ? 1 : 10;
            default:
                return 10;
        }
    }
    if (market == 1) {
        switch (ch(0)) {
            case '0': {
                int value = 0;
                for (const unsigned char value_char : code) {
                    if (!std::isdigit(value_char)) break;
                    value = value * 10 + (value_char - '0');
                }
                return value < 1000 ? 20 : 13;
            }
            case '1':
                return ch(1) != '1' || ch(2) == '2' || ch(2) == '4' || ch(2) == '5'
                    ? 14 : 15;
            case '2': return 16;
            case '5':
                return ch(1) == '8' && (ch(2) == '0' || ch(2) == '2') ? 12 : 17;
            case '6':
                return ch(1) == '8' && (ch(2) == '8' || ch(2) == '9') ? 19 : 11;
            case '7': return ch(1) == '5' || ch(1) == '7' ? 13 : 20;
            case '9': return ch(1) == '0' && ch(2) == '0' ? 18 : 20;
            default: return 20;
        }
    }
    if (market == 2) {
        if (ch(0) == '4') return ch(1) == '3' ? 21 : 24;
        if (ch(0) == '8') {
            if (ch(1) == '3' || ch(1) == '7') return 21;
            if (ch(1) == '1') return 22;
            if (ch(1) == '2' && ch(2) != '0') return 23;
            return 24;
        }
        if (ch(0) == '9' && ch(1) == '2' && ch(2) == '0') return 21;
        return 24;
    }
    return 10;
}

double tcalc_mcst_volume_scale(const std::string& market, const std::string& code) {
    const int market_id = tcalc_market_id(market);
    if (market_id >= 0 && market_id <= 2) {
        static const std::set<int> tenfold{2, 3, 4, 5, 13, 14, 15, 16, 22, 23};
        return tenfold.count(tcalc_security_class(market_id, code)) ? 10.0 : 1.0;
    }
    return market_id == 71 || market_id == 31 || market_id == 32 || market_id == 49
        ? 100.0 : 1.0;
}

bool tcalc_zxnh_uses_typical_price(const std::string& market,
                                   const std::string& code) {
    const int market_id = tcalc_market_id(market);
    if (market_id > 2) return true;
    long value = 0;
    try { value = std::stol(code); } catch (...) { value = 0; }
    if (market_id == 1)
        return (!code.empty() && code.front() == '8') || value >= 990000 || value <= 999;
    if (market_id == 2) return value >= 899000 && value <= 899999;
    return market_id == 0 && code.size() >= 2 && code[0] == '3' && code[1] == '9';
}

}  // namespace tdx::formula_engine_detail
