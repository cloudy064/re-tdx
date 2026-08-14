#include "threshold_stocks_internal.hpp"

namespace tdx {

Json normalize_threshold_member_rows(
    const Json& rows, const std::string& universe_value,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::threshold_stocks;
    if (!rows.is_array()) throw Error("threshold-stock member rows must be an array");
    const auto& universe = universe_spec(universe_value);
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); }
        catch (...) { continue; }
        const auto status_label = text_value(raw, "ztbs");
        Json row = Json::object();
        row["universe"] = universe.id;
        row["security"] = security_document(id, code, securities);
        row["day_change_pct"] = number_json(number_value(raw, "zaf"));
        if (universe.kind == UniverseKind::high_price) {
            row["close_price_yuan"] = number_json(number_value(raw, "spj"));
            row["price_net_change_yuan"] = number_json(number_value(raw, "gjjz"));
        } else {
            row["market_cap_100m_yuan"] = number_json(number_value(raw, "spj"));
            row["market_cap_net_change_100m_yuan"] = number_json(number_value(raw, "gjjz"));
        }
        row["status"] = normalized_status(status_label);
        row["status_label"] = status_label;
        row["region"] = text_value(raw, "dq");
        row["controlling_shareholder"] = text_value(raw, "dgd");
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
