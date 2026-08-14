#include "tdx/thematic_opportunities.hpp"
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
        securities[{0, "000777"}] = {0, "SZ", "深圳", "000777", "中核科技"};
        securities[{1, "601727"}] = {1, "SH", "上海", "601727", "上海电气"};

        auto groups = tdx::Json::array();
        groups.push_back(tdx::Json::parse(R"({
          "$S_ZQDM":"1|601727,0|000777,0|000777","$ZQDM":"hy57",
          "DLHY":"能源装备","ZLHY":"IGCC技术"
        })"));
        const auto normalized = tdx::normalize_opportunity_groups(
            groups, "industry", securities);
        const auto& group = normalized.as_array().front();
        require(group.at("group_id").as_string() == "hy57" &&
                    group.at("type").as_string() == "industry" &&
                    group.at("name").as_string() == "IGCC技术" &&
                    group.at("category").as_string() == "能源装备" &&
                    group.at("raw_member_count").as_number() == 3 &&
                    group.at("member_count").as_number() == 2 &&
                    group.at("duplicate_member_count").as_number() == 1 &&
                    group.at("detail_resource").as_string() == "ydyl1/hy57.jsn",
                "opportunity-group master hierarchy and membership normalization");

        auto legacy_groups = tdx::Json::array();
        legacy_groups.push_back(tdx::Json::parse(R"({
          "$S_ZQDM":"1|600328,1|601106,1|601727","$ZQDM":"299",
          "flname":"内容应用","flzl":"快中子反应堆"
        })"));
        const auto normalized_legacy = tdx::normalize_opportunity_groups(
            legacy_groups, "legacy-client-theme", securities);
        const auto& legacy_group = normalized_legacy.as_array().front();
        require(legacy_group.at("group_id").as_string() == "299" &&
                    legacy_group.at("type").as_string() ==
                        "legacy-client-theme" &&
                    legacy_group.at("name").as_string() == "快中子反应堆" &&
                    legacy_group.at("category").as_string() == "内容应用" &&
                    legacy_group.at("page_declared_name").as_string() ==
                        "虚拟现实" &&
                    legacy_group.at("semantic_mismatch").as_bool() &&
                    legacy_group.at("detail_resource").as_string() ==
                        "xnxs/299.jsn" &&
                    legacy_group.at("source_resource").as_string() ==
                        "list/func_xnxs101_1.jsn",
                "legacy theme must retain the current row name and expose the page-label conflict");

        auto details = tdx::Json::array();
        details.push_back(tdx::Json::parse(R"({
          "$SC":"0","$ZQDM":"000777","JSYSP":"19.63","NZJSP":"25.74",
          "TZLJ":"公司主营工业用阀门设计、制造、销售等",
          "XXSM":"公司主营工业用阀门设计、制造、销售等。","lzcs":"0"
        })"));
        const auto detail_rows = tdx::normalize_opportunity_group_details(
            details, securities);
        const auto& detail = detail_rows.as_array().front();
        require(detail.at("security").at("name").as_string() == "中核科技" &&
                    std::abs(detail.at("three_month_adjusted_close").as_number() -
                             19.63) < 1e-9 &&
                    std::abs(detail.at("year_start_adjusted_close").as_number() -
                             25.74) < 1e-9 &&
                    detail.at("logic").as_string().find("工业用阀门") !=
                        std::string::npos,
                "opportunity detail reference prices and investment logic");

        auto legacy_details = tdx::Json::array();
        legacy_details.push_back(tdx::Json::parse(R"({
          "$SC":"1","$ZQDM":"601727","T005":"突破了快堆复杂系统设备集成的技术瓶颈。",
          "fqprice_d3":"6.97","fqprice_d5":"6.90","fqprice_d20":"6.81",
          "price":"8.14","lzcs":"0","kd":"突破快堆设备集成技术瓶颈",
          "xq":"突破了快堆复杂系统设备集成的技术瓶颈。"
        })"));
        const auto legacy_detail_rows =
            tdx::normalize_legacy_client_theme_details(
                legacy_details, securities);
        const auto& legacy_detail = legacy_detail_rows.as_array().front();
        require(legacy_detail.at("security").at("name").as_string() ==
                    "上海电气" &&
                    legacy_detail.at("logic").as_string().find("快堆") !=
                        std::string::npos &&
                    std::abs(legacy_detail.at("reference_close_3d").as_number() -
                             6.97) < 1e-9 &&
                    std::abs(legacy_detail.at("reference_close_20d").as_number() -
                             6.81) < 1e-9 &&
                    std::abs(legacy_detail.at("three_month_adjusted_close").as_number() -
                             8.14) < 1e-9 &&
                    legacy_detail.at("year_start_adjusted_close").is_null() &&
                    legacy_detail.at("detail_variant").as_string() ==
                        "legacy-client-theme" && legacy_detail.at("raw").is_object(),
                "legacy theme detail must preserve its own reference-price and logic fields");

        auto completed = tdx::Json::array();
        completed.push_back(tdx::Json::parse(R"({
          "$SC":"0","$ZQDM":"880001","bkmc":"芯片","$SC1":"0",
          "$ZQDM1":"000777","gpmc":"中核科技","kssj":"20260601",
          "jssj":"20260731","jtjb":"42天11板","qjzf":"148.36",
          "czfx":"龙头带动板块炒作","gnid":"123"
        })"));
        const auto completed_rows = tdx::normalize_completed_hype_rows(
            completed, securities);
        const auto& completed_row = completed_rows.as_array().front();
        require(completed_row.at("status").as_string() == "completed" &&
                    completed_row.at("block_name").as_string() == "芯片" &&
                    completed_row.at("leader").at("security_id").as_string() ==
                        "SZ000777" &&
                    std::abs(completed_row.at("interval_return_pct").as_number() -
                             148.36) < 1e-9,
                "completed hype row keeps block, leader, interval and analysis");

        auto active = tdx::Json::array();
        active.push_back(tdx::Json::parse(R"({
          "$SC":"1","$ZQDM":"601727","gpmc":"上海电气",
          "kssj":"20260720","jssj":"20260807","jtjb":"15天5板",
          "ggzf":"68.5","zszf":"3.5"
        })"));
        const auto active_rows = tdx::normalize_active_hype_rows(active, securities);
        const auto& active_row = active_rows.as_array().front();
        require(active_row.at("status").as_string() == "active" &&
                    active_row.at("security").at("name").as_string() == "上海电气" &&
                    std::abs(active_row.at("relative_return_pct").as_number() - 65.0) <
                        1e-9,
                "active hype row preserves stock/index relative performance");

        std::cout << "thematic opportunity tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "thematic opportunity test failed: " << error.what() << '\n';
        return 1;
    }
}
