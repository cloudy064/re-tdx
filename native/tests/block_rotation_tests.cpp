#include "tdx/block_rotation.hpp"

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
        require(tdx::block_rotation_resource_for("industry") ==
                    "list/func_bkld101_1.jsn" &&
                tdx::block_rotation_resource_for("concept") ==
                    "list/func_bkld102_1.jsn" &&
                tdx::block_rotation_resource_for("region") ==
                    "list/func_bkld103_1.jsn" &&
                tdx::block_rotation_resource_for("style") ==
                    "list/func_bkld104_1.jsn",
                "four client branches must map to their exact resources");

        tdx::BlockData blocks;
        blocks.blocks.push_back(tdx::Block{
            "industry:T0102", "industry", "通达信行业", "880305", "电力",
            "T0102", "industry:T01", 2, false, std::nullopt, 85, "", "", "fixture"});
        blocks.blocks.push_back(tdx::Block{
            "style:880836", "style", "风格板块", "880836", "高市净率",
            "880836", "", 1, true, 12, 12, "", "", "fixture"});
        blocks.securities[{1, "880217"}] =
            tdx::Security{1, "SH", "上海", "880217", "山西板块"};

        auto raw = tdx::Json::array();
        raw.push_back(tdx::Json::parse(
            R"({"$SC":"1","$ZQDM":"880305","date":"20260805","ts":"2","pl":"8.31","yd_1w":"4","zyd_1w":"3","dyd_1w":"1","zf_1w":"2.5","yd_1m":"10","zyd_1m":"4","dyd_1m":"6","zf_1m":"-1.2","yd_3m":"20","zyd_3m":"10","dyd_3m":"10","zf_3m":"","yd_1y":"40","zyd_1y":"21","dyd_1y":"19","zf_1y":"9.4"})"));
        raw.push_back(tdx::Json::parse(
            R"({"$SC":"1","$ZQDM":"880217","date":"20260730","ts":"6","pl":"3.5","yd_1w":"1","zyd_1w":"0","dyd_1w":"1","zf_1w":"-3","yd_1m":"8","zyd_1m":"2","dyd_1m":"6","zf_1m":"-5","yd_3m":"","zyd_3m":"","dyd_3m":"","zf_3m":"","yd_1y":"20","zyd_1y":"8","dyd_1y":"12","zf_1y":"-7"})"));
        auto industry = tdx::normalize_block_rotation_rows(raw, "industry", blocks);
        const auto& first = industry.as_array()[0];
        require(first.at("block").at("name").as_string() == "电力" &&
                    first.at("block").at("block_id").as_string() == "industry:T0102" &&
                    first.at("block").at("member_count").as_number() == 85 &&
                    std::abs(first.at("cycle_gap_days").as_number() - 6.31) < 0.000001 &&
                    first.at("periods").at("1w").at("direction_imbalance").as_number() == 2 &&
                    first.at("periods").at("3m").at("return_pct").is_null() &&
                    first.at("raw").is_object(),
                "normalizer must preserve local block identity, client formula, periods and raw row");

        auto region_raw = tdx::Json::array();
        region_raw.push_back(raw.as_array()[1]);
        const auto region = tdx::normalize_block_rotation_rows(region_raw, "region", blocks);
        require(region.as_array()[0].at("block").at("name").as_string() == "山西板块" &&
                    region.as_array()[0].at("block").at("block_id").is_null() &&
                    !region.as_array()[0].at("block").at("members_available").as_bool(),
                "region name may resolve from the index directory without fabricating local membership");

        auto style_raw = tdx::Json::array();
        style_raw.push_back(tdx::Json::parse(
            R"({"$SC":"","$ZQDM":"880836","date":"","ts":"0","pl":"120.5","yd_1w":"0","zyd_1w":"0","dyd_1w":"0","zf_1w":""})"));
        const auto style = tdx::normalize_block_rotation_rows(style_raw, "style", blocks);
        require(style.as_array()[0].at("block").at("name").as_string() == "高市净率" &&
                    style.as_array()[0].at("block").at("security_id").is_null(),
                "blank upstream market must remain explicit while local block identity still resolves");

        tdx::sort_block_rotation_rows(industry, "anomalies", "1w", "desc");
        require(industry.as_array()[0].at("block").at("code").as_string() == "880305",
                "period anomaly sort must use the selected period");
        tdx::sort_block_rotation_rows(industry, "last-date", "1w", "asc");
        require(industry.as_array()[0].at("block").at("code").as_string() == "880217",
                "date sort should be stable and chronological");

        bool rejected = false;
        try { tdx::sort_block_rotation_rows(industry, "unknown", "1w", "desc"); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, "unknown sort must be rejected");

        std::cout << "block-rotation tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "block-rotation test failed: " << error.what() << '\n';
        return 1;
    }
}
