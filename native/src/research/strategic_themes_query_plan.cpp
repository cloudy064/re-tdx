#include "strategic_themes_internal.hpp"

namespace tdx::detail::strategic_themes {

QueryPlan make_query_plan(const StrategicThemeQuery& input) {
    QueryPlan plan;
    plan.options = input;
    plan.view = &view_spec(plan.options.view);
    plan.options.view = plan.view->id;
    plan.options.category = trim(plan.options.category);
    plan.options.theme_id = trim(plan.options.theme_id);
    plan.options.market = lower_ascii(trim(plan.options.market));
    plan.options.code = trim(plan.options.code);
    plan.options.query = trim(plan.options.query);
    plan.sort = &sort_spec(plan.options.sort);
    plan.options.sort = plan.sort->id;
    plan.options.order = lower_ascii(trim(plan.options.order));
    if (plan.view->requires_theme && plan.options.theme_id.empty())
        throw Error("theme view requires theme_id");
    if (plan.view->requires_security) {
        if (plan.options.market.empty() || !digits(plan.options.code, 6))
            throw Error("security view requires market and six-digit code");
        plan.selected_market = market_id(plan.options.market);
        plan.options.market = market_name(plan.selected_market);
    }
    if (plan.options.order != "asc" && plan.options.order != "desc")
        throw Error("order must be asc or desc");
    if (plan.options.offset < 0 || plan.options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (plan.options.limit < 1 || plan.options.limit > 5000)
        throw Error("limit must be in 1..5000");
    if (plan.options.master_cache_ttl_seconds < 0 ||
        plan.options.master_cache_ttl_seconds > 86400 ||
        plan.options.detail_cache_ttl_seconds < 0 ||
        plan.options.detail_cache_ttl_seconds > 86400)
        throw Error("cache TTL is outside the supported range");
    if (plan.options.timeout_ms < 100 || plan.options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    return plan;
}

}  // namespace tdx::detail::strategic_themes
