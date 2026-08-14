#include "fund_analytics_internal.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace tdx::fund_analytics_detail {

std::optional<Json> normalize_risk_row(const Json& row) {
    if (text(row, "fund_code").empty()) return std::nullopt;
    Json item = common_fund_record(row);
    Json returns = Json::object();
    returns["cumulative_pct"] = number_json(number(row, "sumRate"));
    returns["cumulative_excess_pct"] = number_json(
        number(row, "excessSumRate"));
    returns["annualized_pct"] = number_json(number(row, "yearRate"));
    returns["annualized_excess_pct"] = number_json(
        number(row, "excessRateYear"));
    item["returns"] = std::move(returns);
    Json risk = Json::object();
    risk["volatility_pct"] = number_json(number(row, "sigma"));
    risk["downside_risk_pct"] = number_json(number(row, "downsideRisk"));
    risk["calmar_ratio"] = number_json(number(row, "kama"));
    risk["skewness"] = number_json(number(row, "skewness"));
    item["risk"] = std::move(risk);
    Json adjusted = Json::object();
    adjusted["sharpe_ratio"] = number_json(number(row, "sharpe"));
    adjusted["sortino_ratio"] = number_json(number(row, "sortio"));
    adjusted["information_ratio"] = number_json(number(row, "infoRatio"));
    adjusted["tracking_error_pct"] = number_json(number(row, "trackingError"));
    adjusted["omega_ratio"] = number_json(number(row, "omega"));
    adjusted["jensen_alpha_pct"] = number_json(number(row, "jensenIndex"));
    adjusted["up_capture_ratio"] = number_json(number(row, "upCapture"));
    adjusted["down_capture_ratio"] = number_json(number(row, "downCapture"));
    adjusted["volatility_to_benchmark_ratio"] = number_json(
        number(row, "stdRatio"));
    item["risk_adjusted"] = std::move(adjusted);
    Json relationship = Json::object();
    relationship["beta"] = number_json(number(row, "beta"));
    relationship["correlation"] = number_json(number(row, "corr"));
    relationship["r_squared"] = number_json(number(row, "rSquared"));
    item["benchmark_relationship"] = std::move(relationship);
    Json drawdown = Json::object();
    drawdown["maximum_pct"] = number_json(number(row, "maxRetracement"));
    drawdown["start_date"] = optional_date(text(row, "withdrawStart"));
    drawdown["end_date"] = optional_date(text(row, "withdrawEnd"));
    drawdown["duration_days"] = number_json(number(row, "withdrawDuration"));
    drawdown["recovery_date"] = optional_date(text(row, "recoverDate"));
    const auto recovery_days = number(row, "recoverdayNum");
    drawdown["recovery_days"] = recovery_days && *recovery_days >= 0
        ? Json(*recovery_days) : Json(nullptr);
    drawdown["recovered"] = !text(row, "recoverDate").empty() &&
                             text(row, "recoverDate") != "-1";
    item["maximum_drawdown"] = std::move(drawdown);
    return item;
}

std::optional<Json> normalize_risk_history_row(const Json& row) {
    if (text(row, "date").empty()) return std::nullopt;
    Json item = Json::object();
    item["date"] = display_date(text(row, "date"));
    item["fund_nav"] = number_json(number(row, "nv_now"));
    item["fund_previous_nav"] = number_json(number(row, "nv_last"));
    item["fund_start_nav"] = number_json(number(row, "nv_start"));
    item["fund_daily_return_pct"] = number_json(number(row, "return_last"));
    item["fund_cumulative_return_pct"] = number_json(number(row, "return_start"));
    item["benchmark_value"] = number_json(number(row, "bp_now"));
    item["benchmark_previous_value"] = number_json(number(row, "bp_last"));
    item["benchmark_start_value"] = number_json(number(row, "bp_start"));
    item["benchmark_daily_return_pct"] = number_json(
        number(row, "bp_return_last"));
    item["benchmark_cumulative_return_pct"] = number_json(
        number(row, "bp_return_start"));
    item["excess_daily_return_pct"] = number_json(number(row, "ei_last"));
    item["excess_cumulative_return_pct"] = number_json(
        number(row, "ei_start"));
    return item;
}

std::optional<Json> normalize_monthly_risk_row(const Json& row) {
    if (text(row, "fund_code").empty()) return std::nullopt;
    Json item = common_fund_record(row);
    item["average_monthly_return_pct"] = number_json(number(row, "m_return_avg"));
    item["maximum_monthly_return_pct"] = number_json(number(row, "return_max"));
    item["minimum_monthly_return_pct"] = number_json(number(row, "return_min"));
    item["average_monthly_loss_pct"] = number_json(number(row, "loss_avg"));
    item["average_bull_market_return_pct"] = number_json(
        number(row, "return_avg_bull"));
    item["average_bear_market_return_pct"] = number_json(
        number(row, "return_avg_bear"));
    item["monthly_win_rate_ratio"] = number_json(number(row, "pro_rate"));
    item["monthly_win_rate_pct"] = scaled_number_json(row, "pro_rate", 100.0);
    item["monthly_volatility_pct"] = number_json(number(row, "std_month"));
    return item;
}

std::optional<Json> normalize_monthly_history_row(const Json& row) {
    if (text(row, "month").empty()) return std::nullopt;
    Json item = Json::object();
    const auto month = text(row, "month");
    item["month"] = month.size() == 6
        ? month.substr(0, 4) + "-" + month.substr(4, 2) : month;
    item["fund_nav"] = number_json(number(row, "nv_now"));
    item["fund_previous_nav"] = number_json(number(row, "nv_last"));
    item["fund_start_nav"] = number_json(number(row, "nv_start"));
    item["fund_monthly_return_pct"] = scaled_number_json(
        row, "return_last", 100.0);
    item["fund_cumulative_return_pct"] = scaled_number_json(
        row, "return_start", 100.0);
    item["benchmark_value"] = number_json(number(row, "bp_now"));
    item["benchmark_previous_value"] = number_json(number(row, "bp_last"));
    item["benchmark_start_value"] = number_json(number(row, "bp_start"));
    item["benchmark_monthly_return_pct"] = scaled_number_json(
        row, "bp_return_last", 100.0);
    item["benchmark_cumulative_return_pct"] = scaled_number_json(
        row, "bp_return_start", 100.0);
    item["excess_monthly_return_pct"] = number_json(number(row, "ei_last"));
    item["excess_cumulative_return_pct"] = number_json(number(row, "ei_start"));
    return item;
}

std::optional<Json> normalize_selection_skill_row(const Json& row) {
    if (text(row, "fund_code").empty()) return std::nullopt;
    Json item = common_fund_record(row);
    struct ModelDefinition {
        std::string_view key;
        std::string_view suffix;
    };
    static constexpr std::array<ModelDefinition, 3> models{{
        {"chang-lewellen", "CL"},
        {"treynor-mazuy", "TM"},
        {"henriksson-merton", "HM"},
    }};
    Json values = Json::object();
    for (const auto& definition : models) {
        const auto suffix = std::string(definition.suffix);
        Json model = Json::object();
        model["timing_ability"] = number_json(number(row, "timing_" + suffix));
        model["selection_ability_pct"] = number_json(number(row, "select_" + suffix));
        model["alpha"] = number_json(number(row, "alpha_" + suffix));
        model["beta1"] = number_json(number(row, "beta1_" + suffix));
        model["beta2"] = number_json(number(row, "beta2_" + suffix));
        values[std::string(definition.key)] = std::move(model);
    }
    item["models"] = std::move(values);
    return item;
}

}  // namespace tdx::fund_analytics_detail
