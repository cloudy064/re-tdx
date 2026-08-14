#include "ratings_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <utility>

namespace tdx::ratings_detail {

Json hong_kong_summary(const Json& rows) {
    std::uint64_t reports = 0, target_latest = 0, target_average = 0;
    std::uint64_t positive = 0, neutral = 0, negative = 0, unknown = 0;
    std::string earliest, latest;
    for (const auto& row : rows.as_array()) {
        reports += static_cast<std::uint64_t>(std::max(
            0.0, number_value(row, "six_month_report_count").value_or(0.0)));
        if (number_value(row, "latest_target_price_hkd")) ++target_latest;
        if (number_value(row, "six_month_average_target_price_hkd")) ++target_average;
        const auto stance = text_value(row, "stance");
        if (stance == "positive") ++positive;
        else if (stance == "neutral") ++neutral;
        else if (stance == "negative") ++negative;
        else ++unknown;
        const auto date = text_value(row, "latest_report_date");
        if (!date.empty() && (earliest.empty() || date < earliest)) earliest = date;
        if (date > latest) latest = date;
    }
    Json result = Json::object();
    result["securities"] = static_cast<std::uint64_t>(rows.size());
    result["six_month_reports"] = reports;
    result["positive_latest_ratings"] = positive;
    result["neutral_latest_ratings"] = neutral;
    result["negative_latest_ratings"] = negative;
    result["unknown_latest_ratings"] = unknown;
    result["latest_target_prices"] = target_latest;
    result["average_target_prices"] = target_average;
    result["earliest_latest_report_date"] = earliest;
    result["latest_report_date"] = latest;
    return result;
}

Json industry_summary(const Json& rows) {
    std::uint64_t reports = 0, bullish = 0, bearish = 0;
    std::string earliest, latest;
    for (const auto& row : rows.as_array()) {
        reports += static_cast<std::uint64_t>(std::max(
            0.0, number_value(row, "six_month_report_count").value_or(0.0)));
        bullish += static_cast<std::uint64_t>(std::max(
            0.0, number_value(row, "six_month_bullish_count").value_or(0.0)));
        bearish += static_cast<std::uint64_t>(std::max(
            0.0, number_value(row, "six_month_bearish_count").value_or(0.0)));
        const auto date = text_value(row, "latest_report_date");
        if (!date.empty() && (earliest.empty() || date < earliest)) earliest = date;
        if (date > latest) latest = date;
    }
    Json result = Json::object();
    result["industries"] = static_cast<std::uint64_t>(rows.size());
    result["six_month_reports"] = reports;
    result["six_month_bullish_reports"] = bullish;
    result["six_month_bearish_reports"] = bearish;
    result["six_month_unclassified_reports"] = reports > bullish + bearish
        ? reports - bullish - bearish : 0;
    result["earliest_latest_report_date"] = earliest;
    result["latest_report_date"] = latest;
    return result;
}

Json united_states_summary(const Json& rows) {
    std::uint64_t institution_coverage = 0, targets = 0;
    std::uint64_t positive = 0, neutral = 0, negative = 0, unknown = 0;
    std::set<std::string> industries;
    std::string earliest, latest;
    for (const auto& row : rows.as_array()) {
        institution_coverage += static_cast<std::uint64_t>(std::max(
            0.0, number_value(row, "institution_count").value_or(0.0)));
        if (number_value(row, "latest_target_price_usd")) ++targets;
        const auto stance = text_value(row, "stance");
        if (stance == "positive") ++positive;
        else if (stance == "neutral") ++neutral;
        else if (stance == "negative") ++negative;
        else ++unknown;
        const auto industry = text_value(row, "industry");
        if (!industry.empty()) industries.insert(industry);
        const auto date = text_value(row, "latest_report_date");
        if (!date.empty() && (earliest.empty() || date < earliest)) earliest = date;
        if (date > latest) latest = date;
    }
    Json result = Json::object();
    result["securities"] = static_cast<std::uint64_t>(rows.size());
    result["industries"] = static_cast<std::uint64_t>(industries.size());
    result["institution_coverage_sum"] = institution_coverage;
    result["positive_latest_ratings"] = positive;
    result["neutral_latest_ratings"] = neutral;
    result["negative_latest_ratings"] = negative;
    result["unknown_latest_ratings"] = unknown;
    result["latest_target_prices"] = targets;
    result["earliest_latest_report_date"] = earliest;
    result["latest_report_date"] = latest;
    return result;
}

}  // namespace tdx::ratings_detail
