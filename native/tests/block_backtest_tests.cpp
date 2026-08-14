#include "tdx/block_backtest.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json rows(std::initializer_list<tdx::Json> values) {
    tdx::Json result = tdx::Json::array();
    for (const auto& value : values) result.push_back(value);
    return result;
}

}  // namespace

int main() {
    try {
        tdx::BlockData blocks;
        tdx::Block bank;
        bank.block_id = "industry:15";
        bank.family = "industry";
        bank.family_name = "行业板块";
        bank.block_code = "880471";
        bank.name = "银行";
        blocks.blocks.push_back(bank);
        blocks.securities[{0, "000001"}] =
            tdx::Security{0, "SZ", "深圳", "000001", "平安银行"};

        tdx::BlockBacktestQuery query;
        query.category = "industry";
        query.sort = "return";
        query.order = "desc";
        const auto overview = tdx::normalize_block_backtest_rows(rows({
            tdx::Json::parse(
                "{\"setcode\":\"1\",\"code\":\"880471\",\"name\":\"银行\","
                "\"trade_date\":\"20260805\",\"qspj\":\"3163.8101\","
                "\"spj\":\"3404.4900\",\"qjzdf\":\"7.6073\","
                "\"qjzdhc\":\"-6.5746\",\"qjcje\":\"853,348,894,720\"}"),
            tdx::Json::parse(
                "{\"setcode\":\"1\",\"code\":\"880472\",\"name\":\"证券\","
                "\"trade_date\":\"20260805\",\"qjzdf\":\"-1.3174\"}")}),
            query, blocks);
        require(overview.size() == 2 &&
                    overview.as_array()[0].at("block").at("block_id").as_string() ==
                        "industry:15" &&
                    overview.as_array()[0].at("trade_date").as_string() == "2026-08-05" &&
                    overview.as_array()[0].at("rank").as_number() == 1 &&
                    std::abs(overview.as_array()[0].at("amount").as_number() -
                             853348894720.0) < 0.5,
                "overview normalizes fields, resolves blocks and sorts by interval return");

        query.block_code = "880471";
        query.sort = "max-drawdown";
        query.order = "asc";
        const auto members = tdx::normalize_block_backtest_rows(rows({
            tdx::Json::parse(
                "{\"setcode\":\"0\",\"code\":\"000001\",\"name\":\"平安银行\","
                "\"trade_date\":\"20260805\",\"qjzdf\":\"11.9403\","
                "\"qjzdhc\":\"-4.1166\",\"qjjlr\":\"1514211168\"}"),
            tdx::Json::parse(
                "{\"setcode\":\"1\",\"code\":\"600000\",\"name\":\"浦发银行\","
                "\"trade_date\":\"20260805\",\"qjzdf\":\"5\","
                "\"qjzdhc\":\"-8.5\"}")}), query, blocks);
        require(members.as_array()[0].at("security").at("security_id").as_string() ==
                    "SH600000" &&
                    members.as_array()[1].at("security").at("name_resolved").as_bool() &&
                    members.as_array()[1].at("net_inflow").as_number() == 1514211168.0,
                "member mode maps markets, security identities and signed drawdown ordering");

        bool invalid_category = false;
        try {
            query = {};
            query.category = "guessed";
            (void)tdx::normalize_block_backtest_rows(tdx::Json::array(), query, blocks);
        } catch (const tdx::Error&) { invalid_category = true; }
        require(invalid_category, "unknown category is rejected instead of guessed");

        std::cout << "block backtest tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
