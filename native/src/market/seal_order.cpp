#include "tdx/seal_order.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace tdx {
namespace {

constexpr double present_threshold = 0.00009999999747378752;
constexpr double limit_match_threshold = 0.000090000001;
constexpr double rounding_bias = 0.503000020980835;

std::map<std::string, std::string, std::less<>> read_rule_ini(const fs::path& path) {
    std::map<std::string, std::string, std::less<>> values;
    if (!fs::is_regular_file(path)) return values;
    std::istringstream input(read_text_utf8(path));
    std::string line, section;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(std::move(line));
        if (line.empty() || line.front() == ';' || line.front() == '#') continue;
        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            section = lower_ascii(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        if (section != "rule") continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos) continue;
        values[lower_ascii(trim(line.substr(0, equal)))] = trim(line.substr(equal + 1));
    }
    return values;
}

int integer_rule(const std::map<std::string, std::string, std::less<>>& rules,
                 std::string_view key, int fallback) {
    const auto found = rules.find(lower_ascii(std::string(key)));
    if (found == rules.end()) return fallback;
    try {
        std::size_t used = 0;
        const int value = std::stoi(found->second, &used);
        return used == found->second.size() ? value : fallback;
    } catch (...) { return fallback; }
}

double decimal_rule(const std::map<std::string, std::string, std::less<>>& rules,
                    std::string_view key, double fallback) {
    const auto found = rules.find(lower_ascii(std::string(key)));
    if (found == rules.end()) return fallback;
    try {
        std::size_t used = 0;
        const double value = std::stod(found->second, &used);
        return used == found->second.size() && std::isfinite(value) && value > 0.0
            ? value : fallback;
    } catch (...) { return fallback; }
}

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

int security_class(int market_id, std::string_view code) {
    if (code.size() != 6) return -1;
    if (market_id == 0) {
        if (starts_with(code, "000") || starts_with(code, "001")) return 0;
        if (starts_with(code, "002") || starts_with(code, "003") ||
            starts_with(code, "004")) return 8;
        if (starts_with(code, "300") || starts_with(code, "301")) return 9;
    } else if (market_id == 1) {
        if (starts_with(code, "600") || starts_with(code, "601") ||
            starts_with(code, "603") || starts_with(code, "605")) return 11;
        if (starts_with(code, "688") || starts_with(code, "689")) return 19;
    } else if (market_id == 2 && starts_with(code, "920")) {
        return 21;
    }
    return -1;
}

bool st_name(std::string_view name) {
    return name.find("ST") != std::string_view::npos || starts_with(name, "S") ||
           starts_with(name, "XDS") || starts_with(name, "XRS") ||
           starts_with(name, "DRS");
}

double stored_float(double value) { return static_cast<double>(static_cast<float>(value)); }

double tdx_upper(double previous_close, double rate, double scale = 100.0) {
    const float previous = static_cast<float>(previous_close);
    const float native_rate = static_cast<float>(rate);
    const float increment = static_cast<float>(
        static_cast<int>(static_cast<double>(native_rate) * previous * scale + rounding_bias) /
        scale);
    return stored_float(static_cast<int>((static_cast<double>(previous) + increment) * scale +
                                         rounding_bias) / scale);
}

double tdx_lower(double previous_close, double rate, double scale = 100.0) {
    const float previous = static_cast<float>(previous_close);
    const float native_rate = static_cast<float>(rate);
    return stored_float(static_cast<int>((1.0 - static_cast<double>(native_rate)) * previous *
                                         scale + rounding_bias) / scale);
}

double bj_limit(double previous_close, double rate, bool upper) {
    const float previous = static_cast<float>(previous_close);
    const float native_rate = static_cast<float>(rate);
    const double factor = upper ? 1.0 + native_rate : 1.0 - native_rate;
    const double bias = upper ? 0.003 : 0.997;
    return stored_float(static_cast<int>(factor * previous * 100.0 + bias) / 100.0);
}

bool present(double value) { return value > present_threshold; }
bool same_price(double left, double right, double tolerance = present_threshold) {
    return std::fabs(left - right) < tolerance;
}

std::uint64_t absolute_queue(std::int64_t value) {
    if (value >= 0) return static_cast<std::uint64_t>(value);
    // Avoid signed overflow for the theoretical INT64_MIN input.
    return static_cast<std::uint64_t>(-(value + 1)) + 1;
}

}  // namespace

LimitRuleConfig load_limit_rule_config(const fs::path& tdx_root) {
    LimitRuleConfig result;
    const auto path = tdx_root / "T0002" / "hq_cache" / "hqrule.dat";
    const auto values = read_rule_ini(path);
    result.sz_st_10_date = integer_rule(values, "SZST10Date", 0);
    result.sh_st_10_date = integer_rule(values, "SHST10Date", 0);
    result.chinext_rate = decimal_rule(values, "CYBZDRatio", 0.20);
    result.star_rate = decimal_rule(values, "KCBZDRatio", 0.20);
    result.beijing_rate = decimal_rule(values, "BJBZDRatio", 0.30);
    result.source = fs::is_regular_file(path) ? path_utf8(path) : "TdxW-defaults";
    return result;
}

