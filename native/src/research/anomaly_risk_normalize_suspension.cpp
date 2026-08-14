#include "anomaly_risk_internal.hpp"

#include <set>

namespace tdx {

Json normalize_suspension_risk_rows(const Json& rows, const BlockData& blocks) {
    using namespace detail::anomaly_risk;
    if (!rows.is_array()) throw Error("suspension risk rows must be an array");
    Json result = Json::array();
    std::set<std::string> identities;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object()) continue;
        const auto code = text(row, "N001");
        if (code.empty()) continue;
        const int market = row_market_id(row, "N002");
        const auto identity = market_prefix(market) + code;
        if (!identities.insert(identity).second)
            throw Error("suspension risk contains duplicate security: " + identity);
        const auto index_code = text(row, "N015");
        const int index_market = row_market_id(row, "N016");
        const auto warning_number = number(row, "N014");
        const int warning_code = warning_number ? static_cast<int>(*warning_number) : 0;
        const auto& warning_specification = warning_spec(warning_code);
        Json item = Json::object();
        item["security"] = identity_document(blocks, market, code, {}, "security");
        item["listing_board"] = text(row, "N003");
        item["classification_index"] = identity_document(
            blocks, index_market, index_code, {}, "classification-index");
        Json anomaly = Json::object();
        anomaly["latest_date"] = date_json(text(row, "N004"));
        anomaly["suspension_date"] = date_json(text(row, "N005"));
        anomaly["resumption_date"] = date_json(text(row, "N006"));
        anomaly["calculation_date"] = date_json(text(row, "N007"));
        anomaly["deviation_start_date"] = date_json(text(row, "N008"));
        anomaly["deviation_end_date"] = date_json(text(row, "N009"));
        const auto deviation_ratio = number(row, "N010");
        anomaly["reported_deviation_ratio"] = number_json(deviation_ratio);
        anomaly["deviation_pct"] = deviation_ratio
            ? Json(*deviation_ratio * 100.0) : Json(nullptr);
        anomaly["trigger_standard_pct"] = number_json(number(row, "N011"));
        anomaly["trigger_price"] = number_json(number(row, "N012"));
        anomaly["recent_10_day_anomaly_count"] = number_json(number(row, "N013"));
        item["anomaly"] = std::move(anomaly);
        Json warning = Json::object();
        warning["code"] = warning_specification.code;
        warning["status"] = warning_specification.status;
        warning["label"] = warning_specification.label;
        warning["active"] = warning_specification.active;
        item["warning"] = std::move(warning);
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx
