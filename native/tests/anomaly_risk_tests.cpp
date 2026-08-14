#include "tdx/anomaly_risk.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
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

}  // namespace

int main() {
    try {
        const auto statistics = tdx::normalize_anomaly_statistics_rows(rows({
            tdx::Json::parse(
                "{\"N001\":\"300615\",\"N002\":\"0\",\"N003\":\"创业板\","
                "\"N004\":\"42.65\",\"N005\":\"20260805\",\"N006\":\"2\","
                "\"N007\":\"101.77\",\"N008\":\"20260803\",\"N009\":\"4\","
                "\"N010\":\"1\",\"N011\":\"101.77\",\"N012\":\"20260803\","
                "\"N013\":\"4\",\"N014\":\"399102\",\"N015\":\"0\","
                "\"N016\":\"创业板综\",\"N017\":\"72.77\",\"N018\":\"106.65\","
                "\"N019\":\"68.10\",\"N020\":\"5.95\",\"N021\":\"1.63\","
                "\"N022\":\"-17.54\",\"N023\":\"欣天科技\",\"N024\":\"18.02\","
                "\"N025\":\"19.97\",\"N026\":\"4039.06\",\"N027\":\"-0.85\","
                "\"N028\":\"20.82\",\"N029\":\"1.28\",\"N030\":\"5.60\","
                "\"N031\":\"5.60\",\"N032\":\"17.87\",\"N033\":\"18.95\","
                "\"N034\":\"26.56\",\"N035\":\"76.81\",\"N036\":\"0\","
                "\"N037\":\"18.02\","
                "\"N038\":\"严重异常波动参考最低触及价格17.87(18.95%)\","
                "\"N039\":\"已触及\"}")}));
        require(statistics.size() == 1, "statistics row count");
        const auto& statistic = statistics.as_array().front();
        require(statistic.at("security").at("name").as_string() == "欣天科技" &&
                    statistic.at("classification_index").at("name").as_string() ==
                        "创业板综" &&
                    statistic.at("windows").at("days_10")
                        .at("positive_anomaly_count").as_number() == 1.0,
                "statistics identities and windows");
        require(std::abs(statistic.at("live_snapshot")
                             .at("computed_deviation_pct").as_number() - 20.82) < 1e-9 &&
                    statistic.at("live_snapshot")
                        .at("deviation_consistent_with_rounded_quotes").as_bool(),
                "statistics live deviation consistency");
        require(statistic.at("warning").at("status").as_string() == "reached" &&
                    statistic.at("warning").at("kind").as_string() ==
                        "severe-abnormal-volatility" &&
                    std::abs(statistic.at("warning").at("reference_price").as_number() -
                             17.87) < 1e-9 &&
                    std::abs(statistic.at("warning").at("reference_change_pct").as_number() -
                             18.95) < 1e-9,
                "statistics warning text becomes structured evidence");
        require(std::abs(statistic.at("upstream_auxiliary_unresolved")
                             .at("N029").as_number() - 1.28) < 1e-9,
                "unresolved upstream values remain available without guessed labels");

        tdx::BlockData blocks;
        blocks.securities[{0, "300615"}] =
            tdx::Security{0, "SZ", "深圳", "300615", "欣天科技"};
        blocks.securities[{0, "399102"}] =
            tdx::Security{0, "SZ", "深圳", "399102", "创业板综"};
        const auto suspension = tdx::normalize_suspension_risk_rows(rows({
            tdx::Json::parse(
                "{\"N001\":\"300615\",\"N002\":\"0\",\"N003\":\"创业板\","
                "\"N004\":\"20260804\",\"N005\":\"0\",\"N006\":\"0\","
                "\"N007\":\"20260805\",\"N008\":\"20260805\","
                "\"N009\":\"20260806\",\"N010\":\"0.43\","
                "\"N011\":\"30.00\",\"N012\":\"16.43\",\"N013\":\"1\","
                "\"N014\":\"3\",\"N015\":\"399102\",\"N016\":\"0\"}")}),
            blocks);
        require(suspension.size() == 1, "suspension row count");
        const auto& risk = suspension.as_array().front();
        require(risk.at("security").at("name").as_string() == "欣天科技" &&
                    risk.at("classification_index").at("name").as_string() ==
                        "创业板综" &&
                    risk.at("warning").at("status").as_string() == "triggered",
                "suspension identities and warning catalog");
        require(risk.at("anomaly").at("suspension_date").is_null() &&
                    risk.at("anomaly").at("resumption_date").is_null() &&
                    risk.at("anomaly").at("latest_date").as_string() == "2026-08-04" &&
                    std::abs(risk.at("anomaly").at("reported_deviation_ratio").as_number() -
                             0.43) < 1e-9 &&
                    std::abs(risk.at("anomaly").at("deviation_pct").as_number() - 43.0) < 1e-9,
                "suspension dates and ratio-to-percent conversion");

        std::cout << "anomaly risk tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
