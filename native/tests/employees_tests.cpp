#include "tdx/employees.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        tdx::Security security;
        security.market_id = 0;
        security.market = "sz";
        security.code = "000034";
        security.name = "神州数码";
        securities.emplace(std::make_pair(0, security.code), security);

        auto rows = tdx::Json::array();
        auto raw = tdx::Json::object();
        raw["$SC"] = "0";
        raw["$ZQDM"] = "000034";
        raw["price1"] = "10.500";
        raw["gmgs"] = "123.4500";
        raw["zb"] = "1.25";
        raw["jhzqs"] = "4";
        raw["ssqs"] = "3";
        raw["zt"] = "股票购买中";
        raw["date1"] = "20260408";
        raw["date2"] = "20260510";
        raw["date3"] = "20260510";
        raw["date4"] = "20340509";
        raw["date5"] = "20260510";
        raw["date6"] = "20270510";
        raw["hy"] = "软件服务";
        raw["xqsm"] = "员工持股计划详情";
        rows.push_back(std::move(raw));

        const auto normalized = tdx::normalize_employee_share_plan_rows(rows, securities);
        const auto& row = normalized.as_array().front();
        require(row.at("security").at("name").as_string() == "神州数码" &&
                row.at("active").as_bool(), "share plan security and status");
        require(std::abs(row.at("purchase_shares").as_number() - 1234500.0) < 1e-6,
                "share plan ten-thousand-share conversion");
        require(std::abs(row.at("purchase_amount_yuan").as_number() - 12962250.0) < 1e-6,
                "share plan amount derivation");
        require(std::abs(row.at("tranche_completion_pct").as_number() - 75.0) < 1e-9,
                "share plan tranche completion");
        require(row.at("dates").at("lock_end").as_string() == "20270510" &&
                row.at("raw").is_object(), "share plan dates and raw evidence");
        std::cout << "employees tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
