#include "tdx/fund_statistics.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool close(double left, double right, double epsilon = 1e-6) {
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

const tdx::Json& normalized(const char* resource, tdx::Json value,
                            tdx::Json& storage) {
    storage = tdx::normalize_fund_statistics_rows(resource, rows(std::move(value)));
    require(storage.size() == 1, "one normalized row expected");
    return storage.as_array().front();
}

}  // namespace

int main() {
    try {
        tdx::Json storage;
        const auto& issuance = normalized("list/func_jjtj101_1.jsn", row({
            {"$ZQDM", "023205"}, {"$SC", "33"}, {"jjjc", "兴全合辰混合A"},
            {"fxksr", "20260527"}, {"fxjzr", "20260826"},
            {"rgfl", "0.8"}, {"rgje", "1"}, {"sgfl", "0.8"},
            {"sgje", "1"}, {"shfl", "1.5"}, {"shje", "10"},
            {"jjlx", "开放式基金"}, {"tzlx", "混合型"},
            {"glr", "兴证全球基金"}, {"jjjl", "童兰"}}), storage);
        require(issuance.at("security").at("security_id").as_string() ==
                    "FUND023205", "OTC market 33 identity");
        require(close(issuance.at("subscription_fee_pct").as_number(), 0.8),
                "subscription fee stays percent");

        const auto& dividend = normalized("list/func_jjtj102_1.jsn", row({
            {"$ZQDM", "000028"}, {"$SC", "33"}, {"ZQJC", "华富安鑫债券A"},
            {"DWJZ", "1.0718"}, {"LJJZ", "1.6619"}, {"GGRQ", "20260708"},
            {"QYDJR", "20260709"}, {"CXR", "20260709"},
            {"HLPFR", "20260710"}, {"PXBL", "1.36"},
            {"FHSM", "每10份派红利0.136元"}}), storage);
        require(dividend.at("distribution_description").as_string() ==
                    "每10份派红利0.136元", "dividend statement retained");
        require(close(dividend.at("distribution_ratio_raw").as_number(), 1.36),
                "ambiguous distribution ratio stays raw");

        const auto& performance = normalized("list/func_jjtj103_1.jsn", row({
            {"$ZQDM", "003634"}, {"$SC", "33"}, {"ZQJC", "嘉实农业产业股票A"},
            {"ZXRQ", "20260806"}, {"DWJZ", "1.1431"}, {"LJJZ", "1.1431"},
            {"JYY", "1.63"}, {"JSY", "-10.52"}, {"JLY", "-14.10"},
            {"JYN", "-17.85"}, {"JNYL", "-14.61"}}), storage);
        require(close(performance.at("return_1y_pct").as_number(), -17.85),
                "performance remains percentage points");

        const auto& market_size = normalized("list/func_jjtj104_1.jsn", row({
            {"jzrq", "20260630"}, {"qbzs", "14399"}, {"qbjs", "180"},
            {"qbfe", "332056.22"}, {"qbzc", "392841.72"},
            {"cgsz", "88150.33"}, {"kfzs", "14311"}, {"kffe", "331537.45"},
            {"kfzc", "392823.31"}, {"fbzs", "88"}, {"fbfe", "18.1"},
            {"fbzc", "18.41"}}), storage);
        require(close(market_size.at("all_fund_nav_yuan").as_number(),
                      39284172000000.0, 0.1), "hundred-million yuan conversion");

        const auto& size_chart = normalized("list/func_jjtj104_2.jsn", row({
            {"rq", "20260630"}, {"data", "332056.22"}}), storage);
        require(close(size_chart.at("all_fund_units").as_number(),
                      33205622000000.0, 0.1), "fund units chart conversion");

        const auto& etf_size = normalized("list/func_jjtj105_1.jsn", row({
            {"rq", "20260806"}, {"shgm", "20369.01"}, {"szgm", "11767.17"},
            {"hjgm", "32136.19"}, {"shss", "-81.70"}, {"szss", "-24.11"},
            {"hjss", "-105.81"}, {"szzs", "3900.35"}, {"zdf", "0.57"}}), storage);
        require(close(etf_size.at("total_net_subscription_units").as_number(),
                      -10581000000.0, 0.1), "ETF subscription units conversion");

        const auto& subscription_chart = normalized(
            "list/func_jjtj105_2.jsn", row({
                {"rq", "20260806"}, {"data", "-105.81"}}), storage);
        require(close(subscription_chart.at("total_net_subscription_units").as_number(),
                      -10581000000.0, 0.1), "ETF chart units conversion");

        const auto& weekly = normalized("list/func_jjtj108_1.jsn", row({
            {"BZJZRQ", "20260806"}, {"BZCJE", "7720.3033290913"},
            {"CJEBD", "-2867.4352649739"}, {"BZZFE", "22898.278396"},
            {"ZFEBD", "-312.005877"}, {"ZRZYE", "570.32042236"},
            {"RZYEBD", "-22.77640107"}, {"ZRQYL", "26.90258878"},
            {"RQYLBD", "1.29585493"}, {"SPJ", "3900.35"},
            {"ZZF", "1.7767583619"}}), storage);
        require(close(weekly.at("turnover_yuan").as_number(),
                      772030332909.13, 0.1), "ETF turnover conversion");

        const auto& listed = normalized("list/func_jjtj109_1.jsn", row({
            {"$ZQDM", "159017"}, {"$SC", "0"}, {"jjjc", "石油ETF工银"},
            {"ssrq", "20260508"}, {"ssggr", "20260423"},
            {"mjfe", "2.32256813"}, {"ssjyfe", "2.002568"},
            {"ssdrjz", "0.992"}, {"cyrhs", "1696"}, {"jjjl", "李锐敏"},
            {"tzfg", "复制指数型"}, {"glr", "工银瑞信基金管理有限公司"}}), storage);
        require(listed.at("security").at("security_id").as_string() ==
                    "SZ159017", "listed fund exchange identity");
        require(close(listed.at("raised_units").as_number(), 232256813.0, 0.1),
                "listed fund raised units conversion");

        std::cout << "fund statistics tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
