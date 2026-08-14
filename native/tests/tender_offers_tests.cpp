#include "tdx/tender_offers.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{1, "600491"}] =
            tdx::Security{1, "SH", "上海", "600491", "龙元建设"};
        auto source = tdx::Json::array();
        source.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"600491\",\"$SC\":\"1\",\"date\":\"20260805\","
            "\"sgr\":\"宁波开海\",\"jd\":\"要约进行中\",\"gflx\":\"无限售流通股\","
            "\"njg\":\"1.25\",\"ngs\":\"9178.60\",\"nbl\":\"6.00\","
            "\"nzzj\":\"11473.25\",\"bz\":\"人民币\",\"qsr\":\"20260728\","
            "\"zzr\":\"20260826\",\"sjgs\":\"4589.30\",\"sjbl\":\"3.00\","
            "\"ghr\":\"\",\"ts\":\"否\",\"md\":\"巩固控制权\"}"));
        source.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920808\",\"$SC\":\"2\",\"date\":\"20251209\","
            "\"sgr\":\"海光信息\",\"jd\":\"要约失败\",\"ngs\":\"7046.36\","
            "\"nzzj\":\"384942.64\",\"ts\":\"否\"}"));
        const auto rows = tdx::normalize_tender_offer_rows(source, securities);
        require(rows.size() == 2, "two valid offer rows should be normalized");
        const auto& first = rows.as_array()[0];
        require(first.at("security").at("security_id").as_string() == "SH600491" &&
                    first.at("security").at("name").as_string() == "龙元建设" &&
                    first.at("status_category").as_string() == "active" &&
                    first.at("announcement_date").as_string() == "2026-08-05" &&
                    first.at("planned_shares").as_number() == 91786000 &&
                    first.at("planned_funds_yuan").as_number() == 114732500 &&
                    std::abs(first.at("actual_to_planned_pct").as_number() - 50.0) < 0.0001 &&
                    !first.at("delisting_flag").as_bool(),
                "units, status, date, security, and derived completion should match TDX config");
        require(rows.as_array()[1].at("security").at("security_id").as_string() ==
                    "BJ920808" &&
                    rows.as_array()[1].at("status_category").as_string() == "failed",
                "Beijing market and failed offers should be supported");
        auto sorted = rows;
        tdx::sort_tender_offer_rows(sorted, "planned-funds", "desc");
        require(sorted.as_array()[0].at("security").at("security_id").as_string() ==
                    "BJ920808",
                "numeric tender-offer sorting should use normalized yuan values");
        std::cout << "tender-offer tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "tender-offer test failed: " << error.what() << '\n';
        return 1;
    }
}
