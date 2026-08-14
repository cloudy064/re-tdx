#include "forecasts_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace tdx {

Json normalize_forecast_industry_rows(
    const Json& rows, const std::map<std::string, std::string>& industry_names) {
    using namespace forecast_detail;
    if (!rows.is_array()) throw Error("forecast industry rows must be an array");
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 13>
        category_fields{{
            {"big_increase", "GSJS1"}, {"big_decrease", "GSJS2"},
            {"increase", "GSJS3"}, {"decrease", "GSJS4"},
            {"profit", "GSJS5"}, {"loss", "GSJS6"}, {"flat", "GSJS7"},
            {"turnaround", "GSJS8"}, {"loss_reduction", "GSJS9"},
            {"uncertain", "GSJS10"}, {"cancelled", "GSJS11"},
            {"no_big_change", "GSJS12"}, {"big_loss_reduction", "GSJS13"},
        }};
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM1");
        const auto detail_id = text_value(row, "$ZQDM");
        if (!forecast_group_code(code) || !safe_identifier(detail_id)) continue;
        Json categories = Json::object();
        double forecast_count = 0.0;
        for (const auto& [name, field] : category_fields) {
            const auto count = number_value(row, field).value_or(0.0);
            categories[std::string(name)] = count;
            forecast_count += count;
        }
        const auto company_count = number_value(row, "HYSL").value_or(0.0);
        const auto favorable_count =
            number_value(row, "GSJS1").value_or(0.0) +
            number_value(row, "GSJS3").value_or(0.0) +
            number_value(row, "GSJS5").value_or(0.0) +
            number_value(row, "GSJS7").value_or(0.0) +
            number_value(row, "GSJS8").value_or(0.0) +
            number_value(row, "GSJS9").value_or(0.0) +
            number_value(row, "GSJS13").value_or(0.0);
        const auto adverse_count =
            number_value(row, "GSJS2").value_or(0.0) +
            number_value(row, "GSJS4").value_or(0.0) +
            number_value(row, "GSJS6").value_or(0.0);
        Json industry = Json::object();
        industry["code"] = code;
        const auto found = industry_names.find(code);
        industry["name"] = code == "880001" ? "沪深京 A 股" :
            found == industry_names.end() ? "" : found->second;
        Json item = Json::object();
        item["detail_id"] = detail_id;
        item["industry"] = std::move(industry);
        item["is_market_summary"] = code == "880001";
        item["report_period"] = text_value(row, "BGQ");
        item["previous_period_close"] = number_or_null(number_value(row, "BGQGJ"));
        item["company_count"] = company_count;
        item["forecast_count"] = forecast_count;
        item["coverage_pct"] = company_count > 0.0
            ? Json(forecast_count / company_count * 100.0) : Json(nullptr);
        item["favorable_count"] = favorable_count;
        item["adverse_count"] = adverse_count;
        item["favorable_pct"] = forecast_count > 0.0
            ? Json(favorable_count / forecast_count * 100.0) : Json(nullptr);
        const auto client_denominator = forecast_count +
            number_value(row, "GSJS12").value_or(0.0) +
            number_value(row, "GSJS13").value_or(0.0);
        item["client_favorable_pct"] = client_denominator > 0.0
            ? Json(favorable_count / client_denominator * 100.0) : Json(nullptr);
        item["category_counts"] = std::move(categories);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return forecast_detail::text_value(left.at("industry"), "code") <
                   forecast_detail::text_value(right.at("industry"), "code");
        });
    return result;
}

}  // namespace tdx
