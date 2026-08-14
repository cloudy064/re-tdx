#include "repurchases_internal.hpp"

#include <algorithm>

namespace tdx {

using namespace detail::repurchases;

Json normalize_repurchase_month_rows(const Json& rows) {
    if (!rows.is_array()) throw Error("repurchase month rows must be an array");
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        const auto month = text_value(row, "YF");
        if (!digits(month, 6)) continue;
        const auto planned = number_value(row, "NHG5");
        const auto actual = number_value(row, "SJ5");
        Json item = Json::object();
        item["month"] = month;
        item["planned_shares"] = scaled_number(row, "NHG4", 100000000.0);
        item["planned_amount_yuan"] = scaled_number(row, "NHG5", 100000000.0);
        item["planned_capital_pct"] = number_or_null(number_value(row, "NHG6"));
        item["actual_shares"] = scaled_number(row, "SJ3", 100000000.0);
        item["actual_amount_yuan"] = scaled_number(row, "SJ5", 100000000.0);
        item["actual_capital_pct"] = number_or_null(number_value(row, "SJ4"));
        item["completion_pct"] = planned && actual && *planned > 0.0
            ? Json(*actual / *planned * 100.0) : Json(nullptr);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "month") > text_value(right, "month");
        });
    return result;
}

Json normalize_repurchase_annual_rows(const Json& rows,
                                      const std::string& segment) {
    if (!rows.is_array()) throw Error("repurchase annual rows must be an array");
    const auto& spec = annual_spec(segment);
    Json result = Json::array();
    for (const auto& row : rows.as_array()) {
        auto year = text_value(row, "$ZQDM");
        if (year.empty()) year = text_value(row, "date");
        if (!valid_year(year) && !digits(year, 6)) continue;
        Json item = Json::object();
        item["segment"] = spec.id;
        item["segment_label"] = spec.label;
        item["year"] = year;
        item["shares"] = scaled_number(row, "hgsl", 10000.0);
        item["amount_yuan"] = scaled_number(row, "hgsz", 100000000.0);
        item["company_count"] = number_or_null(number_value(row, "hggps"));
        item["financing_amount_yuan"] = scaled_number(row, "rzje", 100000000.0);
        result.push_back(std::move(item));
    }
    std::stable_sort(result.as_array().begin(), result.as_array().end(),
        [](const Json& left, const Json& right) {
            return text_value(left, "year") > text_value(right, "year");
        });
    return result;
}

}  // namespace tdx
