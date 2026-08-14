#include "tdx/specialized_metrics.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool close(double left, double right, double epsilon = 1e-7) {
    return std::abs(left - right) <= epsilon;
}
tdx::Json one(std::initializer_list<std::pair<const std::string, tdx::Json>> fields) {
    tdx::Json row = tdx::Json::object();
    for (const auto& [key, value] : fields) row[key] = value;
    tdx::Json rows = tdx::Json::array(); rows.push_back(std::move(row)); return rows;
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000001"}] = {0, "sz", "深圳", "000001", "平安银行"};
        securities[{1, "600030"}] = {1, "sh", "上海", "600030", "中信证券"};
        securities[{1, "601318"}] = {1, "sh", "上海", "601318", "中国平安"};

        const auto banks = tdx::normalize_specialized_metrics_rows(
            "list/func_hyjyfx101_1.jsn", one({
                {"$ZQDM", "000001"}, {"$SC", "0"}, {"date", "20260331"},
                {"zbje", "638993000000"}, {"zbczl", "0.1346"},
                {"ckze", "3749682000000"}, {"dkze", "3473204000000"},
                {"bldkbl", "0.0105"}, {"bldkbbl", "2.1959"}}), securities);
        const auto& bank = banks.as_array().front();
        require(bank.at("security").at("name").as_string() == "平安银行", "bank name");
        require(close(bank.at("capital_adequacy_ratio_pct").as_number(), 13.46), "bank ratio percent");
        require(close(bank.at("capital_net_yuan").as_number(), 638993000000.0), "bank yuan");

        const auto brokers = tdx::normalize_specialized_metrics_rows(
            "list/func_hyjyfx102_1.jsn", one({
                {"$ZQDM", "600030"}, {"$SC", "1"}, {"yf", "202603"},
                {"yysr", "23155089056.75"}, {"snyysr", "17761364917.53"},
                {"jlr", "10458865787.60"}, {"snjlr", "6761816587.54"},
                {"jzbfzl", "0.1851"}}), securities);
        const auto& broker = brokers.as_array().front();
        require(close(broker.at("net_capital_to_liabilities_ratio_pct").as_number(), 18.51), "broker ratio");
        require(close(broker.at("monthly_revenue_yoy_pct").as_number(), 30.3677344859, 1e-6), "broker yoy");

        const auto insurers = tdx::normalize_specialized_metrics_rows(
            "list/func_hyjyfx103_1.jsn", one({
                {"$ZQDM", "601318"}, {"$SC", "1"}, {"T002", "20251231"},
                {"T003", "1504288000000"}, {"T012", "1.607"},
                {"T013", "1.933"}, {"T031", "0.037"}}), securities);
        const auto& insurer = insurers.as_array().front();
        require(close(insurer.at("core_solvency_adequacy_ratio_pct").as_number(), 160.7), "solvency ratio");
        require(close(insurer.at("net_investment_yield_ratio_pct").as_number(), 3.7), "investment yield");
        require(insurer.at("event_id").as_string() == "insurers:SH601318:20251231", "event id");

        std::cout << "specialized metrics tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
