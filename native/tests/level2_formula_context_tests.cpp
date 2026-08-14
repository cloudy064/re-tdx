#include "tdx/level2.hpp"

#include "tdx/common.hpp"

#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json value(std::optional<double> number) {
    return number ? tdx::Json(*number) : tdx::Json(nullptr);
}

tdx::Json matrix(std::size_t rows, std::size_t columns, double number,
                 bool missing_first = false) {
    auto result = tdx::Json::array();
    for (std::size_t row = 0; row < rows; ++row) {
        auto values = tdx::Json::array();
        for (std::size_t column = 0; column < columns; ++column)
            values.push_back(row == 0 && column == 0 && missing_first
                ? tdx::Json(nullptr) : tdx::Json(number));
        result.push_back(std::move(values));
    }
    return result;
}

tdx::Json record(const std::string& date, std::optional<double> host,
                 double volume, double amount, double count,
                 double bid_cancel, double offer_cancel,
                 double current_buy, double current_sell,
                 std::optional<double> average_bid,
                 std::optional<double> average_offer,
                 bool missing_first_volume = false) {
    auto row = tdx::Json::object();
    row["formula_context_stamp"] = date + "|15:00";
    row["host_auxiliary_f32"] = value(host);
    row["l2_vol"] = matrix(4, 4, volume, missing_first_volume);
    row["l2_amo"] = matrix(4, 4, amount);
    row["l2_volnum_raw"] = matrix(2, 2, count);
    auto bindings = tdx::Json::object();
    bindings["L2_VOL#0#0"] = missing_first_volume
        ? tdx::Json(nullptr) : tdx::Json(volume);
    bindings["L2_AMO#3#3"] = amount;
    bindings["L2_VOLNUM#0#0"] = count;
    bindings["ACTINVOL"] = volume * 4.0;
    bindings["BIDORDERVOL"] = 1.0;
    bindings["BIDCANCELVOL"] = bid_cancel;
    bindings["OFFERORDERVOL"] = 1.0;
    bindings["OFFERCANCELVOL"] = offer_cancel;
    bindings["AVGBIDPX"] = value(average_bid);
    bindings["AVGOFFERPX"] = value(average_offer);
    bindings["CUR_BUYORDER"] = current_buy;
    bindings["CUR_SELLORDER"] = current_sell;
    bindings["TRADENUM"] = value(host);
    bindings["TRADEINNUM"] = count * 2.0;
    bindings["TRADEOUTNUM"] = count * 2.0;
    bindings["LARGETRDINNUM"] = count;
    bindings["LARGETRDOUTNUM"] = count;
    row["formula_bindings"] = std::move(bindings);
    return row;
}

tdx::Json context(std::vector<tdx::Json> records) {
    auto result = tdx::Json::object();
    result["records"] = tdx::Json::array();
    for (auto& row : records) result["records"].push_back(std::move(row));
    result["_formula_context"] = tdx::Json::object();
    result["_formula_context"]["schema"] =
        "tdx-formula-explicit-context-from-tcalc-l2-v1";
    result["_formula_context"]["context_complete"] = true;
    result["_formula_context"]["invalid_date_record_count"] = 0;
    return result;
}

tdx::Json kline(const std::string& period,
                const std::vector<std::pair<std::string,
                                            std::pair<double, double>>>& rows) {
    auto result = tdx::Json::object();
    result["period"] = period;
    result["bars"] = tdx::Json::array();
    for (const auto& [date, prices] : rows) {
        auto bar = tdx::Json::object();
        bar["date"] = date;
        bar["time"] = "15:00";
        bar["low"] = prices.first;
        bar["high"] = prices.second;
        result["bars"].push_back(std::move(bar));
    }
    return result;
}

const tdx::Json& point(const tdx::Json& document, const std::string& name,
                       const std::string& date) {
    return document.at("series").at(name).at(date + "|15:00");
}

void require_number(const tdx::Json& actual, double expected,
                    const char* message) {
    require(actual.is_number() &&
                std::fabs(actual.as_number() - expected) < 1.0e-5,
            message);
}

}  // namespace

