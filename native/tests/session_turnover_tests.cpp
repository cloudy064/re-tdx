#include "tdx/session_turnover.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() {
    try {
        require(tdx::session_turnover_resource_for("a", "after-hours") ==
                    "list/func_phcje101_1.jsn" &&
                tdx::session_turnover_resource_for("a", "opening") ==
                    "list/func_phcje101_2.jsn" &&
                tdx::session_turnover_resource_for("etf", "after-hours") ==
                    "list/func_phcje103_1.jsn" &&
                tdx::session_turnover_resource_for("etf", "opening-share") ==
                    "list/func_phcje104_1.jsn",
                "four client sort branches must map to their exact resources");

        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{2, "920001"}] =
            tdx::Security{2, "BJ", "北京", "920001", "测试北证"};
        auto raw = tdx::Json::array();
        raw.push_back(tdx::Json::parse(
            R"({"$SC":"2","$ZQDM":"920001","date":"20260806","price1":"11.4","price2":"11.0","price3":"11.2","zcje":"1000000","kpje":"100000","phje":"50000"})"));
        raw.push_back(tdx::Json::parse(
            R"({"$SC":"0","$ZQDM":"159001","date":"20260806","price1":"1.0","price2":"1.0","price3":"1.0","zcje":"2000000","kpje":"","phje":"20000"})"));
        auto rows = tdx::normalize_session_turnover_rows(raw, securities);
        const auto& first = rows.as_array()[0];
        require(first.at("security").at("security_id").as_string() == "BJ920001" &&
                    first.at("security").at("name").as_string() == "测试北证" &&
                    std::abs(first.at("close_change_pct").as_number() -
                             3.63636363636) < 0.000001 &&
                    first.at("opening_share_total_pct").as_number() == 10.0 &&
                    first.at("after_hours_share_total_pct").as_number() == 5.0 &&
                    first.at("raw").is_object(),
                "normalizer should preserve identity, yuan amounts, derived ratios and raw row");
        require(rows.as_array()[1].at("opening_turnover_yuan").is_null(),
                "blank upstream opening amount must remain null");

        tdx::sort_session_turnover_rows(rows, "after-hours", "desc");
        require(rows.as_array()[0].at("security").at("security_id").as_string() ==
                    "BJ920001",
                "descending after-hours sort should rank the larger amount first");
        tdx::sort_session_turnover_rows(rows, "after-hours", "asc");
        require(rows.as_array()[0].at("security").at("security_id").as_string() ==
                    "SZ159001",
                "ascending after-hours sort should rank the smaller amount first");

        std::cout << "session-turnover tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "session-turnover test failed: " << error.what() << '\n';
        return 1;
    }
}