LimitPrices calculate_limit_prices(int market_id, std::string_view code,
                                   std::string_view name, double previous_close,
                                   int as_of_yyyymmdd, std::uint64_t quote_flags,
                                   const LimitRuleConfig& rules,
                                   const std::optional<SpecialLimitPrice>& special) {
    LimitPrices result;
    result.security_class = security_class(market_id, code);
    if (!std::isfinite(previous_close) || previous_close <= present_threshold ||
        (quote_flags & 0x6000U) == 0x4000U || result.security_class < 0 ||
        (!name.empty() && name.front() == 'N')) {
        result.source = "tdxw-sub_5AA080-not-price-limited";
        return result;
    }
    if (special && std::isfinite(special->upper) && std::isfinite(special->lower) &&
        special->upper > 0.0 && special->lower > 0.0) {
        result.available = true;
        result.upper = stored_float(special->upper);
        result.lower = stored_float(special->lower);
        result.source = "public-special-limit-0x0452";
        return result;
    }

    double rate = 0.10;
    const bool sz_st_class = result.security_class == 0 || result.security_class == 7 ||
                             result.security_class == 8;
    const bool sh_st_class = result.security_class == 11 || result.security_class == 18;
    if (((sz_st_class && rules.sz_st_10_date < as_of_yyyymmdd) ||
         (sh_st_class && rules.sh_st_10_date < as_of_yyyymmdd)) && st_name(name)) {
        rate = 0.05;
    } else if (result.security_class == 9) {
        rate = rules.chinext_rate;
    } else if (result.security_class == 19) {
        rate = rules.star_rate;
    } else if (result.security_class == 21) {
        rate = rules.beijing_rate;
    }
    if (!std::isfinite(rate) || rate <= present_threshold) {
        result.source = "tdxw-sub_5AA080-disabled-rate";
        return result;
    }
    result.available = true;
    result.rate = rate;
    if (result.security_class == 21) {
        result.upper = bj_limit(previous_close, rate, true);
        result.lower = bj_limit(previous_close, rate, false);
    } else {
        result.upper = tdx_upper(previous_close, rate);
        result.lower = tdx_lower(previous_close, rate);
    }
    result.source = "local-hqrule-sub_5AA080";
    return result;
}

SealOrderResult calculate_seal_order(const SealOrderInput& quote,
                                     const LimitPrices& limits) {
    SealOrderResult result;
    if (!limits.available || !std::isfinite(quote.last_price) ||
        !std::isfinite(quote.trade_unit)) return result;
    result.available = true;
    result.ratio_available = quote.total_hand != 0;
    result.amount_available = quote.trade_unit > 0.0;

    const auto& bid1 = quote.buys[0];
    const auto& ask1 = quote.sells[0];
    const auto& bid2 = quote.buys[1];
    const auto& ask2 = quote.sells[1];
    int direction = 0;
    std::uint64_t queue = 0;
    std::uint64_t denominator = quote.total_hand;
    std::string mode = "not-sealed";

    if (!present(quote.last_price) && present(bid1.price) &&
        same_price(bid1.price, ask1.price)) {
        if (same_price(bid1.price, limits.upper, limit_match_threshold) &&
            quote.auction_imbalance_hand > 0) {
            direction = 1;
            queue = absolute_queue(quote.auction_imbalance_hand);
            mode = "auction-imbalance";
        } else if (same_price(bid1.price, limits.lower, limit_match_threshold) &&
                   quote.auction_imbalance_hand < 0) {
            direction = -1;
            queue = absolute_queue(quote.auction_imbalance_hand);
            mode = "auction-imbalance";
        }
    } else if (present(quote.last_price) && present(bid1.price) &&
               same_price(bid1.price, ask1.price) && !present(bid2.price) &&
               !present(ask2.price)) {
        if (bid2.volume_hand && same_price(bid1.price, limits.upper)) {
            direction = 1;
            queue = bid2.volume_hand;
            denominator += bid1.volume_hand;
            mode = "auction-second-level";
        } else if (ask2.volume_hand && same_price(bid1.price, limits.lower)) {
            direction = -1;
            queue = ask2.volume_hand;
            denominator += ask1.volume_hand;
            mode = "auction-second-level";
        }
    }

    const bool rejected_state = (quote.quote_flags & 0x3CU) == 0x1CU;
    if (!direction && !rejected_state &&
        same_price(quote.last_price, limits.upper, limit_match_threshold) &&
        present(bid1.price) && !present(ask1.price)) {
        direction = 1;
        queue = bid1.volume_hand;
        mode = "continuous";
    } else if (!direction && !rejected_state &&
               same_price(quote.last_price, limits.lower, limit_match_threshold) &&
               !present(bid1.price) && present(ask1.price)) {
        direction = -1;
        queue = ask1.volume_hand;
        mode = "continuous";
    }

    result.direction = direction;
    result.queue_hand = queue;
    result.denominator_hand = denominator;
    result.mode = std::move(mode);
    if (!direction) return result;
    const double price = direction > 0 ? bid1.price : ask1.price;
    if (result.amount_available)
        result.amount_yuan = static_cast<double>(direction) * price *
                             static_cast<double>(queue) * quote.trade_unit;
    if (denominator != 0) {
        result.ratio_available = true;
        result.ratio = static_cast<double>(direction) * static_cast<double>(queue) /
                       static_cast<double>(denominator);
    } else {
        result.ratio_available = false;
    }
    return result;
}

}  // namespace tdx
