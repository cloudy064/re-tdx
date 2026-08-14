#include "tdx/curated_data.hpp"

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

tdx::Json rows(tdx::Json value) {
    tdx::Json result = tdx::Json::array();
    result.push_back(std::move(value));
    return result;
}

}  // namespace

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000802"}] = {0, "sz", "深圳", "000802", "北京文化"};
        securities[{1, "600519"}] = {1, "sh", "上海", "600519", "贵州茅台"};
        securities[{1, "601328"}] = {1, "sh", "上海", "601328", "交通银行"};

        const auto media = tdx::normalize_curated_data_rows(
            "list/func_cmyl101_1.jsn", rows(row({
                {"$SC", "0"}, {"$ZQDM", "000802"}, {"sldy", "48"},
                {"sldsj", "4"}, {"slzy", "3"}, {"date", "20260217"},
                {"name", "《星河入梦》"}, {"bjzl", "影视项目背景"}})), securities);
        require(media.size() == 1, "media row count");
        require(media.as_array().front().at("security").at("name").as_string() ==
                    "北京文化", "media security resolution");
        require(media.as_array().front().at("movie_count").as_number() == 48,
                "media movie count");

        const auto low = tdx::normalize_curated_data_rows(
            "list/func_dgzxz101.jsn", rows(row({
                {"$SC", "0"}, {"$ZQDM", "000802"}, {"ZXRQ", "20260805"},
                {"JGSL", "4"}, {"ZHPJ", "4.5"}, {"PE", "20"},
                {"YCEPS", "25"}, {"SZ", "5000000000"}, {"PE_TTM", "18"},
                {"MBJ", "10.5"}, {"bgq", "20260630"}, {"jlr1", "100000000"},
                {"jlr2", "120000000"}, {"zj1", "73.69"}, {"zj2", "110.25"}})),
            securities);
        const auto& low_row = low.as_array().front();
        require(close(low_row.at("estimated_peg").as_number(), 0.8),
                "estimated PEG reproduces PE/YCEPS");
        require(close(low_row.at("forecast_profit_growth_pct_lower").as_number(), 73.69),
                "forecast growth exposed as intuitive percent");

        const auto returns = tdx::normalize_curated_data_rows(
            "list/func_fhmz101_1.jsn", rows(row({
                {"$SC", "1"}, {"$ZQDM", "600519"}, {"ljfh", "4011.454372"},
                {"ljfhcs", "30"}, {"ljmj", "22.443850"},
                {"ljmjcs", "1"}, {"GXL", "3.98"}})), securities);
        const auto& return_row = returns.as_array().front();
        require(close(return_row.at("cumulative_dividend_yuan").as_number(),
                      4011.454372e8), "hundred-million dividend converted to yuan");
        require(close(return_row.at("dividend_fundraising_ratio").as_number(),
                      4011.454372 / 22.443850), "dividend/fundraising formula");

        const auto buyback = tdx::normalize_curated_data_rows(
            "list/func_gfhgtj101_1.jsn", rows(row({
                {"date", "202607"}, {"hgsl", "154054.07"}, {"hgsz", "328.02"},
                {"nlr", "142583314382.26"}, {"J_ZSZ", "112874344575665.03"},
                {"J_LTSZ", "98628404951219.43"}, {"hggps", "193"}})), securities);
        const auto& buyback_row = buyback.as_array().front();
        require(buyback_row.at("security").is_null(), "buyback statistic is aggregate");
        require(close(buyback_row.at("planned_buyback_yuan").as_number(), 328.02e8),
                "buyback hundred-million yuan conversion");
        require(close(buyback_row.at("market_cap_ratio_pct").as_number(),
                      328.02e10 / 112874344575665.03),
                "buyback market-cap ratio formula");

        const auto dividend = tdx::normalize_curated_data_rows(
            "list/func_gfhl101_1.jsn", rows(row({
                {"$SC", "1"}, {"$ZQDM", "600519"}, {"N001", "20231231"},
                {"N002", "1309467294"}, {"N003", "1259201327"},
                {"N004", "103.99"}})), securities);
        require(dividend.as_array().front().at("payout_ratio_pct").as_number() == 103.99,
                "payout ratio remains percent");

        std::map<std::string, std::string> hk_names{{"00700", "腾讯控股"}};
        const auto hk = tdx::normalize_curated_data_rows(
            "list/func_ggthq101_1.jsn", rows(row({
                {"$SC", "31"}, {"$ZQDM", "00700"}, {"rq", "20260806"},
                {"spj", "550.5"}, {"drcje", "1000000"}, {"wrzf", "5.2"},
                {"wrcje", "5000000"}, {"eszf", "10"}, {"lszf", "20"},
                {"byzf", "3"}, {"nczf", "40"}, {"syl", "25"}})),
            securities, hk_names);
        const auto& hk_row = hk.as_array().front();
        require(hk_row.at("security").at("security_id").as_string() == "HK00700",
                "Hong Kong identity");
        require(hk_row.at("security").at("name").as_string() == "腾讯控股",
                "Hong Kong local name mapping");
        const auto hk_fund = tdx::normalize_curated_data_rows(
            "list/func_ggthq101_1.jsn", rows(row({
                {"$SC", "49"}, {"$ZQDM", "02800"}, {"rq", "20260806"},
                {"spj", "26"}, {"wrzf", "1"}})), securities, hk_names);
        require(hk_fund.size() == 1 &&
                    hk_fund.as_array().front().at("security").at("market_id").as_number() == 49,
                "Hong Kong fund market 49 is retained");

        const auto lending = tdx::normalize_curated_data_rows(
            "list/func_ggzrt101_1.jsn", rows(row({
                {"$SC", "0"}, {"$ZQDM", "000802"}, {"LTSZ", "1839954000"},
                {"zsz", "3820800000"}, {"jyrq", "20240118"}, {"zxye", "9.58"},
                {"yezb", "0.005206651905"}, {"rqyl", "1.25"}})), securities);
        const auto& lending_row = lending.as_array().front();
        require(lending_row.at("has_lending_data").as_bool(), "lending presence flag");
        require(close(lending_row.at("refinancing_lending_balance_yuan").as_number(),
                      95800), "ten-thousand yuan conversion");
        require(close(lending_row.at("refinancing_lending_shares").as_number(),
                      12500), "ten-thousand shares conversion");

        const auto soe = tdx::normalize_curated_data_rows(
            "list/func_gqpjg101_1.jsn", rows(row({
                {"$SC", "1"}, {"$ZQDM", "601328"}, {"N001", "20260806"},
                {"N002", "613244662507.62"}, {"N003", "1301771000000"},
                {"N004", "0.471084900883"}, {"N005", "财政部"},
                {"N006", "中央国家机关"}, {"N007", "35.01"},
                {"N008", "无"}, {"N009", "其它"}, {"N010", ""}})), securities);
        const auto& soe_row = soe.as_array().front();
        require(close(soe_row.at("price_to_book_ratio").as_number(), 0.471084900883),
                "below-book PB");
        require(soe_row.at("controlling_shareholder").as_string() == "财政部",
                "controller preserved");

        std::cout << "curated data tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
