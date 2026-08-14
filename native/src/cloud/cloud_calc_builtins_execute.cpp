#include "cloud_calc_builtins_internal.hpp"

#include "tdx/common.hpp"

#include <cmath>

namespace tdx::cloud_calc_detail {

Json execute_builtin(const Builtin& builtin, const std::vector<Json>& args, int as_of) {
    if (!builtin.implemented) throw Error("builtin handler is catalog-only: " + std::string(builtin.name));
    switch (builtin.id) {
    case 100:
        return day_difference(json_date(args[0], "left date"), json_date(args[1], "right date"));
    case 101:
    case 103: {
        const auto difference = year_day_difference(json_date(args[0], "left date"),
                                                    json_date(args[1], "right date"));
        return difference.years;
    }
    case 102:
    case 104: {
        const auto difference = year_day_difference(json_date(args[0], "left date"),
                                                    json_date(args[1], "right date"));
        return difference.days;
    }
    case 105: {
        const auto difference = year_day_difference(json_date(args[0], "left date"),
                                                    json_date(args[1], "right date"));
        const double denominator = json_number(args[2], "year-time denominator");
        if (denominator == 0.0) return 0.0;
        return static_cast<double>(difference.years) + difference.days / denominator;
    }
    case 106: {
        const int base = json_date(args[0], "base date");
        const auto numerator = year_day_difference(base, json_date(args[1], "target date"));
        const auto denominator = year_day_difference(base, json_date(args[2], "period date"));
        if (denominator.days == 0) return static_cast<double>(numerator.years);
        return static_cast<double>(numerator.years) +
               static_cast<double>(numerator.days) / static_cast<double>(denominator.days);
    }
    case 107:
        return add_days(json_date(args[0], "date"), -json_integer(args[1], "day count"));
    case 108:
        return add_days(json_date(args[0], "date"), json_integer(args[1], "day count"));
    case 109:
        return day_of_week(json_date(args[0], "date"));
    case 110:
        return remain_time(json_date(args[0], "begin date"), json_date(args[1], "end date"),
                           json_number(args[2], "remain-time denominator"));
    case 111:
        return remain_time_auto(json_date(args[0], "begin date"),
                                json_date(args[1], "end date"));
    case 112: {
        const int type = json_integer(args[0], "accrual type");
        const int end = json_date(args[1], "accrual end date");
        const int begin = json_date(args[2], "accrual begin date");
        const double days = json_number(args[3], "accrual period days");
        return ai_time(begin, end, type == 4 ? 0.0 : days);
    }
    case 113:
        return ai_time(json_date(args[0], "accrual begin date"),
                       json_date(args[1], "accrual end date"),
                       json_number(args[2], "accrual denominator"));
    case 114: {
        const int value = json_date(args[0], "accrual date");
        const int begin = json_date(args[1], "period begin date");
        const int end = json_date(args[2], "period end date");
        const int denominator = day_difference(begin, end);
        return denominator <= 0 ? 0.0 :
            static_cast<double>(day_difference(value, begin)) / denominator;
    }
    case 115: {
        const int end = json_date(args[0], "next date");
        const int begin = json_date(args[1], "period begin date");
        const int value = json_date(args[2], "current date");
        const int denominator = day_difference(value, begin);
        return denominator <= 0 ? 0.0 :
            static_cast<double>(day_difference(value, end)) / denominator;
    }
    case 116: {
        const double time = json_number(args[0], "remaining time");
        const int frequency = json_integer(args[1], "payment frequency");
        if (time <= 0.0 || frequency <= 0) return 0;
        const double period = 1.0 / static_cast<double>(frequency);
        if (period >= time) return 1;
        const int complete = static_cast<int>(std::floor(time / period));
        return time - complete * period <= 0.0 ? complete : complete + 1;
    }
    case 117: {
        const double time = json_number(args[0], "remaining time");
        const int frequency = json_integer(args[1], "payment frequency");
        if (time <= 0.0 || frequency <= 0) return 0.0;
        const double period = 1.0 / static_cast<double>(frequency);
        return period < time ? time - std::floor(time / period) * period : time;
    }
    case 118: {
        const int type = json_integer(args[0], "AI type");
        const double face = json_number(args[1], "AI face");
        const double rate = json_number(args[2], "AI rate");
        const int frequency = json_integer(args[3], "AI frequency");
        const double time = json_number(args[4], "AI time");
        if (type == 4) return face * rate * time;
        if (type == 5) {
            const double issue_price = json_number(args[5], "AI issue price");
            const int start = json_date(args[6], "AI start date");
            const int current = json_date(args[7], "AI current date");
            const int end = json_date(args[8], "AI end date");
            const int total = day_difference(start, end);
            (void)face;
            // sub_10001B00 forwards the issue price to sub_10001A50; that
            // helper uses a fixed redemption value of 100, not the face
            // argument carried by the other AI branches.
            return total == 0 ? 0.0 : (100.0 - issue_price) *
                static_cast<double>(day_difference(start, current)) / static_cast<double>(total);
        }
        return frequency > 0 ? face * rate / static_cast<double>(frequency) * time : 0.0;
    }
    case 119: {
        const double face = json_number(args[0], "AI face");
        const double rate = json_number(args[1], "AI rate");
        const int frequency = json_integer(args[2], "AI frequency");
        const double time = json_number(args[3], "AI time");
        return frequency > 0 ? face * rate / static_cast<double>(frequency) * time : 0.0;
    }
    case 120:
        return json_number(args[0], "AI face") * json_number(args[1], "AI rate") *
               json_number(args[2], "AI time");
    case 121: {
        const double issue_price = json_number(args[0], "issue price");
        const int start = json_date(args[1], "issue date");
        const int current = json_date(args[2], "current date");
        const int end = json_date(args[3], "maturity date");
        const int total = day_difference(start, end);
        return total <= 0 ? 0.0 : (100.0 - issue_price) *
            static_cast<double>(day_difference(start, current)) / total;
    }
    case 122:
        return pv_all(args);
    case 123:
        return coupon_pv(json_number(args[0], "PV face"),
                         json_rates(args[1], "PV rate vector"),
                         json_integer(args[2], "PV frequency"),
                         json_number(args[3], "PV yield"),
                         json_number(args[4], "PV next-time"));
    case 124:
        return fixed_pv(json_number(args[0], "PV face"),
                        json_number(args[1], "PV fixed rate"),
                        json_integer(args[2], "PV frequency"),
                        json_number(args[3], "PV yield"),
                        json_number(args[4], "PV next-time"),
                        json_integer(args[5], "PV payment count"));
    case 125: {
        const double face = json_number(args[0], "PV face");
        const double rate = json_number(args[1], "PV rate");
        const int frequency = json_integer(args[2], "PV frequency");
        const double yield = json_number(args[3], "PV yield");
        const double term = json_number(args[4], "PV term");
        return frequency == 0 ? 0.0 :
            (face + rate * face / static_cast<double>(frequency)) / (yield * term + 1.0);
    }
    case 126:
        return pv_compound_interest(json_number(args[0], "PV face"),
                                    json_number(args[1], "PV rate"),
                                    json_number(args[2], "PV accrual-time"),
                                    json_number(args[3], "PV yield"),
                                    json_number(args[4], "PV remaining-time"));
    case 127:
        return json_number(args[0], "PV face") /
               (json_number(args[2], "PV term") * json_number(args[1], "PV yield") + 1.0);
    case 128:
        return json_number(args[0], "full price") - json_number(args[1], "accrued interest");
    case 129:
        return ytm_all(args);
    case 130:
        return coupon_ytm(json_number(args[0], "YTM face"),
                          json_rates(args[1], "YTM rate vector"),
                          json_integer(args[2], "YTM frequency"),
                          json_number(args[3], "YTM price"),
                          json_number(args[4], "YTM next-time"));
    case 131:
        return fixed_ytm(json_number(args[0], "YTM face"),
                         json_number(args[1], "YTM fixed rate"),
                         json_integer(args[2], "YTM frequency"),
                         json_number(args[3], "YTM price"),
                         json_number(args[4], "YTM next-time"),
                         json_integer(args[5], "YTM payment count"));
    case 132: {
        const double face = json_number(args[0], "YTM face");
        const double rate = json_number(args[1], "YTM rate");
        const int frequency = json_integer(args[2], "YTM frequency");
        const double price = json_number(args[3], "YTM price");
        const double time = json_number(args[4], "YTM time");
        if (frequency == 0 || price == 0.0 || time == 0.0) return 0.0;
        return (face + face * rate / static_cast<double>(frequency) - price) / price / time;
    }
    case 133:
        return ytm_compound_interest(json_number(args[0], "YTM face"),
                                     json_number(args[1], "YTM rate"),
                                     json_number(args[2], "YTM accrual-time"),
                                     json_number(args[3], "YTM price"),
                                     json_number(args[4], "YTM remaining-time"));
    case 134: {
        const double face = json_number(args[0], "YTM face");
        const double price = json_number(args[1], "YTM price");
        const double time = json_number(args[2], "YTM time");
        return price == 0.0 || time == 0.0 ? 0.0 : (face - price) / price / time;
    }
    case 1000:
        return as_of == 0 ? local_yyyymmdd() : as_of;
    default:
        throw Error("unreachable builtin dispatcher ID " + std::to_string(builtin.id));
    }
}

}  // namespace tdx::cloud_calc_detail

