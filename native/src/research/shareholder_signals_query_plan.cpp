#include "shareholder_signals_internal.hpp"

#include <algorithm>

namespace tdx::detail::shareholder_signals {

QueryPlan make_query_plan(const ShareholderSignalsQuery& input) {
    QueryPlan plan;
    plan.options = input;
    plan.view = &view_spec(plan.options.view);
    plan.options.view = plan.view->id;
    plan.options.query = trim(plan.options.query);
    plan.options.market = lower_ascii(trim(plan.options.market));
    plan.options.code = trim(plan.options.code);
    plan.options.investor_id = trim(plan.options.investor_id);
    plan.options.investor_query = trim(plan.options.investor_query);
    plan.sort = &sort_spec(plan.options.sort);
    plan.options.sort = plan.sort->id;
    plan.options.order = lower_ascii(trim(plan.options.order));
    if (plan.options.limit < 1 || plan.options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (plan.options.market.empty() != plan.options.code.empty())
        throw Error("market and code must be provided together");
    plan.selected_market = plan.options.market;
    if (!plan.selected_market.empty() && plan.selected_market != "sz" &&
        plan.selected_market != "sh" && plan.selected_market != "bj")
        throw Error("market must be sz/sh/bj");
    if (!plan.options.code.empty() && !digits(plan.options.code))
        throw Error("code must contain six digits");
    if (plan.options.order != "asc" && plan.options.order != "desc")
        throw Error("order must be asc or desc");
    return plan;
}

void sort_signal_rows(Json& rows, const SortSpec& sort, std::string_view order) {
    const bool ascending = order == "asc";
    std::sort(rows.as_array().begin(), rows.as_array().end(),
        [&](const Json& left, const Json& right) {
            const auto lid = left.at("record_id").as_string();
            const auto rid = right.at("record_id").as_string();
            if (sort.kind == SortKind::code) return ascending ? lid < rid : lid > rid;
            const auto lv = normalized_number(left, sort.field);
            const auto rv = normalized_number(right, sort.field);
            if (lv && rv && *lv != *rv) return ascending ? *lv < *rv : *lv > *rv;
            if (lv.has_value() != rv.has_value()) return lv.has_value();
            return lid < rid;
        });
}

}  // namespace tdx::detail::shareholder_signals
