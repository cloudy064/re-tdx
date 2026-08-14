#include "tdx/repurchases.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{2, "920078"}] =
            tdx::Security{2, "BJ", "北京", "920078", "科强股份"};

        auto plan_rows = tdx::Json::array();
        plan_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920078\",\"$SC\":\"44\",\"ggrq\":\"20260701\","
            "\"NHG1\":\"20260702\",\"NHG2\":\"20270701\",\"NHG5\":\"30000\","
            "\"NHG3\":\"20\",\"NHG4\":\"1500\",\"SJ1\":\"18\","
            "\"SJ2\":\"16\",\"SJ3\":\"750\",\"SJ4\":\"1.5\","
            "\"SJ5\":\"12000\",\"SF\":\"否\",\"HGYT\":\"减少注册资本\","
            "\"date\":\"20260804\",\"dq\":\"北京\",\"cs\":\"北京市\"}"));
        const auto plans = tdx::normalize_repurchase_plan_rows(plan_rows, securities);
        require(plans.size() == 1 &&
                    plans.as_array()[0].at("security").at("security_id").as_string() ==
                        "BJ920078" &&
                    plans.as_array()[0].at("planned_shares").as_number() == 15000000 &&
                    plans.as_array()[0].at("actual_amount_yuan").as_number() == 120000000 &&
                    std::abs(plans.as_array()[0].at("amount_completion_pct").as_number() -
                             40.0) < 0.0001 &&
                    !plans.as_array()[0].at("completed").as_bool(),
                "plans should normalize Beijing market, units, and progress");

        auto month_rows = tdx::Json::array();
        month_rows.push_back(tdx::Json::parse(
            "{\"YF\":\"202608\",\"NHG4\":\"2.5\",\"NHG5\":\"10\","
            "\"NHG6\":\"1.2\",\"SJ3\":\"2\",\"SJ5\":\"8\",\"SJ4\":\"0.9\"}"));
        const auto months = tdx::normalize_repurchase_month_rows(month_rows);
        require(months.size() == 1 &&
                    months.as_array()[0].at("planned_shares").as_number() == 250000000 &&
                    months.as_array()[0].at("actual_amount_yuan").as_number() == 800000000 &&
                    months.as_array()[0].at("completion_pct").as_number() == 80,
                "monthly statistics should normalize hundred-million units");

        auto annual_rows = tdx::Json::array();
        annual_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"2026\",\"hgsl\":\"1000\",\"hgsz\":\"20\","
            "\"hggps\":\"12\",\"rzje\":\"3\"}"));
        const auto annual = tdx::normalize_repurchase_annual_rows(annual_rows, "a");
        require(annual.size() == 1 &&
                    annual.as_array()[0].at("segment").as_string() == "a" &&
                    annual.as_array()[0].at("shares").as_number() == 10000000 &&
                    annual.as_array()[0].at("amount_yuan").as_number() == 2000000000,
                "annual statistics should normalize shares and yuan");

        auto hk_rows = tdx::Json::array();
        hk_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"08188\",\"$SC\":\"48\",\"jyrq\":\"20260722\","
            "\"hgjj\":\"1.25\",\"jybz\":\"港元\",\"spj\":\"1.3\","
            "\"hgyj\":\"-3.85\",\"hgsl\":\"200000\",\"hgje\":\"250000\","
            "\"zgj\":\"1.3\",\"zdj\":\"1.2\",\"hgfs\":\"联交所\"}"));
        const auto hk = tdx::normalize_hk_repurchase_rows(hk_rows, securities);
        require(hk.size() == 1 &&
                    hk.as_array()[0].at("security").at("market_id").as_number() == 48 &&
                    hk.as_array()[0].at("security").at("code").as_string() == "08188" &&
                    hk.as_array()[0].at("amount").as_number() == 250000 &&
                    hk.as_array()[0].at("currency").as_string() == "港元",
                "Hong Kong rows should preserve market, units, and currency");

        std::cout << "repurchase tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "repurchase test failed: " << error.what() << '\n';
        return 1;
    }
}
