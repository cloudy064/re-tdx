#include "capital_strength_internal.hpp"

namespace tdx::detail::capital_strength {

QueryPlan make_query_plan(const CapitalStrengthQuery& input) {
    QueryPlan plan;
    plan.options = input;
    plan.view = &view_spec(plan.options.view);
    plan.options.view = plan.view->id;
    plan.options.period = period_spec(plan.options.period).id;
    plan.options.market = lower_ascii(trim(plan.options.market));
    plan.options.code = trim(plan.options.code);
    plan.options.query = trim(plan.options.query);
    plan.options.sort = lower_ascii(trim(plan.options.sort));
    plan.options.order = lower_ascii(trim(plan.options.order));
    if (plan.options.market.empty() != plan.options.code.empty())
        throw Error("market and code must be provided together");
    if (!plan.options.market.empty()) {
        plan.selected_market = market_id(plan.options.market);
        plan.options.market = market_name(plan.selected_market);
        if (!digits(plan.options.code, 6)) throw Error("code must contain six digits");
    }
    if (plan.view->requires_security && plan.options.code.empty())
        throw Error("security view requires market and code");
    if (plan.options.sort.empty()) plan.options.sort = plan.view->default_sort;
    if (plan.options.order.empty()) plan.options.order = plan.view->default_order;
    if (plan.options.min_periods < 1 || plan.options.min_periods > 5)
        throw Error("min_periods must be in 1..5");
    if (plan.options.offset < 0 || plan.options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (plan.options.limit < 1 || plan.options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (plan.options.cache_ttl_seconds < 0 || plan.options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (plan.options.timeout_ms < 100 || plan.options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");

    if (plan.view->kind != ViewKind::catalog)
        plan.sort = &sort_spec(plan.view->kind, plan.options.sort);
    if (plan.view->kind == ViewKind::ranking)
        plan.periods.push_back(&period_spec(plan.options.period));
    else if (plan.view->all_periods)
        for (const auto& period : all_period_specs()) plan.periods.push_back(&period);
    return plan;
}

Json filter_capital_strength_records(const Json& records, const QueryPlan& plan) {
    Json filtered = Json::array();
    const auto needle = lower_ascii(plan.options.query);
    for (const auto& row : records.as_array()) {
        if (plan.view->kind == ViewKind::catalog) {
            if (needle.empty() || json_contains(row, needle)) filtered.push_back(row);
            continue;
        }
        const auto& security = row.at("security");
        if (plan.selected_market >= 0 && plan.view->kind != ViewKind::security &&
            static_cast<int>(security.at("market_id").as_number()) != plan.selected_market)
            continue;
        if (!plan.options.code.empty() && plan.view->kind != ViewKind::security &&
            security.at("code").as_string() != plan.options.code)
            continue;
        if (plan.view->kind == ViewKind::confluence &&
            row.at("period_count").as_number() < plan.options.min_periods)
            continue;
        if (!needle.empty() && !json_contains(row, needle)) continue;
        filtered.push_back(row);
    }
    return filtered;
}

}  // namespace tdx::detail::capital_strength
