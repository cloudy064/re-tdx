#include "formula_function_dispatch_internal.hpp"
#include "formula_native_constants.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace tdx::formula_engine_detail {
namespace {

using FunctionHandler = Series (*)(const std::string&,
                                   const std::vector<Series>&,
                                   const Environment&, std::size_t);

std::tuple<int, unsigned, unsigned> calendar_date_from_day_number(
    long long value) {
    const long long era =
        value >= 0 ? value / 146097 : (value - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(value - era * 146097);
    const unsigned yoe =
        (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int year = static_cast<int>(yoe) + static_cast<int>(era * 400);
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned day = doy - (153 * mp + 2) / 5 + 1;
    const unsigned month = mp < 10 ? mp + 3 : mp - 9;
    year += month <= 2;
    return {year, month, day};
}

Series date_to_current(const std::string& name,
                       const std::vector<Series>& args,
                       const Environment& env, std::size_t size) {
    require_arity(name, args, 1, 1);
    Series out(size, missing);
    if (!size) return out;
    const auto dates = env.find("DATE");
    if (dates == env.end() || dates->second.size() != size)
        throw Error("DATETOCUR requires DATE bar context");
    int target = std::numeric_limits<int>::min();
    const float native_target = native_float(args[0].back());
    if (std::isfinite(native_target) &&
        static_cast<double>(native_target) >=
            static_cast<double>(std::numeric_limits<int>::min()) &&
        static_cast<double>(native_target) < 2147483648.0)
        target = static_cast<int>(native_target);

    std::map<int, std::size_t> counts;
    for (double value : dates->second) {
        const float native_date = native_float(value);
        if (!std::isfinite(native_date) ||
            static_cast<double>(native_date) <
                static_cast<double>(std::numeric_limits<int>::min()) ||
            static_cast<double>(native_date) >= 2147483648.0)
            continue;
        ++counts[static_cast<int>(native_date)];
    }
    std::map<int, std::size_t> cumulative;
    std::size_t total = 0;
    for (const auto& [date, count] : counts) {
        total += count;
        cumulative.emplace(date, total);
    }
    const auto at_or_before = [&](int date) -> std::size_t {
        const auto upper = cumulative.upper_bound(date);
        return upper == cumulative.begin() ? 0 : std::prev(upper)->second;
    };
    const std::size_t excluded = at_or_before(target);
    for (std::size_t i = 0; i < size; ++i) {
        const float native_date = native_float(dates->second[i]);
        if (!std::isfinite(native_date) ||
            static_cast<double>(native_date) <
                static_cast<double>(std::numeric_limits<int>::min()) ||
            static_cast<double>(native_date) >= 2147483648.0)
            continue;
        const int current = static_cast<int>(native_date);
        const std::size_t included = at_or_before(current);
        out[i] = static_cast<double>(
            included > excluded ? included - excluded : 0);
    }
    return out;
}

Series date_to_day(const std::string& name,
                   const std::vector<Series>& args,
                   const Environment&, std::size_t size) {
    require_arity(name, args, 1, 1);
    Series out(size, missing);
    const auto valid_date = [](int year, unsigned month, unsigned day) {
        if (month < 1 || month > 12 || day < 1) return false;
        static constexpr unsigned month_days[]{
            31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        unsigned limit = month_days[month - 1];
        const bool leap =
            year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
        if (month == 2 && leap) ++limit;
        return day <= limit;
    };
    const auto epoch = calendar_day_number(1990, 12, 19);
    for (std::size_t i = 0; i < size; ++i) {
        if (!std::isfinite(args[0][i])) continue;
        const auto encoded = static_cast<int>(
            native_float(args[0][i]) + tdx::formula_engine_detail::tcalc_constants::integer_bias);
        if (encoded < 901219 || encoded > 1341231) continue;
        const int year = 1900 + encoded / 10000;
        const auto month = static_cast<unsigned>((encoded / 100) % 100);
        const auto day = static_cast<unsigned>(encoded % 100);
        if (valid_date(year, month, day))
            out[i] = static_cast<double>(
                calendar_day_number(year, month, day) - epoch);
    }
    return out;
}

Series day_to_date(const std::string& name,
                   const std::vector<Series>& args,
                   const Environment&, std::size_t size) {
    require_arity(name, args, 1, 1);
    Series out(size, missing);
    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    const auto epoch = calendar_day_number(1990, 12, 19);
    for (std::size_t i = 0; i < size; ++i) {
        if (!std::isfinite(args[0][i])) continue;
        const double value = native_float(args[0][i]);
        const double tolerance =
            std::abs(value) * relative_epsilon + native_epsilon;
        if (value + tolerance <= 0.0 || value - tolerance >= 100000.0)
            continue;
        const int offset =
            static_cast<int>(value + tdx::formula_engine_detail::tcalc_constants::integer_bias);
        const auto [year, month, day] =
            calendar_date_from_day_number(epoch + offset);
        out[i] = static_cast<double>(
            (year - 1900) * 10000 + static_cast<int>(month) * 100 +
            static_cast<int>(day));
    }
    return out;
}

Series time_conversion(const std::string& name,
                       const std::vector<Series>& args,
                       const Environment&, std::size_t size) {
    require_arity(name, args, 1, 1);
    Series out(size, missing);
    for (std::size_t i = 0; i < size; ++i) {
        if (!std::isfinite(args[0][i])) continue;
        const int value = static_cast<int>(
            native_float(args[0][i]) + tdx::formula_engine_detail::tcalc_constants::integer_bias);
        if (name == "TIMETOSEC") {
            const int hour = value / 10000;
            const int minute = value % 10000 / 100;
            const int second = value % 100;
            if (hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
                second < 0 || second > 59)
                continue;
            out[i] = static_cast<double>(
                second + 60 * (minute + 60 * hour));
        } else {
            if (value < 0 || value > 86399) continue;
            const int hour = value / 3600;
            const int minute = value % 3600 / 60;
            const int second = value % 60;
            out[i] = static_cast<double>(
                hour * 10000 + minute * 100 + second);
        }
    }
    return out;
}

Series align_right(const std::string& name,
                   const std::vector<Series>& args,
                   const Environment&, std::size_t size) {
    require_arity(name, args, 1, 1);
    Series out(size, missing);
    std::size_t write = size;
    for (std::size_t read = size; read-- > 0;)
        if (std::isfinite(args[0][read])) out[--write] = args[0][read];
    return out;
}

Series time_filter(const std::string& name,
                   const std::vector<Series>& args,
                   const Environment& env, std::size_t size) {
    require_arity(name, args, 5, 5);
    Series out(size, missing);
    if (!size) return out;
    const auto dates = env.find("DATE");
    const auto times = env.find("TIME");
    if (dates == env.end() || times == env.end())
        throw Error("TFILT requires DATE/TIME bar context");
    if (!std::isfinite(args[1].back()) ||
        !std::isfinite(args[2].back()) ||
        !std::isfinite(args[3].back()) ||
        !std::isfinite(args[4].back()) ||
        !std::isfinite(dates->second.back()))
        return out;
    int start_date = static_cast<int>(native_float(args[1].back()));
    int start_time = static_cast<int>(native_float(args[2].back()));
    int end_date = static_cast<int>(native_float(args[3].back()));
    int end_time = static_cast<int>(native_float(args[4].back()));
    const int last_date = static_cast<int>(dates->second.back());
    if (start_date < 700000) start_date = last_date;
    if (end_date < 700000) end_date = last_date;
    start_time = std::clamp(start_time, 0, 2359);
    end_time = std::clamp(end_time, 0, 2359);
    const long long start =
        static_cast<long long>(start_date) * 10000 + start_time;
    const long long end =
        static_cast<long long>(end_date) * 10000 + end_time;
    for (std::size_t i = 0; i < size; ++i) {
        if (!std::isfinite(dates->second[i]) ||
            !std::isfinite(times->second[i]))
            continue;
        const auto stamp =
            static_cast<long long>(dates->second[i]) * 10000 +
            static_cast<int>(times->second[i]);
        if (stamp >= start && stamp <= end) out[i] = args[0][i];
    }
    return out;
}

bool tcalc_filter_signal(double value) {
    constexpr double native_epsilon = tdx::formula_engine_detail::tcalc_constants::absolute_epsilon;
    constexpr double relative_epsilon = tdx::formula_engine_detail::tcalc_constants::relative_epsilon;
    if (!std::isfinite(value)) return false;
    value = native_float(value);
    return value - std::abs(value) * relative_epsilon - native_epsilon >=
           0.5;
}

Series transaction_filter(const std::string& name,
                          const std::vector<Series>& args,
                          const Environment&, std::size_t size) {
    require_arity(name, args, 3, 3);
    Series out(size, 0.0);
    if (!size || !std::isfinite(args[2].back())) return out;
    const unsigned mode = static_cast<unsigned>(
        static_cast<int>(native_float(args[2].back())));
    bool state = false;
    bool next_state = false;
    for (std::size_t i = 0; i < size; ++i) {
        bool buy = tcalc_filter_signal(args[0][i]);
        bool sell = tcalc_filter_signal(args[1][i]);
        if (!buy) {
            if (sell) {
                if ((mode == 0 || mode == 2) && !state) sell = false;
                else next_state = false;
            }
        } else {
            if (mode <= 1 && state) buy = false;
            else next_state = true;
        }
        if (mode == 1) out[i] = buy ? args[0][i] : 0.0;
        else if (mode == 2) out[i] = sell ? args[1][i] : 0.0;
        else out[i] = buy ? 1.0 : sell ? 2.0 : 0.0;
        state = next_state;
    }
    return out;
}

Series four_way_transaction_filter(const std::string& name,
                                   const std::vector<Series>& args,
                                   const Environment&, std::size_t size) {
    require_arity(name, args, 5, 5);
    Series out(size, 0.0);
    if (!size || !std::isfinite(args[4].back())) return out;
    const unsigned mode = static_cast<unsigned>(
        static_cast<int>(native_float(args[4].back())));
    bool long_state = false;
    bool next_long_state = false;
    bool short_state = false;
    for (std::size_t i = 0; i < size; ++i) {
        bool buy_open = tcalc_filter_signal(args[0][i]);
        bool sell_close = tcalc_filter_signal(args[1][i]);
        bool sell_open = tcalc_filter_signal(args[2][i]);
        bool buy_close = tcalc_filter_signal(args[3][i]);
        if (buy_open) {
            if (mode <= 1 && long_state) buy_open = false;
            else {
                long_state = true;
                next_long_state = true;
            }
        }
        if (sell_close) {
            if ((mode != 0 && mode != 2) || long_state)
                next_long_state = false;
            else
                sell_close = false;
        }
        if (sell_open) {
            if ((mode == 0 || mode == 3) && short_state)
                sell_open = false;
            else
                short_state = true;
        }
        if (buy_close) {
            if ((mode != 0 && mode != 4) || short_state)
                short_state = false;
            else
                buy_close = false;
        }
        if (mode == 1) out[i] = buy_open ? args[0][i] : 0.0;
        else if (mode == 2) out[i] = sell_close ? args[1][i] : 0.0;
        else if (mode == 3) out[i] = sell_open ? args[2][i] : 0.0;
        else if (mode == 4) out[i] = buy_close ? args[3][i] : 0.0;
        else
            out[i] = buy_open ? 1.0 : sell_close ? 2.0 :
                     sell_open ? 3.0 : buy_close ? 4.0 : 0.0;
        long_state = next_long_state;
    }
    return out;
}

const std::unordered_map<std::string_view, FunctionHandler>&
calendar_registry() {
    static const std::unordered_map<std::string_view, FunctionHandler> registry{
        {"DATETOCUR", date_to_current},
        {"DATETODAY", date_to_day},
        {"DATETOTODAY", date_to_day},
        {"DAYTODATE", day_to_date},
        {"TIMETOSEC", time_conversion},
        {"SECTOTIME", time_conversion},
        {"ALIGNRIGHT", align_right},
        {"TFILT", time_filter},
        {"TFILTER", transaction_filter},
        {"TTFILTER", four_way_transaction_filter},
    };
    return registry;
}

}  // namespace

std::optional<Series> evaluate_calendar_function(
    const std::string& name, const std::vector<Series>& args,
    const Environment& env, std::size_t size) {
    const auto found = calendar_registry().find(name);
    if (found == calendar_registry().end()) return std::nullopt;
    return found->second(name, args, env, size);
}

}  // namespace tdx::formula_engine_detail
