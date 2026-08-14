#include "tdx/futures_issuance.hpp"

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
        tdx::Json futures = tdx::Json::array();
        tdx::Json contract = tdx::Json::object();
        contract["SPQH"] = "聚丙烯";
        contract["$ZQDM"] = "PPL8";
        contract["$SC"] = "29";
        contract["DATE"] = "20260730";
        contract["zdf5"] = "-1.25";
        contract["ccl"] = "472168.00";
        futures.push_back(std::move(contract));
        const auto normalized = tdx::normalize_futures_contract_rows(futures);
        require(normalized.as_array().front().at("contract_key").as_string() == "29PPL8",
                "futures contract key");
        require(std::abs(normalized.as_array().front().at("return_5d_pct").as_number() + 1.25) < 1e-9,
                "futures numeric conversion");

        std::map<std::pair<int, std::string>, tdx::Security> securities;
        tdx::Security security;
        security.market_id = 0;
        security.market = "sz";
        security.code = "301683";
        security.name = "海科新源";
        securities.emplace(std::make_pair(0, security.code), security);
        tdx::Json stocks = tdx::Json::array();
        tdx::Json stock = tdx::Json::object();
        stock["$SC"] = "0";
        stock["$ZQDM"] = "301683";
        stock["ssrq"] = "20260401";
        stock["mjzj"] = "123676.5900";
        stock["ssfqj"] = "77.280";
        stocks.push_back(std::move(stock));
        const auto ipo = tdx::normalize_ipo_security_rows(stocks, securities);
        require(ipo.as_array().front().at("security").at("security_id").as_string() == "SZ301683",
                "IPO security mapping");
        require(ipo.as_array().front().at("security").at("name").as_string() == "海科新源",
                "IPO security name");
        require(std::abs(ipo.as_array().front().at("issue_price").as_number() - 77.28) < 1e-9,
                "IPO issue price");

        tdx::Json placements = tdx::Json::array();
        tdx::Json placement = tdx::Json::object();
        placement["$SC"] = "1";
        placement["$ZQDM"] = "603076";
        placement["date1"] = "20260311";
        placement["date2"] = "20260327";
        placement["date3"] = "20260801";
        placement["fajd"] = "注册生效";
        placement["fxgm"] = "3621.0403";
        placement["mzje"] = "35000.0000";
        placement["price1"] = "19.780";
        placement["hy"] = "工业机械";
        placement["hzgg"] = "https://example.test/announcement.pdf";
        placement["fxxq"] = "发行对象与募集投向";
        placements.push_back(std::move(placement));
        const auto registered = tdx::normalize_private_placement_rows(
            placements, "list/func_qxfa601_1.jsn", "registered", securities);
        const auto& registered_row = registered.as_array().front();
        require(registered_row.at("lifecycle_label").as_string() == "注册生效" &&
                registered_row.at("dates").at("registered").as_string() == "20260801",
                "private placement registration lifecycle");
        require(std::abs(registered_row.at("expected_raise_10k_yuan").as_number() - 35000.0) < 1e-9 &&
                registered_row.at("actual_net_10k_yuan").is_null(),
                "private placement expected versus actual units");

        tdx::Json locked_rows = tdx::Json::array();
        tdx::Json locked = tdx::Json::object();
        locked["$SC"] = "0";
        locked["$ZQDM"] = "002939";
        locked["date1"] = "20220822";
        locked["date2"] = "20270822";
        locked["date3"] = "20220413";
        locked["date4"] = "20220819";
        locked["price1"] = "8.679";
        locked["price2"] = "10.000";
        locked["fxzl"] = "100";
        locked["fxhgb"] = "400";
        locked_rows.push_back(std::move(locked));
        const auto normalized_locked = tdx::normalize_private_placement_rows(
            locked_rows, "list/func_qxfa201_1.jsn", "implemented-locked", securities);
        const auto& locked_row = normalized_locked.as_array().front();
        require(locked_row.at("dates").at("unlock").as_string() == "20270822" &&
                std::abs(locked_row.at("issue_to_post_shares_pct").as_number() - 25.0) < 1e-9,
                "private placement locked dates and dilution ratio");
        require(locked_row.at("lock_period_return_pct").is_number(),
                "private placement lock-period derived return");

        tdx::Json rights_rows = tdx::Json::array();
        rights_rows.push_back(tdx::Json::parse(
            R"({"$ZQDM":"000049","$SC":"0","ggrq":"20231127","dm":"080049","jc":"德赛A1配","bl":"3.0000","jg":"21.1600","sl":"89816058.0000","zj":"1900507787.0000","qsjkr":"20231130","jzjkr":"20231206","djr":"20231129","cqr":"20231208","dgd":"现金全额认购"})"));
        const auto implemented_rights = tdx::normalize_rights_offering_rows(
            rights_rows, "list/func_qxfa107_1.jsn", "implemented", securities);
        const auto& rights = implemented_rights.as_array().front();
        require(rights.at("security").at("security_id").as_string() ==
                    "SZ000049" &&
                rights.at("phase_label").as_string() == "配股实施" &&
                rights.at("stage").as_string() == "配股实施" &&
                rights.at("amount_semantics").as_string() == "actual" &&
                std::abs(rights.at("rights_per_10_shares").as_number() - 3.0) <
                    1e-9 &&
                std::abs(rights.at("raised_yuan").as_number() - 1900507787.0) <
                    0.01 && rights.at("raw").is_object(),
                "implemented rights offering must preserve dates, units and raw evidence");

        tdx::Json abnormal_rows = tdx::Json::array();
        abnormal_rows.push_back(tdx::Json::parse(
            R"({"$ZQDM":"600153","$SC":"1","ggrq":"20230429","jd":"已终止","hzr":"20230523","bl":"3.5000","sl":"1051424968.0000","zj":"4980000000.0000","fxxq":"配股预案与募资投向"})"));
        const auto abnormal_rights = tdx::normalize_rights_offering_rows(
            abnormal_rows, "list/func_qxfa109_1.jsn", "abnormal", securities);
        require(abnormal_rights.as_array().front().at("stage").as_string() ==
                    "已终止" &&
                abnormal_rights.as_array().front().at("sort_date").as_string() ==
                    "20230523" &&
                abnormal_rights.as_array().front().at("amount_semantics").as_string() ==
                    "planned",
                "abnormal rights offering must use progress status/date and planned units");

        tdx::Security preferred_underlying;
        preferred_underlying.market_id = 1;
        preferred_underlying.market = "sh";
        preferred_underlying.code = "600998";
        preferred_underlying.name = "九州通";
        securities.emplace(std::make_pair(1, preferred_underlying.code),
                           preferred_underlying);
        tdx::Json preferred_rows = tdx::Json::array();
        preferred_rows.push_back(tdx::Json::parse(
            R"({"$ZQDM":"600998","$SC":"1","dm":"360047","jc":"九州优","ssrq":"20240924","mgmz":"100.0000","fxjg":"100.0000","fxsl":"1790.0000","fxgm":"17.9000","fxfs":"向特定对象发行","cspmgxl":"5.0000","sflj":"是","sftx":"是","nfxcs":"1","zffs":"现金"})"));
        const auto preferred =
            tdx::normalize_preferred_share_rows(preferred_rows, securities);
        const auto& preferred_row = preferred.as_array().front();
        require(preferred_row.at("kind").as_string() == "preferred-share" &&
                preferred_row.at("underlying_security").at("security_id").as_string() ==
                    "SH600998" &&
                preferred_row.at("underlying_security").at("name").as_string() ==
                    "九州通" &&
                preferred_row.at("preferred_code").as_string() == "360047",
                "preferred-share identity and underlying security mapping");
        require(std::abs(preferred_row.at("issue_shares").as_number() -
                         17900000.0) < 0.01 &&
                std::abs(preferred_row.at("issue_size_yuan").as_number() -
                         1790000000.0) < 0.01 &&
                std::abs(preferred_row.at("initial_dividend_yield_pct").as_number() -
                         5.0) < 1e-9 &&
                preferred_row.at("raw").is_object(),
                "preferred-share CFG units and raw evidence");
        std::cout << "futures/issuance tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
