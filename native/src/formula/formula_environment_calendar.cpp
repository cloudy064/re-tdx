#include "formula_environment_internal.hpp"

#include "formula_context_support_internal.hpp"
#include "formula_engine_support_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tdx::formula_runtime_detail {
using namespace formula_engine_detail;
using namespace formula_engine_support;

namespace {

// Splits `now` into local calendar fields.  The three per-bar phases each read
// their own fields off the result, so the clock is sampled once by build().
std::tm local_calendar(std::time_t now) {
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return local;
}

bool uses_default_a_share_sessions(const Json& document) {
    const auto* market = optional(document, "market");
    if (!market || !market->is_string()) return false;
    const auto value = lower_ascii(market->as_string());
    return value == "sz" || value == "sh" || value == "bj" ||
           value == "0" || value == "1" || value == "2";
}

struct TradingSession {
    int start = 0;
    int end = 0;
};

int parse_session_time(const Json& value, std::size_t index,
                       std::string_view field) {
    if (!value.is_string())
        throw Error("trading_sessions[" + std::to_string(index) + "]." +
                    std::string(field) + " must use strict HH:MM text");
    const auto& text = value.as_string();
    if (text.size() != 5 || text[2] != ':' ||
        text[0] < '0' || text[0] > '9' || text[1] < '0' || text[1] > '9' ||
        text[3] < '0' || text[3] > '9' || text[4] < '0' || text[4] > '9')
        throw Error("trading_sessions[" + std::to_string(index) + "]." +
                    std::string(field) + " must use strict HH:MM text");
    const int hour = (text[0] - '0') * 10 + text[1] - '0';
    const int minute = (text[3] - '0') * 10 + text[4] - '0';
    if (hour > 23 || minute > 59)
        throw Error("trading_sessions[" + std::to_string(index) + "]." +
                    std::string(field) + " is outside 00:00..23:59");
    return hour * 60 + minute;
}

std::optional<std::vector<TradingSession>> explicit_trading_sessions(
    const Json& document) {
    const auto* value = optional(document, "trading_sessions");
    if (!value) return std::nullopt;
    if (!value->is_array() || value->size() == 0 || value->size() > 4)
        throw Error("trading_sessions must be a non-empty array of at most four sessions");

    std::vector<TradingSession> sessions;
    sessions.reserve(value->size());
    for (std::size_t index = 0; index < value->size(); ++index) {
        const auto& row = value->as_array()[index];
        const auto* start_value = optional(row, "start");
        const auto* end_value = optional(row, "end");
        if (!row.is_object() || row.size() != 2 || !start_value || !end_value)
            throw Error("trading_sessions[" + std::to_string(index) +
                        "] must contain only start and end");
        const int raw_start = parse_session_time(*start_value, index, "start");
        const int raw_end = parse_session_time(*end_value, index, "end");
        if (raw_start == raw_end)
            throw Error("trading_sessions[" + std::to_string(index) +
                        "] must have non-equal start and end");

        int start = raw_start;
        if (!sessions.empty()) {
            // Linearize a listed next-day segment exactly once.  A second wrap
            // would make the declared trading cycle exceed 24 hours.
            if (start < sessions.back().end) start += 24 * 60;
            if (start < sessions.back().end)
                throw Error("trading_sessions must be ordered and non-overlapping");
        }
        int end = raw_end;
        if (end <= start) end += 24 * 60;
        if (end <= start ||
            (!sessions.empty() && end - sessions.front().start > 24 * 60))
            throw Error("trading_sessions must fit one ordered 24-hour trading cycle");
        sessions.push_back({start, end});
    }
    if (sessions.back().end - sessions.front().start > 24 * 60)
        throw Error("trading_sessions must fit one ordered 24-hour trading cycle");
    return sessions;
}

double explicit_sessions_from_open(
    int minute_of_day, const std::vector<TradingSession>& sessions) {
    int minute = minute_of_day;
    // A cycle ending on the following civil day maps only that morning's tail
    // onto +1440.  After the tail closes, values before the next first session
    // are correctly treated as pre-open rather than as the prior day's close.
    if (sessions.back().end > 24 * 60 &&
        minute <= sessions.back().end - 24 * 60)
        minute += 24 * 60;
    if (minute < sessions.front().start - 5) return 0.0;
    if (minute <= sessions.front().start) return 1.0;

    int elapsed = 0;
    for (const auto& session : sessions) {
        if (minute < session.start) return static_cast<double>(elapsed);
        if (minute < session.end)
            return static_cast<double>(elapsed + minute - session.start + 1);
        elapsed += session.end - session.start;
    }
    return static_cast<double>(elapsed);
}

double explicit_sessions_total(const std::vector<TradingSession>& sessions) {
    int total = 0;
    for (const auto& session : sessions) total += session.end - session.start;
    return static_cast<double>(total);
}

std::optional<double> explicit_formula_scalar(const Json* context,
                                              std::string_view name) {
    if (!context || !context->is_object()) return std::nullopt;
    const auto* scalars = optional(*context, "formula_scalar_bindings");
    if (!scalars || !scalars->is_object()) return std::nullopt;
    const Json* selected = nullptr;
    // Match bind_context_numeric_groups' canonicalization and deterministic
    // last-entry-wins behavior for case-only duplicate keys.
    for (const auto& [key, value] : scalars->as_object())
        if (upper_ascii(key) == name) selected = &value;
    if (!selected || !selected->is_number() ||
        !std::isfinite(selected->as_number()))
        return std::nullopt;
    return selected->as_number();
}

// TCalc sub_100172B0 obtains the type-105 session pairs and passes the current
// minute to sub_10002070.  For TdxW's set-code 0/1/2 defaults, that helper is
// one-based inside a session, clamps the lunch break, and clamps after close.
// The five-minute pre-open allowance is the guard immediately before the call.
double default_a_share_from_open(int minute_of_day) {
    constexpr int first_open = 9 * 60 + 30;
    constexpr int first_close = 11 * 60 + 30;
    constexpr int second_open = 13 * 60;
    constexpr int second_close = 15 * 60;
    if (minute_of_day < first_open - 5) return 0.0;
    if (minute_of_day <= first_open) return 1.0;
    if (minute_of_day < first_close)
        return static_cast<double>(minute_of_day - first_open + 1);
    if (minute_of_day < second_open)
        return static_cast<double>(first_close - first_open);
    if (minute_of_day < second_close)
        return static_cast<double>(first_close - first_open +
                                   minute_of_day - second_open + 1);
    return static_cast<double>((first_close - first_open) +
                               (second_close - second_open));
}

double formula_minimum_price_increment(const Json& document) {
    double value = 0.01;
    if (const auto* min_tick = optional(document, "min_tick");
        min_tick && min_tick->is_number() &&
        std::isfinite(min_tick->as_number()) && min_tick->as_number() > 0.0) {
        value = min_tick->as_number();
    } else if (const auto* precision = optional(document, "price_precision");
               precision && precision->is_number() &&
               std::isfinite(precision->as_number())) {
        const auto digits = static_cast<int>(precision->as_number());
        if (digits >= 0 && digits <= 8 &&
            static_cast<double>(digits) == precision->as_number())
            value = std::pow(10.0, -digits);
    }
    // TCalc sub_100269C0 first reads the type-105 +44 float, then replaces
    // values at or below its float32 1e-5 constant before broadcasting the
    // result through a float output buffer.
    const double native_value = native_float(value);
    const double native_floor = native_float(0.00001);
    return native_value > native_floor ? native_value : native_floor;
}

}  // namespace

