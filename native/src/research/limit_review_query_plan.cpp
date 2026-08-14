#include "limit_review_internal.hpp"

namespace tdx::detail::limit_review {
namespace {

void add_fixed(QueryPlan& plan, NormalizeKind kind) {
    const auto& spec = resource_spec(kind);
    plan.records.push_back({spec.resource, spec.normalize, false});
}

bool category_is(const QueryPlan& plan, std::string_view value) {
    return plan.options.category == value;
}

}  // namespace

QueryPlan make_query_plan(const LimitReviewQuery& input) {
    QueryPlan plan;
    plan.options = input;
    plan.view = &view_spec(plan.options.view);
    plan.options.view = plan.view->id;
    plan.options.category = lower_ascii(trim(plan.options.category));
    plan.options.market = lower_ascii(trim(plan.options.market));
    plan.options.code = trim(plan.options.code);
    plan.options.date = trim(plan.options.date);
    plan.options.query = trim(plan.options.query);
    if (plan.view->kind == ViewKind::catalog) return plan;

    if (plan.options.offset < 0 || plan.options.offset > 1000000)
        throw Error("offset must be in 0..1000000");
    if (plan.options.limit < 1 || plan.options.limit > 5000)
        throw Error("limit must be in 1..5000");
    if (plan.options.cache_ttl_seconds < 0 || plan.options.cache_ttl_seconds > 86400)
        throw Error("cache_ttl_seconds must be in 0..86400");
    if (plan.options.timeout_ms < 100 || plan.options.timeout_ms > 60000)
        throw Error("timeout_ms must be in 100..60000");
    if (!plan.options.code.empty() && plan.options.market.empty())
        throw Error("market is required when code is supplied");
    if (!plan.options.code.empty() && !digits(plan.options.code, 6))
        throw Error("code must contain six digits");
    if (!plan.options.market.empty()) {
        plan.selected_market = market_id(plan.options.market);
        plan.options.market = market_name(plan.selected_market);
    }
    if (plan.view->kind == ViewKind::security &&
        (plan.options.market.empty() || plan.options.code.empty()))
        throw Error("security view requires market and code");
    if (plan.view->kind == ViewKind::history &&
        (!plan.options.market.empty() || !plan.options.code.empty()))
        throw Error("history view is market-wide and does not accept market or code");
    if (plan.view->kind == ViewKind::daily && !digits(plan.options.date, 8))
        throw Error("daily view requires date=YYYYMMDD");
    if (plan.view->kind != ViewKind::daily && !plan.options.date.empty())
        throw Error("date is only supported by daily view");
    if (plan.view->kind == ViewKind::current &&
        !category_is(plan, "all") && !category_is(plan, "limit-up") &&
        !category_is(plan, "limit-down") && !category_is(plan, "surge"))
        throw Error("current category must be all, limit-up, limit-down, or surge");
    if (plan.view->kind == ViewKind::daily &&
        !category_is(plan, "all") && !category_is(plan, "limit-up") &&
        !category_is(plan, "limit-down"))
        throw Error("daily category must be all, limit-up, or limit-down");
    if (plan.view->kind != ViewKind::current &&
        plan.view->kind != ViewKind::daily && !category_is(plan, "all"))
        throw Error("the selected view only supports category=all");

    switch (plan.view->kind) {
    case ViewKind::current:
        if (category_is(plan, "all") || category_is(plan, "limit-up"))
            add_fixed(plan, NormalizeKind::current_limit_up);
        if (category_is(plan, "all") || category_is(plan, "limit-down"))
            add_fixed(plan, NormalizeKind::current_limit_down);
        if (category_is(plan, "all") || category_is(plan, "surge"))
            add_fixed(plan, NormalizeKind::current_surge);
        break;
    case ViewKind::annual:
        add_fixed(plan, NormalizeKind::annual);
        break;
    case ViewKind::history:
        add_fixed(plan, NormalizeKind::market_history);
        break;
    case ViewKind::daily:
        if (category_is(plan, "all") || category_is(plan, "limit-up"))
            plan.records.push_back({"zdtfx2/" + plan.options.date + ".jsn",
                                    NormalizeKind::daily_limit_up, true});
        if (category_is(plan, "all") || category_is(plan, "limit-down"))
            plan.records.push_back({"zdtfx3/" + plan.options.date + ".jsn",
                                    NormalizeKind::daily_limit_down, true});
        break;
    case ViewKind::security:
        add_fixed(plan, NormalizeKind::current_limit_up);
        add_fixed(plan, NormalizeKind::current_limit_down);
        add_fixed(plan, NormalizeKind::current_surge);
        add_fixed(plan, NormalizeKind::annual);
        plan.history_resource = "zdtfx1/" + std::to_string(plan.selected_market) +
                                plan.options.code + ".jsn";
        break;
    case ViewKind::catalog:
        break;
    }
    return plan;
}

Json normalize_resource_rows(
    NormalizeKind kind, const Json& rows, const QueryPlan& plan,
    const std::map<std::pair<int, std::string>, Security>& securities) {
    switch (kind) {
    case NormalizeKind::current_limit_up:
        return normalize_limit_review_current_rows(rows, "limit-up", securities);
    case NormalizeKind::current_limit_down:
        return normalize_limit_review_current_rows(rows, "limit-down", securities);
    case NormalizeKind::current_surge:
        return normalize_limit_review_current_rows(rows, "surge", securities);
    case NormalizeKind::annual:
        return normalize_limit_review_annual_rows(rows, securities);
    case NormalizeKind::market_history:
        return normalize_limit_review_market_history_rows(rows);
    case NormalizeKind::daily_limit_up:
        return normalize_limit_review_daily_rows(
            rows, "limit-up", plan.options.date, securities);
    case NormalizeKind::daily_limit_down:
        return normalize_limit_review_daily_rows(
            rows, "limit-down", plan.options.date, securities);
    }
    throw Error("unknown limit-review normalizer");
}

}  // namespace tdx::detail::limit_review
