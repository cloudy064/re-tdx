#include "capital_strength_internal.hpp"

namespace tdx {

Json normalize_capital_strength_rows(
    const Json& rows, const std::string& period_value,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::capital_strength;
    if (!rows.is_array()) throw Error("capital-strength rows must be an array");
    const auto& spec = period_spec(period_value);
    Json result = Json::array();
    std::uint64_t source_rank = 0;
    for (const auto& raw : rows.as_array()) {
        ++source_rank;
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int id = -1;
        try { id = market_id(text_value(raw, "$SC")); }
        catch (...) { continue; }
        Json row = Json::object();
        row["period"] = spec.id;
        row["period_label"] = spec.label;
        row["source_rank"] = source_rank;
        row["security"] = security_document(id, code, securities);
        row["statistics_date"] = text_value(raw, "date");
        row["float_shares"] = number_json(number_value(raw, "ltgb"));
        row["period_return_pct"] = number_json(number_value(raw, spec.return_key));
        row["total_net_inflow_yuan"] = number_json(number_value(raw, spec.total_net_key));
        row["main_net_inflow_yuan"] = number_json(number_value(raw, spec.main_net_key));
        row["ddx_float_share_pct"] = number_json(number_value(raw, "ddx"));
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    return result;
}

}  // namespace tdx
