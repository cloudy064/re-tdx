#include "tdx/formula_strategy.hpp"

#include "tdx/common.hpp"
#include "tdx/formula_engine.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw tdx::Error(message);
}

tdx::Json security(const std::string& market, const std::string& code,
                   const std::vector<double>& opens,
                   const std::vector<double>& closes) {
    require(opens.size() == closes.size(), "fixture shape");
    tdx::Json result = tdx::Json::object();
    result["market"] = market;
    result["code"] = code;
    result["name"] = market + code;
    result["period"] = "day";
    result["bars"] = tdx::Json::array();
    // Wire K-lines are newest first; the formula engine restores chronology.
    for (std::size_t offset = 0; offset < opens.size(); ++offset) {
        const auto index = opens.size() - 1 - offset;
        tdx::Json bar = tdx::Json::object();
        bar["date"] = "2026-01-0" + std::to_string(index + 1);
        bar["time"] = "15:00";
        bar["open"] = opens[index];
        bar["high"] = std::max(opens[index], closes[index]) + 0.5;
        bar["low"] = std::min(opens[index], closes[index]) - 0.5;
        bar["close"] = closes[index];
        bar["volume"] = 10000.0;
        bar["amount"] = closes[index] * 10000.0;
        result["bars"].push_back(std::move(bar));
    }
    return result;
}

tdx::Json manifest(const std::string& operation, int minimum = 1) {
    tdx::Json result = tdx::Json::object();
    result["code"] = "TREND_CONFIRM";
    result["name"] = "trend confirmation";
    result["operator"] = operation;
    if (operation == "at-least") result["minimum_matches"] = minimum;
    result["rules"] = tdx::Json::array();
    tdx::Json price = tdx::Json::object();
    price["id"] = "price";
    price["source"] = "RESULT:CLOSE>10;";
    result["rules"].push_back(std::move(price));
    tdx::Json momentum = tdx::Json::object();
    momentum["id"] = "momentum";
    momentum["source"] = "RESULT:CLOSE>REF(CLOSE,1);";
    result["rules"].push_back(std::move(momentum));
    return result;
}

const tdx::Json& attribution(const tdx::Json& result, const std::string& id) {
    for (const auto& row : result.at("attribution").as_array())
        if (row.at("security_id").as_string() == id) return row;
    throw tdx::Error("missing attribution: " + id);
}

}  // namespace

