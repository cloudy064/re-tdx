#include "tdx/strategic_themes.hpp"
#include "tdx/common.hpp"

#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}
}

int main() {
    try {
        require(tdx::strategic_theme_categories().size() == 26,
                "strategic theme taxonomy must contain 26 client categories");
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000063"}] = {0, "SZ", "深圳", "000063", "中兴通讯"};
        securities[{1, "600050"}] = {1, "SH", "上海", "600050", "中国联通"};
        auto rows = tdx::Json::array();
        rows.push_back(tdx::Json::parse(R"({
          "gname":"5G概念","$S_ZQDM":"0 |000063,1 |600050,0 |000063",
          "S_NUM":"3","$ZQDM":"657"
        })"));
        const auto normalized = tdx::normalize_strategic_theme_master(
            rows, tdx::strategic_theme_categories()[8], securities);
        const auto& theme = normalized.as_array()[0];
        require(theme.at("theme_id").as_string() == "657" &&
                    theme.at("source_theme_id").as_string() == "657" &&
                    theme.at("categories").as_array()[0].as_string() == "5G6G" &&
                    theme.at("master_raw_member_count").as_number() == 3 &&
                    theme.at("master_member_count").as_number() == 2 &&
                    theme.at("master_duplicate_member_count").as_number() == 1 &&
                    theme.at("count_matches_raw").as_bool() &&
                    theme.at("members").as_array()[0].at("name").as_string() ==
                        "中兴通讯" &&
                    theme.at("detail_resource").as_string() == "zttzty/657.jsn",
                "master normalizer must preserve hierarchy, raw count and deduplication");

        auto legacy_rows = tdx::Json::array();
        legacy_rows.push_back(tdx::Json::parse(R"({
          "gname":"芯片","$S_ZQDM":"0|000063,1|600050","$ZQDM":"31"
        })"));
        const auto legacy = tdx::normalize_strategic_theme_master(
            legacy_rows, tdx::strategic_theme_categories().back(), securities);
        const auto& legacy_theme = legacy.as_array().front();
        require(legacy_theme.at("theme_id").as_string() == "HLW:31" &&
                    legacy_theme.at("source_theme_id").as_string() == "31" &&
                    !legacy_theme.at("declared_member_count_available").as_bool() &&
                    legacy_theme.at("master_declared_member_count").as_number() == 2 &&
                    legacy_theme.at("detail_resource").as_string() == "zttzty/31.jsn",
                "legacy Internet+ themes must be namespaced and derive missing S_NUM");

        auto detail_rows = tdx::Json::array();
        detail_rows.push_back(tdx::Json::parse(R"({
          "$SC":"0","$ZQDM":"000063","tzlj":"核心设备商","xxsm":"完整说明",
          "fqprice_d3":"42.1","fqprice_d5":"41.8","fqprice_d20":"39.0",
          "fqprice_d60":"35.0","price1":"33.0"
        })"));
        const auto details = tdx::normalize_strategic_theme_details(detail_rows, securities);
        const auto& detail = details.as_array()[0];
        require(detail.at("security").at("security_id").as_string() == "SZ000063" &&
                    detail.at("security").at("name").as_string() == "中兴通讯" &&
                    detail.at("logic").as_string() == "核心设备商" &&
                    detail.at("reference_prices").at("3m").as_string() == "33.0",
                "detail normalizer must bind security, logic and reference prices");

        bool rejected = false;
        try {
            auto duplicate = detail_rows;
            duplicate.push_back(detail_rows.as_array()[0]);
            (void)tdx::normalize_strategic_theme_details(duplicate, securities);
        } catch (const tdx::Error&) { rejected = true; }
        require(rejected, "duplicate detail securities must be rejected");

        std::cout << "strategic theme tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "strategic theme test failed: " << error.what() << '\n';
        return 1;
    }
}
