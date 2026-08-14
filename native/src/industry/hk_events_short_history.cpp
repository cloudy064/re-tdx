#include "hk_events_internal.hpp"

#include <cmath>
#include <deque>
#include <map>
#include <set>

namespace tdx {

Json normalize_hk_short_history(const Json& kline_document,
                                const Json& short_event_rows,
                                const std::string& market,
                                const std::string& code) {
    if (!kline_document.is_object() || !detail::json_field(kline_document, "bars") ||
        !kline_document.at("bars").is_array())
        throw Error("HK short history requires a K-line document with bars");
    if (!short_event_rows.is_array())
        throw Error("HK short history event rows must be an array");

    std::map<std::string, const Json*> events_by_date;
    std::string security_name;
    std::uint64_t event_row_count = 0;
    for (const auto& event : short_event_rows.as_array()) {
        if (detail::json_text(event, "kind") != "short-selling") continue;
        const auto* security = detail::json_field(event, "security");
        if (!security || security->is_null() ||
            detail::json_text(*security, "code") != code) continue;
        const auto event_market = detail::json_number_value(*security, "market_id");
        if (!event_market || std::to_string(static_cast<int>(*event_market)) != market)
            continue;
        const auto day = detail::hk_date_key(detail::json_text(event, "date"));
        if (day.empty()) continue;
        events_by_date[day] = &event;
        ++event_row_count;
        if (security_name.empty()) security_name = detail::json_text(*security, "name");
    }

    Json history = Json::array();
    std::deque<double> window5;
    std::deque<double> window20;
    double sum5 = 0.0;
    double sum20 = 0.0;
    std::optional<double> previous_short;
    std::set<std::string> matched_event_days;
    std::uint64_t exact_count = 0;
    std::uint64_t mismatch_count = 0;
    std::string earliest_date;
    std::string latest_date;

    auto push_window = [](std::deque<double>& values, double& sum,
                          double value, std::size_t width) -> Json {
        values.push_back(value);
        sum += value;
        if (values.size() > width) {
            sum -= values.front();
            values.pop_front();
        }
        return values.size() == width ? Json(sum / static_cast<double>(width))
                                      : Json(nullptr);
    };

    for (const auto& bar : kline_document.at("bars").as_array()) {
        const auto short_shares = detail::json_number_value(bar, "hk_short_volume");
        if (!short_shares) continue;
        const auto raw_date = detail::json_text(bar, "date");
        const auto compact_date = detail::hk_date_key(raw_date);
        if (compact_date.empty()) continue;
        const auto volume_lots = detail::json_number_value(bar, "volume");
        const auto volume_shares = volume_lots
            ? std::optional<double>(*volume_lots * 100.0) : std::nullopt;

        Json point = Json::object();
        point["date"] = raw_date;
        point["close_hkd"] = detail::json_number(bar, "close");
        point["short_shares"] = *short_shares;
        point["short_shares_10k"] = *short_shares / 10000.0;
        point["volume_lots"] = volume_lots ? Json(*volume_lots) : Json(nullptr);
        point["volume_shares"] = volume_shares ? Json(*volume_shares) : Json(nullptr);
        point["short_share_volume_pct"] =
            volume_shares && std::abs(*volume_shares) > 0.000001
                ? Json(*short_shares * 100.0 / *volume_shares) : Json(nullptr);
        point["short_volume_change_pct"] =
            previous_short && std::abs(*previous_short) > 0.000001
                ? Json((*short_shares - *previous_short) * 100.0 / *previous_short)
                : Json(nullptr);
        point["short_shares_ma5"] = push_window(window5, sum5, *short_shares, 5);
        point["short_shares_ma20"] = push_window(window20, sum20, *short_shares, 20);
        previous_short = *short_shares;

        const auto event = events_by_date.find(compact_date);
        if (event != events_by_date.end()) {
            matched_event_days.insert(compact_date);
            const auto event_shares = detail::json_number_value(*event->second, "short_shares");
            const bool exact = event_shares &&
                std::abs(*event_shares - *short_shares) <= 0.5;
            if (exact) ++exact_count;
            else ++mismatch_count;
            point["event_short_shares"] = event_shares ? Json(*event_shares) : Json(nullptr);
            point["event_short_amount_hkd"] = detail::json_number(*event->second,
                                                       "short_amount_currency_units");
            point["event_turnover_hkd"] = detail::json_number(*event->second,
                                                   "turnover_currency_units");
            point["event_short_turnover_pct"] = detail::json_number(*event->second,
                                                         "short_turnover_pct");
            point["event_exact_match"] = exact;
        } else {
            point["event_short_shares"] = Json(nullptr);
            point["event_short_amount_hkd"] = Json(nullptr);
            point["event_turnover_hkd"] = Json(nullptr);
            point["event_short_turnover_pct"] = Json(nullptr);
            point["event_exact_match"] = Json(nullptr);
        }
        if (earliest_date.empty() || compact_date < detail::hk_date_key(earliest_date))
            earliest_date = raw_date;
        if (latest_date.empty() || compact_date > detail::hk_date_key(latest_date))
            latest_date = raw_date;
        history.push_back(std::move(point));
    }

    Json security = Json::object();
    security["market_id"] = static_cast<std::uint64_t>(std::stoi(market));
    security["market"] = "hk";
    security["code"] = code;
    security["security_id"] = "HK" + code;
    security["name"] = security_name;
    security["name_resolved"] = !security_name.empty();

    Json reconciliation = Json::object();
    reconciliation["event_row_count"] = event_row_count;
    reconciliation["overlap_day_count"] =
        static_cast<std::uint64_t>(matched_event_days.size());
    reconciliation["exact_match_count"] = exact_count;
    reconciliation["mismatch_count"] = mismatch_count;
    reconciliation["event_only_day_count"] =
        static_cast<std::uint64_t>(events_by_date.size() - matched_event_days.size());
    reconciliation["history_only_day_count"] =
        static_cast<std::uint64_t>(history.size() - matched_event_days.size());
    reconciliation["all_overlaps_exact"] =
        !matched_event_days.empty() && mismatch_count == 0 &&
        exact_count == matched_event_days.size();

    Json summary = Json::object();
    summary["history_count"] = static_cast<std::uint64_t>(history.size());
    summary["earliest_date"] = earliest_date;
    summary["latest_date"] = latest_date;
    if (!history.as_array().empty()) {
        const auto& latest = history.as_array().back();
        for (const auto* name : {"close_hkd", "short_shares", "short_shares_10k",
                                 "volume_shares", "short_share_volume_pct",
                                 "short_shares_ma5", "short_shares_ma20"})
            summary[std::string("latest_") + name] = latest.at(name);
    }

    Json source = Json::object();
    for (const auto* name : {"endpoint", "server_name", "transport", "period",
                             "period_id", "auxiliary_field", "volume_unit"})
        if (const auto* value = detail::json_field(kline_document, name)) source[name] = *value;
    source["market"] = market;
    source["code"] = code;
    source["volume_lot_size_shares"] = 100;

    Json result = Json::object();
    result["schema"] = "tdx-market-hk-short-history-native-v1";
    result["security"] = std::move(security);
    result["summary"] = std::move(summary);
    result["reconciliation"] = std::move(reconciliation);
    result["history"] = std::move(history);
    result["source"] = std::move(source);
    result["semantics"] =
        "7727 expansion daily K-line auxiliary_price is HK short-selling shares for markets 31/48. "
        "The 100-share volume lot conversion and rolling/change ratios are local derivatives; "
        "overlapping GGRL104 event rows are retained as an independent exact-share reconciliation.";
    return result;
}

}  // namespace tdx
