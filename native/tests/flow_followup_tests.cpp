#include "tdx/flow_followup.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json rows(std::initializer_list<tdx::Json> values) {
    tdx::Json result = tdx::Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

std::string date(int day) {
    std::ostringstream output;
    output << "202408" << std::setw(2) << std::setfill('0') << day;
    return output.str();
}

}  // namespace

int main() {
    try {
        const auto margin = tdx::normalize_flow_followup_rows(
            "margin", rows({
                tdx::Json::parse(
                    "{\"date\":\"20260805\",\"valuerzyq\":\"25962.61\","
                    "\"valuerqye\":\"239.29\",\"close\":\"4658.15\","
                    "\"rankvaluerzl\":\"5.04\",\"rankvaluerql\":\"0.05\","
                    "\"close1\":\"5154.12\",\"zdfavg1\":\"\","
                    "\"zdfavg3\":\"-\",\"zdfavg5\":\"--\",\"zdfavg10\":\"\"}"),
                tdx::Json::parse(
                    "{\"date\":\"20171115\",\"valuerzyq\":\"10325.55\","
                    "\"valuerqye\":\"51.77\",\"close\":\"4073.67\","
                    "\"rankvaluerzl\":\"4.22\",\"rankvaluerql\":\"0.02\","
                    "\"close1\":\"2446.4\",\"zdfavg1\":\"0.77\","
                    "\"zdfavg3\":\"1.72\",\"zdfavg5\":\"3.78\","
                    "\"zdfavg10\":\"-0.49\"}") }));
        require(margin.size() == 2 &&
                    margin.as_array()[0].at("date").as_string() == "2017-11-15" &&
                    std::abs(margin.as_array()[0].at("financing_rate_pct").as_number() -
                             4.22) < 1e-9 &&
                    margin.as_array()[1].at("forward_returns_pct").at("days_1").is_null(),
                "margin rows sort dates, preserve units and normalize immature returns to null");

        tdx::Json north_rows = tdx::Json::array();
        tdx::Json before = tdx::Json::object();
        before["date"] = date(1); before["valuejlr"] = "-53.22";
        before["valuejmr"] = "-67.75"; before["close"] = "3345.63";
        before["zdfavg1"] = "0.34"; before["zdfavg3"] = "-0.72";
        before["zdfavg5"] = "-0.55"; north_rows.push_back(before);
        for (int day = 2; day <= 21; ++day) {
            tdx::Json placeholder = tdx::Json::object();
            placeholder["date"] = date(day);
            placeholder["valuejlr"] = "1040.00";
            placeholder["valuejmr"] = "0.00";
            placeholder["close"] = 3300 + day;
            placeholder["zdfavg1"] = "0.1";
            placeholder["zdfavg3"] = "0.2";
            placeholder["zdfavg5"] = "0.3";
            north_rows.push_back(std::move(placeholder));
        }
        tdx::Json after = tdx::Json::object();
        after["date"] = date(22); after["valuejlr"] = "10";
        after["valuejmr"] = "8"; after["close"] = "3400";
        north_rows.push_back(after);
        const auto north = tdx::normalize_flow_followup_rows("northbound", north_rows);
        require(north.size() == 22 &&
                    !north.as_array()[1].at("signal_available").as_bool() &&
                    north.as_array()[1].at("net_inflow_100m_cny").is_null() &&
                    north.as_array()[1].at("reported_net_inflow_100m_cny").as_number() ==
                        1040.0 &&
                    north.as_array().back().at("signal_available").as_bool() &&
                    north.as_array().back().at("net_purchase_100m_cny").as_number() == 8,
                "long 1040/0 runs are retained as evidence but excluded as usable signals");

        tdx::Json short_run = tdx::Json::array();
        for (int day = 1; day <= 3; ++day) {
            tdx::Json item = tdx::Json::object();
            item["date"] = date(day); item["valuejlr"] = "1040";
            item["valuejmr"] = "0"; short_run.push_back(std::move(item));
        }
        const auto short_result =
            tdx::normalize_flow_followup_rows("northbound", short_run);
        require(short_result.as_array()[0].at("signal_available").as_bool(),
                "an isolated exact pair is not guessed to be a placeholder");

        const auto financing_buckets = tdx::normalize_flow_model_rows(
            "financing-model", rows({tdx::Json::parse(
                "{\"rankvalue\":\"4.8\",\"count\":\"237\","
                "\"zdfavg1\":\"0.05\",\"uppercentum1\":\"51.48\","
                "\"zdfavg3\":\"0.19\",\"uppercentum3\":\"52.77\","
                "\"zdfavg5\":\"0.30\",\"uppercentum5\":\"54.94\","
                "\"zdfavg10\":\"0.49\",\"uppercentum10\":\"54.39\","
                "\"dcts\":\"1\"}")}));
        require(financing_buckets.size() == 1 &&
                    financing_buckets.as_array()[0].at("bucket_label").as_string() ==
                        ">= 4.8" &&
                    financing_buckets.as_array()[0].at("is_current_bucket").as_bool() &&
                    std::abs(financing_buckets.as_array()[0]
                        .at("csi300_forward_performance").at("days_10")
                        .at("mean_return_pct").as_number() - 0.49) < 1e-9,
                "financing buckets retain client ranges and four forward horizons");

        const auto northbound_buckets = tdx::normalize_flow_model_rows(
            "northbound-purchase-model", rows({
                tdx::Json::parse(
                    "{\"rankvalue\":\"-0\",\"times\":\"303\","
                    "\"zdfavg1\":\"-0.03\",\"uppercentum1\":\"49.17\","
                    "\"zdfavg3\":\"-0.05\",\"uppercentum3\":\"50.50\","
                    "\"zdfavg5\":\"0.08\",\"uppercentum5\":\"54.79\","
                    "\"dcts\":\"0\"}"),
                tdx::Json::parse(
                    "{\"rankvalue\":\"+0\",\"times\":\"457\","
                    "\"zdfavg1\":\"0.01\",\"uppercentum1\":\"54.49\","
                    "\"zdfavg3\":\"0.11\",\"uppercentum3\":\"53.39\","
                    "\"zdfavg5\":\"0.21\",\"uppercentum5\":\"54.05\","
                    "\"dcts\":\"1\"}")}));
        require(northbound_buckets.size() == 2 &&
                    northbound_buckets.as_array()[0].at("bucket_label").as_string() ==
                        "-20 ~ 0" &&
                    northbound_buckets.as_array()[1].at("bucket_label").as_string() ==
                        "0 ~ 20" &&
                    northbound_buckets.as_array()[1].at("is_current_bucket").as_bool(),
                "signed zero northbound buckets remain distinct");

        const auto financing_summary = tdx::normalize_flow_model_summary(
            "financing-model", rows({tdx::Json::parse(
                "{\"zwts\":\"20260805融资余额25962.61亿，融资率5.04%，"
                "预测沪深300指数次日涨跌幅为0.05%，上涨概率为51.87%；"
                "未来3日涨跌幅为0.19%，上涨概率为53.14%；"
                "未来5日涨跌幅为0.28%，上涨概率为54.85%；"
                "未来10日涨跌幅为0.46%，上涨概率为53.88%。\"}")}));
        require(financing_summary.at("date").as_string() == "2026-08-05" &&
                    std::abs(financing_summary.at("balance_100m_cny").as_number() -
                             25962.61) < 1e-9 &&
                    std::abs(financing_summary.at("rate_pct").as_number() - 5.04) < 1e-9 &&
                    std::abs(financing_summary.at("csi300_forward_performance")
                        .at("days_10").at("positive_ratio_pct").as_number() -
                             53.88) < 1e-9,
                "financing summary is parsed into dates, values and forward statistics");

        const auto purchase_summary = tdx::normalize_flow_model_summary(
            "northbound-purchase-model", rows({tdx::Json::parse(
                "{\"zwts\":\"2026-08-05北上资金净买入0.00亿，"
                "预测沪深300指数次日涨跌幅为0.00%，上涨概率为0.00%；"
                "未来3日涨跌幅为0.00%，上涨概率为0.00%；"
                "未来5日涨跌幅为0.00%，上涨概率为0.00%。\"}")}));
        require(purchase_summary.at("date").as_string() == "2026-08-05" &&
                    purchase_summary.at("flow_100m_cny").as_number() == 0.0 &&
                    purchase_summary.at("signal_kind").as_string() ==
                        "northbound-net-purchase",
                "northbound model summaries preserve zero values for paired auditing");

        bool duplicate_rejected = false;
        try {
            (void)tdx::normalize_flow_followup_rows("margin", rows({
                tdx::Json::parse("{\"date\":\"20260805\"}"),
                tdx::Json::parse("{\"date\":\"20260805\"}")}));
        } catch (const tdx::Error&) { duplicate_rejected = true; }
        require(duplicate_rejected, "duplicate dates are rejected");

        std::cout << "flow follow-up tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