int main() {
    try {
        const auto weekly = context({
            record("2025-12-30", 100.0, 100.0, 100.0, 100.0,
                   100.0, 100.0, 100.0, 100.0, 100.0, 100.0),
            record("2026-01-02", 1.0, 1.0, 11.0, 1.0,
                   1.0, 1.0, 1.0, 2.0, 10.0, 20.0),
            record("2026-01-05", 2.0, 2.0, 12.0, 2.0,
                   2.0, 3.0, 10.0, 20.0, 50.0, 60.0),
            record("2026-01-06", 3.0, 3.0, 13.0, 2.0,
                   4.0, 5.0, 11.0, 21.0, 40.0, 50.0),
            record("2026-01-09", 4.0, 4.0, 14.0, 2.0,
                   6.0, 7.0, 12.0, 22.0, 0.0, std::nullopt),
            record("2026-01-12", 5.0, 5.0, 15.0, 1.0,
                   1.0, 1.0, 30.0, 40.0, 70.0, 80.0),
            record("2026-01-19", 6.0, 6.0, 16.0, 1.0,
                   2.0, 3.0, 31.0, 41.0, 71.0, 81.0),
            record("2026-01-23", 7.0, 7.0, 17.0, 1.0,
                   4.0, 5.0, 32.0, 42.0, 72.0, 82.0)});
        const auto weekly_result =
            tdx::materialize_tcalc_level2_formula_context(
                weekly, kline("week", {
                    {"2026-01-02", {9.0, 21.0}},
                    {"2026-01-09", {90.0, 110.0}},
                    {"2026-01-16", {80.0, 120.0}},
                    {"2026-01-23", {70.0, 130.0}},
                    {"2026-01-30", {60.0, 140.0}}}));

        require_number(point(weekly_result, "TRADENUM", "2026-01-02"), 1.0,
                       "leading partial records are cropped from the first week");
        require_number(point(weekly_result, "TRADENUM", "2026-01-09"), 9.0,
                       "weekly host auxiliary is summed before count conversion");
        require_number(point(weekly_result, "L2_VOL#0#0", "2026-01-09"), 9.0,
                       "weekly matrix fields are summed");
        require_number(point(weekly_result, "BIDCANCELVOL", "2026-01-09"), 33.0,
                       "weekly bid cancel carries the preceding current buy value");
        require_number(point(weekly_result, "OFFERCANCELVOL", "2026-01-09"), 56.0,
                       "weekly offer cancel carries the preceding current sell value");
        require_number(point(weekly_result, "CUR_BUYORDER", "2026-01-09"), 12.0,
                       "weekly current buy keeps the final daily value");
        require_number(point(weekly_result, "AVGBIDPX", "2026-01-09"), 90.0,
                       "weekly invalid average bid falls back to target low");
        require_number(point(weekly_result, "AVGOFFERPX", "2026-01-09"), 110.0,
                       "weekly missing average offer falls back to target high");
        require(point(weekly_result, "TRADENUM", "2026-01-16").is_null(),
                "weekly partial aggregate without an exact end date is missing");
        require_number(point(weekly_result, "TRADENUM", "2026-01-30"), 13.0,
                       "weekly bars after the final record inherit the prior output");
        const auto& weekly_metadata = weekly_result.at("_formula_context");
        require(weekly_metadata.at("leading_cropped_record_count").as_number() == 1.0 &&
                    weekly_metadata.at("exact_match_count").as_number() == 3.0 &&
                    weekly_metadata.at("missing_exact_end_date_count").as_number() == 1.0 &&
                    weekly_metadata.at("discarded_partial_record_count").as_number() == 1.0 &&
                    weekly_metadata.at("trailing_inherited_count").as_number() == 1.0,
                "weekly materialization reports crop, exact, missing and trailing rules");

        const auto monthly = context({
            record("2026-01-31", 2.0, 2.0, 20.0, 1.0,
                   1.0, 1.0, 1.0, 2.0, 10.0, 20.0),
            record("2026-02-02", std::nullopt, 3.0, 30.0, 1.0,
                   2.0, 3.0, 4.0, 5.0, 30.0, 40.0, true),
            record("2026-02-27", 5.0, 4.0, 40.0, 1.0,
                   4.0, 5.0, 6.0, 7.0, 50.0, 60.0)});
        const auto monthly_result =
            tdx::materialize_tcalc_level2_formula_context(
                monthly, kline("month", {
                    {"2026-01-31", {8.0, 22.0}},
                    {"2026-02-27", {18.0, 62.0}}}));
        require(point(monthly_result, "TRADENUM", "2026-02-27").is_null() &&
                    point(monthly_result, "L2_VOL#0#0", "2026-02-27").is_null(),
                "monthly sums propagate null raw values without sentinel artifacts");
        require_number(point(monthly_result, "L2_AMO#3#3", "2026-02-27"), 70.0,
                       "monthly valid matrix fields aggregate across the month");
        require_number(point(monthly_result, "CUR_SELLORDER", "2026-02-27"), 7.0,
                       "monthly final-value fields use the exact end-date record");
        require(monthly_result.dump().find("-4.0") == std::string::npos,
                "materialized context never exposes a TCalc sentinel magnitude");

        const auto daily = context({
            record("2026-03-02", 8.0, 1.0, 1.0, 1.0,
                   1.0, 1.0, 1.0, 1.0, 1.0, 1.0)});
        const auto daily_result =
            tdx::materialize_tcalc_level2_formula_context(
                daily, kline("day", {
                    {"2026-03-01", {1.0, 2.0}},
                    {"2026-03-02", {1.0, 2.0}},
                    {"2026-03-03", {1.0, 2.0}}}));
        require(point(daily_result, "TRADENUM", "2026-03-01").is_null() &&
                    point(daily_result, "TRADENUM", "2026-03-02").as_number() == 8.0 &&
                    point(daily_result, "TRADENUM", "2026-03-03").as_number() == 8.0,
                "daily exact, leading crop and trailing inheritance stay unchanged");

        std::cout << "Level2 formula context tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