void FormulaEnvironmentBuilder::bind_bar_position_series(Environment& env) const {
    // TCalc sub_1000E6A0 broadcasts the evaluator's complete input-bar count;
    // unlike BARSCOUNT, USEDDATANUM is not cumulative at each output point.
    Series current_bars(bars_.size()),
           used_bars(bars_.size(), static_cast<double>(bars_.size())),
           last_bar(bars_.size(), 0.0), bar_status(bars_.size(), 0.0),
           total_bars(bars_.size(), static_cast<double>(bars_.size()));
    for (std::size_t i = 0; i < bars_.size(); ++i) {
        current_bars[i] = static_cast<double>(bars_.size() - i);
        last_bar[i] = i + 1 == bars_.size() ? 1.0 : 0.0;
        bar_status[i] = i == 0 ? 1.0 : i + 1 == bars_.size() ? 2.0 : 0.0;
        if (bars_.size() == 1) bar_status[i] = 2.0;
    }
    env["CURRBARSCOUNT"] = std::move(current_bars);
    env["USEDDATANUM"] = std::move(used_bars);
    env["ISLASTBAR"] = std::move(last_bar);
    env["BARSTATUS"] = std::move(bar_status);
    env["TOTALBARSCOUNT"] = std::move(total_bars);
}

void FormulaEnvironmentBuilder::bind_calendar_series(Environment& env,
                                                     std::time_t now) const {
    Series dates(bars_.size(), 0.0), bar_days(bars_.size(), missing),
           years(bars_.size(), missing), months(bars_.size(), missing),
           month_days(bars_.size(), missing), weekdays(bars_.size(), missing),
           weeks_of_year(bars_.size(), missing), days_to_today(bars_.size(), missing);
    const auto local = local_calendar(now);
    const auto today_number = calendar_day_number(
        local.tm_year + 1900, static_cast<unsigned>(local.tm_mon + 1),
        static_cast<unsigned>(local.tm_mday));
    for (std::size_t i = 0; i < bars_.size(); ++i) {
        if (bars_[i].date.size() < 10) continue;
        try {
            const int year = std::stoi(bars_[i].date.substr(0, 4));
            const unsigned month = static_cast<unsigned>(std::stoi(bars_[i].date.substr(5, 2)));
            const unsigned day = static_cast<unsigned>(std::stoi(bars_[i].date.substr(8, 2)));
            dates[i] = (year - 1900) * 10000 + month * 100 + day;
            const auto day_number = calendar_day_number(year, month, day);
            bar_days[i] = static_cast<double>(day_number);
            years[i] = static_cast<double>(year);
            months[i] = static_cast<double>(month);
            month_days[i] = static_cast<double>(day);
            weekdays[i] = static_cast<double>((day_number + 3) % 7);
            const auto first_day = calendar_day_number(year, 1, 1);
            const auto first_weekday = (first_day + 3) % 7;
            weeks_of_year[i] = static_cast<double>(
                (first_weekday + day_number - first_day) / 7 + 1);
            days_to_today[i] = static_cast<double>(today_number - day_number);
        } catch (...) {}
    }
    env["DATE"] = std::move(dates); env["YEAR"] = std::move(years);
    env["MONTH"] = std::move(months); env["DAY"] = std::move(month_days);
    env["WEEKDAY"] = std::move(weekdays); env["WEEKOFYEAR"] = std::move(weeks_of_year);
    env["__IVOLAT_BAR_DAY"] = std::move(bar_days);
    env["DAYSTOTODAY"] = std::move(days_to_today);
    env["MACHINEDATE"] = constant(native_float(
        local.tm_year * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday),
        bars_.size());
    env["MACHINETIME"] = constant(native_float(
        local.tm_hour * 10000 + local.tm_min * 100 + local.tm_sec),
        bars_.size());
    env["MACHINEWEEK"] = constant(native_float(local.tm_wday), bars_.size());
}

