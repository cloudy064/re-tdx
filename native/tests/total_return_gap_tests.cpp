#include "tdx/total_return_gap.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

int main() {
    try {
        tdx::Json rows = tdx::Json::array();
        rows.push_back(tdx::Json::parse(
            "{\"startDate\":\"20260105\",\"endDate\":\"20260806\","
            "\"basicCode\":\"000009\",\"basicSet\":\"1\","
            "\"basicAbbre\":\"上证380\",\"basicPricePreZone\":\"6588.100\","
            "\"basicPriceZone\":\"6856.030\",\"basicZDFZone\":\"4.067%\","
            "\"wholeCode\":\"H00009\",\"wholeSet\":\"62\","
            "\"wholeAbbre\":\"上证380全收益\\n\","
            "\"wholePricePreZone\":\"8882.756\",\"wholePriceZone\":\"9350.093\","
            "\"wholeZDFZone\":\"5.261%\",\"zoneZDFGap\":\"1.194%\"}"));
        const auto result = tdx::normalize_total_return_gap_rows(rows);
        if (result.size() != 1) throw tdx::Error("total-return-gap row count failed");
        const auto& row = result.as_array()[0];
        if (row.at("price_index").at("index_id").as_string() != "SH000009" ||
            row.at("total_return_index").at("index_id").as_string() != "62:H00009" ||
            row.at("total_return_index").at("name").as_string() != "上证380全收益" ||
            row.at("start_date").as_string() != "2026-01-05" ||
            std::abs(row.at("price_index_performance").at("return_pct").as_number() -
                     4.067) > 1e-9 ||
            std::abs(row.at("total_return_advantage_pct").as_number() - 1.194) > 1e-9)
            throw tdx::Error("total-return-gap normalization failed");
        std::cout << "total return gap tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
