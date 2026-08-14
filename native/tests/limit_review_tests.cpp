#include "tdx/limit_review.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{2, "920001"}] =
            tdx::Security{2, "BJ", "北京", "920001", "测试北证"};

        auto current = tdx::Json::array();
        current.push_back(tdx::Json::parse(
            R"({"$SC":"2","$ZQDM":"920001","ztrq":"20260806","ztcs":"2","ztsj1":"09:31","ztsj2":"10:02","lbts":"3","ztcs1":"8","yj5cs":"2","crlpl":"0.75","crjzf":"3.5","sbfbl":"0.8","ztlbl":"0.4","ztlx":"涨停","bx":"换手板","yy":"测试原因"})"));
        const auto current_rows = tdx::normalize_limit_review_current_rows(
            current, "limit-up", securities);
        require(current_rows.size() == 1 &&
                    current_rows.as_array()[0].at("security")
                        .at("security_id").as_string() == "BJ920001" &&
                    current_rows.as_array()[0].at("limit_gene")
                        .at("next_day_red_rate").as_number() == 0.75 &&
                    current_rows.as_array()[0].at("raw").is_object(),
                "current limit-up rows should preserve market, gene and raw data");

        auto annual = tdx::Json::array();
        annual.push_back(tdx::Json::parse(
            R"({"$SC":"2","$ZQDM":"920001","NZTC1":"4","NZTC2":"2","NZTC":"6","NDTC1":"1","NDTC2":"3","NDTC":"4","PJHSL1":"8.5","GKL":"40","GSL":"30","PJHSL2":"6.5","DKL":"20","DSL":"10","DATE":"20260806","CXG":"是"})"));
        const auto annual_rows = tdx::normalize_limit_review_annual_rows(
            annual, securities);
        require(annual_rows.as_array()[0].at("limit_up")
                    .at("total_count").as_number() == 6 &&
                    annual_rows.as_array()[0].at("limit_down")
                    .at("intraday_count").as_number() == 3,
                "annual limit behavior should retain close and intraday counts");

        auto market = tdx::Json::array();
        market.push_back(tdx::Json::parse(
            R"({"$ZQDM":"20260806","szzs1":"1.2","szzs2":"20000","ztjs1":"100","ztjs2":"80","ztjs3":"20","ztjs4":"10","ztjs5":"3","ztjs6":"3000000000","ztjs7":"500000000","ztjs8":"8000000000","ztjs9":"1000000000","dtjs1":"5","dtjs2":"3","dtjs3":"2","zdtb1":"20","zdtb2":"4","szcz13":"6","szcz14":"80","szcz1":"50","szcz2":"8"})"));
        const auto market_rows =
            tdx::normalize_limit_review_market_history_rows(market);
        require(market_rows.as_array()[0].at("market_turnover_yuan").as_number() ==
                    2000000000000.0 &&
                    market_rows.as_array()[0].at("limit_up")
                    .at("streak_distribution").at("2").as_number() == 8,
                "market history should normalize 100m-yuan turnover and streak breadth");

        auto daily = tdx::Json::array();
        daily.push_back(tdx::Json::parse(
            R"({"$SC":"2","$ZQDM":"920001","zdf":"29.98","lb":"涨停","yy":"测试","time1":"09:30","time2":"09:31","dkcs":"1","lbts":"2"})"));
        const auto daily_rows = tdx::normalize_limit_review_daily_rows(
            daily, "limit-up", "20260806", securities);
        require(daily_rows.as_array()[0].at("date").as_string() == "20260806" &&
                    daily_rows.as_array()[0].at("daily_change_pct").as_number() == 29.98,
                "daily detail should bind the requested date and price change");

        tdx::LimitReviewService service;
        tdx::LimitReviewQuery invalid_history;
        invalid_history.view = "history";
        invalid_history.market = "sz";
        bool rejected_market_history = false;
        try { (void)service.query(invalid_history); }
        catch (const std::exception&) { rejected_market_history = true; }
        require(rejected_market_history,
                "market-wide history must reject an inapplicable market filter");

        tdx::LimitReviewQuery invalid_code;
        invalid_code.view = "current";
        invalid_code.code = "000001";
        bool rejected_code_without_market = false;
        try { (void)service.query(invalid_code); }
        catch (const std::exception&) { rejected_code_without_market = true; }
        require(rejected_code_without_market,
                "a security code must not be accepted without its market identity");

        std::cout << "limit-review tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "limit-review test failed: " << error.what() << '\n';
        return 1;
    }
}
