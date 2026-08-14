#include "tdx/exchange_supervision.hpp"
#include "tdx/common.hpp"

#include <cmath>
#include <iostream>

namespace {
void require(bool value, const char* message) {
    if (!value) throw tdx::Error(message);
}

tdx::Json row(const char* code, const char* market, const char* start,
              const char* end, const char* price1, const char* price2 = "") {
    auto value = tdx::Json::object();
    value["$ZQDM"] = code; value["$SC"] = market;
    value["jgksrq"] = start; value["jgjsrq"] = end;
    value["price1"] = price1; value["price2"] = price2;
    value["ydgg"] = "https://example.test/notice.pdf";
    value["syl"] = "18.25";
    return value;
}
}

int main() {
    try {
        std::map<std::pair<int, std::string>, tdx::Security> securities;
        securities[{0, "000001"}] = {0, "SZ", "深圳", "000001", "平安银行"};
        auto current = tdx::Json::array();
        current.push_back(row("000001", "0", "20260801", "20260814", "10.00"));
        auto quotes = tdx::Json::array();
        auto quote = tdx::Json::object();
        quote["market_id"] = 0; quote["code"] = "000001";
        quote["name"] = "平安银行"; quote["last_price"] = 11.0;
        quote["change_pct"] = 1.25; quote["amount"] = 123456789.0;
        quotes.push_back(quote);
        auto rows = tdx::normalize_exchange_supervision_rows(
            current, "current", quotes, securities);
        require(rows.size() == 1, "current row count");
        const auto& current_row = rows.as_array().front();
        require(current_row.at("security").at("name").as_string() == "平安银行",
                "security name resolved");
        require(current_row.at("start_date").as_string() == "2026-08-01" &&
                    current_row.at("end_date").as_string() == "2026-08-14",
                "dates normalized");
        require(std::abs(current_row.at("since_start_return_pct").as_number() - 10.0) < 1e-9,
                "client current return formula");
        require(current_row.at("period_return_pct").is_null() &&
                    current_row.at("announcement_url").as_string().find("notice.pdf") != std::string::npos,
                "current semantics and PDF");

        auto history = tdx::Json::array();
        history.push_back(row("000001", "0", "20260701", "20260714", "8.00", "10.00"));
        auto history_rows = tdx::normalize_exchange_supervision_rows(
            history, "history", tdx::Json::array(), securities);
        const auto& history_row = history_rows.as_array().front();
        require(std::abs(history_row.at("period_return_pct").as_number() - 25.0) < 1e-9,
                "client history return formula");
        require(!history_row.at("quote_available").as_bool() &&
                    history_row.at("since_start_return_pct").is_null(),
                "history does not claim live quote");

        rows.push_back(history_row);
        tdx::sort_exchange_supervision_rows(rows, "start-date", "asc");
        require(rows.as_array().front().at("record_kind").as_string() == "history",
                "date sort");
        std::cout << "exchange-supervision tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "exchange-supervision test failed: " << error.what() << '\n';
        return 1;
    }
}
