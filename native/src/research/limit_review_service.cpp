#include "limit_review_internal.hpp"

#include "tdx/jsn.hpp"

namespace tdx {

Json LimitReviewService::query(const LimitReviewQuery& input) {
    using namespace detail::limit_review;
    const auto plan = make_query_plan(input);
    if (plan.view->kind == ViewKind::catalog) return catalog_document();

    Json candidates = Json::array();
    Json history = Json::array();
    Json sources = Json::array();
    for (const auto& request : plan.records) {
        const auto document = fetch_resource(
            request.resource, plan.options, request.allow_missing);
        append_rows(candidates, normalize_resource_rows(
            request.normalize, document.at("rows"), plan, securities_));
        sources.push_back(source_summary(document));
    }
    if (!plan.history_resource.empty()) {
        const auto document = fetch_resource(
            plan.history_resource, plan.options, true);
        history = normalize_limit_review_security_history_rows(document.at("rows"));
        sources.push_back(source_summary(document));
        if (static_cast<int>(history.size()) > plan.options.limit)
            history.as_array().resize(static_cast<std::size_t>(plan.options.limit));
    }

    std::uint64_t matched = 0;
    auto records = select_records(candidates, plan, matched);
    const auto health = jsn_sources_health(sources);
    std::uint64_t missing_sources = 0;
    for (const auto& source : sources.as_array()) {
        const auto* missing = value_ptr(source, "missing");
        if (missing && missing->is_bool() && missing->as_bool()) ++missing_sources;
    }
    const bool stale = health.at("stale").as_bool();
    const bool empty = records.size() == 0 && history.size() == 0;
    Json counts = Json::object();
    counts["source_rows"] = static_cast<std::uint64_t>(candidates.size());
    counts["matched"] = matched;
    counts["returned"] = static_cast<std::uint64_t>(records.size());
    counts["history"] = static_cast<std::uint64_t>(history.size());
    counts["missing_sources"] = missing_sources;
    Json result = Json::object();
    result["schema"] = "tdx-market-limit-review-native-v1";
    result["generated_at"] = now_text();
    result["view"] = plan.options.view;
    result["category"] = plan.options.category;
    result["market"] = plan.options.market.empty()
        ? Json(nullptr) : Json(plan.options.market);
    result["code"] = plan.options.code.empty()
        ? Json(nullptr) : Json(plan.options.code);
    result["date"] = plan.options.date.empty()
        ? Json(nullptr) : Json(plan.options.date);
    result["availability"] = stale ? "stale-cache" : empty ? "empty" : "live";
    result["counts"] = std::move(counts);
    result["records"] = std::move(records);
    result["history"] = std::move(history);
    result["sources"] = std::move(sources);
    result["upstream_health"] = health;
    Json cache = Json::object();
    cache["ttl_seconds"] = plan.options.cache_ttl_seconds;
    cache["refreshed"] = plan.options.refresh;
    result["cache"] = std::move(cache);
    result["semantics"] =
        "TDX ZDTFX resources are progressively updated non-realtime review data. Live depth/seal quality remains market limit-quality; quote-only CFG columns are not fabricated.";
    return result;
}

}  // namespace tdx
