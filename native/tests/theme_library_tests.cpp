#include "tdx/theme_library.hpp"
#include "tdx/common.hpp"

#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}
}

int main() {
    try {
        require(tdx::theme_library_sources().size() == 5,
                "theme library must preserve five client source snapshots");
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "300759"}] = {0, "SZ", "深圳", "300759", "康龙化成"};
        securities[{1, "688710"}] = {1, "SH", "上海", "688710", "益诺思"};
        auto rows = tdx::Json::array();
        rows.push_back(tdx::Json::parse(R"({
          "name":"实验猴","$S_ZQDM":"0|300759,1|688710,0|300759",
          "ztgs":"1","dtgs":"0","zbs":"0","cjrq":"20260716",
          "gpsl":"3","zzts":"22","zttype":"化学制药",
          "ztms":"实验动物主题","$ZQDM":"3137"
        })"));
        const auto normalized = tdx::normalize_theme_library_rows(
            rows, tdx::theme_library_sources()[3], securities);
        const auto& theme = normalized.as_array().front();
        require(theme.at("record_id").as_string() == "general:3137" &&
                    theme.at("theme_id").as_string() == "3137" &&
                    theme.at("source").as_string() == "general" &&
                    theme.at("raw_member_count").as_number() == 3 &&
                    theme.at("member_count").as_number() == 2 &&
                    theme.at("duplicate_member_count").as_number() == 1 &&
                    theme.at("count_matches_raw").as_bool() &&
                    theme.at("members").as_array()[0].at("name").as_string() ==
                        "康龙化成" &&
                    theme.at("detail_resource").as_string() == "zttz/3137.jsn" &&
                    theme.at("chart_resource").as_string() == "zttz1/3137.jsn",
                "master normalizer must preserve source identity and member evidence");

        auto detail_rows = tdx::Json::array();
        detail_rows.push_back(tdx::Json::parse(R"({
          "$ZQDM":"300759","$SC":"0","fqprice_d3":"39.92",
          "fqprice_d5":"36.80","fqprice_d20":"33.94","fqprice_d60":"26.40",
          "FQPrice_M3":"27.90","lzcs":"2","glcd":"★★★",
          "Contents":"覆盖实验猴繁育与应用"
        })"));
        const auto details = tdx::normalize_theme_library_details(detail_rows, securities);
        const auto& detail = details.as_array().front();
        require(detail.at("security").at("security_id").as_string() == "SZ300759" &&
                    detail.at("reference_prices").at("3d").as_number() == 39.92 &&
                    detail.at("leader_count").as_number() == 2 &&
                    detail.at("relation_strength").as_string() == "★★★" &&
                    detail.at("description").as_string() == "覆盖实验猴繁育与应用",
                "detail normalizer must retain numeric references and inclusion evidence");

        auto chart_rows = tdx::Json::array();
        chart_rows.push_back(tdx::Json::parse(R"({"date":"20260715","jgzst":"1000.00"})"));
        chart_rows.push_back(tdx::Json::parse(R"({"date":"20260716","jgzst":"1021.50"})"));
        const auto chart = tdx::normalize_theme_library_chart(chart_rows);
        require(chart.size() == 2 &&
                    chart.as_array()[1].at("date").as_string() == "20260716" &&
                    chart.as_array()[1].at("value").as_number() == 1021.5,
                "chart normalizer must expose date/value points");

        bool rejected = false;
        try {
            auto duplicate = detail_rows;
            duplicate.push_back(detail_rows.as_array().front());
            (void)tdx::normalize_theme_library_details(duplicate, securities);
        } catch (const tdx::Error&) { rejected = true; }
        require(rejected, "duplicate dynamic detail securities must be rejected");

        std::cout << "theme library tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "theme library test failed: " << error.what() << '\n';
        return 1;
    }
}
