#include "tdx/equity_performance.hpp"

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
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000001"}] = {0, "sz", "深圳", "000001", "平安银行"};
        tdx::Json row = tdx::Json::object();
        row["$SC"] = "0";
        row["$ZQDM"] = "000001";
        row["hqrq"] = "20260807";
        row["price0"] = "11.190";
        row["price1"] = "11.630";
        row["cje"] = "986373760.00";
        row["5cje"] = "6491881472.00";
        row["zdf_5d"] = "-3.78";
        row["zdf_20d"] = "7.08";
        row["zdf_60d"] = "4.68";
        row["zdf_ys"] = "1.27";
        row["pe"] = "5.0430";
        tdx::Json rows = tdx::Json::array();
        rows.push_back(std::move(row));
        const auto normalized = tdx::normalize_equity_performance_rows(rows, securities);
        require(normalized.size() == 1, "row count");
        const auto& item = normalized.as_array().front();
        require(item.at("security").at("name").as_string() == "平安银行", "name");
        require(close(item.at("daily_turnover_yuan").as_number(), 986373760.0), "turnover");
        require(close(item.at("return_5d_pct").as_number(), -3.78), "5d return");
        require(close(item.at("month_to_date_pct").as_number(),
                      (11.19 - 11.63) * 100.0 / 11.63), "month formula");
        require(item.at("source_resource").as_string() ==
                    "list/func_aghq101.jsn", "source");
        std::cout << "equity performance tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
