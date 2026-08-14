#include "market_internal.hpp"

namespace tdx::market_detail {

Json snapshot_json(
    const Snapshot &item,
    const std::map<std::pair<int, std::string>, Security> &names) {
    Json value = Json::object();
    value["security_id"] = item.security.display();
    value["market_id"] = item.security.market_id;
    value["code"] = item.security.code;
    const auto name = names.find(item.security.key());
    value["name"] = name == names.end() ? "" : name->second.name;
    value["active"] = static_cast<std::uint64_t>(item.active);
    value["last_price"] = item.last;
    value["pre_close_price"] = item.previous;
    value["open_price"] = item.open;
    value["high_price"] = item.high;
    value["low_price"] = item.low;
    value["change_pct"] = item.last > 0 && item.previous > 0
                              ? Json((item.last / item.previous - 1.0) * 100.0)
                              : Json(nullptr);
    value["time_raw"] = item.time_raw;
    value["auxiliary_price_delta_raw"] = item.auxiliary_price_delta_raw;
    value["fund_iopv"] = item.fund_iopv ? Json(*item.fund_iopv) : Json(nullptr);
    value["total_hand"] = item.total_hand;
    value["current_hand"] = item.current_hand;
    value["amount"] = item.amount;
    value["inside_dish"] = item.inside;
    value["outer_disc"] = item.outside;
    value["auction_imbalance_hand_raw"] = item.auction_imbalance_hand;
    value["open_amount_yuan"] = item.open_amount;
    return value;
}

namespace {

Json level_json(const Level &level) {
    Json value = Json::object();
    value["price"] = level.price;
    value["volume_hand"] = level.volume;
    value["amount_yuan"] = level.price * level.volume * 100.0;
    return value;
}

void append_levels(Json &value, const std::array<Level, 5> &buys,
                   const std::array<Level, 5> &sells) {
    Json buy_rows = Json::array();
    Json sell_rows = Json::array();
    for (std::size_t level = 0; level < 5; ++level) {
        buy_rows.push_back(level_json(buys[level]));
        sell_rows.push_back(level_json(sells[level]));
    }
    value["buy_levels"] = std::move(buy_rows);
    value["sell_levels"] = std::move(sell_rows);
}

} // namespace

Json speed_json(const Speed &item,
                const std::map<std::pair<int, std::string>, Security> &names) {
    Json value = snapshot_json(item.quote, names);
    value["rise_speed_pct"] = static_cast<double>(item.rise_speed_raw) / 100.0;
    value["rise_speed_raw"] = static_cast<std::int64_t>(item.rise_speed_raw);
    value["status_raw"] = static_cast<std::uint64_t>(item.status);
    value["extension_marker_raw"] =
        static_cast<std::uint64_t>(item.extension_marker);
    Json extension = Json::array();
    for (const auto raw : item.extension_values)
        extension.push_back(raw);
    value["extension_values_raw"] = std::move(extension);
    value["tail_raw"] = static_cast<std::uint64_t>(item.tail_raw);
    append_levels(value, item.buys, item.sells);
    return value;
}

Json depth_json(const Depth &item,
                const std::map<std::pair<int, std::string>, Security> &names) {
    Json value = snapshot_json(item.quote, names);
    value["update_time_raw"] = static_cast<std::uint64_t>(item.update_time);
    value["status_raw"] = item.status;
    value["unknown_after_outer_raw"] = item.unknown_after_outer;
    value["tail_hex"] = item.tail_hex;
    append_levels(value, item.buys, item.sells);
    value["bid1_amount_yuan"] =
        item.buys[0].price * item.buys[0].volume * 100.0;
    value["ask1_amount_yuan"] =
        item.sells[0].price * item.sells[0].volume * 100.0;
    if (item.quote.security.market_id == 1 &&
        item.quote.security.code == "999997") {
        // TCalc TOTALMMPAMO asks the host for this sentinel. Its five pairs
        // are aggregate amount components rather than an ordinary order book.
        Json raw_pairs = Json::array();
        for (std::size_t index = 0; index < item.first_level_raw.size(); ++index) {
            Json pair = Json::array();
            pair.push_back(item.first_level_raw[index]);
            pair.push_back(item.second_level_raw[index]);
            raw_pairs.push_back(std::move(pair));
        }
        Json fields = Json::array();
        fields.push_back(Json(nullptr));
        fields.push_back(item.first_level_raw[0] + item.second_level_raw[0]);
        fields.push_back(item.first_level_raw[1] + item.second_level_raw[1]);
        fields.push_back(item.first_level_raw[3] + item.second_level_raw[3]);
        fields.push_back(item.first_level_raw[4] + item.second_level_raw[4]);
        fields.push_back(Json(nullptr));
        fields.push_back(Json(nullptr));
        Json summary = Json::object();
        summary["schema"] = "tdx-total-market-order-amount-l1-v1";
        summary["unit"] = "100m-yuan";
        summary["depth_levels"] = 5;
        summary["values_by_selector"] = std::move(fields);
        summary["buy1_total"] =
            item.first_level_raw[0] + item.second_level_raw[0];
        summary["sell1_total"] =
            item.first_level_raw[1] + item.second_level_raw[1];
        summary["buy_depth_total"] =
            item.first_level_raw[3] + item.second_level_raw[3];
        summary["sell_depth_total"] =
            item.first_level_raw[4] + item.second_level_raw[4];
        summary["total_bid"] = Json(nullptr);
        summary["total_ask"] = Json(nullptr);
        summary["level2_required_selectors"] = Json::array();
        summary["level2_required_selectors"].push_back(5);
        summary["level2_required_selectors"].push_back(6);
        summary["raw_component_pairs"] = std::move(raw_pairs);
        value["level_semantics"] =
            "market-aggregate-sentinel; buy_levels/sell_levels are not an order book";
        value["market_amount_summary"] = std::move(summary);
    }
    return value;
}

Json quote_transport_metadata(int connection_attempts, int transient_retries,
                              int endpoints_attempted,
                              const CommonOptions &selected) {
    Json transport = Json::object();
    transport["connection_attempts"] = connection_attempts;
    transport["transient_retries"] = transient_retries;
    transport["endpoints_attempted"] = endpoints_attempted;
    transport["max_attempts_per_endpoint"] = 3;
    transport["recovered_after_retry"] = transient_retries > 0;
    transport["endpoint_source"] = selected.endpoint_source;
    transport["available_endpoint_count"] =
        static_cast<std::uint64_t>(selected.available_endpoint_count);
    transport["primary_configured"] = selected.primary_configured;
    transport["endpoint_failover"] = endpoints_attempted > 1;
    return transport;
}

namespace {

template <typename Row, typename Project>
Json downloaded_document(const BatchDownload<Row> &downloaded,
                         const CommonOptions &selected, QuoteSurface surface_kind,
                         const std::map<std::pair<int, std::string>, Security> &names,
                         Project project) {
    Json records = Json::array();
    for (const auto &item : downloaded.rows)
        records.push_back(project(item, names));
    const auto &surface = quote_surface(surface_kind);
    Json document = Json::object();
    document["schema"] = std::string(surface.schema);
    document["generated_at"] = now_text();
    document["command"] = std::string(surface.command_text);
    document["endpoint"] = downloaded.endpoint;
    document["server_name"] = downloaded.server;
    document["requested"] = static_cast<std::uint64_t>(selected.codes.size());
    document["received"] = static_cast<std::uint64_t>(downloaded.rows.size());
    document["transport"] = quote_transport_metadata(
        downloaded.connection_attempts, downloaded.transient_retries,
        downloaded.endpoints_attempted, selected);
    document["records"] = std::move(records);
    return document;
}

} // namespace

Json snapshot_document(
    const BatchDownload<Snapshot> &downloaded, const CommonOptions &selected,
    const std::map<std::pair<int, std::string>, Security> &names) {
    return downloaded_document(downloaded, selected, QuoteSurface::Snapshot, names,
                               snapshot_json);
}

Json speed_document(
    const BatchDownload<Speed> &downloaded, const CommonOptions &selected,
    const std::map<std::pair<int, std::string>, Security> &names) {
    return downloaded_document(downloaded, selected, QuoteSurface::Speed, names,
                               speed_json);
}

} // namespace tdx::market_detail
