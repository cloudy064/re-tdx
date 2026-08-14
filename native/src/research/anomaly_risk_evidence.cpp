#include "anomaly_risk_internal.hpp"

#include <cmath>

namespace tdx::detail::anomaly_risk {

std::optional<double> parse_number_text(const std::string& value) {
    try {
        std::size_t used = 0;
        const double parsed = std::stod(value, &used);
        if (used == value.size() && std::isfinite(parsed)) return parsed;
    } catch (...) {}
    return std::nullopt;
}

Json warning_evidence(const Json& row) {
    const auto value = text(row, "N038");
    const auto status = text(row, "N039");
    Json result = Json::object();
    result["text"] = value.empty() ? Json(nullptr) : Json(value);
    result["status_text"] = status.empty() ? Json(nullptr) : Json(status);
    result["status"] = status == "已触及" ? "reached"
        : status == "已公告" ? "announced" : status.empty() ? "none" : "reported";
    result["active"] = !value.empty();
    result["kind"] = value.rfind("严重异常波动", 0) == 0
        ? "severe-abnormal-volatility"
        : value.rfind("异常波动", 0) == 0 ? "abnormal-volatility"
        : value.empty() ? "none" : "other";
    std::optional<double> price, change;
    const auto marker = value.find("价格");
    const auto opening = marker == std::string::npos
        ? std::string::npos : value.find('(', marker);
    const auto closing = opening == std::string::npos
        ? std::string::npos : value.find("%)", opening);
    if (marker != std::string::npos && opening != std::string::npos) {
        price = parse_number_text(value.substr(
            marker + std::string("价格").size(),
            opening - marker - std::string("价格").size()));
        if (closing != std::string::npos)
            change = parse_number_text(
                value.substr(opening + 1, closing - opening - 1));
    }
    result["reference_price"] = number_json(price);
    result["reference_change_pct"] = number_json(change);
    return result;
}

Json window_document(const Json& row, int days,
                     std::string_view deviation_key,
                     std::string_view start_key,
                     std::string_view periods_key,
                     std::string_view security_return_key,
                     std::string_view index_return_key) {
    Json result = Json::object();
    result["window_days"] = days;
    result["deviation_pct"] = number_json(number(row, deviation_key));
    result["start_date"] = date_json(text(row, start_key));
    result["trading_periods"] = number_json(number(row, periods_key));
    result["security_return_pct"] = number_json(number(row, security_return_key));
    result["classification_index_return_pct"] =
        number_json(number(row, index_return_key));
    return result;
}

Json result_set_metadata(const Json& upstream, std::size_t decoded_rows) {
    Json result = Json::object();
    const auto* sets = field(upstream.at("response"), "ResultSets");
    if (!sets || !sets->is_array() || sets->size() == 0) return result;
    const auto& table = sets->as_array().front();
    const auto declared_rows = number(table, "RowNum");
    const auto declared_columns = number(table, "ColNum");
    const auto* descriptions = field(table, "ColDes");
    const std::size_t decoded_columns = descriptions && descriptions->is_array()
        ? descriptions->size() : 0;
    result["declared_row_count"] = number_json(declared_rows);
    result["decoded_row_count"] = static_cast<std::uint64_t>(decoded_rows);
    result["declared_column_count"] = number_json(declared_columns);
    result["decoded_column_count"] = static_cast<std::uint64_t>(decoded_columns);
    result["consistent"] = declared_rows && declared_columns &&
        static_cast<std::size_t>(*declared_rows) == decoded_rows &&
        static_cast<std::size_t>(*declared_columns) == decoded_columns;
    result["row_count_trusted"] = "decoded-content";
    return result;
}

Json source_document(const Json& upstream, const ViewSpec& view,
                     std::size_t decoded_rows) {
    Json result = Json::object();
    result["transport"] = "TQLEX reqformat=2";
    result["request_id"] = view.request_id;
    result["entry"] = upstream.at("entry");
    result["source_file"] = upstream.at("source_file");
    result["module"] = view.module;
    result["all_pages"] = upstream.at("all_pages");
    result["result_set_metadata"] = result_set_metadata(upstream, decoded_rows);
    return result;
}

}  // namespace tdx::detail::anomaly_risk
