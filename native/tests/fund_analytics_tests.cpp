#include "tdx/fund_analytics.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}
}

int main() {
    try {
        tdx::Json risk = tdx::Json::array();
        risk.push_back(tdx::Json::parse(
            "{\"fund_code\":\"000711\",\"fund_name\":\"嘉实医疗保健股票型证券投资基金\","
            "\"fund_market\":\"33\",\"netValue\":\"2.4160\",\"changeRate\":\"1.4274\","
            "\"style_details\":\"005001\",\"FundCopName\":\"嘉实基金\","
            "\"FundSize\":\"338174688.00\",\"FundEstTime\":\"20140813\","
            "\"ManagerName\":\"郝淼, 张三\",\"sumRate\":\"13.53\","
            "\"excessSumRate\":\"19.22\",\"yearRate\":\"181.60\","
            "\"sigma\":\"2.67\",\"sharpe\":\"0.10\",\"beta\":\"0.37\","
            "\"maxRetracement\":\"-15.32\",\"withdrawStart\":\"20260430\","
            "\"withdrawEnd\":\"20260609\",\"recoverdayNum\":\"-1\","
            "\"recoverDate\":\"-1\",\"withdrawDuration\":\"18\"}"));
        const auto normalized_risk = tdx::normalize_fund_analytics_rows(risk, "risk");
        const auto& fund = normalized_risk.as_array()[0];
        require(fund.at("fund").at("fund_id").as_string() == "FUND:000711",
                "fund identity failed");
        require(fund.at("style").at("name").as_string() == "普通股票型",
                "fund style failed");
        require(fund.at("managers").size() == 2, "fund manager split failed");
        require(std::abs(fund.at("returns").at("cumulative_pct").as_number() - 13.53) < 1e-9,
                "fund risk return failed");
        require(fund.at("maximum_drawdown").at("recovery_date").is_null() &&
                !fund.at("maximum_drawdown").at("recovered").as_bool(),
                "fund recovery sentinel failed");

        tdx::Json points = tdx::Json::array();
        points.push_back(tdx::Json::parse(
            "{\"date\":\"20260805\",\"nv_now\":\"2.4160\","
            "\"return_last\":\"1.4274\",\"bp_return_last\":\"1.4690\","
            "\"ei_last\":\"-0.0416\"}"));
        const auto history = tdx::normalize_fund_analytics_rows(points, "risk-history");
        require(history.as_array()[0].at("date").as_string() == "2026-08-05" &&
                std::abs(history.as_array()[0].at("excess_daily_return_pct").as_number() +
                         0.0416) < 1e-9,
                "fund risk history failed");

        tdx::Json months = tdx::Json::array();
        months.push_back(tdx::Json::parse(
            "{\"month\":\"202608\",\"return_start\":\"0.2264\","
            "\"return_last\":\"0.0414\",\"bp_return_start\":\"0.1795\","
            "\"bp_return_last\":\"0.0120\",\"ei_start\":\"4.6853\","
            "\"ei_last\":\"2.9332\"}"));
        const auto monthly = tdx::normalize_fund_analytics_rows(months, "monthly-history");
        require(std::abs(monthly.as_array()[0].at("fund_monthly_return_pct").as_number() -
                         4.14) < 1e-9 &&
                std::abs(monthly.as_array()[0].at("benchmark_monthly_return_pct").as_number() -
                         1.2) < 1e-9 &&
                std::abs(monthly.as_array()[0].at("excess_monthly_return_pct").as_number() -
                         2.9332) < 1e-9,
                "fund monthly mixed-unit normalization failed");

        tdx::Json positions = tdx::Json::array();
        positions.push_back(tdx::Json::parse(
            "{\"fund_code\":\"000326\",\"fund_name\":\"南方中小盘成长\","
            "\"fund_market\":\"33\",\"reportDate\":\"20251231\","
            "\"reportHoldPos\":\"94.0543\",\"estimateDate\":\"20260805\","
            "\"estimateHoldPos\":\"87.5364\"}"));
        const auto normalized_positions =
            tdx::normalize_fund_analytics_rows(positions, "position-estimates");
        require(std::abs(normalized_positions.as_array()[0]
            .at("estimated_change_from_report_pct_points").as_number() + 6.5179) < 1e-6,
            "fund position delta failed");

        tdx::Json reported = tdx::Json::array();
        reported.push_back(tdx::Json::parse(
            "{\"fund_code\":\"000326\",\"fund_name\":\"南方中小盘成长\"," 
            "\"fund_market\":\"33\",\"reportDate\":\"20250630\"," 
            "\"turnoverRate\":\"0.6070\",\"holdPosNum\":\"106\"," 
            "\"stockConcentration\":\"30.9239\",\"holdIndustryNum\":\"12\"," 
            "\"IndustryConcentration\":\"24.7546\",\"marketValue\":\"247034368\"}"));
        const auto normalized_reported =
            tdx::normalize_fund_analytics_rows(reported, "reported-holdings");
        const auto& reported_fund = normalized_reported.as_array()[0];
        require(reported_fund.at("report_date").as_string() == "2025-06-30" &&
                std::abs(reported_fund.at("turnover_pct").as_number() - 60.7) < 1e-9 &&
                reported_fund.at("reported_security_count").as_number() == 106 &&
                reported_fund.at("stock_market_value_yuan").as_number() == 247034368,
                "reported fund holdings failed");

        tdx::Json industries = tdx::Json::array();
        industries.push_back(tdx::Json::parse(
            "{\"InduatryName\":\"住宿和餐饮业\",\"InduatryValue\":\"5265897.50\"," 
            "\"IndustryRatio\":\"2.13\"}"));
        const auto normalized_industries = tdx::normalize_fund_analytics_rows(
            industries, "reported-holding-industries");
        require(normalized_industries.as_array()[0].at("industry").as_string() ==
                    "住宿和餐饮业" &&
                normalized_industries.as_array()[0]
                    .at("holding_market_value_yuan").as_number() == 5265897.5,
                "reported fund industries failed");

        tdx::Json securities = tdx::Json::array();
        securities.push_back(tdx::Json::parse(
            "{\"StockCode\":\"688772\",\"StockName\":\"珠海冠宇\"," 
            "\"NowPrice\":\"14.14\",\"ChangeRate\":\"-4.717\"," 
            "\"SharesNumber\":\"150516\",\"SharesValue\":\"2150873.75\"," 
            "\"SharesRatio\":\"0.78\"}"));
        const auto normalized_securities = tdx::normalize_fund_analytics_rows(
            securities, "reported-holding-securities");
        require(normalized_securities.as_array()[0].at("security_code").as_string() ==
                    "688772" &&
                normalized_securities.as_array()[0].at("holding_shares").as_number() ==
                    150516 &&
                normalized_securities.as_array()[0].at("share_of_fund_nav_pct")
                    .as_number() == 0.78,
                "reported fund securities failed");

        tdx::Json market = tdx::Json::array();
        market.push_back(tdx::Json::parse(
            "{\"estimateDate\":\"20250806\",\"StockEstPos\":\"84.92\","
            "\"MixedEstPos\":\"78.49\",\"MarketEstPos\":\"79.49\"}"));
        const auto market_history =
            tdx::normalize_fund_analytics_rows(market, "market-position-history");
        require(market_history.as_array()[0].at("date").as_string() == "2025-08-06" &&
                market_history.as_array()[0].at("combined_market_position_pct").as_number() ==
                    79.49,
                "market fund position failed");
        std::cout << "fund analytics tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
