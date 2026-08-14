#include "tdx/trades.hpp"
#include "tdx/trades_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>
#include <map>

namespace tdx {

namespace detail {
namespace trades {

double amount_yuan(const TradeTick& tick) {
    return tick.price * static_cast<double>(tick.volume_hand) * 100.0;
}

Json aggregate_group(const std::vector<const TradeTick*>& values) {
    std::int64_t volume = 0, orders = 0, buy_volume = 0, sell_volume = 0,
                 neutral_volume = 0;
    double amount = 0, buy_amount = 0, sell_amount = 0, neutral_amount = 0;
    std::map<std::int64_t, std::size_t> statuses;
    for (const auto* tick : values) {
        volume += tick->volume_hand;
        orders += tick->order_count;
        const double tick_amount = amount_yuan(*tick);
        amount += tick_amount;
        ++statuses[tick->status_raw];
        if (tick->side == "buy") { buy_volume += tick->volume_hand; buy_amount += tick_amount; }
        else if (tick->side == "sell") { sell_volume += tick->volume_hand; sell_amount += tick_amount; }
        else if (tick->side == "neutral") {
            neutral_volume += tick->volume_hand;
            neutral_amount += tick_amount;
        }
    }
    Json result = Json::object();
    result["tick_count"] = static_cast<std::uint64_t>(values.size());
    result["volume_hand"] = volume;
    result["amount_yuan"] = amount;
    result["order_count"] = orders;
    result["vwap"] = volume ? Json(amount / static_cast<double>(volume) / 100.0) : Json(nullptr);
    result["first_price"] = values.empty() ? Json(nullptr) : Json(values.front()->price);
    result["last_price"] = values.empty() ? Json(nullptr) : Json(values.back()->price);
    if (values.empty()) {
        result["high_price"] = nullptr;
        result["low_price"] = nullptr;
    } else {
        double high = values.front()->price, low = values.front()->price;
        for (const auto* tick : values) {
            high = std::max(high, tick->price);
            low = std::min(low, tick->price);
        }
        result["high_price"] = high;
        result["low_price"] = low;
    }
    result["buy_volume_hand"] = buy_volume;
    result["sell_volume_hand"] = sell_volume;
    result["neutral_volume_hand"] = neutral_volume;
    result["buy_amount_yuan"] = buy_amount;
    result["sell_amount_yuan"] = sell_amount;
    result["neutral_amount_yuan"] = neutral_amount;
    Json counts = Json::object();
    for (const auto& [status, count] : statuses)
        counts[std::to_string(status)] = static_cast<std::uint64_t>(count);
    result["status_counts"] = std::move(counts);
    return result;
}

}  // namespace trades
}  // namespace detail

Json aggregate_trade_ticks(const std::vector<TradeTick>& ticks) {
    using detail::trades::aggregate_group;
    using detail::trades::time_label;

    std::map<int, std::vector<const TradeTick*>> groups;
    std::vector<const TradeTick*> all;
    all.reserve(ticks.size());
    for (const auto& tick : ticks) {
        groups[tick.time_minutes].push_back(&tick);
        all.push_back(&tick);
    }
    Json result = aggregate_group(all);
    result["first_time"] = ticks.empty() ? Json(nullptr) : Json(ticks.front().time_label);
    result["last_time"] = ticks.empty() ? Json(nullptr) : Json(ticks.back().time_label);
    result["minute_count"] = static_cast<std::uint64_t>(groups.size());
    auto minute_summary = [&](int minute, const char* execution_key) {
        const auto found = groups.find(minute);
        const std::vector<const TradeTick*> empty;
        Json value = aggregate_group(found == groups.end() ? empty : found->second);
        value["execution_status"] = found == groups.end() ?
            std::string("no-") + execution_key + "-trade" :
            std::string("executed-") + execution_key;
        return value;
    };
    result["auction_0925"] = minute_summary(9 * 60 + 25, "0925");
    result["closing_1500"] = minute_summary(15 * 60, "1500");
    std::vector<const TradeTick*> post_close;
    for (const auto& tick : ticks)
        if (tick.status_raw == 5 && tick.time_minutes > 15 * 60) post_close.push_back(&tick);
    Json post = aggregate_group(post_close);
    post["first_time"] = post_close.empty() ? Json(nullptr) : Json(post_close.front()->time_label);
    post["last_time"] = post_close.empty() ? Json(nullptr) : Json(post_close.back()->time_label);
    result["post_close_status_5"] = std::move(post);
    Json minutes = Json::array();
    for (const auto& [minute, values] : groups) {
        Json value = aggregate_group(values);
        value["time"] = time_label(minute);
        value["time_minutes"] = minute;
        minutes.push_back(std::move(value));
    }
    result["minutes"] = std::move(minutes);
    return result;
}

}  // namespace tdx
