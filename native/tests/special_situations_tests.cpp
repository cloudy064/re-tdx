#include "tdx/special_situations.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Security security(int market, const std::string& code,
                       const std::string& name) {
    return {market, market == 0 ? "SZ" : market == 1 ? "SH" : "BJ",
            "", code, name};
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{1, "600449"}] = security(1, "600449", "宁夏建材");
        securities[{2, "834082"}] = security(2, "834082", "中建信息");
        securities[{0, "200581"}] = security(0, "200581", "苏威孚B");
        securities[{0, "000581"}] = security(0, "000581", "威孚高科");
        securities[{0, "000002"}] = security(0, "000002", "万科A");

        auto merger_rows = tdx::Json::array();
        auto merger = tdx::Json::object();
        merger["$SC"] = "1";
        merger["$ZQDM"] = "600449";
        merger["hgj1"] = "13.21";
        merger["jd"] = "股东大会通过";
        merger["$SC1"] = "";
        merger["$ZQDM1"] = "834082";
        merger["xjxzq"] = "15.36";
        merger["hgj2"] = "15.36";
        merger["xq"] = "换股方案与说明";
        merger_rows.push_back(std::move(merger));
        auto mergers = tdx::normalize_special_situation_rows(
            "list/func_agtl101_1.jsn", merger_rows, securities);
        require(mergers.size() == 1, "merger row normalization");
        auto& merger_row = mergers.as_array().front();
        require(merger_row.at("primary_security").at("security_id").as_string() ==
                    "SH600449" &&
                merger_row.at("related_security").at("security_id").as_string() ==
                    "BJ834082",
                "blank related market must resolve from security directory");
        require(merger_row.at("active").as_bool() &&
                merger_row.at("absorbed_cash_option_price").as_number() == 15.36,
                "merger status and cash option");

        auto quote_rows = tdx::Json::array();
        auto primary_quote = tdx::Json::object();
        primary_quote["market_id"] = 1;
        primary_quote["code"] = "600449";
        primary_quote["last_price"] = 14.95;
        quote_rows.push_back(std::move(primary_quote));
        auto related_quote = tdx::Json::object();
        related_quote["market_id"] = 2;
        related_quote["code"] = "834082";
        related_quote["last_price"] = 14.00;
        quote_rows.push_back(std::move(related_quote));
        tdx::apply_special_situation_quotes(mergers, quote_rows);
        require(std::abs(merger_row.at("absorber_exchange_premium_pct").as_number() -
                         (14.95 - 13.21) * 100.0 / 13.21) < 1e-9,
                "absorber exchange premium formula");
        require(std::abs(merger_row.at("cash_option_premium_pct").as_number() -
                         (14.00 - 15.36) * 100.0 / 15.36) < 1e-9,
                "absorbed cash-option premium formula");

        auto b_rows = tdx::Json::array();
        auto b = tdx::Json::object();
        b["$SC"] = "0";
        b["$ZQDM"] = "200581";
        b["bz"] = "港币";
        b["xjxzq"] = "12.680";
        b["jd"] = "董事会预案";
        b["$SC1"] = "0";
        b["$ZQDM1"] = "000581";
        b["yagg"] = "https://example.test/announcement";
        b["xq"] = "B股转换为H股";
        b_rows.push_back(std::move(b));
        const auto b_normalized = tdx::normalize_special_situation_rows(
            "list/func_agtl102_1.jsn", b_rows, securities);
        const auto& b_row = b_normalized.as_array().front();
        require(b_row.at("kind").as_string() == "b-to-h" &&
                b_row.at("currency").as_string() == "港币" &&
                b_row.at("announcement_url").as_string().find("https://") == 0,
                "B-to-H currency and announcement");

        auto risk_rows = tdx::Json::array();
        auto risk = tdx::Json::object();
        risk["$SC"] = "0";
        risk["$ZQDM"] = "000002";
        risk["ybzs"] = "沪深300,创业板指";
        risk["rxlx"] = "同时触发";
        risk["date1"] = "20260806";
        risk["zaf1"] = "-26.87";
        risk["price4"] = "9.50";
        risk["zaf2"] = "-54.85";
        risk_rows.push_back(std::move(risk));
        const auto risks = tdx::normalize_special_situation_rows(
            "list/func_cdgc101_1.jsn", risk_rows, securities);
        const auto& risk_row = risks.as_array().front();
        require(risk_row.at("twenty_day_triggered").as_bool() &&
                risk_row.at("one_year_triggered").as_bool() &&
                risk_row.at("breach_count").as_number() == 2,
                "market-cap risk dual threshold");
        require(risk_row.at("sample_indexes").size() == 2 &&
                risk_row.at("sample_indexes").as_array()[1].as_string() == "创业板指",
                "sample index list split");

        auto corporate_rows = tdx::Json::array();
        auto corporate = tdx::Json::object();
        corporate["$SC"] = "1";
        corporate["$ZQDM"] = "600449";
        corporate["bdhdf"] = "收购方";
        corporate["bdcrf"] = "出让方";
        corporate["bdlx"] = "发行股份购买";
        corporate["sjje"] = "40600910000";
        corporate["xmjd"] = "实施完成";
        corporate["hy"] = "建材";
        corporate["date"] = "20260613";
        corporate["bdjj"] = "重大资产重组简介";
        corporate_rows.push_back(corporate);
        corporate_rows.push_back(std::move(corporate));
        const auto completed = tdx::normalize_special_situation_rows(
            "list/func_qxfa110_1.jsn", corporate_rows, securities);
        const auto& completed_row = completed.as_array().front();
        require(completed_row.at("kind").as_string() ==
                    "major-restructuring-completed" &&
                completed_row.at("transaction_amount_yuan").as_number() ==
                    40600910000.0 &&
                std::abs(completed_row.at("transaction_amount_100m_yuan").as_number() -
                         406.0091) < 1e-9,
                "corporate-action stage and amount units");
        require(completed.size() == 2 &&
                completed.as_array()[0].at("event_id").as_string() !=
                    completed.as_array()[1].at("event_id").as_string() &&
                completed.as_array()[1].at("source_row_index").as_number() == 1,
                "duplicate business keys retain stable source-row identity");

        auto transfer_rows = tdx::Json::array();
        auto transfer = tdx::Json::object();
        transfer["$SC"] = "44";
        transfer["$ZQDM"] = "830838";
        transfer["ssbk"] = "创业板";
        transfer["bgq"] = "20251231";
        transfer["JZC"] = "500000000";
        transfer["jlr1"] = "65000000";
        transfer["jlr2"] = "52000000";
        transfer["yysr1"] = "800000000";
        transfer["mqjd"] = "辅导备案";
        transfer["fdjg"] = "示例证券";
        transfer["xq"] = "拟转板详情";
        transfer_rows.push_back(std::move(transfer));
        const auto transfer_plans = tdx::normalize_special_situation_rows(
            "list/func_xsbtj101_1.jsn", transfer_rows, securities);
        const auto& transfer_plan = transfer_plans.as_array().front();
        require(transfer_plan.at("kind").as_string() == "neeq-transfer-plan" &&
                transfer_plan.at("primary_security").at("market").as_string() == "bj" &&
                transfer_plan.at("target_board").as_string() == "创业板" &&
                transfer_plan.at("net_profit_yuan").as_number() == 65000000.0,
                "NEEQ transfer plan and market normalization");

        auto regulation_rows = tdx::Json::array();
        auto regulation = tdx::Json::object();
        regulation["$SC"] = "44";
        regulation["$ZQDM"] = "830838";
        regulation["JGYY"] = "信息披露违规";
        regulation["JGCS"] = "出具警示函";
        regulation["fxts"] = "监管案情";
        regulation["hy"] = "软件服务";
        regulation["GGRQ"] = "20260701";
        regulation_rows.push_back(std::move(regulation));
        const auto regulations = tdx::normalize_special_situation_rows(
            "list/func_xsbtj102_1.jsn", regulation_rows, securities);
        require(regulations.as_array().front().at("regulation_reason").as_string() ==
                    "信息披露违规" &&
                regulations.as_array().front().at("date").as_string() == "20260701",
                "NEEQ regulation semantics");

        auto transferred_rows = tdx::Json::array();
        auto transferred = tdx::Json::object();
        transferred["$SC"] = "0";
        transferred["$ZQDM"] = "000002";
        transferred["N001"] = "20240101";
        transferred["N002"] = "20240301";
        transferred["N003"] = "20240401";
        transferred["N004"] = "全国股转系统";
        transferred["N005"] = "创业板";
        transferred["N006"] = "已完成转板";
        transferred_rows.push_back(std::move(transferred));
        const auto transferred_result = tdx::normalize_special_situation_rows(
            "list/func_yzb101_1.jsn", transferred_rows, securities);
        const auto& transferred_row = transferred_result.as_array().front();
        require(transferred_row.at("kind").as_string() ==
                    "neeq-transfer-completed" &&
                transferred_row.at("acceptance_date").as_string() == "20240101" &&
                transferred_row.at("listing_venue_after").as_string() == "创业板",
                "completed transfer dates and venues");

        std::cout << "special situations tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
