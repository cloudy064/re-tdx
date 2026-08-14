#include "tdx/forecasts.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        std::map<std::string, std::string> industries{{"881477", "综合"}};
        auto industry_rows = tdx::Json::array();
        industry_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"88147720260630\",\"$ZQDM1\":\"881477\","
            "\"BGQ\":\"20260630\",\"BGQGJ\":\"1849.52\",\"HYSL\":\"18\","
            "\"GSJS1\":\"2\",\"GSJS2\":\"2\",\"GSJS3\":\"0\","
            "\"GSJS4\":\"0\",\"GSJS5\":\"0\",\"GSJS6\":\"2\","
            "\"GSJS7\":\"0\",\"GSJS8\":\"1\",\"GSJS9\":\"1\","
            "\"GSJS10\":\"0\",\"GSJS11\":\"0\",\"GSJS12\":\"0\","
            "\"GSJS13\":\"0\"}"));
        const auto normalized_industries =
            tdx::normalize_forecast_industry_rows(industry_rows, industries);
        require(normalized_industries.size() == 1 &&
                    normalized_industries.as_array()[0].at("forecast_count").as_number() == 8 &&
                    normalized_industries.as_array()[0].at("industry").at("name").as_string() ==
                        "综合",
                "forecast industry rows should preserve composite keys and counts");

        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000833"}] =
            tdx::Security{0, "SZ", "深圳", "000833", "粤桂股份"};
        auto security_rows = tdx::Json::array();
        security_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"000833\",\"$SC\":\"0\",\"ygdate\":\"20260715\","
            "\"bgq\":\"20260630\",\"type\":\"业绩大幅上升\","
            "\"jlr1\":\"620000000\",\"jlr2\":\"690000000\","
            "\"zj3\":\"1.6451\",\"zj4\":\"1.9437\","
            "\"jlr3\":\"234398994.10\",\"zgb\":\"80208.2221\","
            "\"contents\":\"预告正文\",\"result\":\"变动原因\"}"));
        const auto normalized_securities =
            tdx::normalize_forecast_security_rows(security_rows, securities);
        const auto& security = normalized_securities.as_array()[0];
        require(security.at("profit_lower_yuan").as_number() == 620000000 &&
                    std::abs(security.at("growth_lower_pct").as_number() - 164.51) < 0.0001 &&
                    security.at("total_capital_shares").as_number() == 802082221 &&
                    security.at("sentiment").as_string() == "positive",
                "A-share forecasts should normalize percentages and ten-thousand shares");

        auto latest_rows = tdx::Json::array();
        latest_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"002303\",\"$SC\":\"0\",\"ygdate\":\"20260807\"," 
            "\"bgq\":\"20260630\",\"type\":\"业绩预降\"," 
            "\"jlr1\":\"211323608.93\",\"jlr2\":\"228933909.67\"," 
            "\"zj1\":\"20.00\",\"zj2\":\"30.00\"," 
            "\"jlr3\":\"176103007.44\",\"eps\":\"0.1604\"," 
            "\"x\":\"422647217.86\",\"y\":\"457867819.34\"," 
            "\"zgb\":\"153132.3685\",\"contents\":\"预告正文\"," 
            "\"result\":\"成本变动\"}"));
        const auto latest = tdx::normalize_forecast_security_rows(
            latest_rows, securities);
        const auto& latest_row = latest.as_array().front();
        require(std::abs(latest_row.at("growth_lower_pct").as_number() - 20.0) < 0.0001 &&
                    std::abs(latest_row.at("growth_upper_pct").as_number() - 30.0) < 0.0001 &&
                    latest_row.at("annualized_profit_lower_yuan").as_number() == 422647217.86 &&
                    latest_row.at("source_variant").as_string() == "all-market-static" &&
                    latest_row.at("raw").is_object(),
                "all-market forecasts must use direct percent fields and retain the raw row");

        auto hong_kong_rows = tdx::Json::array();
        hong_kong_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"03395\",\"$SC\":\"31\",\"ygdate\":\"20260723\","
            "\"bgq\":\"中报\",\"bz\":\"加拿大元\",\"ksrq\":\"20260101\","
            "\"jzri\":\"20260630\",\"type\":\"大幅减亏\","
            "\"jlr1\":\"-2800000\",\"zj1\":\"0.69\","
            "\"contents\":\"预告内容：测试内容\\n\\n变换原因：成本下降\"}"));
        const auto normalized_hong_kong =
            tdx::normalize_hong_kong_forecast_rows(hong_kong_rows);
        const auto& hong_kong = normalized_hong_kong.as_array()[0];
        require(hong_kong.at("security").at("security_id").as_string() == "HK03395" &&
                    hong_kong.at("growth_lower_pct").as_number() == 69 &&
                    hong_kong.at("reason").as_string() == "成本下降" &&
                    hong_kong.at("sentiment").as_string() == "positive",
                "Hong Kong forecasts should preserve currency units and split reasons");

        std::cout << "forecast tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "forecast test failed: " << error.what() << '\n';
        return 1;
    }
}
