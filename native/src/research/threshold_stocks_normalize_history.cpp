#include "threshold_stocks_internal.hpp"

#include <algorithm>

namespace tdx {

Json normalize_threshold_history_rows(const Json& rows,
                                      const std::string& universe_value) {
    using namespace detail::threshold_stocks;
    if (!rows.is_array()) throw Error("threshold-stock history rows must be an array");
    const auto& universe = universe_spec(universe_value);
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto date = text_value(raw, "rq");
        const auto key = text_value(raw, "$ZQDM");
        if (!digits(date, 8) || !digits(key, 9)) continue;
        Json row = Json::object();
        row["universe"] = universe.id;
        row["date"] = date;
        row["total_count"] = number_json(number_value(raw, "zjs"));
        Json indexes = Json::object();
        indexes["shanghai_composite_change_pct"] = number_json(number_value(raw, "szzs"));
        indexes["chinext_change_pct"] = number_json(number_value(raw, "zzcz"));
        indexes["star_50_change_pct"] = number_json(number_value(raw, "hssb"));
        row["index_change_pct"] = std::move(indexes);
        if (universe.kind == UniverseKind::high_price) {
            row["hundred_to_thousand_yuan_count"] = number_json(number_value(raw, "qyjs"));
            row["thousand_yuan_or_more_count"] = number_json(number_value(raw, "byjs"));
        } else {
            row["trillion_yuan_market_cap_count"] = number_json(number_value(raw, "wyjs"));
            row["hundred_billion_to_trillion_yuan_count"] = number_json(number_value(raw, "qyjs"));
        }
        row["aggregate_market_cap_yuan"] = number_json(number_value(raw, "zsz"));
        row["aggregate_market_share_pct"] = number_json(number_value(raw, "zaf"));
        row["entered_count"] = count_json(raw, "rwjs");
        row["exited_count"] = count_json(raw, "dtjs");
        row["detail_key"] = key;
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return left.at("date").as_string() > right.at("date").as_string();
        });
    return result;
}

Json normalize_threshold_trend_rows(const Json& rows,
                                    const std::string& universe_value) {
    using namespace detail::threshold_stocks;
    if (!rows.is_array()) throw Error("threshold-stock trend rows must be an array");
    const auto& universe = universe_spec(universe_value);
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto date = text_value(raw, "date");
        if (!digits(date, 8)) continue;
        Json row = Json::object();
        row["date"] = date;
        row["count"] = number_json(number_value(raw, universe.trend_count_field));
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return left.at("date").as_string() < right.at("date").as_string();
        });
    return result;
}

}  // namespace tdx
