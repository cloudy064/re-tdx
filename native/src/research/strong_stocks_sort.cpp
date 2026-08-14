#include "strong_stocks_internal.hpp"

#include "tdx/common.hpp"

#include <algorithm>

namespace tdx {

using namespace detail::strong_stocks;

void sort_strong_stock_rows(Json& rows, const std::string& view_value,
                            const std::string& sort_value,
                            const std::string& order_value) {
    if (!rows.is_array()) throw Error("strong-stock sort requires an array");
    const auto view = lower_ascii(trim(view_value));
    const auto sort = lower_ascii(trim(sort_value));
    const auto order = lower_ascii(trim(order_value));
    if (!valid_view(view) || view == "catalog")
        throw Error("sort view must be intervals, security, or detail");
    const auto family = view_definition(view).sort_family;
    if (!valid_sort(family, sort)) {
        if (family == SortFamily::interval)
            throw Error("interval sort must be end-date, start-date, return, index-return, excess-return, days, limit-ups, source-rank, or code");
        throw Error("detail sort must be date, return, amount, market-limit-ups, or seal-rate");
    }
    if (order != "asc" && order != "desc") throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort == "end-date" || sort == "start-date" || sort == "date") {
                const auto key = sort == "end-date" ? "end_date" :
                    sort == "start-date" ? "start_date" : "date";
                const auto a = json_text(left, key), b = json_text(right, key);
                if (a != b) return descending ? a > b : a < b;
            } else if (sort != "code") {
                const auto a = sort_number(left, sort), b = sort_number(right, sort);
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return descending ? *a > *b : *a < *b;
            }
            if (view == "detail") {
                const auto a = json_text(left, "date"), b = json_text(right, "date");
                return descending ? a > b : a < b;
            }
            const auto a = left.at("security").at("security_id").as_string();
            const auto b = right.at("security").at("security_id").as_string();
            return sort == "code" && descending ? a > b : a < b;
        });
}

}  // namespace tdx

