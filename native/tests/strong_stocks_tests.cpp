#include "tdx/strong_stocks.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

tdx::Json interval_fixture(const char* code, const char* market,
                           const char* interval, const char* statistics,
                           const char* stock_return, const char* index_return) {
    auto row = tdx::Json::object();
    row["$ZQDM1"] = code;
    row["$SC1"] = market;
    row["sj1"] = "20260729";
    row["sj2"] = "20260806";
    row["jtjb"] = statistics;
    row["zf1"] = stock_return;
    row["zf2"] = index_return;
    row["$ZQDM"] = interval;
    return row;
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "002827"}] = {0, "SZ", "深圳", "002827", "高争民爆"};
        securities[{1, "600000"}] = {1, "SH", "上海", "600000", "浦发银行"};

        auto raw = tdx::Json::array();
        raw.push_back(interval_fixture("002827", "0", "002827260729260806",
                                       "6天4板", "", ""));
        raw.push_back(interval_fixture("600000", "1", "600000260729260806",
                                       "8天6板", "80.50", "1.25"));
        auto intervals = tdx::normalize_strong_stock_intervals(raw, securities);
        require(intervals.size() == 2, "interval row count");
        const auto& first = intervals.as_array().front();
        require(first.at("security").at("name").as_string() == "高争民爆",
                "security name resolved");
        require(first.at("start_date").as_string() == "2026-07-29",
                "start date normalized");
        require(first.at("trading_days").as_number() == 6,
                "trading days parsed");
        require(first.at("limit_up_days").as_number() == 4,
                "limit-up days parsed");
        require(first.at("stock_return_pct").is_null() &&
                    !first.at("return_finalized").as_bool(),
                "pending return retained as null");
        const auto& second = intervals.as_array()[1];
        require(std::abs(second.at("excess_return_pct").as_number() - 79.25) < 1e-9,
                "excess return calculated");

        auto detail_raw = tdx::Json::array();
        auto day = tdx::Json::object();
        day["sj"] = "20260729";
        day["$ZQDM"] = "002827";
        day["$SC"] = "0";
        day["zf1"] = "10.00";
        day["zcje"] = "65006112.00";
        day["yy"] = "矿山资产重组";
        day["ztjs"] = "86";
        day["zbjs"] = "18";
        day["dtjs"] = "9";
        day["zf2"] = "0.40";
        detail_raw.push_back(day);
        auto details = tdx::normalize_strong_stock_detail(detail_raw, first, securities);
        require(details.size() == 1, "detail row count");
        const auto& detail = details.as_array().front();
        require(detail.at("date").as_string() == "2026-07-29", "detail date");
        require(detail.at("limit_up_reason").as_string() == "矿山资产重组",
                "reason retained");
        require(std::abs(detail.at("market_seal_success_pct").as_number() -
                         82.6923076923077) < 1e-9,
                "client sealing formula reproduced");

        tdx::sort_strong_stock_rows(intervals, "intervals", "return", "desc");
        require(intervals.as_array().front().at("security").at("code").as_string() ==
                    "600000", "non-null interval return sorts first");
        tdx::sort_strong_stock_rows(details, "detail", "date", "asc");

        bool rejected = false;
        try {
            auto duplicate = raw;
            duplicate.push_back(raw.as_array().front());
            (void)tdx::normalize_strong_stock_intervals(duplicate, securities);
        } catch (const tdx::Error&) { rejected = true; }
        require(rejected, "duplicate interval rejected");

        std::cout << "strong-stocks tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "strong-stocks test failed: " << error.what() << '\n';
        return 1;
    }
}
