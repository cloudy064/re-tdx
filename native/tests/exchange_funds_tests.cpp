#include "tdx/exchange_funds.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double left, double right, double epsilon = 1e-8) {
    return std::abs(left - right) <= epsilon;
}

tdx::Json row(std::initializer_list<std::pair<const std::string, tdx::Json>> fields) {
    tdx::Json result = tdx::Json::object();
    for (const auto& [key, value] : fields) result[key] = value;
    return result;
}

}  // namespace

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{1, "510300"}] = {1, "sh", "上海", "510300", "沪深300ETF"};
        securities[{1, "511600"}] = {1, "sh", "上海", "511600", "货币ETF"};
        securities[{1, "519800"}] = {1, "sh", "上海", "519800", "保证金A"};
        securities[{0, "159002"}] = {0, "sz", "深圳", "159002", "货币基金"};
        securities[{0, "180101"}] = {0, "sz", "深圳", "180101", "蛇口产园REIT"};
        securities[{0, "160105"}] = {0, "sz", "深圳", "160105", "南方积配LOF"};
        securities[{1, "500001"}] = {1, "sh", "上海", "500001", "基金金泰"};

        tdx::Json etf_rows = tdx::Json::array();
        etf_rows.push_back(row({
            {"$SC", "1"}, {"$ZQDM", "510300"}, {"date", "20260806"},
            {"price0", "4.200"}, {"price6", "4.000"},
            {"price21", "3.500"}, {"price61", "3.000"},
            {"price1", "4.100"}, {"price11", "3.800"},
            {"drcje", "123456789.00"}, {"cje", "555555555.00"}}));
        const auto etfs = tdx::normalize_exchange_fund_rows(
            "list/func_etfhq101.jsn", etf_rows, securities);
        require(etfs.size() == 1, "ETF row count");
        const auto& etf = etfs.as_array().front();
        require(etf.at("kind").as_string() == "etf-performance", "ETF kind");
        require(etf.at("security").at("name").as_string() == "沪深300ETF",
                "ETF name resolution");
        require(close(etf.at("change_5d_pct").as_number(), 5.0), "ETF 5d formula");
        require(close(etf.at("change_20d_pct").as_number(), 20.0), "ETF 20d formula");
        require(close(etf.at("change_60d_pct").as_number(), 40.0), "ETF 60d formula");
        require(close(etf.at("change_month_pct").as_number(),
                      (4.2 - 4.1) * 100.0 / 4.1), "ETF month formula");
        require(close(etf.at("change_ytd_pct").as_number(),
                      (4.2 - 3.8) * 100.0 / 3.8), "ETF YTD formula");
        require(etf.at("turnover_yuan").as_number() == 123456789.0,
                "ETF turnover remains yuan");

        tdx::Json ranking_rows = tdx::Json::array();
        ranking_rows.push_back(row({
            {"$SC", "1"}, {"$ZQDM", "510300"}, {"$SC1", "1"},
            {"$ZQDM1", "000300"}, {"JZRQ", "20260807"},
            {"JLR", "123456789.00"}, {"ZXFE", "25128487700.00"},
            {"FEBH", "-576900000.00"}, {"ZFEBH", "-1338300000.00"},
            {"YFEBH", "8034300000.00"}, {"YZGM", "123149963168.10"},
            {"YYGM", "82496549840.20"}, {"ZXSSDW", "90.00"}}));
        auto ranking = tdx::normalize_exchange_fund_rows(
            "list/func_tlfeyxetf101_1.jsn", ranking_rows, securities);
        const auto& ranking_raw = ranking.as_array().front();
        require(ranking_raw.at("kind").as_string() == "etf-share-ranking",
                "ETF share ranking kind");
        require(ranking_raw.at("subscription_unit_10k_shares").as_number() == 90.0 &&
                    ranking_raw.at("subscription_unit_shares").as_number() == 900000.0,
                "ETF share ranking subscription unit converts 10k shares exactly once");
        require(ranking_raw.at("reference_instrument").at("code").as_string() == "000300",
                "ETF share ranking reference index retained");
        tdx::Json ranking_quotes = tdx::Json::array();
        ranking_quotes.push_back(row({
            {"market_id", 1}, {"code", "510300"}, {"last_price", 4.10},
            {"fund_iopv", 4.08}, {"name", "沪深300ETF"}}));
        tdx::apply_exchange_fund_quotes(ranking, ranking_quotes);
        const auto& ranking_live = ranking.as_array().front();
        require(close(ranking_live.at("latest_scale_yuan").as_number(),
                      4.10 * 25128487700.0), "ETF live scale formula");
        require(close(ranking_live.at("daily_scale_change_yuan").as_number(),
                      4.10 * -576900000.0), "ETF daily scale change formula");
        require(close(ranking_live.at("weekly_scale_change_yuan").as_number(),
                      4.10 * 25128487700.0 - 123149963168.10),
                "ETF weekly scale change formula");
        require(close(ranking_live.at("monthly_scale_change_yuan").as_number(),
                      4.10 * 25128487700.0 - 82496549840.20),
                "ETF monthly scale change formula");
        require(close(ranking_live.at("premium_pct").as_number(),
                      (4.10 - 4.08) * 100.0 / 4.08), "ETF IOPV premium formula");

        tdx::Json scale_rows = tdx::Json::array();
        scale_rows.push_back(row({
            {"$SC", "1"}, {"$ZQDM", "510300"}, {"$SC1", "27"},
            {"$ZQDM1", "000300"}, {"ZSJC1", "沪深300"}, {"JZRQ", "20260806"},
            {"DWJZ", "4.201"}, {"ZXFE", "987654321"}, {"FEBH", "12345"},
            {"ZFEBH", "22222"}, {"YFEBH", "33333"},
            {"YZGM", "4148148148"}, {"YYGM", "4000000000"},
            {"ZXSSDW", "900000"}, {"ZGSGF", "0.5"}, {"ZGSHF", "0.5"}}));
        const auto scale = tdx::normalize_exchange_fund_rows(
            "list/gxjty_etfjj101.jsn", scale_rows, securities);
        const auto& scale_row = scale.as_array().front();
        require(scale_row.at("kind").as_string() == "etf-scale-flow",
                "ETF scale kind");
        require(scale_row.at("latest_scale_yuan").as_number() == 4148148148.0,
                "ETF scale remains yuan");
        require(scale_row.at("reference_instrument").at("market_id").as_number() == 27,
                "non-A-share reference market remains explicit");

        tdx::Json lof_rows = tdx::Json::array();
        lof_rows.push_back(row({
            {"$SC", "0"}, {"$ZQDM", "160105"}, {"JZRQ", "20260806"},
            {"CNFE", "50000000"}, {"XZFE", "120000"}, {"JJJZ", "1.2345"},
            {"GPZB", "73.2"}, {"ZGSGF", "1.5"}, {"ZGSHF", "0.5"},
            {"SSZT", "开放"}}));
        const auto lofs = tdx::normalize_exchange_fund_rows(
            "list/gxjty_lofjj101.jsn", lof_rows, securities);
        require(lofs.as_array().front().at("equity_ratio_pct").as_number() == 73.2,
                "LOF allocation ratio retained");

        tdx::Json closed_rows = tdx::Json::array();
        closed_rows.push_back(row({
            {"$SC", "1"}, {"$ZQDM", "500001"}, {"JZRQ", "20260806"},
            {"DWJZ", "1.1"}, {"DQRQ", "20271101"}, {"SYNX", "1.23"},
            {"JJC", "-4.5"}, {"GPZB", "62.1"}}));
        const auto closed = tdx::normalize_exchange_fund_rows(
            "list/gxjty_fbjj101.jsn", closed_rows, securities);
        require(closed.as_array().front().at("kind").as_string() == "closed-fund" &&
                closed.as_array().front().at("discount_pct").as_number() == -4.5,
                "closed-fund discount retained");

        tdx::Json cash_calendar_rows = tdx::Json::array();
        cash_calendar_rows.push_back(row({
            {"$SC", "1"}, {"$ZQDM", "511600"}, {"TS", "3"},
            {"SXF", "0"}, {"ZJKY", "20260810"}, {"SCZJJS", "20260811"},
            {"ZJKQ", "20260812"}, {"SJTS", "5"}, {"ZJKYTS", "3"},
            {"ZJKQTS", "4"}}));
        const auto cash_calendar = tdx::normalize_exchange_fund_rows(
            "list/gxjty_xjgl101.jsn", cash_calendar_rows, securities);
        require(cash_calendar.as_array().front().at("available_days").as_number() == 3,
                "cash-management availability calendar retained");

        tdx::Json arbitrage_rows = tdx::Json::array();
        arbitrage_rows.push_back(row({
            {"$SC", "1"}, {"$ZQDM", "511600"}, {"ZJDZ", "20260811"},
            {"ZKTS", "4"}, {"QRNH", "0.9080"}, {"YJQRNH", "0.9062"},
            {"NJQRNH", "1.0383"}, {"FEBH", "100.00"},
            {"ZXFE", "864700.00"}, {"TR", "20260807"},
            {"TYR", "20260810"}, {"MRSHJXR", "3"}, {"SGMCJXR", "1"}}));
        auto arbitrage = tdx::normalize_exchange_fund_rows(
            "list/gxjty_etfjj103.jsn", arbitrage_rows, securities);
        require(arbitrage.size() == 1, "cash arbitrage row count");
        const double theoretical = 100.0 + 3.0 * 0.908 / 365.0;
        require(close(arbitrage.as_array().front().at("theoretical_nav").as_number(),
                      theoretical), "cash ETF theoretical NAV formula");

        tdx::Json quote_rows = tdx::Json::array();
        quote_rows.push_back(row({
            {"market_id", 1}, {"code", "511600"}, {"last_price", 100.02},
            {"name", "货币ETF"}}));
        tdx::apply_exchange_fund_quotes(arbitrage, quote_rows);
        const auto& priced = arbitrage.as_array().front();
        require(close(priced.at("premium_pct").as_number(),
                      (100.02 - theoretical) * 100.0 / theoretical),
                "cash ETF premium formula");
        require(close(priced.at("buy_redeem_annualized_pct").as_number(),
                      (theoretical - 100.02) / 100.02 * 365.0 / 4.0 * 100.0),
                "buy/redeem annualized formula");
        require(close(priced.at("subscribe_sell_annualized_pct").as_number(),
                      ((100.02 - 100.0) + 0.908 / 365.0) / 100.0 *
                          365.0 / 4.0 * 100.0),
                "subscribe/sell annualized formula");

        tdx::Json yield_rows = tdx::Json::array();
        yield_rows.push_back(row({
            {"$SC", "34"}, {"$ZQDM", "519800"}, {"JXLX", "算头不算尾"},
            {"WFSY", "0.0971"}, {"QRNH", "0.3940"},
            {"YJQRNH", "0.3910"}, {"NJQRNH", "0.4933"},
            {"ZXFE", "62.56"}}));
        const auto yields = tdx::normalize_exchange_fund_rows(
            "list/gxjty_etfjj104.jsn", yield_rows, securities);
        require(yields.size() == 1, "cash yield row count");
        const auto& yield = yields.as_array().front();
        require(yield.at("source_market_id").as_number() == 34,
                "source market 34 retained");
        require(yield.at("security").at("market").as_string() == "sh",
                "source market 34 maps to Shanghai trading identity");
        require(yield.at("latest_shares_100m").as_number() == 62.56,
                "cash fund hundred-million share unit retained");
        tdx::Json sz_yield_rows = tdx::Json::array();
        sz_yield_rows.push_back(row({
            {"$SC", "34"}, {"$ZQDM", "159002"}, {"JXLX", "算尾不算头"},
            {"WFSY", "0.3365"}, {"QRNH", "1.2450"},
            {"YJQRNH", "1.2654"}, {"NJQRNH", "1.3478"},
            {"ZXFE", "0.12"}}));
        const auto sz_yields = tdx::normalize_exchange_fund_rows(
            "list/gxjty_etfjj104.jsn", sz_yield_rows, securities);
        require(sz_yields.as_array().front().at("security").at("market").as_string() ==
                    "sz",
                "source market 34 keeps 159-prefix funds in Shenzhen");

        tdx::Json reit_rows = tdx::Json::array();
        reit_rows.push_back(row({
            {"$SC", "0"}, {"$ZQDM", "180101"}, {"ZQJC", "蛇口产园REIT"},
            {"xjsj", "20210525"}, {"xjqj", "2.100-2.420"},
            {"gzrgqsr", "20210531"}, {"gzrgjzr", "20210601"},
            {"rgj", "2.31"}, {"qx", "50年"}, {"zgm", "900000000"},
            {"zlps", "585000000"}, {"ysrrg", "288000000"},
            {"wxgm", "220500000"}, {"wsgm", "94500000"},
            {"jzrq", "20201231"}, {"mgsy", "53526600"},
            {"mgjzc", "450130200"}, {"mgxjl", ""}, {"jj", "项目简介"}}));
        const auto reits = tdx::normalize_exchange_fund_rows(
            "list/func_reits101_1.jsn", reit_rows, securities);
        require(reits.size() == 1, "issued REIT row count");
        const auto& reit = reits.as_array().front();
        require(reit.at("offering_total_units").as_number() == 900000000.0,
                "REIT offering units preserved");
        require(reit.at("inquiry_price_low").as_number() == 2.1 &&
                reit.at("inquiry_price_high").as_number() == 2.42,
                "REIT inquiry range parsed");

        tdx::Json blank_pipeline = tdx::Json::array();
        blank_pipeline.push_back(row({
            {"$ZQDM", ""}, {"ZQJC", ""}, {"gxsj", ""}, {"xmzt", ""}}));
        const auto pipeline = tdx::normalize_exchange_fund_rows(
            "list/func_reits102_1.jsn", blank_pipeline, securities);
        require(pipeline.size() == 0,
                "configured blank REIT pipeline row becomes normal empty relation");

        std::cout << "exchange fund tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
