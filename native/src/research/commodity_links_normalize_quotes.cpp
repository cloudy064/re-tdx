#include "commodity_links_internal.hpp"

namespace tdx {
using namespace commodity_links_detail;

Json normalize_commodity_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("commodity rows must be an array");
    Json result = Json::array();
    std::uint64_t rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++rank;
        const auto id = text_value(raw, "$ZQDM");
        const auto name = text_value(raw, "mc");
        if (id.empty() || name.empty()) continue;
        const auto latest = number_value(raw, "zxjg");
        const auto previous = number_value(raw, "price0");
        const auto price5 = number_value(raw, "price1");
        const auto price10 = number_value(raw, "price2");
        const auto price30 = number_value(raw, "price3");
        const auto price60 = number_value(raw, "price4");
        Json row = Json::object();
        row["commodity_id"] = id;
        row["source_rank"] = rank;
        row["quote_id"] = id + ":" + std::to_string(rank);
        row["name"] = name;
        row["latest_price"] = number_json(latest);
        row["unit"] = text_value(raw, "jjdw");
        row["associated_industry"] = text_value(raw, "yjhy");
        row["quote_date"] = iso_date(text_value(raw, "bjrq"));
        row["previous_price"] = number_json(previous);
        row["price_5d"] = number_json(price5);
        row["price_10d"] = number_json(price10);
        row["price_30d"] = number_json(price30);
        row["price_60d"] = number_json(price60);
        row["day_change_amount"] = latest && previous
            ? Json(*latest - *previous) : Json(nullptr);
        row["day_change_pct"] = number_json(pct_change(latest, previous, true));
        row["change_5d_pct"] = number_json(pct_change(latest, price5));
        row["change_10d_pct"] = number_json(pct_change(latest, price10));
        row["change_30d_pct"] = number_json(pct_change(latest, price30));
        row["change_60d_pct"] = number_json(pct_change(latest, price60));
        row["description"] = text_value(raw, "qdlj");
        row["stocks_resource"] = "zjtc4/" + id + ".jsn";
        row["related_securities_resource"] = "zjtc5/" + id + ".jsn";
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}


}  // namespace tdx
