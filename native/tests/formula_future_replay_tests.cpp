#include "tdx/common.hpp"
#include "tdx/formula_engine.hpp"

#include <iostream>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json sample() {
    tdx::Json result = tdx::Json::object();
    result["market"] = "sz";
    result["code"] = "000001";
    result["period"] = "day";
    result["bars"] = tdx::Json::array();
    for (int index = 0; index < 3; ++index) {
        const double close = index == 2 ? 12.0 : 10.0;
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-08-0" + std::to_string(index + 1);
        bar["time"] = "15:00";
        bar["open"] = close;
        bar["high"] = close;
        bar["low"] = close;
        bar["close"] = close;
        bar["amount"] = close * 1000.0;
        bar["volume"] = 1000;
        result["bars"].push_back(std::move(bar));
    }
    return result;
}

bool rejected(const tdx::FormulaFutureReplayRequest& request) {
    try {
        (void)tdx::replay_formula_future_document(request);
        return false;
    } catch (const tdx::Error&) {
        return true;
    }
}

} // namespace

int main() {
    try {
        tdx::FormulaFutureReplayRequest request;
        request.kline_document = sample();
        request.source = "B:BARSNEXT(CLOSE>11);";
        request.formula_code = "REPAINT";
        request.max_observations = 2;
        request.max_events = 10;
        const auto result = tdx::replay_formula_future_document(request);
        require(result.at("schema").as_string() == "tdx-formula-future-replay-v1" &&
                    result.at("source_bar_count").as_number() == 3.0 &&
                    result.at("baseline_bar_count").as_number() == 1.0 &&
                    result.at("observation_count").as_number() == 2.0 &&
                    result.at("evaluation_count").as_number() == 3.0,
                "future replay envelope");
        require(result.at("event_count").as_number() == 2.0 &&
                    result.at("changed_target_count").as_number() == 2.0 &&
                    !result.at("events_truncated").as_bool(),
                "future replay detects historical repaint only");
        const auto& first = result.at("events").as_array().at(0);
        const auto& second = result.at("events").as_array().at(1);
        require(first.at("observed_at").at("date").as_string() == "2026-08-03" &&
                    first.at("target").at("date").as_string() == "2026-08-01" &&
                    first.at("previous_value").is_null() &&
                    first.at("current_value").as_number() == 2.0 &&
                    second.at("target").at("date").as_string() == "2026-08-02" &&
                    second.at("current_value").as_number() == 1.0,
                "future replay event values");
        require(!result.at("newly_appended_points_counted_as_repaint").as_bool() &&
                    !result.at("scan_allowed").as_bool() &&
                    !result.at("backtest_allowed").as_bool() &&
                    !result.at("account_accessed").as_bool() &&
                    !result.at("orders_submitted").as_bool() &&
                    !result.at("sdk_called").as_bool() &&
                    !result.at("subscription_sent").as_bool() &&
                    result.at("network_requests").as_number() == 0.0 &&
                    !result.at("entitlement_bypass").as_bool(),
                "future replay offline boundary");

        request.max_events = 1;
        const auto truncated = tdx::replay_formula_future_document(request);
        require(truncated.at("event_count").as_number() == 2.0 &&
                    truncated.at("stored_event_count").as_number() == 1.0 &&
                    truncated.at("events_truncated").as_bool(),
                "future replay event cap");

        request.source = "M:MA(CLOSE,2);";
        require(rejected(request), "ordinary formula cannot request future replay");
        request.source = "B:BARSNEXT(CLOSE>11);";
        request.max_observations = 65;
        require(rejected(request), "future replay observation cap");

        std::cout << "formula future replay tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
