#include "tdx/institution_lhb.hpp"

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
        securities[{1, "601872"}] =
            tdx::Security{1, "SH", "上海", "601872", "招商轮船"};
        securities[{1, "601003"}] =
            tdx::Security{1, "SH", "上海", "601003", "柳钢股份"};
        auto rows = tdx::Json::array();
        rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"601872\",\"$SC\":\"1\",\"jgcys\":\"2\","
            "\"jgmr\":\"455311131.7200\",\"jgmc\":\"\","
            "\"jyr1\":\"20260624\",\"jyr2\":\"20260624\"}"));
        rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"601003\",\"$SC\":\"1\",\"jgcys\":\"2\","
            "\"jgmr\":\"\",\"jgmc\":\"4228496.0000\","
            "\"jyr1\":\"20260729\",\"jyr2\":\"20260729\"}"));
        const auto normalized = tdx::normalize_institution_lhb_rows(
            rows, tdx::institution_lhb_periods()[2], securities);
        const auto& buyer = normalized.as_array()[0];
        const auto& seller = normalized.as_array()[1];
        require(buyer.at("security").at("name").as_string() == "招商轮船" &&
                    buyer.at("net_institution_amount_yuan").as_number() == 455311131.72 &&
                    buyer.at("direction").as_string() == "net-buy" &&
                    buyer.at("buy_sell_ratio").is_null() &&
                    buyer.at("period").as_string() == "quarter",
                "institution ranking should normalize blank amounts as zero and sort net buys");
        require(seller.at("net_institution_amount_yuan").as_number() == -4228496 &&
                    seller.at("direction").as_string() == "net-sell",
                "institution ranking should preserve net-sell semantics");

        auto event_rows = tdx::Json::array();
        event_rows.push_back(tdx::Json::parse(
            "{\"stockcode\":\"601003\",\"sc\":\"1\",\"date\":\"20260730\","
            "\"ydlx\":\"连续三个交易日内涨幅偏离值累计达20%的证券\","
            "\"ydrzf\":\"9.97\",\"bje\":\"81600202.0000\","
            "\"sje\":\"43211339.8400\"}"));
        const auto events = tdx::normalize_institution_lhb_event_rows(event_rows);
        const auto& event = events.as_array()[0];
        require(event.at("security").at("security_id").as_string() == "SH601003" &&
                    event.at("event_change_pct").as_number() == 9.97 &&
                    std::abs(event.at("event_net_buy_amount_yuan").as_number() -
                             38388862.16) < 0.001 &&
                    event.at("direction").as_string() == "net-buy",
                "linked anomaly events should preserve percent and full-yuan totals");

        std::cout << "institution-LHB tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "institution-LHB test failed: " << error.what() << '\n';
        return 1;
    }
}
