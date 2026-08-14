#include "tdx/commodity_links.hpp"
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
        securities[{0, "002167"}] = {0, "SZ", "深圳", "002167", "东方锆业"};
        securities[{0, "301580"}] = {0, "SZ", "深圳", "301580", "爱迪特"};
        securities[{1, "600397"}] = {1, "SH", "上海", "600397", "安源煤业"};
        securities[{1, "515220"}] = {1, "SH", "上海", "515220", "煤炭ETF"};

        auto commodity_raw = tdx::Json::array();
        auto commodity = tdx::Json::object();
        commodity["mc"] = "1/3焦煤";
        commodity["$ZQDM"] = "X100102003";
        commodity["zxjg"] = "1900";
        commodity["jjdw"] = "元/吨";
        commodity["yjhy"] = "炼焦煤";
        commodity["bjrq"] = "20260805";
        commodity["price0"] = "1875";
        commodity["price1"] = "1800";
        commodity["price2"] = "2000";
        commodity["price3"] = "0";
        commodity["price4"] = "1560";
        commodity["qdlj"] = "中高挥发分强粘结烟煤";
        commodity_raw.push_back(commodity);
        const auto commodities = tdx::normalize_commodity_rows(commodity_raw);
        require(commodities.size() == 1, "commodity count");
        const auto& normalized_commodity = commodities.as_array().front();
        require(normalized_commodity.at("quote_date").as_string() == "2026-08-05",
                "commodity date");
        require(std::abs(normalized_commodity.at("day_change_pct").as_number() -
                         1.33333333333333) < 1e-9,
                "day formula");
        require(std::abs(normalized_commodity.at("change_5d_pct").as_number() -
                         5.55555555555556) < 1e-9,
                "period formula");
        require(normalized_commodity.at("change_30d_pct").is_null(),
                "zero base remains null");

        auto theme_raw = tdx::Json::array();
        auto theme = tdx::Json::object();
        theme["name"] = "锆金属";
        theme["$S_ZQDM"] = "0|002167,0|301580";
        theme["date"] = "20170404";
        theme["qdlj"] = "锆产品供需逻辑";
        theme["date1"] = "20260622";
        theme["title"] = "日本东曹暂停供应高端氧化锆粉体";
        theme["$ZQDM"] = "70";
        theme["gldm"] = "X140404006";
        theme_raw.push_back(theme);
        const auto themes = tdx::normalize_price_theme_rows(theme_raw, securities);
        require(themes.size() == 1, "theme count");
        const auto& normalized_theme = themes.as_array().front();
        require(normalized_theme.at("stock_count").as_number() == 2,
                "theme stock set parsed");
        require(normalized_theme.at("stock_set").as_array().front()
                    .at("name").as_string() == "东方锆业",
                "theme stock name resolved");
        require(normalized_theme.at("latest_driver_date").as_string() == "2026-06-22",
                "latest driver date");

        auto stock_raw = tdx::Json::array();
        auto stock = tdx::Json::object();
        stock["$ZQDM"] = "002167";
        stock["$SC"] = "0";
        stock["CFJ"] = "17.740";
        stock["MS"] = "国内锆行业核心企业";
        stock_raw.push_back(stock);
        const auto theme_stocks = tdx::normalize_theme_stock_rows(stock_raw, securities);
        require(theme_stocks.as_array()[0].at("trigger_price").as_number() == 17.74,
                "theme trigger price");
        require(theme_stocks.as_array()[0].at("return_since_trigger_pct").is_null() &&
                    !theme_stocks.as_array()[0].at("current_quote_available").as_bool(),
                "host quote not fabricated");

        auto driver_raw = tdx::Json::array();
        auto driver = tdx::Json::object();
        driver["date"] = "20260622";
        driver["name"] = "日本东曹暂停供应高端氧化锆粉体";
        driver["$S_ZQDM"] = "0|002167,0|301580";
        driver["$ZQDM"] = "20028";
        driver_raw.push_back(driver);
        const auto drivers = tdx::normalize_theme_driver_rows(driver_raw, securities);
        require(drivers.as_array()[0].at("driver_id").as_string() == "20028",
                "event selection id");
        require(drivers.as_array()[0].at("stock_count").as_number() == 2,
                "event set count");

        const auto driver_stocks = tdx::normalize_driver_stock_rows(stock_raw, securities);
        require(driver_stocks.as_array()[0].at("driver_price").as_number() == 17.74,
                "driver price normalized");
        require(driver_stocks.as_array()[0].at("return_since_driver_pct").is_null(),
                "driver return not fabricated");

        auto commodity_stock_raw = tdx::Json::array();
        auto commodity_stock = tdx::Json::object();
        commodity_stock["$ZQDM"] = "600397";
        commodity_stock["$SC"] = "1";
        commodity_stock["price1"] = "14.350";
        commodity_stock["tzlj"] = "主营包含1/3焦煤";
        commodity_stock["xxsm"] = "1/3焦煤定义";
        commodity_stock_raw.push_back(commodity_stock);
        const auto commodity_stocks = tdx::normalize_commodity_stock_rows(
            commodity_stock_raw, securities);
        require(commodity_stocks.as_array()[0].at("reference_price_3m").as_number() == 14.35,
                "commodity reference price");
        require(commodity_stocks.as_array()[0].at("return_since_reference_pct").is_null(),
                "commodity security return not fabricated");

        auto related_raw = tdx::Json::array();
        auto related = tdx::Json::object();
        related["$ZQDM"] = "515220";
        related["$SC"] = "1";
        related_raw.push_back(related);
        const auto related_securities = tdx::normalize_commodity_security_rows(
            related_raw, securities);
        require(related_securities.as_array()[0].at("security").at("name").as_string() ==
                    "煤炭ETF", "related ETF resolved");
        require(!related_securities.as_array()[0].at("quote_fields_available").as_bool(),
                "related quote syscols absent");

        auto duplicates = commodity_raw;
        auto second_quote = commodity;
        second_quote["mc"] = "JM 焦煤";
        duplicates.push_back(second_quote);
        const auto duplicate_rows = tdx::normalize_commodity_rows(duplicates);
        require(duplicate_rows.size() == 2 &&
                    duplicate_rows.as_array()[0].at("quote_id").as_string() !=
                    duplicate_rows.as_array()[1].at("quote_id").as_string(),
                "shared commodity relation keeps distinct quote rows");

        std::cout << "commodity-links tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "commodity-links test failed: " << error.what() << '\n';
        return 1;
    }
}
