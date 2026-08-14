#include "threshold_stocks_internal.hpp"

#include <cmath>

namespace tdx::detail::threshold_stocks {

const Json& selected_history(const Json& history, const std::string& requested) {
    if (history.size() == 0) throw Error("threshold-stock history is empty");
    if (requested.empty()) return history.as_array().front();
    if (!digits(requested, 8))
        throw Error("date must contain eight digits (YYYYMMDD)");
    for (const auto& row : history.as_array())
        if (json_text(row, "date") == requested) return row;
    throw Error("date is absent from the selected threshold-stock history: " + requested);
}

Json history_summary(const Json& history) {
    Json result = Json::object();
    if (history.size() == 0) {
        result["latest"] = Json(nullptr);
        result["earliest_date"] = Json(nullptr);
        result["periods"] = 0;
        return result;
    }
    result["latest"] = history.as_array().front();
    result["earliest_date"] = history.as_array().back().at("date");
    result["periods"] = static_cast<std::uint64_t>(history.size());
    return result;
}

Json member_summary(const Json& rows, const Json& period) {
    std::uint64_t active = 0, continuing = 0, entered = 0, exited = 0,
                  names_resolved = 0;
    for (const auto& row : rows.as_array()) {
        const auto status = json_text(row, "status");
        if (status != "exited") ++active;
        if (status == "continuing") ++continuing;
        else if (status == "entered") ++entered;
        else if (status == "exited") ++exited;
        if (row.at("security").at("name_resolved").as_bool()) ++names_resolved;
    }
    const auto expected_active = json_number(period, "total_count");
    const auto expected_entered = json_number(period, "entered_count");
    const auto expected_exited = json_number(period, "exited_count");
    Json result = Json::object();
    result["rows"] = static_cast<std::uint64_t>(rows.size());
    result["active_count"] = active;
    result["continuing_count"] = continuing;
    result["entered_count"] = entered;
    result["exited_count"] = exited;
    result["names_resolved"] = names_resolved;
    result["expected_active_count"] = number_json(expected_active);
    result["expected_entered_count"] = number_json(expected_entered);
    result["expected_exited_count"] = number_json(expected_exited);
    result["active_count_matches"] = expected_active
        ? Json(std::abs(*expected_active - static_cast<double>(active)) < 0.5)
        : Json(nullptr);
    result["entered_count_matches"] = expected_entered
        ? Json(std::abs(*expected_entered - static_cast<double>(entered)) < 0.5)
        : Json(nullptr);
    result["exited_count_matches"] = expected_exited
        ? Json(std::abs(*expected_exited - static_cast<double>(exited)) < 0.5)
        : Json(nullptr);
    return result;
}

}  // namespace tdx::detail::threshold_stocks
