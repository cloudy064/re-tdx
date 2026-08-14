#include "tdx/ratings.hpp"

#include "tdx/common.hpp"

#include <iostream>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json row(std::initializer_list<std::pair<std::string, tdx::Json>> fields) {
    auto result = tdx::Json::object();
    for (const auto& [key, value] : fields) result[key] = value;
    return result;
}

}  // namespace

int main() {
    try {
        require(tdx::classify_rating_stance("强烈推荐") == "positive",
                "positive rating classification failed");
        require(tdx::classify_rating_stance("大市同步") == "neutral",
                "neutral rating classification failed");
        require(tdx::classify_rating_stance("跑输大市") == "negative",
                "negative rating classification failed");
        require(tdx::classify_rating_stance("Positive") == "positive" &&
                    tdx::classify_rating_stance("Underperform") == "negative" &&
                    tdx::classify_rating_stance("Hold") == "neutral",
                "English rating classification failed");

        auto hk_rows = tdx::Json::array();
        hk_rows.push_back(row({
            {"$ZQDM", "00700"}, {"$SC", "31"}, {"ZXRQ", "20260731"},
            {"YJJG", "中银国际"}, {"ZXPJ", "买入"}, {"ZXMBJ", "655.00"},
            {"JGSL", "35"}, {"MBJ", "704.375"}}));
        const auto hk = tdx::normalize_hong_kong_rating_rows(
            hk_rows, {{"00700", "腾讯控股"}});
        require(hk.size() == 1, "Hong Kong master row was not normalized");
        require(hk.as_array()[0].at("security").at("name").as_string() == "腾讯控股",
                "Hong Kong name was not resolved");
        require(hk.as_array()[0].at("latest_target_price_hkd").as_number() == 655.0,
                "Hong Kong target price was not parsed");

        auto us_rows = tdx::Json::array();
        us_rows.push_back(row({
            {"$ZQDM", "abnb"}, {"$SC", "74"}, {"ZQJC", "爱彼迎"},
            {"YJRQ", "20260807"}, {"PJJG", "Wedbush"},
            {"ZXPJ", "Outperform"}, {"ZXMBJ", "200.00"},
            {"JGS", "37"}, {"hy", "软件服务"}}));
        const auto us = tdx::normalize_united_states_rating_rows(us_rows);
        require(us.size() == 1 &&
                    us.as_array()[0].at("security").at("code").as_string() == "ABNB" &&
                    us.as_array()[0].at("latest_target_price_usd").as_number() == 200.0 &&
                    us.as_array()[0].at("stance").as_string() == "positive",
                "United States master row was not normalized");

        auto industry_rows = tdx::Json::array();
        industry_rows.push_back(row({
            {"$ZQDM", "881001"}, {"$SC", "1"}, {"ZXRQ", "20260804"},
            {"YJJG", "大同证券"}, {"ZXPJ", "增持"}, {"PJBH", "未知"},
            {"JGSL", "481"}, {"KDS", "449"}, {"KKS", "16"}}));
        const auto industries = tdx::normalize_industry_rating_rows(
            industry_rows, {{"881001", "煤炭"}});
        require(industries.size() == 1, "industry master row was not normalized");
        require(industries.as_array()[0].at("industry").at("name").as_string() == "煤炭",
                "industry name was not resolved");
        require(industries.as_array()[0].at("six_month_unclassified_count").as_number() == 16.0,
                "industry unclassified count is incorrect");

        auto report_rows = tdx::Json::array();
        report_rows.push_back(row({
            {"$ZQDM1", "00700"}, {"BGRQ", "20260731"},
            {"YJJG", "中银国际"}, {"ZXPJ", "买入"}, {"SCPJ", "买入"},
            {"MBJ", "655.00"}, {"yy", "维持目标价及买入评级"}, {"$ZQDM", "1"}}));
        const auto reports = tdx::normalize_rating_report_rows(report_rows, "hong-kong");
        require(reports.size() == 1, "rating report was not normalized");
        require(reports.as_array()[0].at("target_price_hkd").as_number() == 655.0,
                "report target price was not parsed");
        require(!reports.as_array()[0].at("rating_changed").as_bool(),
                "unchanged rating was marked changed");

        auto us_report_rows = tdx::Json::array();
        us_report_rows.push_back(row({
            {"$ZQDM", "ABNB"}, {"$SC", "74"}, {"YJRQ", "20260807"},
            {"PJJG", "韦德布什"}, {"ZXPJ", "跑赢大市"},
            {"SCPJ", "中性"}, {"TZLX", "上调"},
            {"ZXMBJ", "200.00"}, {"QCJ", "152.00"}}));
        const auto us_reports = tdx::normalize_rating_report_rows(us_report_rows, "us");
        require(us_reports.size() == 1 &&
                    us_reports.as_array()[0].at("report_date").as_string() == "20260807" &&
                    us_reports.as_array()[0].at("rating_change").as_string() == "上调" &&
                    us_reports.as_array()[0].at("target_price_usd").as_number() == 200.0 &&
                    us_reports.as_array()[0].at("initial_price_usd").as_number() == 152.0,
                "United States rating report was not normalized");

        std::cout << "ratings tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