void FormulaEnvironmentBuilder::bind_intraday_series(Environment& env) const {
    Series times(bars_.size(), 0.0), times2(bars_.size(), 0.0),
           from_open(bars_.size(), missing), hours(bars_.size(), 0.0),
           minutes(bars_.size(), 0.0);
    const bool default_a_share_sessions =
        uses_default_a_share_sessions(kline_document_);
    const auto sessions = explicit_trading_sessions(kline_document_);
    for (std::size_t i = 0; i < bars_.size(); ++i) {
        if (bars_[i].time.size() < 5) continue;
        try {
            const int hour = std::stoi(bars_[i].time.substr(0, 2));
            const int minute = std::stoi(bars_[i].time.substr(3, 2));
            int second = 0;
            if (bars_[i].time.size() >= 8)
                second = std::stoi(bars_[i].time.substr(6, 2));
            hours[i] = hour; minutes[i] = minute;
            times[i] = hour * 100 + minute;
            times2[i] = hour * 10000 + minute * 100 + second;
            const int minute_of_day = hour * 60 + minute;
            from_open[i] = sessions
                               ? explicit_sessions_from_open(minute_of_day, *sessions)
                               : default_a_share_sessions
                                     ? default_a_share_from_open(minute_of_day)
                                     : missing;
        } catch (...) {}
    }
    env["TIME"] = std::move(times); env["TIME2"] = std::move(times2);
    env["HOUR"] = std::move(hours); env["MINUTE"] = std::move(minutes);
    env["FROMOPEN"] = std::move(from_open);
}

