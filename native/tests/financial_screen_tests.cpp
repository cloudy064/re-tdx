#include "tdx/financial_screen.hpp"

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
        securities[{1, "600000"}] = {1, "sh", "上海", "600000", "浦发银行"};
        const auto rows = tdx::normalize_financial_screen_rows(
            "list/func_cwzb101_1.jsn", one({
                {"$ZQDM", "600000"}, {"$SC", "1"}, {"BGQ", "20260331"},
                {"SZ", "30941.12"}, {"PE", "6.15"}, {"PB", "0.41"},
                {"ZCFZ", "91.84"}, {"TB1", "1.49"}, {"TB2", "1.42"},
                {"htfzbq", "852638742.34"}, {"htfzsq", "652840708.94"},
                {"ROE", "2.14"}, {"JGCC", "75.38"},
                {"FHND", "20251231"}, {"GXL", "4.52"}}), securities);
        const auto& row = rows.as_array().front();
        require(row.at("board").as_string() == "sh-main", "board");
        require(row.at("security").at("name").as_string() == "浦发银行", "name");
        require(close(row.at("market_cap_yuan").as_number(), 309411200000.0), "market cap unit");
        require(close(row.at("debt_ratio_pct").as_number(), 91.84), "percentage points");
        require(close(row.at("contract_liability_yoy_pct").as_number(),
                      30.6044078845, 1e-6), "contract liability yoy");

        const auto star = tdx::normalize_financial_screen_rows(
            "list/func_cwzb105_1.jsn", one({
                {"$ZQDM", "688001"}, {"$SC", "1"}, {"ZQJC", "华兴源创"},
                {"BGQ", "20260331"}, {"SZ", "2316.33"}}));
        require(star.as_array().front().at("security").at("name").as_string() ==
                    "华兴源创", "embedded STAR name fallback");

        const auto growth = tdx::normalize_small_cap_growth_rows(
            "list/func_xpcz104_1.jsn", one({
                {"$ZQDM", "688001"}, {"$SC", "1"}, {"ZQJC", "华兴源创"},
                {"bgq", "20260331"}, {"jlzs1", "42.10"},
                {"jlzs2", "30.00"}, {"jlzs3", "20.00"},
                {"jlzs4", "10.00"}, {"jlfh", "40.78"},
                {"yszs1", "18.00"}, {"yszs2", "16.00"},
                {"yszs3", "14.00"}, {"yszs4", "12.00"},
                {"ysfh", "18.80"}}), securities);
        const auto& growth_row = growth.as_array().front();
        require(growth_row.at("dataset").as_string() == "small-cap-growth" &&
                    growth_row.at("board").as_string() == "star" &&
                    growth_row.at("report_period").as_string() == "20260331",
                "small-cap growth board and report period");
        require(close(growth_row.at("adjusted_net_profit_yoy_pct").as_number(),
                      42.10) &&
                    close(growth_row.at("adjusted_net_profit_cagr_3y_pct").as_number(),
                          40.78) &&
                    close(growth_row.at("revenue_yoy_t_minus_3_pct").as_number(),
                          12.0) &&
                    close(growth_row.at("revenue_cagr_3y_pct").as_number(), 18.80) &&
                    growth_row.at("raw").is_object(),
                "small-cap growth T/T-1/T-2/T-3 and CAGR semantics");

        std::cout << "financial screen tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
