#include "tdx/ownership.hpp"

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
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{2, "920078"}] =
            tdx::Security{2, "BJ", "北京", "920078", "科强股份"};

        auto changes = tdx::Json::array();
        changes.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920078\",\"$SC\":\"44\",\"jzrq\":\"20260801\","
            "\"bdgs\":\"-12.5\",\"cjjj\":\"18.2\",\"bdhcg\":\"100\","
            "\"qsrq\":\"20260701\",\"ggrq\":\"20260802\",\"bdr\":\"测试股东\"}"));
        const auto normalized_changes = tdx::normalize_ownership_change_rows(
            changes, "auto", securities);
        require(normalized_changes.size() == 1 &&
                    normalized_changes.as_array()[0].at("security")
                        .at("security_id").as_string() == "BJ920078" &&
                    normalized_changes.as_array()[0].at("direction").as_string() ==
                        "decrease" &&
                    normalized_changes.as_array()[0].at("change_shares").as_number() ==
                        -125000,
                "ownership changes should normalize Beijing market and share units");

        auto plans = tdx::Json::array();
        plans.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920078\",\"$SC\":\"2\",\"ggrq\":\"20260801\","
            "\"price\":\"20\",\"qsrq\":\"20260802\",\"jzrq\":\"20270201\","
            "\"fw\":\"小于等于\",\"gm\":\"100万股\",\"zb\":\"1.5\","
            "\"zcr\":\"控股股东\",\"zcfs\":\"集中竞价\"}"));
        const auto normalized_plans = tdx::normalize_ownership_plan_rows(
            plans, "increase", securities);
        require(normalized_plans.size() == 1 &&
                    normalized_plans.as_array()[0].at("capital_pct").as_number() == 1.5 &&
                    normalized_plans.as_array()[0].at("direction_label").as_string() ==
                        "拟增持",
                "ownership plans should preserve direction and capital percentage");

        auto ranking_rows = tdx::Json::array();
        ranking_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920078\",\"$SC\":\"2\",\"zcgs\":\"6.15\"," 
            "\"jcgs\":\"-753.20\",\"jzcgs\":\"-747.05\",\"jzczb\":\"8.98\"," 
            "\"price1\":\"28.92\",\"price2\":\"98.06\"," 
            "\"date1\":\"20250804\",\"date2\":\"20260615\"}"));
        const auto normalized_rankings = tdx::normalize_ownership_ranking_rows(
            ranking_rows, "decrease", "ratio", securities);
        require(normalized_rankings.size() == 1 &&
                    normalized_rankings.as_array()[0].at("rank").as_number() == 1 &&
                    normalized_rankings.as_array()[0]
                            .at("net_change_shares").as_number() == -7470500 &&
                    normalized_rankings.as_array()[0]
                            .at("signed_float_change_pct").as_number() == -8.98 &&
                    normalized_rankings.as_array()[0].at("raw").is_object(),
                "ownership rankings should preserve order, signed units, and raw rows");

        auto shareholder_rows = tdx::Json::array();
        shareholder_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920078\",\"$SC\":\"44\",\"date1\":\"20260301\"," 
            "\"date\":\"20260401\",\"date3\":\"31\",\"gdrs1\":\"1000\"," 
            "\"gdrs2\":\"-100\",\"gdrs3\":\"-9.09\",\"gdrs4\":\"20000\"," 
            "\"jzr1\":\"20260331\",\"jzr2\":\"20260331\"," 
            "\"sdlt1\":\"500\",\"sdlt2\":\"50\",\"sdgd1\":\"600\"," 
            "\"sdgd2\":\"60\",\"bgq\":\"20260331\",\"jgcc1\":\"400\"," 
            "\"jgcc2\":\"40\"}"));
        const auto normalized_shareholders = tdx::normalize_shareholder_count_rows(
            shareholder_rows, "bj", securities);
        require(normalized_shareholders.size() == 1 &&
                    normalized_shareholders.as_array()[0].at("security")
                            .at("security_id").as_string() == "BJ920078" &&
                    normalized_shareholders.as_array()[0]
                            .at("top10_float_shares").as_number() == 5000000 &&
                    normalized_shareholders.as_array()[0]
                            .at("institution_shares").as_number() == 4000000 &&
                    normalized_shareholders.as_array()[0]
                            .at("daily_household_change_pct").as_number() < 0 &&
                    normalized_shareholders.as_array()[0].at("raw").is_object(),
                "shareholder-count rows should normalize BJ market and ten-thousand-share units");

        auto insider_rows = tdx::Json::array();
        insider_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920078\",\"$SC\":\"44\",\"date\":\"20260803\","
            "\"bdr\":\"测试高管\",\"cjjj\":\"18.20\",\"bdgs\":\"-125\","
            "\"bdje\":\"-2275\",\"bdhcg\":\"999875\","
            "\"bdyy\":\"二级市场买卖\",\"zw\":\"董事\",\"gx\":\"本人\"}"));
        const auto normalized_insiders = tdx::normalize_insider_change_rows(
            insider_rows, securities);
        require(normalized_insiders.size() == 1 &&
                    normalized_insiders.as_array()[0].at("change_shares").as_number() ==
                        -125 &&
                    normalized_insiders.as_array()[0].at("change_amount_yuan").as_number() ==
                        -2275 &&
                    normalized_insiders.as_array()[0].at("direction").as_string() ==
                        "decrease",
                "insider changes should preserve base share and yuan units");

        auto commitment_rows = tdx::Json::array();
        commitment_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920078\",\"$SC\":\"2\",\"ggrq\":\"20260803\","
            "\"price\":\"18.20\",\"qsrq\":\"20260804\",\"jzrq\":\"20991231\","
            "\"cnr\":\"实际控制人\","
            "\"xq\":\"股东名称：测试股东\\\\r\\\\n股东身份：实际控制人\\\\r\\\\n"
            "变动方式：承诺不减持\\\\r\\\\n变动目的：稳定市场\"}"));
        const auto normalized_commitments =
            tdx::normalize_no_reduction_commitment_rows(
                commitment_rows, securities);
        require(normalized_commitments.size() == 1 &&
                    normalized_commitments.as_array()[0].at("actor").as_string() ==
                        "测试股东" &&
                    normalized_commitments.as_array()[0].at("purpose").as_string() ==
                        "稳定市场" &&
                    normalized_commitments.as_array()[0].at("detail").as_string().find('\n') !=
                        std::string::npos,
                "commitments should structure labeled text and restore newlines");

        auto pledge_history = tdx::Json::array();
        pledge_history.push_back(tdx::Json::parse(
            "{\"zyrq\":\"20260801\",\"gdmc\":\"测试股东\",\"zyf\":\"测试券商\","
            "\"zygs\":\"5350\",\"zcgb\":\"11.52\",\"ljzy\":\"53.48\","
            "\"zzgb\":\"2.07\",\"ljzb\":\"9.63\",\"ts\":\"安全\"}"));
        const auto normalized_pledges =
            tdx::normalize_pledge_history_rows(pledge_history);
        require(normalized_pledges.size() == 1 &&
                    normalized_pledges.as_array()[0].at("shares").as_number() ==
                        53500000 &&
                    normalized_pledges.as_array()[0].at("risk_status").as_string() ==
                        "安全",
                "pledge history should normalize ten-thousand-share units");

        auto monthly = tdx::Json::array();
        monthly.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"2026-08\",\"ZCE\":\"0.14\",\"JCE\":\"-2.92\","
            "\"JE\":\"-2.78\",\"ZCS\":\"5\",\"JCS\":\"4\"}"));
        const auto normalized_monthly =
            tdx::normalize_ownership_statistics_rows(monthly, "month");
        require(normalized_monthly.as_array()[0].at("increase_amount_yuan").as_number() ==
                    14000000 &&
                    normalized_monthly.as_array()[0].at("decrease_amount_yuan").as_number() ==
                    -292000000,
                "ownership statistics should normalize hundred-million yuan");

        const auto count_trend = tdx::normalize_ownership_change_count_trend_rows(
            tdx::Json::parse(
                "[{\"date\":\"202608\",\"zcs\":\"5\",\"jcs\":\"4\"}]") );
        require(count_trend.as_array()[0].at("period").as_string() == "2026-08" &&
                    count_trend.as_array()[0].at("increase_companies").as_number() == 5.0 &&
                    count_trend.as_array()[0].at("decrease_companies").as_number() == 4.0,
                "ownership chart count trend normalization failed");

        auto institutions = tdx::Json::array();
        institutions.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"5001\",\"zyf\":\"测试券商\",\"zybs\":\"3\","
            "\"zygss\":\"2\",\"zysz\":\"52173750\",\"tspc2\":\"100\"}"));
        const auto normalized_institutions =
            tdx::normalize_pledge_institution_rows(institutions, "broker");
        require(normalized_institutions.size() == 1 &&
                    normalized_institutions.as_array()[0].at("institution_id").as_string() ==
                        "5001" &&
                    normalized_institutions.as_array()[0].at("category_label").as_string() ==
                        "券商",
                "pledge institutions should preserve dynamic identifiers");

        std::cout << "ownership tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ownership test failed: " << error.what() << '\n';
        return 1;
    }
}
