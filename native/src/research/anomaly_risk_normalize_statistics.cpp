#include "anomaly_risk_internal.hpp"

#include <cmath>
#include <set>

namespace tdx {

Json normalize_anomaly_statistics_rows(const Json& rows, const BlockData& blocks) {
    using namespace detail::anomaly_risk;
    if (!rows.is_array()) throw Error("anomaly statistics rows must be an array");
    Json result = Json::array();
    std::set<std::string> identities;
    for (const auto& row : rows.as_array()) {
        if (!row.is_object()) continue;
        const auto code = text(row, "N001");
        if (code.empty()) continue;
        const int market = row_market_id(row, "N002");
        const auto identity = market_prefix(market) + code;
        if (!identities.insert(identity).second)
            throw Error("anomaly statistics contain duplicate security: " + identity);
        const auto index_code = text(row, "N014");
        const int index_market = row_market_id(row, "N015");
        Json item = Json::object();
        item["security"] = identity_document(
            blocks, market, code, text(row, "N023"), "security");
        item["listing_board"] = text(row, "N003");
        item["classification_index"] = identity_document(
            blocks, index_market, index_code, text(row, "N016"),
            "classification-index");
        Json windows = Json::object();
        windows["days_3"] = window_document(
            row, 3, "N004", "N005", "N006", "N017", "N020");
        windows["days_10"] = window_document(
            row, 10, "N007", "N008", "N009", "N018", "N021");
        windows["days_10"]["positive_anomaly_count"] = number_json(number(row, "N010"));
        windows["days_30"] = window_document(
            row, 30, "N011", "N012", "N013", "N019", "N022");
        item["windows"] = std::move(windows);
        Json live = Json::object();
        live["security_price"] = number_json(number(row, "N024"));
        live["security_change_pct"] = number_json(number(row, "N025"));
        live["classification_index_price"] = number_json(number(row, "N026"));
        live["classification_index_change_pct"] = number_json(number(row, "N027"));
        live["reported_deviation_pct"] = number_json(number(row, "N028"));
        const auto security_change = number(row, "N025");
        const auto index_change = number(row, "N027");
        const auto reported_deviation = number(row, "N028");
        const auto computed_deviation = security_change && index_change
            ? std::optional<double>(*security_change - *index_change) : std::nullopt;
        live["computed_deviation_pct"] = number_json(computed_deviation);
        live["deviation_consistent_with_rounded_quotes"] =
            computed_deviation && reported_deviation
                ? Json(std::abs(*computed_deviation - *reported_deviation) <= 0.02)
                : Json(nullptr);
        item["live_snapshot"] = std::move(live);
        item["warning"] = warning_evidence(row);
        Json unresolved = Json::object();
        for (int column = 29; column <= 37; ++column) {
            const auto key = "N" + (column < 100 ? std::string("0") : std::string{}) +
                             std::to_string(column);
            unresolved[key] = number_json(number(row, key));
        }
        item["upstream_auxiliary_unresolved"] = std::move(unresolved);
        result.push_back(std::move(item));
    }
    return result;
}

}  // namespace tdx
