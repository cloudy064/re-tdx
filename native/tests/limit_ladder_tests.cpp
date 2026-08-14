#include "tdx/limit_ladder.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() {
    try {
        tdx::BlockData blocks;
        blocks.blocks.push_back(tdx::Block{
            "research-industry:X1203", "research-industry", "研究行业",
            "881026", "化学原料", "X1203", "research-industry:X12", 2,
            false, std::nullopt, 81, "", "", "fixture"});
        blocks.blocks.push_back(tdx::Block{
            "concept:880978", "concept", "概念板块", "880978", "DeepSeek",
            "880978", "", 1, true, 774, 774, "", "", "fixture"});

        auto raw = tdx::Json::array();
        raw.push_back(tdx::Json::parse(
            R"({"$ZQDM":"881026","$SC":"1","mc":"*化学原料","rq":"20260806","ztjs":"2","zbs":"1","zrzt":"1","lbs":"1","lbgd":"3","hzgd":"4","lbjjl":"100.00"})"));
        raw.push_back(tdx::Json::parse(
            R"({"$ZQDM":"880978","$SC":"1","mc":"DeepSeek概念","rq":"20260806","ztjs":"9","zbs":"3","zrzt":"12","lbs":"2","lbgd":"2","hzgd":"11","lbjjl":"16.67"})"));
        raw.push_back(tdx::Json::parse(
            R"({"$ZQDM":"880999","$SC":"1","mc":"测试概念","rq":"20260806","ztjs":"0","zbs":"1","zrzt":"","lbs":"0","lbgd":"","hzgd":"","lbjjl":""})"));
        const auto normalized = tdx::normalize_limit_ladder_rows(raw, blocks);
        require(normalized.size() == 3, "all valid block rows must survive");
        const auto& industry = normalized.as_array()[0];
        require(industry.at("block").at("category").as_string() == "industry" &&
                    industry.at("block").at("name").as_string() == "化学原料" &&
                    industry.at("block").at("member_count").as_number() == 81 &&
                    industry.at("max_streak_height").as_number() == 3 &&
                    industry.at("sum_streak_heights").as_number() == 4 &&
                    industry.at("advancement_rate_formula_matches").as_bool() &&
                    industry.at("raw").is_object(),
                "industry identity, member boundary, height and formula must be preserved");
        const auto& concept = normalized.as_array()[1];
        require(concept.at("block").at("category").as_string() == "concept" &&
                    concept.at("block").at("name").as_string() == "DeepSeek" &&
                    std::abs(concept.at("calculated_advancement_rate_pct").as_number() -
                             16.67) < 0.001 &&
                    concept.at("advancement_rate_formula_matches").as_bool(),
                "concept mapping and lbs/zrzt advancement formula must match");
        require(normalized.as_array()[2].at("block").at("name").as_string() ==
                    "测试概念" &&
                    normalized.as_array()[2].at("advancement_rate_formula_matches").is_null(),
                "raw name fallback and missing formula must remain explicit");

        auto sorted = normalized;
        tdx::sort_limit_ladder_rows(sorted, "total-height", "desc");
        require(sorted.as_array()[0].at("block").at("code").as_string() == "880978",
                "client default total-height ordering must be reproducible");
        tdx::sort_limit_ladder_rows(sorted, "advancement-rate", "desc");
        require(sorted.as_array()[0].at("block").at("code").as_string() == "881026",
                "advancement-rate ordering must prefer present high values");

        bool rejected = false;
        try { tdx::sort_limit_ladder_rows(sorted, "unknown", "desc"); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, "unknown sort must be rejected");

        std::cout << "limit-ladder tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "limit-ladder test failed: " << error.what() << '\n';
        return 1;
    }
}
