#include "tdx/financial_insights.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
bool close(double a, double b, double epsilon = 1e-7) { return std::abs(a - b) <= epsilon; }
tdx::Json one(std::initializer_list<std::pair<const std::string, tdx::Json>> fields) {
    tdx::Json row = tdx::Json::object(); for (const auto& [k,v] : fields) row[k] = v;
    tdx::Json rows = tdx::Json::array(); rows.push_back(std::move(row)); return rows;
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0,"000404"}] = {0,"sz","深圳","000404","长虹华意"};
        auto rows = tdx::normalize_financial_insight_rows(
            "list/func_cbpl107_1.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"bgq","20260630"},{"cyje1","390518683.08"},{"cyje2","415218758.73"},
                {"cyje3","395753738.38"}}), securities);
        const auto& property = rows.as_array().front();
        require(close(property.at("investment_property_qoq_pct").as_number(),
            (390518683.08 - 415218758.73) * 100.0 / 415218758.73), "property qoq");

        rows = tdx::normalize_financial_insight_rows(
            "list/func_fhbdb101.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"N001","6045.3778"},{"N002","15831.574891"},{"N003","158315748.91"},
                {"N004","5998.319266"},{"N005","9694.610106"},
                {"N007","96946101.06"},{"N008","1609531473.39"}}), securities);
        const auto& dividend = rows.as_array().front();
        require(close(dividend.at("latest_dividend_yuan").as_number(), 60453778.0), "10k yuan");
        require(close(dividend.at("rd_to_revenue_3y_pct").as_number(),
            96946101.06 * 100.0 / 1609531473.39), "rd ratio");

        rows = tdx::normalize_financial_insight_rows(
            "list/func_gxjcb101.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"date","20260331"},{"zsz","5345249118.72"},{"hbzj","5481006228.09"},
                {"zcfzl","0.6122176606109"},{"gxl","4.17"},{"ljfh","11.365368"},
                {"ljmj","21.4"}}), securities);
        const auto& cash = rows.as_array().front();
        require(close(cash.at("debt_ratio_pct").as_number(), 61.22176606109), "debt pct");
        require(close(cash.at("cumulative_dividend_yuan").as_number(), 1136536800.0), "100m yuan");

        rows = tdx::normalize_financial_insight_rows(
            "list/func_gqtz101_1.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"DATE","20251231"},{"ZQS1","1"},{"ZQS2","1"},
                {"ZJE","6366793.54"},{"ZSY","1913281.33"}}), securities);
        require(close(rows.as_array().front().at("investment_total_yuan").as_number(),
                      63667935400.0), "investment 10k yuan");

        rows = tdx::normalize_financial_insight_rows(
            "list/func_knzx101_1.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"N001","20260331"},{"N002","业绩报告"},{"N003","621129812.65"},
                {"N004","98656471.86"},{"N005","-26115409.54"},
                {"N006","20260630"},{"N007","-181000000"},
                {"N008","-131000000"},{"N009","20260829"}}), securities);
        const auto& warning = rows.as_array().front();
        require(warning.at("kind").as_string() == "profit-warning", "profit kind");
        require(close(warning.at("forecast_profit_midpoint_yuan").as_number(),
                      -156000000.0), "forecast midpoint");
        require(warning.at("has_forecast").as_bool() &&
                !warning.at("actual_profit_positive").as_bool(), "profit flags");

        rows = tdx::normalize_financial_insight_rows(
            "list/func_xjl101_1.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"bgq","20260630"},{"zyxjl","-801527566.39"},{"xjl","3.504"},
                {"xjje","8349299931"},{"jxjbl","57.29"},{"jlrbl","131.319"},
                {"zcfzl","53.722"},{"ldbl","1.186"},{"sdbl","0.806"},
                {"chzcbl","32.0677482679586925"},{"syl","17.271"}}), securities);
        const auto& cash_flow = rows.as_array().front();
        require(cash_flow.at("kind").as_string() == "cash-flow-quality", "cash kind");
        require(cash_flow.at("screen_criteria_complete").as_bool(), "cash criteria");
        require(close(cash_flow.at("operating_cash_flow_per_share_yuan").as_number(),
                      3.504), "cash per share");
        require(close(cash_flow.at("amount_value_yuan").as_number(),
                      -801527566.39), "free cash flow signal amount");

        rows = tdx::normalize_financial_insight_rows(
            "list/func_yjfz101_1.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"ZXRQ","20260520"},{"JGSL","4"},{"ZHPJ","5"},{"MBJ",""},
                {"BGQ","2025"},{"bglx","年度报告"},{"JLR1","-5716279.52"},
                {"JLR2","568957500"},{"JLRZZL","10053.28"},
                {"YYSR1","6884708007.99"},{"YYSR2","9901500000"},
                {"YSZZL","43.82"}}), securities);
        const auto& reversal = rows.as_array().front();
        require(reversal.at("kind").as_string() == "earnings-reversal" &&
                reversal.at("profit_reversal").as_bool(), "earnings reversal");
        require(close(reversal.at("profit_growth_recalculated_pct").as_number(),
                      (568957500.0 + 5716279.52) * 100.0 / 5716279.52),
                "absolute-base profit growth");

        rows = tdx::normalize_financial_insight_rows(
            "list/func_wjcg101_1.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"price","4.34"},{"sszf","125.50"},{"ljfh","10.40"},
                {"ljfhcs","6"},{"zjlr","2999478903.76"},{"beta","0.25"},
                {"pe_ttm","18.5"}}), securities);
        const auto& steady = rows.as_array().front();
        require(steady.at("kind").as_string() == "steady-growth", "steady kind");
        require(close(steady.at("cumulative_dividend_yuan").as_number(),
                      1040000000.0), "steady dividend unit");
        require(close(steady.at("dividend_payout_pct").as_number(),
                      10.40 * 100000000.0 * 100.0 / 2999478903.76),
                "steady payout formula");

        rows = tdx::normalize_financial_insight_rows(
            "list/func_lxsnzz101_1.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"zxbgq","20251231"},{"dsnyysr","100"},{"denyysr","120"},
                {"dynyysr","150"},{"dsnssfyl","3"},{"denssfyl","2"},
                {"dynssfyl","1"},{"dsnmll","20"},{"denmll","22"},
                {"dynmll","25"},{"dsnyffy","10"},{"denyffy","12"},
                {"dynyffy","18"}}), securities);
        const auto& quality = rows.as_array().front();
        require(quality.at("kind").as_string() == "quality-growth" &&
                quality.at("screen_criteria_complete").as_bool(), "quality criteria");
        require(close(quality.at("revenue_growth_t_pct").as_number(), 25.0),
                "quality revenue growth");

        rows = tdx::normalize_financial_insight_rows(
            "list/func_tqwclr101.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"zxrq","20260630"},{"bqlr","1200000000"},
                {"qntq","20250630"},{"qntqlr","300000000"},
                {"bjqrq","20251231"},{"sqlr","500000000"}}), securities);
        const auto& breakout = rows.as_array().front();
        require(breakout.at("kind").as_string() == "profit-breakout", "breakout kind");
        require(close(breakout.at("profit_yoy_growth_pct").as_number(), 300.0) &&
                close(breakout.at("profit_breakout_growth_pct").as_number(), 140.0),
                "breakout formulas");

        rows = tdx::normalize_financial_insight_rows(
            "list/func_qxfa101_1.jsn", one({{"$SC","0"},{"$ZQDM","000404"},
                {"ggdate","20260808"},{"fhdate","20260630"},
                {"szbl","2.5"},{"xjfh","3.2"},{"djdate","20260813"},
                {"qxdate","20260814"},{"fajd","实施方案"},
                {"WeekPrice","2.5"},{"MonthPrice","4.5"},
                {"QuarterPrice","8.5"},{"hy","家电"}}), securities);
        const auto& plan = rows.as_array().front();
        require(plan.at("kind").as_string() == "dividend-plan", "dividend plan kind");
        require(close(plan.at("cash_dividend_per_share_yuan").as_number(), 0.32) &&
                close(plan.at("stock_transfer_per_share").as_number(), 0.25),
                "dividend per-share conversion");
        std::cout << "financial insights tests passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
