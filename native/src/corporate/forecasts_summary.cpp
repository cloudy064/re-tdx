#include "forecasts_internal.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>

namespace tdx::forecast_detail {

Json build_core_summary(const Json& document) {
    double company_count = 0.0;
    double forecast_count = 0.0;
    double favorable_count = 0.0;
    double adverse_count = 0.0;
    bool has_market_summary = false;
    for (const auto& row : document.at("industries").as_array()) {
        if (row.at("is_market_summary").as_bool()) {
            company_count = number_value(row, "company_count").value_or(0.0);
            forecast_count = number_value(row, "forecast_count").value_or(0.0);
            favorable_count = number_value(row, "favorable_count").value_or(0.0);
            adverse_count = number_value(row, "adverse_count").value_or(0.0);
            has_market_summary = true;
            continue;
        }
        if (has_market_summary) continue;
        company_count += number_value(row, "company_count").value_or(0.0);
        forecast_count += number_value(row, "forecast_count").value_or(0.0);
        favorable_count += number_value(row, "favorable_count").value_or(0.0);
        adverse_count += number_value(row, "adverse_count").value_or(0.0);
    }

    std::set<std::string> hk_securities;
    std::uint64_t hk_positive = 0;
    std::uint64_t hk_negative = 0;
    std::uint64_t hk_uncertain = 0;
    for (const auto& row : document.at("hong_kong").as_array()) {
        hk_securities.insert(text_value(row.at("security"), "security_id"));
        const auto mood = text_value(row, "sentiment");
        if (mood == "positive") ++hk_positive;
        else if (mood == "negative") ++hk_negative;
        else ++hk_uncertain;
    }

    Json summary = Json::object();
    summary["industries"] = static_cast<std::uint64_t>(std::count_if(
        document.at("industries").as_array().begin(),
        document.at("industries").as_array().end(), [](const Json& row) {
            return !row.at("is_market_summary").as_bool();
        }));
    summary["market_summary_rows"] = static_cast<std::uint64_t>(
        document.at("industries").size()) -
        static_cast<std::uint64_t>(summary.at("industries").as_number());
    summary["companies"] = company_count;
    summary["forecast_companies"] = forecast_count;
    summary["coverage_pct"] = company_count > 0.0
        ? Json(forecast_count / company_count * 100.0) : Json(nullptr);
    summary["favorable"] = favorable_count;
    summary["adverse"] = adverse_count;
    summary["hong_kong_forecasts"] =
        static_cast<std::uint64_t>(document.at("hong_kong").size());
    summary["hong_kong_securities"] =
        static_cast<std::uint64_t>(hk_securities.size());
    summary["hong_kong_positive"] = hk_positive;
    summary["hong_kong_negative"] = hk_negative;
    summary["hong_kong_uncertain"] = hk_uncertain;

    std::map<std::string, std::uint64_t> latest_periods;
    std::set<std::string> latest_securities;
    std::uint64_t latest_positive = 0;
    std::uint64_t latest_negative = 0;
    std::uint64_t latest_uncertain = 0;
    std::string latest_min_date;
    std::string latest_max_date;
    for (const auto& row : document.at("latest").as_array()) {
        ++latest_periods[text_value(row, "report_period")];
        latest_securities.insert(text_value(row.at("security"), "security_id"));
        const auto mood = text_value(row, "sentiment");
        if (mood == "positive") ++latest_positive;
        else if (mood == "negative") ++latest_negative;
        else ++latest_uncertain;
        const auto date = text_value(row, "forecast_date");
        if (!date.empty()) {
            latest_min_date = latest_min_date.empty()
                ? date : std::min(latest_min_date, date);
            latest_max_date = std::max(latest_max_date, date);
        }
    }
    Json period_counts = Json::array();
    for (const auto& [period, count] : latest_periods) {
        Json item = Json::object();
        item["report_period"] = period;
        item["count"] = count;
        period_counts.push_back(std::move(item));
    }
    std::string current_period;
    for (const auto& row : document.at("industries").as_array()) {
        if (!row.at("is_market_summary").as_bool()) continue;
        current_period = text_value(row, "report_period");
        break;
    }
    const auto current_rows = current_period.empty() ? 0ULL :
        latest_periods[current_period];
    const auto expected = static_cast<std::uint64_t>(
        std::max(0.0, forecast_count));
    summary["latest_forecasts"] =
        static_cast<std::uint64_t>(document.at("latest").size());
    summary["latest_securities"] =
        static_cast<std::uint64_t>(latest_securities.size());
    summary["latest_positive"] = latest_positive;
    summary["latest_negative"] = latest_negative;
    summary["latest_uncertain"] = latest_uncertain;
    summary["latest_min_date"] = latest_min_date;
    summary["latest_max_date"] = latest_max_date;
    summary["latest_report_periods"] = std::move(period_counts);
    summary["current_report_period"] = current_period;
    summary["current_report_expected"] = expected;
    summary["current_report_rows"] = current_rows;
    summary["current_report_gap"] = expected >= current_rows
        ? expected - current_rows : 0;
    summary["future_report_rows"] =
        static_cast<std::uint64_t>(document.at("latest").size()) - current_rows;
    return summary;
}

}  // namespace tdx::forecast_detail