int main() {
    try {
        const auto prices = security("sz", "000001",
            {10.0, 10.0, 11.0, 12.0}, {9.0, 11.0, 12.0, 13.0});

        const auto all = tdx::normalize_formula_strategy_document(manifest("all"));
        require(all.at("minimum_matches").as_number() == 2.0,
                "all must require every rule");
        const auto all_evaluation = tdx::evaluate_formula_strategy_document(prices, all);
        const auto& all_points = all_evaluation.at("points").as_array();
        require(!all_points[0].at("matched").as_bool() &&
                all_points[1].at("matched").as_bool(),
                "all rules must combine on the exact same bar");
        require(all_points[1].at("matched_rule_count").as_number() == 2.0,
                "same-bar evidence must retain the matching rule count");

        const auto any = tdx::normalize_formula_strategy_document(manifest("any"));
        require(any.at("minimum_matches").as_number() == 1.0,
                "any must require one rule");
        const auto any_evaluation = tdx::evaluate_formula_strategy_document(prices, any);
        require(any_evaluation.at("points").as_array()[2].at("matched").as_bool(),
                "any rule should match a bar");

        tdx::Json trade_manifest = tdx::Json::object();
        trade_manifest["rules"] = tdx::Json::array();
        tdx::Json trade_rule = tdx::Json::object();
        trade_rule["id"] = "read_only_trade_signal";
        trade_rule["source"] = "BUY(CLOSE>10,LOW);";
        trade_manifest["rules"].push_back(std::move(trade_rule));
        const auto trade_strategy =
            tdx::normalize_formula_strategy_document(trade_manifest);
        const auto trade_evaluation =
            tdx::evaluate_formula_strategy_document(prices, trade_strategy);
        require(trade_evaluation.at("points").as_array()[1]
                    .at("matched").as_bool() &&
                !trade_evaluation.at("points").as_array()[0]
                     .at("matched").as_bool(),
                "strategy evaluation consumes the compatible trading-signal numeric series");
        const auto direct_trade = tdx::evaluate_formula_document(
            prices,
            trade_strategy.at("rules").as_array()[0]
                .at("formula_definition"));
        require(direct_trade.at("trade_event_ir")
                    .at("primitives").as_array()[0]
                    .at("latest_host_action").at("host_action_bits")
                    .as_number() == 1.0 &&
                !direct_trade.at("trade_event_ir")
                     .at("execution_side_effects").as_bool() &&
                !direct_trade.at("trade_event_ir")
                     .at("order_submission").as_bool(),
                "strategy formula exposes BUY as read-only trade-event IR without execution");

        const auto threshold = tdx::normalize_formula_strategy_document(
            manifest("at-least", 2));
        require(threshold.at("minimum_matches").as_number() == 2.0,
                "at-least threshold");

        bool future_rejected = false;
        try {
            tdx::Json unsafe = tdx::Json::object();
            unsafe["rules"] = tdx::Json::array();
            tdx::Json rule = tdx::Json::object();
            rule["id"] = "future";
            rule["source"] = "RESULT:REFX(CLOSE,1)>CLOSE;";
            unsafe["rules"].push_back(std::move(rule));
            (void)tdx::normalize_formula_strategy_document(unsafe);
        } catch (const tdx::Error&) {
            future_rejected = true;
        }
        require(future_rejected, "future functions must not enter a strategy");

        const auto falling = security("sh", "600000",
            {10.0, 10.0, 9.0, 8.0}, {9.0, 11.0, 8.0, 7.0});
        const std::vector<tdx::Json> universe{prices, falling};
        const auto scan = tdx::scan_formula_strategy_documents(universe, all, 3);
        require(scan.at("evaluated").as_number() == 2.0 &&
                scan.at("error_count").as_number() == 0.0,
                "strategy scan must evaluate the fixed test universe");

        auto adjusted_prices = prices;
        auto adjusted_falling = falling;
        for (auto* document : {&adjusted_prices, &adjusted_falling}) {
            (*document)["adjustment_mode"] = "qfq";
            (*document)["adjustment"] = tdx::Json::object();
            (*document)["adjustment"]["mode"] = "qfq";
            (*document)["adjustment"]["source_command"] = "0x000F";
        }
        const std::vector<tdx::Json> adjusted_universe{
            adjusted_prices, adjusted_falling};
        const auto adjusted_evaluation =
            tdx::evaluate_formula_strategy_document(adjusted_prices, all);
        require(adjusted_evaluation.at("adjustment_mode").as_string() == "qfq" &&
                    adjusted_evaluation.at("adjustment").at("source_command")
                        .as_string() == "0x000F",
                "strategy evaluation preserves security adjustment metadata");
        const auto adjusted_scan = tdx::scan_formula_strategy_documents(
            adjusted_universe, all, 3);
        require(adjusted_scan.at("matches").as_array()[0]
                    .at("adjustment_mode").as_string() == "qfq",
                "strategy scan match identifies its adjustment mode");

        tdx::Json one_rule = tdx::Json::object();
        one_rule["operator"] = "all";
        one_rule["rules"] = tdx::Json::array();
        tdx::Json rule = tdx::Json::object();
        rule["id"] = "price";
        rule["source"] = "RESULT:CLOSE>10;";
        one_rule["rules"].push_back(std::move(rule));
        const auto portfolio_strategy =
            tdx::normalize_formula_strategy_document(one_rule);
        const auto backtest = tdx::backtest_formula_strategy_documents(
            universe, portfolio_strategy, 100000.0, 0.0, 0.0);
        require(backtest.at("aligned_bar_count").as_number() == 4.0,
                "portfolio must use shared date/time bars");
        require(backtest.at("signal_timing").as_string().find("next shared bar open") !=
                    std::string::npos,
                "portfolio must document next-open execution");
        // The second close activates both securities at the third open. Only the
        // third-to-fourth open interval is exposed: 50% * (12/11 + 8/9).
        const double expected = 100000.0 * 0.5 * (12.0 / 11.0 + 8.0 / 9.0);
        require(std::abs(backtest.at("final_equity").as_number() - expected) < 1e-6,
                "portfolio must not trade at the signal close");
        require(attribution(backtest, "sz000001").at("gross_contribution").as_number() > 0 &&
                attribution(backtest, "sh600000").at("gross_contribution").as_number() < 0,
                "per-security attribution must preserve winners and losers");
        double net_sum = 0.0;
        for (const auto& row : backtest.at("attribution").as_array())
            net_sum += row.at("net_contribution").as_number();
        require(std::abs(100000.0 + net_sum -
                         backtest.at("final_equity").as_number()) < 1e-6,
                "attribution must reconcile to final equity");
        const auto adjusted_backtest = tdx::backtest_formula_strategy_documents(
            adjusted_universe, portfolio_strategy, 100000.0, 0.0, 0.0);
        require(adjusted_backtest.at("adjustment_mode").as_string() == "qfq",
                "strategy portfolio result preserves the uniform adjustment mode");

        std::cout << "Formula strategy tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
