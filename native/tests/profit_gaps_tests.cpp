#include "tdx/profit_gaps.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

int main() {
    try {
        tdx::BlockData blocks;
        blocks.securities[{1, "600521"}] =
            tdx::Security{1, "SH", "上海", "600521", "华海药业"};
        tdx::Json rows = tdx::Json::array();
        rows.push_back(tdx::Json::parse(
            "{\"SetCode\":\"1\",\"Code\":\"600521\",\"Name\":\"\","
            "\"Date\":\"20260706\",\"Dchzf%\":\"7.59\","
            "\"Bglx\":\"60000\",\"Aqf\":\"79\"}"));
        rows.push_back(tdx::Json::parse(
            "{\"SetCode\":\"0\",\"Code\":\"300191\",\"Name\":\"潜能恒信\","
            "\"Date\":\"20260806\",\"Dchzf%\":\"7.59\","
            "\"Bglx\":\"6\",\"Aqf\":\"72\"}"));
        const auto result = tdx::normalize_profit_gap_rows(rows, blocks);
        if (result.size() != 2) throw tdx::Error("profit-gap row count failed");
        const auto& first = result.as_array()[0];
        if (first.at("security").at("security_id").as_string() != "SH600521" ||
            first.at("security").at("name").as_string() != "华海药业" ||
            first.at("event_date").as_string() != "2026-07-06" ||
            first.at("disclosure").at("kind").as_string() != "forecast" ||
            first.at("disclosure").at("report_period").as_string() != "half-year" ||
            std::abs(first.at("gap_change_pct").as_number() - 7.59) > 1e-9)
            throw tdx::Error("profit-gap normalization failed");
        const auto& second = result.as_array()[1];
        if (second.at("disclosure").at("kind").as_string() != "report" ||
            second.at("disclosure").at("name").as_string() != "半年度报告")
            throw tdx::Error("profit-gap disclosure mapping failed");
        std::cout << "profit gaps tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
