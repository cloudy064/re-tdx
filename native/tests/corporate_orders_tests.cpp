#include "tdx/corporate_orders.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace { void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); } }

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{1, "601208"}] = {1, "sh", "上海", "601208", "东材科技"};
        tdx::Json raw = tdx::Json::object(); raw["$SC"] = "1"; raw["$ZQDM"] = "601208";
        raw["zbrq"] = "20200108"; raw["zbjd"] = "中标"; raw["zbje"] = "556000000";
        raw["yysr"] = "5181213271.79"; raw["znyszb"] = "10.7310772754";
        raw["nrgg"] = "收到中标通知书TXT:http://example.test/a.pdf";
        tdx::Json rows = tdx::Json::array(); rows.push_back(raw);
        const auto result = tdx::normalize_corporate_order_rows("list/func_zb101_1.jsn", rows, securities);
        require(result.size() == 1, "row count"); const auto& item = result.as_array().front();
        require(item.at("security").at("name").as_string() == "东材科技", "name");
        require(item.at("source_url").as_string() == "http://example.test/a.pdf", "url");
        require(item.at("revenue_share_formula_matches").as_bool(), "ratio formula");
        require(std::abs(item.at("calculated_revenue_share_pct").as_number() - 10.7310772754) < 1e-8, "ratio");
        std::cout << "corporate orders tests passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
