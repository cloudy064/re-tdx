#include "capital_strength_internal.hpp"

#include <algorithm>

namespace tdx::detail::capital_strength {

std::optional<double> row_sort_metric(const Json& row, const SortSpec& sort) {
    if (sort.kind == SortKind::code) return std::nullopt;
    if (sort.kind == SortKind::period)
        return static_cast<double>(period_order(json_text(row, "period")));
    return json_number(row, sort.field);
}

}  // namespace tdx::detail::capital_strength

namespace tdx {

void sort_capital_strength_rows(Json& rows, const std::string& view_value,
                                const std::string& sort_value,
                                const std::string& order_value) {
    using namespace detail::capital_strength;
    if (!rows.is_array()) throw Error("capital-strength sort requires an array");
    const auto view_name = lower_ascii(trim(view_value));
    if (view_name != "ranking" && view_name != "confluence" && view_name != "security")
        throw Error("sort view must be ranking, confluence, or security");
    const auto& view = view_spec(view_name);
    const auto& sort = sort_spec(view.kind, sort_value);
    const auto order = lower_ascii(trim(order_value));
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort.kind != SortKind::code) {
                const auto a = row_sort_metric(left, sort);
                const auto b = row_sort_metric(right, sort);
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return descending ? *a > *b : *a < *b;
            }
            if (view.kind == ViewKind::confluence &&
                sort.kind == SortKind::period_count) {
                const auto a = json_number(left, "average_ddx_float_share_pct");
                const auto b = json_number(right, "average_ddx_float_share_pct");
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return *a > *b;
            }
            const auto a = left.at("security").at("security_id").as_string();
            const auto b = right.at("security").at("security_id").as_string();
            return sort.kind == SortKind::code && descending ? a > b : a < b;
        });
}

}  // namespace tdx
