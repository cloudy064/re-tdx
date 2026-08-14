#include "formula_strategy_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace tdx {

using namespace formula_strategy_detail;

Json backtest_formula_strategy_documents(const std::vector<Json>& kline_documents,
                                         const Json& strategy,
                                         double initial_capital,
                                         double commission_bps,
                                         double slippage_bps) {
    if (!std::isfinite(initial_capital) || initial_capital <= 0)
        throw Error("strategy initial capital must be positive");
    if (!std::isfinite(commission_bps) || !std::isfinite(slippage_bps) ||
        commission_bps < 0 || commission_bps > 1000 ||
        slippage_bps < 0 || slippage_bps > 1000)
        throw Error("strategy commission/slippage bps must be in 0..1000");
    if (kline_documents.empty()) throw Error("strategy backtest requires securities");
    struct Entry { double open{}; bool matched{}; };
    struct Series {
        std::string id, market, code, name;
        std::map<std::string, Entry, std::less<>> points;
    };
    std::vector<Series> series;
    std::set<std::string> security_ids;
    std::string period, adjustment_mode;
    bool adjustment_mode_initialized = false;
    for (const auto& kline : kline_documents) {
        const auto evaluation = evaluate_formula_strategy_document(kline, strategy);
        Series item;
        item.market = required_text(evaluation, "market");
        item.code = required_text(evaluation, "code");
        item.id = item.market + item.code;
        if (!security_ids.insert(item.id).second)
            throw Error("duplicate strategy backtest security: " + item.id);
        if (const auto* name = optional(evaluation, "name"); name && name->is_string())
            item.name = name->as_string();
        const auto item_period = required_text(evaluation, "period");
        if (period.empty()) period = item_period;
        else if (period != item_period)
            throw Error("strategy backtest securities must use the same period");
        const auto* item_adjustment = optional(evaluation, "adjustment_mode");
        const auto item_adjustment_mode = item_adjustment && item_adjustment->is_string()
            ? item_adjustment->as_string() : std::string{};
        if (!adjustment_mode_initialized) {
            adjustment_mode = item_adjustment_mode;
            adjustment_mode_initialized = true;
        }
        else if (adjustment_mode != item_adjustment_mode)
            throw Error("strategy backtest securities must use the same adjustment mode");
        for (const auto& point : evaluation.at("points").as_array()) {
            const auto* open = optional(point, "open");
            if (!open || !open->is_number() || open->as_number() <= 0) continue;
            item.points[point_key(point)] = Entry{
                open->as_number(), point.at("matched").as_bool()};
        }
        if (item.points.size() < 3)
            throw Error("strategy backtest security requires at least three valid bars: " + item.id);
        series.push_back(std::move(item));
    }
    std::set<std::string> shared;
    for (const auto& [key, entry] : series.front().points) {
        (void)entry;
        shared.insert(key);
    }
    for (std::size_t index = 1; index < series.size(); ++index) {
        for (auto iterator = shared.begin(); iterator != shared.end();) {
            if (!series[index].points.count(*iterator)) iterator = shared.erase(iterator);
            else ++iterator;
        }
    }
    if (shared.size() < 3)
        throw Error("strategy backtest requires at least three shared bars");
    const std::vector<std::string> timeline(shared.begin(), shared.end());
    struct Attribution {
        double gross{};
        double cost{};
        double turnover{};
        std::uint64_t active_intervals{};
        std::uint64_t entries{};
        std::uint64_t exits{};
    };
    std::map<std::string, Attribution, std::less<>> attribution;
    std::map<std::string, double, std::less<>> weights;
    double equity = initial_capital;
    double peak = equity;
    double max_drawdown = 0.0;
    double total_turnover = 0.0;
    double total_cost = 0.0;
    std::uint64_t rebalance_count = 0;
    std::uint64_t holding_count_sum = 0;
    Json rebalances = Json::array(), curve = Json::array();
    const double cost_rate = (commission_bps + slippage_bps) / 10000.0;
    const auto target_at = [&](std::size_t index) {
        std::map<std::string, double, std::less<>> target;
        std::vector<std::string> active;
        for (const auto& item : series)
            if (item.points.at(timeline[index]).matched) active.push_back(item.id);
        if (!active.empty()) {
            const double weight = 1.0 / static_cast<double>(active.size());
            for (const auto& id : active) target[id] = weight;
        }
        return target;
    };
    const auto split_key = [](const std::string& key) {
        const auto separator = key.find('|');
        return std::pair<std::string, std::string>{
            key.substr(0, separator), separator == std::string::npos ? "" : key.substr(separator + 1)};
    };
    auto rebalance = [&](const std::map<std::string, double, std::less<>>& target,
                         const std::string& key, const std::string& reason) {
        std::set<std::string> ids;
        for (const auto& [id, value] : weights) { (void)value; ids.insert(id); }
        for (const auto& [id, value] : target) { (void)value; ids.insert(id); }
        double turnover = 0.0;
        std::map<std::string, double, std::less<>> changes;
        for (const auto& id : ids) {
            const double before = weights.count(id) ? weights.at(id) : 0.0;
            const double after = target.count(id) ? target.at(id) : 0.0;
            const double change = std::abs(after - before);
            if (change <= 1e-15) continue;
            changes[id] = change;
            turnover += change;
            attribution[id].turnover += change;
            if (before <= 1e-15 && after > 1e-15) ++attribution[id].entries;
            if (before > 1e-15 && after <= 1e-15) ++attribution[id].exits;
        }
        const double cost = equity * turnover * cost_rate;
        if (turnover > 1e-15) ++rebalance_count;
        for (const auto& [id, change] : changes)
            attribution[id].cost += turnover > 0 ? cost * change / turnover : 0.0;
        equity -= cost;
        total_cost += cost;
        total_turnover += turnover;
        weights = target;
        Json event = Json::object();
        const auto [date, time] = split_key(key);
        event["date"] = date;
        event["time"] = time;
        event["reason"] = reason;
        event["turnover"] = turnover;
        event["cost"] = cost;
        event["equity_after_cost"] = equity;
        Json active = Json::array();
        for (const auto& [id, value] : weights) {
            (void)value;
            active.push_back(id);
        }
        event["active_securities"] = std::move(active);
        rebalances.push_back(std::move(event));
    };
    rebalance(target_at(0), timeline[1], "initial-next-open");
    {
        const auto [date, time] = split_key(timeline[1]);
        Json row = Json::object(); row["date"] = date; row["time"] = time;
        row["equity"] = equity; row["holding_count"] = static_cast<std::uint64_t>(weights.size());
        curve.push_back(std::move(row));
    }
    for (std::size_t index = 1; index + 1 < timeline.size(); ++index) {
        const double before_equity = equity;
        double cash_weight = 1.0;
        double factor = 0.0;
        std::map<std::string, double, std::less<>> drifted;
        for (const auto& [id, weight] : weights) {
            cash_weight -= weight;
            const auto series_it = std::find_if(series.begin(), series.end(),
                [&](const Series& item) { return item.id == id; });
            const double ratio = series_it->points.at(timeline[index + 1]).open /
                                 series_it->points.at(timeline[index]).open;
            const double value = weight * ratio;
            factor += value;
            drifted[id] = value;
            attribution[id].gross += before_equity * weight * (ratio - 1.0);
            ++attribution[id].active_intervals;
        }
        factor += std::max(0.0, cash_weight);
        if (!std::isfinite(factor) || factor <= 0)
            throw Error("strategy portfolio equity factor is invalid");
        equity *= factor;
        for (auto& [id, value] : drifted) value /= factor;
        weights = std::move(drifted);
        holding_count_sum += static_cast<std::uint64_t>(weights.size());
        if (index + 2 < timeline.size())
            rebalance(target_at(index), timeline[index + 1], "signal-close-next-open");
        else
            rebalance({}, timeline[index + 1], "end-of-data-liquidation");
        peak = std::max(peak, equity);
        if (peak > 0) max_drawdown = std::max(max_drawdown, (peak - equity) * 100.0 / peak);
        const auto [date, time] = split_key(timeline[index + 1]);
        Json row = Json::object(); row["date"] = date; row["time"] = time;
        row["equity"] = equity; row["holding_count"] = static_cast<std::uint64_t>(weights.size());
        curve.push_back(std::move(row));
    }
    Json attribution_rows = Json::array();
    for (const auto& item : series) {
        const auto& values = attribution[item.id];
        Json row = Json::object();
        row["security_id"] = item.id; row["market"] = item.market;
        row["code"] = item.code; row["name"] = item.name;
        row["gross_contribution"] = values.gross;
        row["allocated_cost"] = values.cost;
        row["net_contribution"] = values.gross - values.cost;
        row["net_contribution_pct"] = (values.gross - values.cost) * 100.0 / initial_capital;
        row["turnover"] = values.turnover;
        row["active_intervals"] = values.active_intervals;
        row["entries"] = values.entries;
        row["exits"] = values.exits;
        attribution_rows.push_back(std::move(row));
    }
    std::sort(attribution_rows.as_array().begin(), attribution_rows.as_array().end(),
              [](const Json& left, const Json& right) {
                  return left.at("net_contribution").as_number() >
                         right.at("net_contribution").as_number();
              });
    Json result = Json::object();
    result["schema_version"] = 1;
    result["schema"] = "tdx-formula-strategy-portfolio-backtest-v1";
    result["engine"] = "tdx-formula-strategy-portfolio-v1";
    result["execution_mode"] = "native-cpp";
    result["strategy"] = strategy_summary(strategy);
    result["signal_timing"] = "shared bar close signal; rebalance at next shared bar open";
    result["portfolio_method"] = "equal weight among matched securities; fixed input universe";
    result["alignment"] = "intersection of security date/time bars";
    result["period"] = period;
    if (!adjustment_mode.empty()) result["adjustment_mode"] = adjustment_mode;
    result["security_count"] = static_cast<std::uint64_t>(series.size());
    result["aligned_bar_count"] = static_cast<std::uint64_t>(timeline.size());
    result["initial_capital"] = initial_capital;
    result["final_equity"] = equity;
    result["total_return_pct"] = (equity / initial_capital - 1.0) * 100.0;
    result["max_drawdown_pct"] = max_drawdown;
    result["commission_bps"] = commission_bps;
    result["slippage_bps"] = slippage_bps;
    result["total_cost"] = total_cost;
    result["total_turnover"] = total_turnover;
    result["rebalance_count"] = rebalance_count;
    const auto intervals = timeline.size() > 2 ? timeline.size() - 2 : 0;
    result["average_holding_count"] = intervals
        ? static_cast<double>(holding_count_sum) / static_cast<double>(intervals) : 0.0;
    result["equity_curve"] = std::move(curve);
    result["rebalances"] = std::move(rebalances);
    result["attribution"] = std::move(attribution_rows);
    return result;
}

}  // namespace tdx
