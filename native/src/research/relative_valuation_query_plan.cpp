#include "relative_valuation_internal.hpp"

namespace tdx::detail::relative_valuation {

QueryPlan make_query_plan(const RelativeValuationQuery& input) {
    QueryPlan plan;
    plan.options = input;
    plan.index_type = &index_type_spec(plan.options.index_type);
    plan.benchmark = &benchmark_spec(plan.options.benchmark);
    plan.method = &method_spec(plan.options.method);
    plan.options.index_type = plan.index_type->code;
    plan.options.benchmark = plan.benchmark->code;
    plan.options.method = plan.method->code;
    plan.options.code = trim(plan.options.code);
    if (!plan.options.code.empty() &&
        (plan.options.code.size() != 6 || !digits(plan.options.code)))
        throw Error("code must contain six digits");
    const auto today = today_text();
    plan.options.end_date = compact_date(
        plan.options.end_date.empty() ? today : plan.options.end_date, "end_date");
    plan.options.start_date = compact_date(
        plan.options.start_date.empty()
            ? years_before(plan.options.end_date, 2) : plan.options.start_date,
        "start_date");
    if (plan.options.start_date > plan.options.end_date)
        throw Error("start_date must not exceed end_date");
    if (plan.options.limit < 1 || plan.options.limit > 20000 ||
        plan.options.cache_ttl_seconds < 0 ||
        plan.options.cache_ttl_seconds > 86400 ||
        plan.options.timeout_ms < 100 || plan.options.timeout_ms > 60000)
        throw Error("relative valuation query limits are invalid");

    plan.replacements = {
        {"StartDate", plan.options.start_date},
        {"EndDate", plan.options.end_date},
        {"IndexType", plan.options.index_type},
        {"Basics", plan.options.benchmark},
        {"Methods", plan.options.method},
    };
    plan.master_cache_key = plan.options.index_type + "|" +
        plan.options.benchmark + "|" + plan.options.method + "|" +
        plan.options.start_date + "|" + plan.options.end_date;
    return plan;
}

}  // namespace tdx::detail::relative_valuation
