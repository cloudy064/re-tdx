#include "threshold_stocks_internal.hpp"

namespace tdx::detail::threshold_stocks {

QueryPlan make_query_plan(const ThresholdStocksQuery& input) {
    QueryPlan plan;
    plan.options = input;
    plan.view = &view_spec(plan.options.view);
    plan.universe = &universe_spec(plan.options.universe);
    plan.options.view = plan.view->id;
    plan.options.universe = plan.universe->id;
    plan.options.status = lower_ascii(trim(plan.options.status));
    plan.options.market = lower_ascii(trim(plan.options.market));
    plan.options.code = trim(plan.options.code);
    plan.options.sort = lower_ascii(trim(plan.options.sort));
    plan.options.order = lower_ascii(trim(plan.options.order));
    if (plan.options.status != "all" && plan.options.status != "continuing" &&
        plan.options.status != "entered" && plan.options.status != "exited")
        throw Error("status must be all, continuing, entered, or exited");
    if (plan.options.market.empty() != plan.options.code.empty())
        throw Error("market and code must be provided together");
    if (!plan.options.market.empty()) {
        plan.selected_market = market_id(plan.options.market);
        plan.options.market = market_name(plan.selected_market);
        if (!digits(plan.options.code, 6)) throw Error("code must contain six digits");
    }
    if (plan.view->requires_security && plan.options.code.empty())
        throw Error("security view requires market and code");
    if (plan.options.offset < 0 || plan.options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (plan.options.limit < 1 || plan.options.limit > 10000)
        throw Error("limit must be in 1..10000");
    if (plan.options.cache_ttl_seconds < 0 || plan.options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (plan.options.timeout_ms < 100 || plan.options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    if (plan.view->sort_domain != SortDomain::none) {
        plan.sort = &sort_spec(plan.view->sort_domain, plan.options.sort);
        if (plan.options.order != "asc" && plan.options.order != "desc")
            throw Error("order must be asc or desc");
    }
    return plan;
}

}  // namespace tdx::detail::threshold_stocks
