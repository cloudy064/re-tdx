#include "tdx/industry_profile.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

std::map<std::string, tdx::Block> profile_blocks() {
    std::map<std::string, tdx::Block> blocks;
    tdx::Block bank;
    bank.block_id = "research-industry:X50";
    bank.family = "research-industry";
    bank.block_code = "881385";
    bank.name = "银行";
    bank.level = 1;
    bank.member_count = 42;
    blocks[bank.block_code] = bank;

    tdx::Block local_bank;
    local_bank.block_id = "research-industry:X5002";
    local_bank.family = "research-industry";
    local_bank.block_code = "881389";
    local_bank.name = "地方性银行";
    local_bank.level = 2;
    local_bank.member_count = 27;
    blocks[local_bank.block_code] = local_bank;
    return blocks;
}

void test_holdings(const std::map<std::string, tdx::Block>& blocks) {
    auto rows = tdx::Json::array();
    rows.push_back(tdx::Json::parse(
        "{\"$ZQDM\":\"881385\",\"sc\":\"1\",\"bgq\":\"20260331\","
        "\"sz\":\"120\",\"sz_sq\":\"100\",\"jgsl\":\"42\","
        "\"jgsl_bd\":\"2\",\"gs\":\"50\",\"gs_bd\":\"10\","
        "\"bgqltgb\":\"200\",\"bgqzgb\":\"400\"}"));
    const auto profile = tdx::normalize_industry_holdings_rows(
        rows, "q1", "一季度", blocks);
    const auto& item = profile.as_array().front();
    require(profile.size() == 1 &&
            item.at("industry").at("name").as_string() == "银行" &&
            std::abs(item.at("metrics").at("market_value_change_pct")
                         .as_number() - 20.0) < 0.0001 &&
            std::abs(item.at("metrics").at("float_share_pct")
                         .as_number() - 25.0) < 0.0001,
            "industry holding normalization");
}

void test_shareholder_summary(
    const std::map<std::string, tdx::Block>& blocks) {
    auto rows = tdx::Json::array();
    rows.push_back(tdx::Json::parse(
        "{\"$ZQDM\":\"881389\",\"$SC\":\"1\","
        "\"sdate\":\"20250804\",\"edate\":\"20260804\","
        "\"zyltg1\":\"120\",\"zyltg2\":\"100\","
        "\"sdlt1\":\"50\",\"sdlt2\":\"40\",\"sdlt3\":\"60\","
        "\"sdgd1\":\"70\",\"sdgd2\":\"60\",\"sdgd3\":\"55\","
        "\"jgcc1\":\"80\",\"jgcc2\":\"64\",\"jgcc3\":\"48\"}"));
    const auto profile =
        tdx::normalize_industry_shareholder_rows(rows, blocks);
    require(profile.size() == 1 &&
            profile.as_array()[0].at("detail_id").as_string() == "1881389" &&
            std::abs(profile.as_array()[0].at("metrics")
                         .at("institution_holding").at("change_pct")
                         .as_number() - 25.0) < 0.0001,
            "industry shareholder summary normalization");
}

void test_shareholder_securities() {
    std::map<std::pair<int, std::string>, tdx::Security> securities;
    securities[{2, "920179"}] =
        tdx::Security{2, "BJ", "北京", "920179", "凯德石英"};
    auto rows = tdx::Json::array();
    rows.push_back(tdx::Json::parse(
        "{\"$ZQDM\":\"920179\",\"$SC\":\"44\","
        "\"date1\":\"20260301\",\"date\":\"20260401\",\"date3\":\"31\","
        "\"gdrs1\":\"1000\",\"gdrs2\":\"-100\",\"gdrs3\":\"-9.09\","
        "\"gdrs4\":\"20000\",\"jzr1\":\"20260331\","
        "\"sdlt1\":\"500\",\"sdlt2\":\"50\",\"jzr2\":\"20260331\","
        "\"sdgd1\":\"600\",\"sdgd2\":\"60\",\"bgq\":\"20260331\","
        "\"jgcc1\":\"400\",\"jgcc2\":\"40\"}"));
    const auto normalized =
        tdx::normalize_industry_shareholder_security_rows(rows, securities);
    const auto& item = normalized.as_array().front();
    require(normalized.size() == 1 &&
            item.at("security").at("market_id").as_number() == 2 &&
            item.at("security").at("name").as_string() == "凯德石英" &&
            item.at("shareholders").at("daily_change_pct").as_number() < 0,
            "industry shareholder security normalization");
}

}  // namespace

int main() {
    try {
        const auto blocks = profile_blocks();
        test_holdings(blocks);
        test_shareholder_summary(blocks);
        test_shareholder_securities();
        std::cout << "industry profile tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "industry profile tests failed: " << error.what() << '\n';
        return 1;
    }
}
