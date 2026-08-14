#include "tdx/state_owned_reform.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000050"}] = {0, "SZ", "深圳", "000050", "深天马Ａ"};
        securities[{1, "600963"}] = {1, "SH", "上海", "600963", "岳阳林纸"};

        auto master = tdx::Json::object();
        master["HYFL"] = "央企整合";
        master["$S_ZQDM"] = "0|000050,1|600963";
        master["CFGS"] = "2";
        master["$ZQDM"] = "gl197";
        auto master_rows = tdx::Json::array();
        master_rows.push_back(master);
        const auto groups = tdx::normalize_state_owned_groups(
            master_rows, tdx::state_owned_dimensions()[2], securities);
        require(groups.size() == 1, "group count");
        const auto& group = groups.as_array()[0];
        require(group.at("name").as_string() == "央企整合", "group name");
        require(group.at("member_count").as_number() == 2, "member count");
        require(group.at("count_matches").as_bool(), "declared count");
        require(group.at("members").as_array()[0].at("security_id").as_string() ==
                "SZ000050", "member identity");

        auto detail = tdx::Json::object();
        detail["$ZQDM"] = "000050";
        detail["$SC"] = "0";
        detail["skr"] = "中国航空工业集团有限公司";
        detail["kgbl"] = "11.8600";
        detail["fqprice_d3"] = "6.53";
        detail["fqprice_d20"] = "8.04";
        detail["lzcs"] = "2";
        detail["LJSM"] = "科研院所类央企及其下属上市公司";
        auto detail_rows = tdx::Json::array();
        detail_rows.push_back(detail);
        auto quote = tdx::Json::object();
        quote["market_id"] = 0;
        quote["code"] = "000050";
        quote["last_price"] = 7.836;
        quote["change_pct"] = 1.25;
        quote["amount"] = 12345678.0;
        auto quotes = tdx::Json::array();
        quotes.push_back(quote);
        const auto details = tdx::normalize_state_owned_details(
            detail_rows, quotes, securities);
        require(details.size() == 1, "detail count");
        const auto& normalized = details.as_array()[0];
        require(normalized.at("actual_controller").as_string() ==
                "中国航空工业集团有限公司", "controller");
        require(std::abs(normalized.at("returns_pct").at("3d").as_number() - 20.0) < 1e-8,
                "three-day return formula");
        require(normalized.at("returns_pct").at("5d").is_null(),
                "missing reference remains null");

        auto restructuring = tdx::Json::object();
        restructuring["$ZQDM"] = "600963";
        restructuring["$SC"] = "";
        restructuring["DGDCGBL"] = "28.8100";
        restructuring["JLR"] = "1291.83";
        restructuring["ZBYZ"] = "定增募资";
        restructuring["XXSM"] = "剥离亏损资产，注入盈利资产";
        restructuring["SKR"] = "国资委";
        restructuring["KGBL"] = "20.88";
        restructuring["DATE"] = "20260331";
        auto restructuring_rows = tdx::Json::array();
        restructuring_rows.push_back(restructuring);
        const auto normalized_restructuring =
            tdx::normalize_state_owned_restructuring(
                restructuring_rows, tdx::Json::array(), securities);
        require(normalized_restructuring.size() == 1, "restructuring count");
        const auto& reform = normalized_restructuring.as_array()[0];
        require(reform.at("security").at("market").as_string() == "sh",
                "blank market inference");
        require(reform.at("as_of_date").as_string() == "2026-03-31", "date");
        require(reform.at("net_profit_10k_yuan").as_number() == 1291.83,
                "profit unit");

        std::cout << "state-owned reform tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "state-owned reform test failed: " << error.what() << '\n';
        return 1;
    }
}
