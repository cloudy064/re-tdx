#include "tpool_internal.hpp"
#include "tdx/time.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>

namespace tdx::tpool_detail {

namespace {
struct PeriodDefinition { int id; std::string_view name; };
constexpr std::array<PeriodDefinition, 8> kPeriods{{
    {0, "5m"}, {1, "15m"}, {2, "30m"}, {3, "60m"},
    {4, "day"}, {5, "week"}, {6, "month"}, {7, "1m"},
}};
constexpr bool unique_periods() {
    for (std::size_t left = 0; left < kPeriods.size(); ++left)
        for (std::size_t right = left + 1; right < kPeriods.size(); ++right)
            if (kPeriods[left].id == kPeriods[right].id ||
                kPeriods[left].name == kPeriods[right].name) return false;
    return true;
}
static_assert(unique_periods(), "TPool periods must be unique");
}  // namespace

std::string period_name(int period) {
    for (const auto& definition : kPeriods)
        if (definition.id == period) return std::string(definition.name);
    return "";
}

const Json* optional(const Json& object, std::string_view key) {
    if (!object.is_object()) return nullptr;
    const auto found = object.as_object().find(key);
    return found == object.as_object().end() ? nullptr : &found->second;
}

bool json_bool(const Json& object, std::string_view key, bool fallback) {
    const auto* value = optional(object, key);
    return value && value->is_bool() ? value->as_bool() : fallback;
}

std::string local_time_text() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream output;
    output << local_timestamp_text(local);
    return output.str();
}

TpoolFlowClock local_flow_clock() {
    const auto now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    TpoolFlowClock result;
    result.epoch_seconds = static_cast<std::int64_t>(now);
    result.local_date = (local.tm_year + 1900) * 10000 + (local.tm_mon + 1) * 100 + local.tm_mday;
    result.local_hhmmss = local.tm_hour * 10000 + local.tm_min * 100 + local.tm_sec;
    result.local_weekday = local.tm_wday + 1;
    return result;
}

int hhmmss_seconds(int value) {
    if (value < 0 || value > 235959) return -1;
    const int hour = value / 10000;
    const int minute = value / 100 % 100;
    const int second = value % 100;
    if (hour > 23 || minute > 59 || second > 59) return -1;
    return hour * 3600 + minute * 60 + second;
}

std::int64_t json_integer_number(const Json& object, std::string_view key,
                                 std::int64_t fallback) {
    const auto* value = optional(object, key);
    if (!value || !value->is_number() || !std::isfinite(value->as_number())) return fallback;
    const auto number = value->as_number();
    if (number < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
        number > static_cast<double>(std::numeric_limits<std::int64_t>::max())) return fallback;
    return static_cast<std::int64_t>(number);
}

double json_number(const Json& object, std::string_view key, double fallback) {
    const auto* value = optional(object, key);
    return value && value->is_number() && std::isfinite(value->as_number())
               ? value->as_number() : fallback;
}

Json string_set_json(const std::set<std::string, std::less<>>& values) {
    Json result = Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

std::string json_text(const Json& object, std::string_view key) {
    const auto* value = optional(object, key);
    return value && value->is_string() ? value->as_string() : "";
}

int json_integer_text(const Json& object, std::string_view key, int fallback) {
    return integer_text(json_text(object, key), fallback);
}

double json_float_text(const Json& object, std::string_view key, double fallback) {
    return float_text(json_text(object, key), fallback);
}

}  // namespace tdx::tpool_detail

