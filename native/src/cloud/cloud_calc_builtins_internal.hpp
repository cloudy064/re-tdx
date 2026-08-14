#pragma once

#include "tdx/json.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace tdx::cloud_calc_detail {

struct Builtin {
    int id;
    int argc;
    const char* name;
    std::uint32_t handler;
    bool implemented;
};

struct YearDayDifference {
    int years{};
    int days{};
};

const std::array<Builtin, 36>& builtin_registry();
const Builtin* find_builtin(std::string_view name);

Json builtin_catalog(
    const std::map<std::string, std::uint64_t, std::less<>>& usage);

Json execute_builtin(
    const Builtin& builtin,
    const std::vector<Json>& arguments,
    int as_of_yyyymmdd);

double json_number(const Json& value, std::string_view label);
int json_date(const Json& value, std::string_view label);
std::vector<double> json_rates(const Json& value, std::string_view label);
int local_yyyymmdd();
void validate_date(int yyyymmdd);
std::int64_t date_serial(int yyyymmdd);
int day_difference(int left, int right);
YearDayDifference year_day_difference(int left, int right);
int add_days(int value, int offset);
int day_of_week(int value);
double remain_time(int begin, int end, double denominator);
double remain_time_auto(int begin, int end);
double ai_time(int begin, int end, double denominator);
int json_integer(const Json& value, std::string_view label);
double coupon_pv(double face, const std::vector<double>& rates, int frequency,
                 double yield, double next_time);
double coupon_ytm(double face, const std::vector<double>& rates, int frequency,
                  double price, double next_time);
double fixed_pv(double face, double rate, int frequency, double yield,
                double next_time, int count);
double fixed_ytm(double face, double rate, int frequency, double price,
                 double next_time, int count);
double pv_compound_interest(double face, double rate, double accrual_time,
                            double yield, double remaining_time);
double ytm_compound_interest(double face, double rate, double accrual_time,
                             double price, double remaining_time);
double pv_all(const std::vector<Json>& args);
double ytm_all(const std::vector<Json>& args);

}  // namespace tdx::cloud_calc_detail
