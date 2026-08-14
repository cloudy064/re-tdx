#include "tdx/reverse_repo.hpp"
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
        securities[{1, "204001"}] = {1, "SH", "上海", "204001", "GC001"};
        auto schedule = tdx::Json::array();
        auto raw = tdx::Json::object();
        raw["$ZQDM"] = "204001";
        raw["$SC"] = "1";
        raw["TS"] = "1";
        raw["SXF"] = "1";
        raw["ZJKY"] = "20260807";
        raw["SCZJJS"] = "20260807";
        raw["ZJKQ"] = "20260810";
        raw["SJTS"] = "3";
        schedule.push_back(raw);
        auto quotes = tdx::Json::array();
        auto quote = tdx::Json::object();
        quote["market_id"] = 1;
        quote["code"] = "204001";
        quote["name"] = "GC001";
        quote["last_price"] = 1.3;
        quote["pre_close_price"] = 1.34;
        quote["open_price"] = 1.39;
        quote["high_price"] = 1.4;
        quote["low_price"] = 1.2;
        quote["change_pct"] = -2.9850746268;
        quote["amount"] = 2040661147648.0;
        quote["time_raw"] = 1529987000;
        quotes.push_back(quote);
        auto rows = tdx::normalize_reverse_repo_rows(schedule, quotes, 100000, securities);
        require(rows.size() == 1, "reverse repo row count");
        const auto& row = rows.as_array().front();
        require(row.at("security").at("name").as_string() == "GC001",
                "repo name resolved");
        require(row.at("term_days").as_number() == 1 &&
                    row.at("interest_days").as_number() == 3 &&
                    row.at("bonus_interest_days").as_number() == 2,
                "term and actual interest days separated");
        const auto gross = 100000.0 * 0.013 * 3.0 / 365.0;
        require(std::abs(row.at("gross_interest_yuan").as_number() - gross) < 1e-9,
                "client gross interest formula");
        require(std::abs(row.at("fee_yuan").as_number() - 1.0) < 1e-9 &&
                    std::abs(row.at("net_interest_yuan").as_number() - (gross - 1.0)) < 1e-9,
                "fee and net interest");
        require(std::abs(row.at("annualized_rate_pct").as_number() - 1.3) < 1e-9,
                "repo annualized rate scale");
        require(row.at("settlement_date").as_string() == "2026-08-07" &&
                    row.at("funds_withdrawable_date").as_string() == "2026-08-10",
                "repo settlement dates normalized");

        quotes.as_array().front()["last_price"] = 0;
        const auto pre_open_rows = tdx::normalize_reverse_repo_rows(
            schedule, quotes, 100000, securities);
        const auto& pre_open = pre_open_rows.as_array().front();
        require(pre_open.at("quote_available").as_bool() &&
                    pre_open.at("quote_price_source").as_string() == "pre-close" &&
                    std::abs(pre_open.at("annualized_rate_pct").as_number() - 1.34) < 1e-9,
                "pre-open zero last price must fall back to the previous close rate");

        auto calendar_only = tdx::normalize_reverse_repo_rows(
            schedule, tdx::Json::array(), 200000, securities);
        const auto& no_quote = calendar_only.as_array().front();
        require(!no_quote.at("quote_available").as_bool() &&
                    no_quote.at("annualized_rate_pct").is_null() &&
                    no_quote.at("net_interest_yuan").is_null(),
                "calendar survives quote absence");
        require(std::abs(no_quote.at("fee_yuan").as_number() - 2.0) < 1e-9,
                "fee scales with principal without quote");

        rows.push_back(no_quote);
        tdx::sort_reverse_repo_rows(rows, "rate", "desc");
        require(rows.as_array().front().at("quote_available").as_bool(),
                "missing quote sorts last");

        std::cout << "reverse-repo tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "reverse-repo test failed: " << error.what() << '\n';
        return 1;
    }
}
