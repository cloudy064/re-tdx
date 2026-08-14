#include "thematic_opportunities_internal.hpp"

#include "tdx/common.hpp"

namespace tdx {

Json normalize_completed_hype_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::thematic_opportunities;
    if (!rows.is_array()) throw Error("completed-hype rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM1");
        if (!digits(code, 6)) continue;
        int market = -1;
        try { market = market_id(text_value(raw, "$SC1")); } catch (...) { continue; }
        Json row = Json::object();
        row["record_id"] = text_value(raw, "$ZQDM") + ":" + code;
        row["status"] = "completed";
        row["block_code"] = text_value(raw, "$ZQDM");
        row["block_name"] = text_value(raw, "bkmc");
        row["leader"] = security_document(
            market, code, securities, text_value(raw, "gpmc"));
        row["start_date"] = text_value(raw, "kssj");
        row["end_date"] = text_value(raw, "jssj");
        row["limit_pattern"] = text_value(raw, "jtjb");
        row["interval_return_pct"] = number_or_null(number_value(raw, "qjzf"));
        row["analysis"] = text_value(raw, "czfx");
        row["concept_id"] = text_value(raw, "gnid");
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    sort_hype(result, "desc");
    return result;
}

Json normalize_active_hype_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    using namespace detail::thematic_opportunities;
    if (!rows.is_array()) throw Error("active-hype rows must be an array");
    Json result = Json::array();
    for (const auto& raw : rows.as_array()) {
        const auto code = text_value(raw, "$ZQDM");
        if (!digits(code, 6)) continue;
        int market = -1;
        try { market = market_id(text_value(raw, "$SC")); } catch (...) { continue; }
        Json row = Json::object();
        row["record_id"] = market_prefix(market) + code;
        row["status"] = "active";
        row["security"] = security_document(
            market, code, securities, text_value(raw, "gpmc"));
        row["start_date"] = text_value(raw, "kssj");
        row["end_date"] = text_value(raw, "jssj");
        row["interval_stat"] = text_value(raw, "jtjb");
        row["stock_return_pct"] = number_or_null(number_value(raw, "ggzf"));
        row["shanghai_index_return_pct"] = number_or_null(number_value(raw, "zszf"));
        row["relative_return_pct"] =
            number_value(raw, "ggzf") && number_value(raw, "zszf")
                ? Json(*number_value(raw, "ggzf") - *number_value(raw, "zszf"))
                : Json(nullptr);
        row["raw"] = raw;
        result.push_back(std::move(row));
    }
    sort_hype(result, "desc");
    return result;
}

}  // namespace tdx
