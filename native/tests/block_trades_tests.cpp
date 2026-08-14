#include "tdx/block_trades.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{2, "920078"}] =
            tdx::Security{2, "BJ", "北京", "920078", "科强股份"};
        securities[{1, "688775"}] =
            tdx::Security{1, "SH", "上海", "688775", "影石创新"};

        auto trade_rows = tdx::Json::array();
        trade_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"920078\",\"$SC\":\"44\",\"date\":\"20260803\","
            "\"cjj\":\"12.5\",\"spj\":\"13.0\",\"zyj\":\"-3.85\","
            "\"cje\":\"250.5\",\"cjl\":\"20.04\",\"sb7\":\"1\","
            "\"sb30\":\"2\",\"sb90\":\"3\",\"byyb\":\"买方席位\","
            "\"mfxw1\":\"机构\",\"syyb\":\"卖方席位\",\"mfxw2\":\"游资\","
            "\"zttype\":\"A股\"}"));
        const auto trades = tdx::normalize_block_trade_rows(
            trade_rows, securities);
        require(trades.size() == 1 &&
                    trades.as_array()[0].at("security").at("market_id")
                        .as_number() == 2 &&
                    trades.as_array()[0].at("security").at("security_id")
                        .as_string() == "BJ920078" &&
                    std::abs(trades.as_array()[0].at("amount_yuan").as_number() -
                             2505000.0) < 0.01 &&
                    std::abs(trades.as_array()[0].at("volume_shares").as_number() -
                             200400.0) < 0.01 &&
                    trades.as_array()[0].at("frequency").at("days_90")
                        .as_number() == 3,
                "trade rows normalize BJ market, amounts, volumes and frequency");

        auto history_rows = tdx::Json::array();
        history_rows.push_back(tdx::Json::parse(
            "{\"date\":\"20260803\",\"cjj\":\"130.04\",\"cje\":\"1950.6\","
            "\"byyb\":\"机构专用\",\"syyb\":\"卖方甲\"}"));
        history_rows.push_back(tdx::Json::parse(
            "{\"date\":\"20260803\",\"cjj\":\"130.04\",\"cje\":\"1560.48\","
            "\"byyb\":\"机构专用\",\"syyb\":\"卖方乙\"}"));
        const auto history = tdx::normalize_block_trade_history_rows(
            history_rows);
        require(history.size() == 2 &&
                    history.as_array()[0].at("date").as_string() == "20260803" &&
                    history.as_array()[1].at("seller").as_string() == "卖方乙",
                "history keeps multiple transactions on the same date");

        auto intention_rows = tdx::Json::array();
        intention_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"688775\",\"$SC\":\"1\",\"date\":\"20260803\","
            "\"sbjg\":\"120\",\"drspj\":\"130\",\"yjl\":\"-7.69\","
            "\"sbsl\":\"5.5\",\"mmfx\":\"买入\",\"sb30\":\"2\"}"));
        const auto intentions = tdx::normalize_block_trade_intention_rows(
            intention_rows, securities);
        require(intentions.size() == 1 &&
                    intentions.as_array()[0].at("quantity_shares").as_number() ==
                        55000 &&
                    intentions.as_array()[0].at("amount_yuan").as_number() ==
                        6600000,
                "intentions normalize quantity and notional amount");

        auto month_rows = tdx::Json::array();
        month_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"2026-08\",\"cje\":\"100\",\"cjl\":\"20\","
            "\"yzjl\":\"-1.5\",\"cjcs\":\"3\"}"));
        const auto months = tdx::normalize_block_trade_month_rows(month_rows);
        require(months.size() == 1 &&
                    months.as_array()[0].at("amount_yuan").as_number() == 1000000,
                "month rows preserve YYYY-MM and ten-thousand-yuan units");

        auto industry_rows = tdx::Json::array();
        industry_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM1\":\"880489\",\"$ZQDM\":\"2026-08880489\","
            "\"hy\":\"IT设备\",\"cje\":\"3511.08\",\"cjl\":\"27\","
            "\"yzjl\":\"-2.1\",\"cjcs\":\"2\"}"));
        const auto industries = tdx::normalize_block_trade_industry_rows(
            industry_rows);
        require(industries.size() == 1 &&
                    industries.as_array()[0].at("industry_id").as_string() ==
                        "880489" &&
                    industries.as_array()[0].at("detail_id").as_string() ==
                        "2026-08880489" &&
                    industries.as_array()[0].at("amount_yuan").as_number() ==
                        35110800,
                "industry rows preserve hidden month-industry detail key");

        auto broker_rows = tdx::Json::array();
        broker_rows.push_back(tdx::Json::parse(
            "{\"$ZQDM\":\"3722844b\",\"yybmc\":\"华福证券福州五一北路\","
            "\"zjsbr\":\"20260803\",\"zjmmcs\":\"3\",\"mrmmcs\":\"3\","
            "\"mrcje\":\"1711\",\"jmre\":\"1711\",\"cglyr\":\"50\","
            "\"pjzfyr\":\"1.25\"}"));
        const auto brokers = tdx::normalize_block_trade_broker_rows(
            broker_rows, "1m");
        require(brokers.size() == 1 &&
                    brokers.as_array()[0].at("broker_id").as_string() ==
                        "3722844b" &&
                    brokers.as_array()[0].at("buy_amount_yuan").as_number() ==
                        17110000 &&
                    brokers.as_array()[0].at("performance").as_array()[0]
                        .at("success_pct").as_number() == 50,
                "broker rankings normalize amounts and performance windows");

        auto broker_detail_rows = tdx::Json::array();
        broker_detail_rows.push_back(tdx::Json::parse(
            "{\"rq\":\"20260803\",\"$ZQDM\":\"688775\",\"$SC\":\"1\","
            "\"mmfx\":\"买入\",\"cjj\":\"130.04\",\"zyj\":\"-1.2\","
            "\"cjl\":\"15\"}"));
        const auto broker_details =
            tdx::normalize_block_trade_broker_detail_rows(
                broker_detail_rows, securities);
        require(broker_details.size() == 1 &&
                    broker_details.as_array()[0].at("volume_shares").as_number() ==
                        150000 &&
                    std::abs(broker_details.as_array()[0].at("amount_yuan")
                                 .as_number() - 19506000.0) < 0.01,
                "broker details derive shares and notional amount");

        std::cout << "block trades tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "block trades tests failed: " << error.what() << '\n';
        return 1;
    }
}
