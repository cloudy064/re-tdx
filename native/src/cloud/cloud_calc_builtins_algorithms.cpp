#include "cloud_calc_builtins_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <utility>

namespace tdx::cloud_calc_detail {
namespace {

bool parse_double(std::string_view source, double& value) {
    const auto text = trim(std::string(source));
    if (text.empty()) return false;
    char* end = nullptr;
    value = std::strtod(text.c_str(), &end);
    return end == text.c_str() + text.size() && std::isfinite(value);
}

int parse_int(std::string_view source, int fallback = 0) {
    double value = 0.0;
    if (!parse_double(source, value)) return fallback;
    if (value < static_cast<double>(std::numeric_limits<int>::min()) ||
        value > static_cast<double>(std::numeric_limits<int>::max()))
        return fallback;
    return static_cast<int>(value);
}

}  // namespace

struct Date {
    int year{};
    unsigned month{};
    unsigned day{};
};

bool leap(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

unsigned month_days(int year, unsigned month) {
    static constexpr unsigned days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 0;
    return days[month - 1] + (month == 2 && leap(year) ? 1U : 0U);
}

Date date_from_int(int value) {
    Date date{value / 10000, static_cast<unsigned>((value / 100) % 100),
              static_cast<unsigned>(value % 100)};
    if (date.year < 1900 || date.year > 9999 || date.month < 1 || date.month > 12 ||
        date.day < 1 || date.day > month_days(date.year, date.month))
        throw Error("invalid YYYYMMDD date: " + std::to_string(value));
    return date;
}

std::int64_t civil_days(Date date) {
    int year = date.year - (date.month <= 2);
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned adjusted_month = date.month > 2 ? date.month - 3 : date.month + 9;
    const unsigned doy = (153 * adjusted_month + 2) / 5 + date.day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<std::int64_t>(era) * 146097 + static_cast<std::int64_t>(doe);
}

int day_difference(int left, int right) {
    const auto difference = civil_days(date_from_int(left)) - civil_days(date_from_int(right));
    return static_cast<int>(difference < 0 ? -difference : difference);
}

Date anniversary(Date date, int year) {
    date.year = year;
    date.day = std::min(date.day, month_days(date.year, date.month));
    return date;
}

Date date_from_civil_days(std::int64_t value) {
    const std::int64_t era = (value >= 0 ? value : value - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(value - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int year = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned day = doy - (153 * mp + 2) / 5 + 1;
    const unsigned month = mp < 10 ? mp + 3 : mp - 9;
    year += month <= 2;
    return Date{year, month, day};
}

int date_value(Date date) {
    return date.year * 10000 + static_cast<int>(date.month) * 100 + static_cast<int>(date.day);
}

int add_days(int value, int offset) {
    return date_value(date_from_civil_days(civil_days(date_from_int(value)) + offset));
}

int day_of_week(int value) {
    // Hinnant's epoch conversion: 1970-01-01 was Thursday (4 when Sunday=0).
    auto weekday = (civil_days(date_from_int(value)) - 719468 + 4) % 7;
    if (weekday < 0) weekday += 7;
    return static_cast<int>(weekday);
}

YearDayDifference year_day_difference(int left_value, int right_value) {
    auto left = date_from_int(left_value);
    auto right = date_from_int(right_value);
    if (civil_days(left) > civil_days(right)) std::swap(left, right);
    int years = right.year - left.year;
    const unsigned left_month_day = left.month * 100 + left.day;
    const unsigned right_month_day = right.month * 100 + right.day;
    if (right_month_day < left_month_day) --years;
    auto mark = anniversary(left, left.year + years);
    return YearDayDifference{years, static_cast<int>(civil_days(right) - civil_days(mark))};
}

double remain_time(int begin_value, int end_value, double denominator) {
    const auto begin = date_from_int(begin_value);
    const auto end = date_from_int(end_value);
    const auto begin_days = civil_days(begin);
    const auto end_days = civil_days(end);
    if (begin_days >= end_days) return 0.0;
    if (!(denominator > 0.0)) throw Error("remain-time denominator must be positive");
    const auto difference = year_day_difference(begin_value, end_value);
    return static_cast<double>(difference.years) +
           static_cast<double>(difference.days) / denominator;
}

bool interval_contains_feb29(Date begin, Date end) {
    if (civil_days(begin) > civil_days(end)) std::swap(begin, end);
    for (int year = begin.year; year <= end.year; ++year) {
        if (!leap(year)) continue;
        const Date feb29{year, 2, 29};
        const auto day = civil_days(feb29);
        if (day >= civil_days(begin) && day <= civil_days(end)) return true;
    }
    return false;
}

double ai_time(int begin_value, int end_value, double denominator) {
    const auto begin = date_from_int(begin_value);
    const auto end = date_from_int(end_value);
    if (denominator == 0.0) denominator = interval_contains_feb29(begin, end) ? 366.0 : 365.0;
    if (!(denominator > 0.0)) throw Error("accrual-time denominator must be positive");
    const auto difference = year_day_difference(begin_value, end_value);
    return static_cast<double>(difference.years) +
           static_cast<double>(difference.days) / denominator;
}

double remain_time_auto(int begin_value, int end_value) {
    if (civil_days(date_from_int(begin_value)) >= civil_days(date_from_int(end_value))) return 0.0;
    const auto difference = year_day_difference(begin_value, end_value);
    const auto end = date_from_int(end_value);
    int denominator = 365;
    const int month_day = static_cast<int>(end.month) * 100 + static_cast<int>(end.day);
    if (month_day > 229) {
        const int tested_year = difference.years <= 0 ? end.year : end.year - difference.years;
        if (leap(tested_year)) denominator = 366;
    }
    return static_cast<double>(difference.years) +
           static_cast<double>(difference.days) / static_cast<double>(denominator);
}

double json_number(const Json& value, std::string_view label) {
    if (value.is_number()) return value.as_number();
    if (value.is_bool()) return value.as_bool() ? 1.0 : 0.0;
    if (value.is_string()) {
        double result = 0.0;
        if (parse_double(value.as_string(), result)) return result;
    }
    throw Error(std::string(label) + " is not numeric");
}

int json_integer(const Json& value, std::string_view label) {
    const double number = json_number(value, label);
    if (number < static_cast<double>(std::numeric_limits<int>::min()) ||
        number > static_cast<double>(std::numeric_limits<int>::max()))
        throw Error(std::string(label) + " is outside integer range");
    return static_cast<int>(number);
}

int json_date(const Json& value, std::string_view label) {
    if (value.is_number()) return json_integer(value, label);
    if (!value.is_string()) throw Error(std::string(label) + " is not a date");
    std::string digits;
    for (const char ch : value.as_string())
        if (std::isdigit(static_cast<unsigned char>(ch))) digits.push_back(ch);
    if (digits.size() != 8) throw Error(std::string(label) + " must be YYYYMMDD");
    return parse_int(digits);
}

std::vector<double> json_rates(const Json& value, std::string_view label) {
    std::vector<double> result;
    if (value.is_array()) {
        for (const auto& item : value.as_array()) result.push_back(json_number(item, label));
    } else if (value.is_string()) {
        for (auto item : split(value.as_string(), ',')) {
            item = trim(std::move(item));
            if (item.empty()) continue;
            double number = 0.0;
            if (!parse_double(item, number))
                throw Error(std::string(label) + " contains a non-numeric cash-flow rate");
            result.push_back(number);
        }
    } else if (value.is_number()) {
        result.push_back(value.as_number());
    } else {
        throw Error(std::string(label) + " is not a rate vector");
    }
    if (result.empty()) throw Error(std::string(label) + " is an empty rate vector");
    return result;
}

double coupon_pv(double face, const std::vector<double>& rates, int frequency,
                 double yield, double next_time) {
    if (frequency <= 0) return 0.0;
    const double base = 1.0 + yield / static_cast<double>(frequency);
    if (!(base > 0.0)) throw Error("yield produces a non-positive discount base");
    double value = 0.0;
    for (std::size_t index = 0; index < rates.size(); ++index) {
        const double exponent = static_cast<double>(index) + next_time;
        value += face * rates[index] / static_cast<double>(frequency) /
                 std::pow(base, exponent);
    }
    value += face / std::pow(base, static_cast<double>(rates.size() - 1) + next_time);
    return value;
}

double coupon_ytm(double face, const std::vector<double>& rates, int frequency,
                  double price, double next_time) {
    if (!(price > 0.0)) throw Error("bond price must be positive");
    if (frequency == 0) {
        double cash = face;
        for (const auto rate : rates) cash += rate * face;
        return next_time <= 0.0 ? cash / price - 1.0
                                : std::pow(cash / price, 1.0 / next_time) - 1.0;
    }
    if (frequency < 0) return 0.0;
    if (rates.size() == 1) {
        if (!(next_time > 0.0)) throw Error("single-payment remaining time must be positive");
        return (face + face * rates.front() / static_cast<double>(frequency) - price) /
               price / next_time;
    }
    auto residual = [&](double yield) {
        return coupon_pv(face, rates, frequency, yield, next_time) - price;
    };
    double yield = 0.05;
    for (int iteration = 0; iteration < 80; ++iteration) {
        const double current = residual(yield);
        if (std::fabs(current) < 0.0001) return yield;
        constexpr double delta = 1e-7;
        const double derivative = (residual(yield + delta) - current) / delta;
        if (!std::isfinite(derivative) || std::fabs(derivative) < 1e-12) break;
        const double next = yield - current / derivative;
        if (!std::isfinite(next) || next <= -static_cast<double>(frequency) + 1e-9 || next > 100.0)
            break;
        yield = next;
    }
    double low = -static_cast<double>(frequency) + 1e-8;
    double high = 1.0;
    double low_value = residual(low);
    double high_value = residual(high);
    while (low_value * high_value > 0.0 && high < 128.0) {
        high *= 2.0;
        high_value = residual(high);
    }
    if (low_value * high_value > 0.0) throw Error("YTM solver did not bracket a finite root");
    for (int iteration = 0; iteration < 160; ++iteration) {
        const double middle = (low + high) / 2.0;
        const double value = residual(middle);
        if (std::fabs(value) < 0.0001) return middle;
        if (low_value * value <= 0.0) {
            high = middle;
        } else {
            low = middle;
            low_value = value;
        }
    }
    return (low + high) / 2.0;
}

double fixed_pv(double face, double rate, int frequency, double yield,
                double next_time, int count) {
    if (frequency == 0) return 0.0;
    const double base = 1.0 + yield / static_cast<double>(frequency);
    if (!(base > 0.0)) throw Error("yield produces a non-positive discount base");
    double value = 0.0;
    for (int index = 0; index < count; ++index)
        value += face * rate / static_cast<double>(frequency) /
                 std::pow(base, static_cast<double>(index) + next_time);
    return value + face / std::pow(base, static_cast<double>(count) + next_time - 1.0);
}

double fixed_ytm(double face, double rate, int frequency, double price,
                 double next_time, int count) {
    if (!(price > 0.0)) throw Error("bond price must be positive");
    if (count < 0) return 0.0;
    if (count == 1) {
        if (frequency == 0) return 0.0;
        const double ratio = (face + face * rate / static_cast<double>(frequency)) / price - 1.0;
        return next_time <= 0.0 ? ratio : ratio / next_time;
    }
    auto residual = [&](double yield) {
        return fixed_pv(face, rate, frequency, yield, next_time, count) - price;
    };
    double yield = std::pow(face / price, 1.0 / (static_cast<double>(count - 1) + next_time)) - 1.0;
    for (int iteration = 0; iteration < 100; ++iteration) {
        const double current = residual(yield);
        if (!std::isfinite(current) || std::fabs(current) < 0.0001) return yield;
        constexpr double delta = 1e-7;
        const double derivative = (residual(yield + delta) - current) / delta;
        if (!std::isfinite(derivative) || derivative == 0.0) break;
        const double next = yield - current / derivative;
        if (!std::isfinite(next)) break;
        yield = next;
    }
    return yield;
}

double pv_compound_interest(double face, double rate, double accrual_time,
                            double yield, double remaining_time) {
    const double cash = face + face * rate * accrual_time;
    return remaining_time < 1.0 ? cash / (1.0 + yield * remaining_time)
                                : cash / std::pow(1.0 + yield, remaining_time);
}

double ytm_compound_interest(double face, double rate, double accrual_time,
                             double price, double remaining_time) {
    if (!(price > 0.0) || remaining_time == 0.0) return 0.0;
    const double cash = face + face * rate * accrual_time;
    return remaining_time < 1.0 ? (cash - price) / price / remaining_time
                                : std::pow(cash / price, 1.0 / remaining_time) - 1.0;
}

double pv_all(const std::vector<Json>& args) {
    const int type = json_integer(args[0], "PV type");
    const double face = json_number(args[1], "PV face");
    const auto rates = json_rates(args[2], "PV rate vector");
    const int frequency = json_integer(args[3], "PV frequency");
    const double yield = json_number(args[4], "PV yield");
    const double next_time = json_number(args[5], "PV next-time");
    const double remaining = json_number(args[7], "PV remaining-years");
    const double current_rate = json_number(args[8], "PV current-rate");
    const double term = json_number(args[9], "PV term");
    if (type == 4) return pv_compound_interest(face, current_rate, term, yield, remaining);
    if (type == 5) {
        if (yield == 0.0 || remaining == 0.0) return 0.0;
        // Exact branch recovered from the 0x10001D80 wrapper. Its parameter
        // naming is unusual, so this intentionally follows the binary.
        return (face - yield) / yield / remaining;
    }
    if (rates.size() > 1) return coupon_pv(face, rates, frequency, yield, next_time);
    if (rates.size() == 1) {
        if (frequency <= 0) return 0.0;
        return (face + face * rates[0] / static_cast<double>(frequency)) /
               (1.0 + yield * remaining);
    }
    return 0.0;
}

double ytm_all(const std::vector<Json>& args) {
    const int type = json_integer(args[0], "YTM type");
    const double face = json_number(args[1], "YTM face");
    const auto rates = json_rates(args[2], "YTM rate vector");
    const int frequency = json_integer(args[3], "YTM frequency");
    const double price = json_number(args[4], "YTM price");
    const double next_time = json_number(args[5], "YTM next-time");
    const double remaining = json_number(args[7], "YTM remaining-years");
    const double current_rate = json_number(args[8], "YTM current-rate");
    const double term = json_number(args[9], "YTM term");
    if (!(price > 0.0)) throw Error("YTM price must be positive");
    if (type == 4) return ytm_compound_interest(face, current_rate, term, price, remaining);
    if (type == 5) return remaining == 0.0 ? 0.0 : (face - price) / price / remaining;
    if (rates.size() > 1) return coupon_ytm(face, rates, frequency, price, next_time);
    if (rates.size() == 1) {
        if (frequency <= 0 || remaining == 0.0) return 0.0;
        return (face + face * rates[0] / static_cast<double>(frequency) - price) /
               price / remaining;
    }
    return 0.0;
}

int local_yyyymmdd() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return (local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
}

void validate_date(int yyyymmdd) {
    (void)date_from_int(yyyymmdd);
}

std::int64_t date_serial(int yyyymmdd) {
    return civil_days(date_from_int(yyyymmdd));
}

}  // namespace tdx::cloud_calc_detail
