#include "anomaly_risk_internal.hpp"

#include <sstream>

namespace tdx::detail::anomaly_risk {

QueryPlan make_query_plan(const AnomalyRiskQuery& input) {
    QueryPlan plan;
    plan.options = input;
    plan.view = &view_spec(plan.options.view);
    plan.options.view = plan.view->id;
    plan.options.query = trim(plan.options.query);
    plan.options.market = lower_ascii(trim(plan.options.market));
    if (plan.options.market == "all") plan.options.market.clear();
    if (!plan.options.market.empty() && plan.options.market != "sz" &&
        plan.options.market != "sh" && plan.options.market != "bj")
        throw Error("market must be sz, sh, bj, or empty");
    plan.options.code = trim(plan.options.code);
    plan.warning_filter = warning_filter_spec(plan.options.warning);
    plan.options.warning = plan.warning_filter
        ? plan.warning_filter->status : "all";
    if (!plan.options.code.empty() &&
        (plan.options.code.size() != 6 || !digits(plan.options.code)))
        throw Error("code must contain exactly six digits");
    if (plan.view->kind == ViewKind::statistics && plan.warning_filter)
        throw Error("warning status filter is only valid for suspension-risk view");
    if (plan.options.limit < 1 || plan.options.limit > 10000 ||
        plan.options.cache_ttl_seconds < 0 ||
        plan.options.cache_ttl_seconds > 3600 ||
        plan.options.timeout_ms < 100 || plan.options.timeout_ms > 60000)
        throw Error("anomaly-risk query limits are invalid");
    if (plan.view->kind == ViewKind::statistics)
        plan.overrides = {{"sortTpye", "1"}, {"sort", "1"}};
    std::ostringstream key;
    key << plan.options.view << '|' << plan.options.query << '|'
        << plan.options.market << '|' << plan.options.code << '|'
        << plan.options.warning << '|' << plan.options.warnings_only << '|'
        << plan.options.limit;
    plan.cache_key = key.str();
    return plan;
}

}  // namespace tdx::detail::anomaly_risk
