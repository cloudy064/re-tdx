#include "tdx/hk_events.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json one_row() {
    auto rows = tdx::Json::array();
    rows.push_back(tdx::Json::object());
    return rows;
}
}

int main() {
    try {
        auto dividends = one_row();
        auto& dividend = dividends.as_array().front();
        dividend["$SC"] = "31";
        dividend["$ZQDM"] = "00700";
        dividend["ZQJC"] = "腾讯控股";
        dividend["date1"] = "20260318";
        dividend["date2"] = "20251231";
        dividend["date3"] = "20260601";
        dividend["date4"] = "20260515";
        dividend["date5"] = "20260519";
        dividend["date6"] = "20260520";
        dividend["fags"] = "末期股息每股5.3港元";
        const auto dividend_rows = tdx::normalize_hk_event_rows(
            "list/func_ggrl102_1.jsn", dividends);
        const auto& dividend_row = dividend_rows.as_array().front();
        require(dividend_row.at("kind").as_string() == "dividend" &&
                dividend_row.at("security").at("security_id").as_string() == "HK00700",
                "dividend security normalization");
        require(dividend_row.at("dates").at("payment").as_string() == "20260601" &&
                dividend_row.at("plan").as_string().find("5.3") != std::string::npos,
                "dividend dates and plan");

        auto holdings = one_row();
        auto& holding = holdings.as_array().front();
        holding["$SC"] = "31";
        holding["$ZQDM"] = "00035";
        holding["ZQJC"] = "远东发展";
        holding["date"] = "20260730";
        holding["tzz"] = "邱达昌";
        holding["bdgs"] = "0.200000";
        holding["bdhgs"] = "172266.653200";
        holding["bdhcgl"] = "56.31";
        holding["bm"] = "你买入了股份";
        holding["hdc"] = "好仓";
        const auto holding_rows = tdx::normalize_hk_event_rows(
            "list/func_ggrl103_1.jsn", holdings);
        const auto& holding_row = holding_rows.as_array().front();
        require(std::abs(holding_row.at("changed_shares").as_number() - 2000.0) < 1e-9 &&
                std::abs(holding_row.at("holding_after_shares").as_number() - 1722666532.0) < 1e-6,
                "holding ten-thousand-share conversion");
        require(std::abs(holding_row.at("holding_after_pct").as_number() - 56.31) < 1e-9,
                "holding percentage");

        auto shorts = one_row();
        auto& short_row_raw = shorts.as_array().front();
        short_row_raw["$SC"] = "31";
        short_row_raw["$ZQDM"] = "00001";
        short_row_raw["date"] = "20260804";
        short_row_raw["gksl"] = "136.35";
        short_row_raw["gkje"] = "10082.1225";
        short_row_raw["cjje"] = "44147.1392";
        short_row_raw["sssj"] = "全日收市";
        const auto short_rows = tdx::normalize_hk_event_rows(
            "list/func_ggrl104_1.jsn", shorts);
        const auto& short_row = short_rows.as_array().front();
        require(std::abs(short_row.at("short_shares").as_number() - 1363500.0) < 1e-6 &&
                std::abs(short_row.at("short_amount_currency_units").as_number() - 100821225.0) < 1e-6,
                "short-selling ten-thousand-unit conversion");
        require(std::abs(short_row.at("short_turnover_pct").as_number() -
                         10082.1225 * 100.0 / 44147.1392) < 1e-9,
                "short-selling turnover ratio");

        auto kline = tdx::Json::object();
        kline["bars"] = tdx::Json::array();
        const double short_values[]{1000000.0, 1100000.0, 1200000.0,
                                    1363500.0, 1400000.0, 1500000.0};
        for (int i = 0; i < 6; ++i) {
            auto bar = tdx::Json::object();
            bar["date"] = "2026-08-0" + std::to_string(i + 1);
            bar["close"] = 50.0 + i;
            bar["volume"] = 20000 + i * 100;
            bar["hk_short_volume"] = short_values[i];
            kline["bars"].push_back(std::move(bar));
        }
        kline["endpoint"] = "test:7727";
        kline["transport"] = "tdx-7727-0x23ff";
        kline["auxiliary_field"] = "hk_short_volume";
        const auto short_history = tdx::normalize_hk_short_history(
            kline, short_rows, "31", "00001");
        require(short_history.at("schema").as_string() ==
                    "tdx-market-hk-short-history-native-v1" &&
                short_history.at("summary").at("history_count").as_number() == 6,
                "short history schema and count");
        require(short_history.at("reconciliation").at("overlap_day_count").as_number() == 1 &&
                short_history.at("reconciliation").at("exact_match_count").as_number() == 1 &&
                short_history.at("reconciliation").at("all_overlaps_exact").as_bool(),
                "short history exact GGRL104 reconciliation");
        const auto& fifth = short_history.at("history").as_array().at(4);
        require(fifth.at("short_shares_ma5").is_number() &&
                std::abs(fifth.at("volume_shares").as_number() - 2040000.0) < 1e-6 &&
                fifth.at("short_share_volume_pct").as_number() > 60.0,
                "short history moving average and 100-share lot conversion");

        auto applications = one_row();
        auto& application = applications.as_array().front();
        application["mc"] = "测试科技股份有限公司";
        application["rq"] = "20260804";
        application["jc"] = "1";
        application["sc"] = "主板";
        application["lx"] = "常规";
        application["zt"] = "已申请";
        application["rq1"] = "20260805";
        application["jr"] = "测试保荐人";
        application["sr"] = "449286.000";
        application["lr"] = "39574.000";
        application["bz"] = "CNY";
        application["gd"] = "测试控股股东";
        application["zy"] = "智能汽车解决方案";
        const auto application_rows = tdx::normalize_hk_event_rows(
            "list/func_ggrl105_1.jsn", applications);
        const auto& application_row = application_rows.as_array().front();
        require(application_row.at("security").is_null() &&
                application_row.at("date").as_string() == "20260805",
                "listing application identity and primary date");
        require(std::abs(application_row.at("last_year_revenue_currency_units").as_number() -
                         449286000.0) < 1e-6 &&
                application_row.at("currency").as_string() == "CNY",
                "listing application thousand-currency conversion");

        std::cout << "HK events tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
