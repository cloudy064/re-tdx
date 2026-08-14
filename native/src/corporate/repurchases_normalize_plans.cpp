#include "repurchases_internal.hpp"

#include <algorithm>

namespace tdx {

using namespace detail::repurchases;

Json normalize_repurchase_plan_rows(
    const Json& rows,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    if (!rows.is_array()) throw Error("repurchase plan rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto code = text_value(row, "$ZQDM");
        if (!digits(code, 6)) continue;
        const int id = market_id(text_value(row, "$SC"));
        if (id == 31) continue;
        const auto planned_amount = number_value(row, "NHG5");
        const auto actual_amount = number_value(row, "SJ5");
        Json item = Json::object();
        item["security"] = security_document(id, code, securities);
        item["board_approval_date"] = text_value(row, "ggrq");
        item["start_date"] = text_value(row, "NHG1");
        item["end_date"] = text_value(row, "NHG2");
        item["cutoff_date"] = text_value(row, "date");
        item["planned_amount_upper_yuan"] = scaled_number(row, "NHG5", 10000.0);
        item["planned_price_upper"] = number_or_null(number_value(row, "NHG3"));
        item["planned_shares"] = scaled_number(row, "NHG4", 10000.0);
        item["actual_price_high"] = number_or_null(number_value(row, "SJ1"));
        item["actual_price_low"] = number_or_null(number_value(row, "SJ2"));
        item["actual_shares"] = scaled_number(row, "SJ3", 10000.0);
        item["actual_capital_pct"] = number_or_null(number_value(row, "SJ4"));
        item["actual_amount_yuan"] = scaled_number(row, "SJ5", 10000.0);
        item["amount_completion_pct"] = planned_amount && actual_amount &&
            *planned_amount > 0.0
            ? Json(*actual_amount / *planned_amount * 100.0) : Json(nullptr);
        item["completed"] = text_value(row, "SF") == "是";
        item["status"] = text_value(row, "SF");
        item["purpose"] = text_value(row, "HGYT");
        item["region"] = text_value(row, "dq");
        item["city"] = text_value(row, "cs");
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            if (text_value(left, "board_approval_date") !=
                text_value(right, "board_approval_date"))
                return text_value(left, "board_approval_date") >
                       text_value(right, "board_approval_date");
            return text_value(left.at("security"), "security_id") <
                   text_value(right.at("security"), "security_id");
        });
    return result;
}

}  // namespace tdx
