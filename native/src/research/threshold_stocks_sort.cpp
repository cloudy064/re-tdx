#include "threshold_stocks_internal.hpp"

#include <algorithm>

namespace tdx::detail::threshold_stocks {

std::optional<double> sort_metric(const Json& row, const SortSpec& sort) {
    switch (sort.kind) {
    case SortKind::count:
        return json_number(row, "total_count");
    case SortKind::market_cap:
        return json_number(row, "aggregate_market_cap_yuan");
    case SortKind::market_share:
        return json_number(row, "aggregate_market_share_pct");
    case SortKind::entered:
        return json_number(row, "entered_count");
    case SortKind::exited:
        return json_number(row, "exited_count");
    case SortKind::day_return:
        return json_number(row, "day_change_pct");
    case SortKind::threshold_value: {
        const auto value = json_number(row, "close_price_yuan");
        return value ? value : json_number(row, "market_cap_100m_yuan");
    }
    case SortKind::net_increase: {
        const auto value = json_number(row, "price_net_change_yuan");
        return value ? value : json_number(row, "market_cap_net_change_100m_yuan");
    }
    case SortKind::status: {
        const auto status = json_text(row, "status");
        return status == "entered" ? 2.0 : status == "exited" ? 0.0 : 1.0;
    }
    case SortKind::date:
    case SortKind::code:
        return std::nullopt;
    }
    return std::nullopt;
}

void sort_rows(Json& rows, const ViewSpec& view, const SortSpec& sort,
               const std::string& order) {
    if (!rows.is_array()) throw Error("threshold-stock sort requires an array");
    if (order != "asc" && order != "desc")
        throw Error("order must be asc or desc");
    const bool descending = order == "desc";
    std::stable_sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            if (sort.kind == SortKind::date) {
                const auto a = json_text(left, "date");
                const auto b = json_text(right, "date");
                if (a != b) return descending ? a > b : a < b;
            } else if (sort.kind != SortKind::code) {
                const auto a = sort_metric(left, sort);
                const auto b = sort_metric(right, sort);
                if (a.has_value() != b.has_value()) return a.has_value();
                if (a && b && *a != *b) return descending ? *a > *b : *a < *b;
            }
            if (view.sort_domain == SortDomain::members) {
                const auto a = left.at("security").at("security_id").as_string();
                const auto b = right.at("security").at("security_id").as_string();
                return sort.kind == SortKind::code && descending ? a > b : a < b;
            }
            return false;
        });
}

}  // namespace tdx::detail::threshold_stocks

namespace tdx {

void sort_threshold_rows(Json& rows, const std::string& view_value,
                         const std::string& sort_value,
                         const std::string& order_value) {
    using namespace detail::threshold_stocks;
    const auto& view = view_spec(view_value);
    if (view.sort_domain == SortDomain::none)
        throw Error("sort view must be history, members, or security");
    const auto& sort = sort_spec(view.sort_domain, sort_value);
    sort_rows(rows, view, sort, lower_ascii(trim(order_value)));
}

}  // namespace tdx
