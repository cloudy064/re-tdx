#include "tdx/relative_valuation.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <map>
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
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{1, "000026"}] =
            tdx::Security{1, "sh", "上海", "000026", "上证国企"};
        const auto master = tdx::normalize_relative_valuation_master_rows(
            rows({
                tdx::Json::parse(
                    "{\"code\":\"000300\",\"market\":\"1\","
                    "\"statRatios\":\"1.20\",\"statQuantiles\":\"85\","
                    "\"maxRatio\":\"1.4\",\"minRatio\":\"0.8\"}"),
                tdx::Json::parse(
                    "{\"code\":\"000026\",\"market\":\"1\","
                    "\"statRatios\":\"0.89\",\"statQuantiles\":\"75.41\","
                    "\"maxRatio\":\"1.09\",\"minRatio\":\"0.70\"}")}),
            securities);
        require(master.size() == 2 &&
                    master.as_array()[0].at("security").at("code").as_string() ==
                        "000026" &&
                    master.as_array()[0].at("security").at("name").as_string() ==
                        "上证国企" &&
                    std::abs(master.as_array()[0].at("current_ratio").as_number() -
                             0.89) < 1e-12 &&
                    master.as_array()[0].at("relative_position").at("band")
                        .as_string() == "high" &&
                    master.as_array()[1].at("relative_position").at("band")
                        .as_string() == "very-high",
                "master rows resolve names, parse ratios and expose descriptive bands");

        const auto history = tdx::normalize_relative_valuation_history_rows(
            rows({
                tdx::Json::parse("{\"date\":\"20260805\",\"ratio\":\"0.886\"}"),
                tdx::Json::parse("{\"date\":\"20240806\",\"ratio\":\"0.9019\"}"),
                tdx::Json::parse("{\"date\":\"20250102\",\"ratio\":\"-\"}")}));
        require(history.size() == 3 &&
                    history.as_array()[0].at("date").as_string() == "2024-08-06" &&
                    std::abs(history.as_array().back().at("ratio").as_number() -
                             0.886) < 1e-12 &&
                    history.as_array()[1].at("ratio").is_null(),
                "history rows sort dates and preserve missing ratios as null");

        bool duplicate_rejected = false;
        try {
            (void)tdx::normalize_relative_valuation_history_rows(rows({
                tdx::Json::parse("{\"date\":\"20260805\",\"ratio\":\"1\"}"),
                tdx::Json::parse("{\"date\":\"20260805\",\"ratio\":\"2\"}")}));
        } catch (const tdx::Error&) { duplicate_rejected = true; }
        require(duplicate_rejected, "duplicate history dates are rejected");

        const auto internal_market = tdx::normalize_relative_valuation_master_rows(rows({
            tdx::Json::parse("{\"code\":\"000171\",\"market\":\"62\"}")}));
        require(internal_market.as_array()[0].at("security").at("market").as_string() ==
                    "tdx-62" &&
                    internal_market.as_array()[0].at("security").at("security_id")
                        .as_string() == "TDX62:000171",
                "TDX internal index markets are preserved without exchange guessing");

        bool bad_market_rejected = false;
        try {
            (void)tdx::normalize_relative_valuation_master_rows(rows({
                tdx::Json::parse("{\"code\":\"000001\",\"market\":\"invalid\"}")}));
        } catch (const tdx::Error&) { bad_market_rejected = true; }
        require(bad_market_rejected, "invalid upstream markets are rejected");

        std::cout << "relative valuation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
