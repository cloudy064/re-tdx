#include "tdx/index_volatility.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

int main() {
    try {
        tdx::BlockData blocks;
        blocks.securities[{1, "999999"}] =
            tdx::Security{1, "SH", "上海", "999999", "上证指数"};
        tdx::Json catalog = tdx::Json::array();
        catalog.push_back(tdx::Json::parse(
            "{\"Code\":\"999999\",\"Market\":\"1\","
            "\"MeanStdOfLastTwoYears\":\"0.843964\","
            "\"MeanStdOfLastOneYear\":\"0.829516\","
            "\"MeanStdOfLastHalfYear\":\"0.991565\","
            "\"MeanStdOfLastQuarter\":\"1.128640\","
            "\"QuantileOfMQO2Y\":\"78.727631\","
            "\"QuantileOfMQO1Y\":\"76.587303\","
            "\"QuantileOfMQOhY\":\"63.492065\"}"));
        const auto normalized_catalog =
            tdx::normalize_index_volatility_catalog_rows(catalog, 5, blocks);
        const auto& first = normalized_catalog.as_array()[0];
        if (first.at("index").at("index_id").as_string() != "SH999999" ||
            first.at("index").at("name").as_string() != "上证指数" ||
            first.at("window_days").as_number() != 5 ||
            std::abs(first.at("mean_realized_volatility_pct").at("two_years").as_number() -
                     0.843964) > 1e-9 ||
            std::abs(first.at("current_quarter_mean_percentile_pct")
                         .at("within_half_year").as_number() - 63.492065) > 1e-9)
            throw tdx::Error("index-volatility catalog normalization failed");

        tdx::Json history = tdx::Json::array();
        history.push_back(tdx::Json::parse(
            "{\"Date\":\"20260806\",\"RealizedVolatility\":\"0.857203\"}"));
        history.push_back(tdx::Json::parse(
            "{\"Date\":\"20260805\",\"RealizedVolatility\":\"0.812345\"}"));
        const auto normalized_history = tdx::normalize_index_volatility_history_rows(history);
        if (normalized_history.size() != 2 ||
            normalized_history.as_array()[0].at("date").as_string() != "2026-08-05" ||
            std::abs(normalized_history.as_array()[1].at("realized_volatility_pct").as_number() -
                     0.857203) > 1e-9)
            throw tdx::Error("index-volatility history normalization failed");
        std::cout << "index volatility tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