void FormulaEnvironmentBuilder::bind_session_constants(Environment& env) const {
    const auto explicit_mindiff = explicit_formula_scalar(context_, "MINDIFF");
    if (!explicit_mindiff || *explicit_mindiff <= 0.0)
        env["MINDIFF"] = constant(
            formula_minimum_price_increment(kline_document_), bars_.size());
    env["AUTOFILTER"] = constant(0.0, bars_.size());
    // TCalc obtains HQCRBK from host type 122 byte +57.  It only selects
    // drawing colours in the recovered library; use the desktop dark-theme
    // value for deterministic headless execution.
    env["HQCRBK"] = constant(0.0, bars_.size());
    int period_code = 5;
    if (const auto* period = optional(kline_document_, "period");
        period && period->is_string()) {
        const auto value = lower_ascii(period->as_string());
        if (value == "week") period_code = 6; else if (value == "month") period_code = 7;
        else if (value == "1m" || value == "time") period_code = 0; else if (value == "5m") period_code = 1;
        else if (value == "15m") period_code = 2; else if (value == "30m") period_code = 3; else if (value == "60m") period_code = 4;
    }
    env["PERIOD"] = constant(period_code, bars_.size());
    int set_code = 0;
    if (const auto* market = optional(kline_document_, "market");
        market && market->is_string()) {
        const int parsed = formula_context_detail::formula_market_id(
            market->as_string());
        if (parsed >= 0) set_code = parsed;
    }
    env["SETCODE"] = constant(set_code, bars_.size());
    // TCalc TOTALFZNUM requests host type 105 and sums four (end-start)
    // session pairs.  TdxW's set-code 0/1/2 defaults are all
    // 570-690, 780-900, 900-900, 900-900: 240 trading minutes.
    const auto explicit_total = explicit_formula_scalar(context_, "TOTALFZNUM");
    if (!explicit_total || *explicit_total < 0.0) {
        const auto sessions = explicit_trading_sessions(kline_document_);
        env["TOTALFZNUM"] = constant(
            sessions
                ? explicit_sessions_total(*sessions)
                : uses_default_a_share_sessions(kline_document_) ? 240.0 : missing,
            bars_.size());
    }
}

void FormulaEnvironmentBuilder::bind_derived_series(Environment& env) const {
    Series true_range(bars_.size(), missing);
    if (!bars_.empty()) true_range[0] = bars_[0].high - bars_[0].low;
    for (std::size_t i = 1; i < bars_.size(); ++i)
        true_range[i] = std::max({bars_[i].high - bars_[i].low,
                                  std::abs(bars_[i].high - bars_[i - 1].close),
                                  std::abs(bars_[i].low - bars_[i - 1].close)});
    env["TR"] = std::move(true_range);
    Series mtm(bars_.size(), missing);
    int mtm_period = 12;
    // Match bind_parameters' ASCII case-insensitive canonicalization, including
    // its deterministic last-entry-wins behavior for case-only duplicates.
    for (const auto& [name, value] : parameters_)
        if (upper_ascii(name) == "N")
            mtm_period = std::max(1, static_cast<int>(native_float(value)));
    for (std::size_t i = static_cast<std::size_t>(mtm_period); i < bars_.size(); ++i)
        mtm[i] = bars_[i].close - bars_[i - mtm_period].close;
    env["MTM"] = std::move(mtm);
}

FormulaRandomSeed FormulaEnvironmentBuilder::resolve_random_seed(
    Environment& env, std::time_t now) const {
    FormulaRandomSeed result;
    result.uses_random = program_.functions.count("RAND") != 0;
    if (!result.uses_random) return result;
    result.seed = static_cast<std::uint32_t>(now);
    result.mode = "time64-seconds";
    if (context_ && context_->is_object()) {
        if (const auto* supplied_seed = optional(*context_, "formula_random_seed")) {
            if (!supplied_seed->is_number() ||
                !std::isfinite(supplied_seed->as_number()) ||
                std::trunc(supplied_seed->as_number()) != supplied_seed->as_number() ||
                supplied_seed->as_number() < 0.0 ||
                supplied_seed->as_number() > 4294967295.0)
                throw Error("formula_random_seed must be an integer in 0..4294967295");
            result.seed = static_cast<std::uint32_t>(supplied_seed->as_number());
            result.mode = "explicit-context";
        }
    }
    env["__RAND_SEED"] = constant(static_cast<double>(result.seed), bars_.size());
    return result;
}

}  // namespace tdx::formula_runtime_detail
