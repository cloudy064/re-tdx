#include "tdx/trades_internal.hpp"

#include "tdx/common.hpp"

namespace tdx {
namespace detail {
namespace trades {

Json tick_json(const TradeTick& tick) {
    Json value = Json::object();
    value["index"] = static_cast<std::uint64_t>(tick.index);
    value["absolute_index"] = static_cast<std::uint64_t>(tick.absolute_index);
    value["time_minutes"] = static_cast<std::uint64_t>(tick.time_minutes);
    value["time_label"] = tick.time_label;
    value["price"] = tick.price;
    value["volume_hand"] = tick.volume_hand;
    value["amount_yuan"] = amount_yuan(tick);
    value["order_count"] = tick.order_count;
    value["status_raw"] = tick.status_raw;
    value["side"] = tick.side;
    value["price_delta_raw"] = tick.price_delta_raw;
    value["price_acc_raw"] = tick.price_acc_raw;
    value["tail_raw"] = tick.tail_raw;
    return value;
}

Json series_json(const TradeSeries& series,
                 const std::map<std::pair<int, std::string>, Security>& names) {
    Json value = Json::object();
    value["market_id"] = series.market_id;
    value["code"] = series.code;
    value["security_id"] = security_id(series.market_id, series.code);
    const auto found = names.find({series.market_id, series.code});
    value["name"] = found == names.end() ? "" : found->second.name;
    value["name_resolved"] = found != names.end();
    value["trading_date"] = series.trading_date;
    value["source_mode"] = series.source_mode;
    value["pages"] = static_cast<std::uint64_t>(series.pages);
    value["page_size"] = static_cast<std::uint64_t>(series.page_size);
    value["price_divisor"] = series.price_divisor;
    value["price_base_raw"] = series.price_base_raw
        ? Json(static_cast<double>(*series.price_base_raw)) : Json(nullptr);
    value["summary"] = aggregate_trade_ticks(series.ticks);
    Json ticks = Json::array();
    for (const auto& tick : series.ticks) ticks.push_back(tick_json(tick));
    value["ticks"] = std::move(ticks);
    return value;
}

}  // namespace trades
}  // namespace detail
}  // namespace tdx
