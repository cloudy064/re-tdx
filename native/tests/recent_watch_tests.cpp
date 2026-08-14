#include "tdx/recent_watch.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
bool close(double left, double right, double epsilon = 1e-7) { return std::abs(left - right) <= epsilon; }
tdx::Json one(std::initializer_list<std::pair<const std::string, tdx::Json>> fields) {
    tdx::Json row = tdx::Json::object(); for (const auto& [key, value] : fields) row[key] = value;
    tdx::Json rows = tdx::Json::array(); rows.push_back(std::move(row)); return rows;
}
}
int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "301308"}] = {0, "sz", "深圳", "301308", "江波龙"};
        const auto divergence = tdx::normalize_recent_watch_rows(
            "list/func_jqgz101_1.jsn", one({{"$ZQDM","301308"},{"$SC","0"},
                {"ygrq","20260703"},{"zf_gj","-38.87"},{"bgq","20260630"},
                {"lx","业绩大幅上升"},{"zf_yc","74393.95"},{"yy","原因"}}), securities);
        const auto& d = divergence.as_array().front();
        require(d.at("kind").as_string() == "earnings-divergence", "divergence kind");
        require(close(d.at("return_since_announcement_pct").as_number(), -38.87), "snapshot return");
        const auto foreign = tdx::normalize_recent_watch_rows(
            "list/func_jqgz104_1.jsn", one({{"$ZQDM","301308"},{"$SC","0"},
                {"bgq","20260630"},{"yysr","45.57"},{"srje","35.67"},
                {"srzb","39.21"},{"lrje","19.42"},{"lx","利空"}}), securities);
        require(close(foreign.as_array().front().at("foreign_revenue_yuan").as_number(), 3567000000.0), "hundred-million yuan");
        const auto turnaround = tdx::normalize_recent_watch_rows(
            "list/func_jqgz111_1.jsn", one({{"$ZQDM","301308"},{"$SC","0"},
                {"ggrq","20260714"},{"bgq","20260630"},{"lx","预计扭亏"},
                {"ygjl1","1600000"},{"ygjl2","2400000"},{"ygjl3","117.21"}}), securities);
        require(close(turnaround.as_array().front().at("profit_lower_yuan").as_number(), 1600000), "profit yuan");
        std::cout << "recent watch tests passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
