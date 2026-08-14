#include "reverse_repo_internal.hpp"

namespace tdx::detail::reverse_repo {
QueryPlan make_query_plan(const ReverseRepoQuery& input) {
    QueryPlan plan; plan.options = input; plan.view = &view_spec(plan.options.view);
    plan.options.view = plan.view->id; plan.options.market = lower_ascii(trim(plan.options.market));
    plan.options.code = trim(plan.options.code); plan.options.query = trim(plan.options.query);
    plan.options.sort = lower_ascii(trim(plan.options.sort)); plan.options.order = lower_ascii(trim(plan.options.order));
    if (plan.options.market.empty()) plan.options.market = "all";
    if (plan.options.market != "all") { plan.selected_market = market_id(plan.options.market); plan.options.market = market_name(plan.selected_market); }
    if (!plan.options.code.empty() && plan.selected_market < 0) throw Error("code requires a concrete market");
    if (!plan.options.code.empty() && !digits(plan.options.code, 6)) throw Error("code must contain six digits");
    if (plan.view->requires_security && (plan.selected_market < 0 || plan.options.code.empty())) throw Error("security view requires market and code");
    if (plan.options.min_term_days < 0 || plan.options.min_term_days > 1000 || plan.options.max_term_days < 0 || plan.options.max_term_days > 1000) throw Error("term day filters must be in 0..1000");
    if (plan.options.min_term_days && plan.options.max_term_days && plan.options.min_term_days > plan.options.max_term_days) throw Error("min_term_days must not exceed max_term_days");
    if (plan.options.principal_yuan < 1000 || plan.options.principal_yuan > 1000000000) throw Error("principal_yuan must be in 1000..1000000000");
    if (plan.options.offset < 0 || plan.options.offset > 1000000) throw Error("offset must be in 0..1000000");
    if (plan.options.limit < 1 || plan.options.limit > 1000) throw Error("limit must be in 1..1000");
    if (plan.options.cache_ttl_seconds < 0 || plan.options.cache_ttl_seconds > 86400 || plan.options.quote_cache_ttl_seconds < 0 || plan.options.quote_cache_ttl_seconds > 3600) throw Error("cache TTL values are outside the allowed range");
    if (plan.options.timeout_ms < 100 || plan.options.timeout_ms > 60000) throw Error("timeout_ms must be in 100..60000");
    if (plan.view->kind != ViewKind::catalog) {
        plan.sort = &sort_spec(plan.options.sort);
        if (plan.options.order != "asc" && plan.options.order != "desc") throw Error("order must be asc or desc");
    }
    return plan;
}
}  // namespace tdx::detail::reverse_repo
