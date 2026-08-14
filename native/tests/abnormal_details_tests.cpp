#include "tdx/abnormal_details.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json one(std::string key, std::string value) {
    tdx::Json row = tdx::Json::object();
    row[std::move(key)] = std::move(value);
    tdx::Json rows = tdx::Json::array();
    rows.push_back(std::move(row));
    return rows;
}

}  // namespace

int main() {
    try {
        const auto summary = tdx::normalize_abnormal_summary_rows(one(
            "PercentInfo", "刷新时间 14:13   预测准确率 --   共77条"));
        require(summary.at("refresh_time").as_string() == "14:13:00" &&
                    summary.at("predicted_accuracy_pct").is_null() &&
                    summary.at("reported_count").as_number() == 77.0,
                "summary parses time, null accuracy and count");

        const auto numeric_summary = tdx::normalize_abnormal_summary_rows(one(
            "PercentInfo", "刷新时间 15:00   预测准确率 86.25%   共63条"));
        require(std::abs(numeric_summary.at("predicted_accuracy_pct").as_number() -
                         86.25) < 1e-9,
                "summary preserves numeric upstream accuracy");

        const auto explanation = tdx::normalize_abnormal_explanation_rows(one(
            "Detail",
            "深圳主板非ST、*ST和S证券连续三个交易日内收盘价格涨幅偏离值累计达到20%的证券:\n "
            "累计偏离值:20.70%  累计成交量(股):317235200 "
            "累计成交金额(万元):1813677.59 异常期间:20260804-20260806"));
        require(explanation.at("reason").as_string().find("连续三个交易日") !=
                    std::string::npos &&
                    std::abs(explanation.at("cumulative_deviation_pct").as_number() -
                             20.70) < 1e-9 &&
                    explanation.at("cumulative_volume_shares").as_number() ==
                        317235200.0 &&
                    std::abs(explanation.at("cumulative_amount_10k_cny").as_number() -
                             1813677.59) < 1e-9 &&
                    std::abs(explanation.at("cumulative_amount_yuan").as_number() -
                             18136775900.0) < 0.1,
                "explanation parses reason, deviation, volume and amount units");
        require(explanation.at("period").at("start_date").as_string() ==
                    "2026-08-04" &&
                    explanation.at("period").at("end_date").as_string() ==
                    "2026-08-06",
                "explanation parses anomaly period");

        const auto empty_summary =
            tdx::normalize_abnormal_summary_rows(tdx::Json::array());
        const auto empty_explanation =
            tdx::normalize_abnormal_explanation_rows(tdx::Json::array());
        require(empty_summary.is_null() && empty_explanation.is_null(),
                "empty upstream relations remain empty");
        require(tdx::normalize_abnormal_summary_rows(one("PercentInfo", "")).is_null() &&
                    tdx::normalize_abnormal_explanation_rows(one("Detail", "")).is_null(),
                "one-row empty upstream placeholders remain empty relations");

        std::cout << "abnormal details tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
