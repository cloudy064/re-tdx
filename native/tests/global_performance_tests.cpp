#include "tdx/global_performance.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool close(double left, double right, double epsilon = 1e-7) {
    return std::abs(left - right) <= epsilon;
}
tdx::Json row(std::initializer_list<std::pair<const std::string, tdx::Json>> fields) {
    tdx::Json result = tdx::Json::object();
    for (const auto& [key, value] : fields) result[key] = value;
    return result;
}
}

int main() {
    try {
        tdx::GlobalInstrumentNames names;
        names[{74, "VIPS"}] = "唯品会";
        tdx::Json major_rows = tdx::Json::array();
        major_rows.push_back(row({
            {"$SC", "1"}, {"$ZQDM", "000300"}, {"hqrq", "20260807"},
            {"price0", "4694.44"}, {"price5", "4588.20"},
            {"price20", "4780.79"}, {"price60", "4914.60"},
            {"price1", "4588.20"}, {"price2", "4629.94"},
            {"cje", "788405747712"}, {"5cje", "3729920163840"}}));
        const auto major = tdx::normalize_global_performance_rows(
            "list/func_zyzh101.jsn", major_rows, names);
        const auto& index = major.as_array().front();
        require(index.at("kind").as_string() == "major-index" &&
                    index.at("instrument").at("name").as_string() == "沪深300",
                "major-index identity");
        require(close(index.at("return_5d_pct").as_number(),
                      (4694.44 - 4588.20) * 100.0 / 4588.20),
                "major-index 5d return formula");

        tdx::Json overseas_rows = tdx::Json::array();
        overseas_rows.push_back(row({
            {"$SC", "74"}, {"$ZQDM", "VIPS"}, {"date", "20260806"},
            {"price0", "15.41"}, {"price6", "15.19"},
            {"price21", "13.85"}, {"price61", "14.64"},
            {"price1", "15.34"}, {"price11", "17.69"},
            {"drcje", "24227652"}, {"cje", "174541552"}, {"pe", "7.6869"}}));
        const auto overseas = tdx::normalize_global_performance_rows(
            "list/func_zgghq101.jsn", overseas_rows, names);
        const auto& stock = overseas.as_array().front();
        require(stock.at("instrument").at("name").as_string() == "唯品会" &&
                    stock.at("kind").as_string() == "overseas-china",
                "overseas instrument directory resolution");
        require(close(stock.at("year_to_date_pct").as_number(),
                      (15.41 - 17.69) * 100.0 / 17.69),
                "overseas YTD return formula");
        std::cout << "global performance tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
