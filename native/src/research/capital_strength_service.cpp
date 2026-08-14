#include "capital_strength_internal.hpp"

#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"

namespace tdx {
namespace {

using detail::capital_strength::QueryPlan;
using detail::capital_strength::ViewKind;

std::pair<Json, Json> project_records(
    const Json& groups, const QueryPlan& plan) {
    using namespace detail::capital_strength;
    Json records = Json::array();
    Json summary = Json::object();
    switch (plan.view->kind) {
        case ViewKind::catalog:
            records = catalog_rows();
            summary["periods"] = static_cast<std::uint64_t>(records.size());
            break;
        case ViewKind::ranking:
            records = groups.as_array().front().at("rows");
            summary = ranking_summary(records);
            break;
        case ViewKind::confluence: {
            auto composed = compose_capital_strength_confluence(groups);
            records = std::move(composed["records"]);
            summary = std::move(composed["summary"]);
            break;
        }
        case ViewKind::security: {
            std::uint64_t checked = 0;
            for (const auto& group : groups.as_array()) {
                ++checked;
                for (const auto& row : group.at("rows").as_array()) {
                    const auto& security = row.at("security");
                    if (static_cast<int>(security.at("market_id").as_number()) ==
                            plan.selected_market &&
                        security.at("code").as_string() == plan.options.code)
                        records.push_back(row);
                }
            }
            summary["periods_checked"] = checked;
            summary["periods_matched"] = static_cast<std::uint64_t>(records.size());
            summary["all_five_periods"] =
                records.size() == detail::capital_strength::all_period_specs().size();
            break;
        }
    }
    return {std::move(records), std::move(summary)};
}

Json unit_document() {
    Json units = Json::object();
    units["float_shares"] = "shares";
    units["period_return_pct"] = "percent";
    units["total_net_inflow_yuan"] = "yuan";
    units["main_net_inflow_yuan"] = "yuan";
    units["ddx_float_share_pct"] = "percentage-points-of-float-shares";
    return units;
}

}  // namespace

CapitalStrengthService::CapitalStrengthService(
    std::map<std::pair<int, std::string>, Security> securities)
    : securities_(std::move(securities)) {}

Json CapitalStrengthService::query(const CapitalStrengthQuery& input) {
    using namespace detail::capital_strength;
    const auto plan = make_query_plan(input);
    const auto now = std::time(nullptr);
    std::vector<std::string> stale;
    for (const auto* period : plan.periods) {
        const auto found = cache_.find(period->resource);
        if (plan.options.refresh || found == cache_.end() ||
            now - found->second.fetched_at >= plan.options.cache_ttl_seconds)
            stale.push_back(period->resource);
    }
    if (!stale.empty()) {
        const auto fetched = fetch_jsn_resources_rows(
            stale, "bi", plan.options.timeout_ms);
        if (!fetched.is_array() || fetched.size() != stale.size())
            throw Error("capital-strength resource batch is incomplete");
        for (std::size_t index = 0; index < stale.size(); ++index)
            cache_[stale[index]] = {fetched.as_array()[index], std::time(nullptr)};
    }

    Json sources = Json::array();
    Json groups = Json::array();
    for (const auto* period : plan.periods) {
        const auto found = cache_.find(period->resource);
        if (found == cache_.end()) throw Error("capital-strength cache is incomplete");
        sources.push_back(jsn_source_metadata(found->second.document));
        Json group = Json::object();
        group["period"] = period->id;
        group["rows"] = normalize_capital_strength_rows(
            found->second.document.at("rows"), period->id, securities_);
        groups.push_back(std::move(group));
    }

    auto [records, summary] = project_records(groups, plan);
    auto filtered = filter_capital_strength_records(records, plan);
    if (plan.view->kind != ViewKind::catalog)
        sort_capital_strength_rows(
            filtered, plan.view->id, plan.options.sort, plan.options.order);

    const auto matched = filtered.size();
    Json paged = Json::array();
    for (std::size_t index = static_cast<std::size_t>(plan.options.offset);
         index < filtered.size() &&
             paged.size() < static_cast<std::size_t>(plan.options.limit);
         ++index)
        paged.push_back(filtered.as_array()[index]);
    const auto health = jsn_sources_health(sources);

    Json filters = Json::object();
    filters["market"] = plan.options.market.empty()
        ? Json(nullptr) : Json(plan.options.market);
    filters["code"] = plan.options.code.empty()
        ? Json(nullptr) : Json(plan.options.code);
    filters["query"] = plan.options.query;
    filters["min_periods"] = plan.options.min_periods;
    filters["sort"] = plan.options.sort;
    filters["order"] = plan.options.order;
    Json counts = Json::object();
    counts["matched"] = static_cast<std::uint64_t>(matched);
    counts["returned"] = static_cast<std::uint64_t>(paged.size());
    counts["sources"] = static_cast<std::uint64_t>(sources.size());
    Json cache = Json::object();
    cache["ttl_seconds"] = plan.options.cache_ttl_seconds;
    cache["refreshed"] = !stale.empty();
    cache["resource_count"] = static_cast<std::uint64_t>(plan.periods.size());

    Json result = Json::object();
    result["schema"] = "tdx-market-capital-strength-native-v1";
    result["generated_at"] = now_text();
    result["view"] = plan.view->id;
    result["period"] = plan.view->kind == ViewKind::ranking
        ? Json(plan.options.period) : Json(nullptr);
    result["availability"] = plan.view->kind == ViewKind::catalog ? "catalog" :
        health.at("stale").as_bool() ? "stale-cache" : matched == 0 ? "empty" : "live";
    result["filters"] = std::move(filters);
    result["summary"] = std::move(summary);
    result["counts"] = std::move(counts);
    result["records"] = std::move(paged);
    result["catalog"] = catalog_rows();
    result["sources"] = std::move(sources);
    result["upstream_health"] = health;
    result["cache"] = std::move(cache);
    result["units"] = unit_document();
    result["semantics"] =
        "TDX QSZJ 资金强势 ranks the top 100 securities by cumulative DDX for 5d, 10d, 20d, 30d, or 3m. DDX is the client-provided net active large-order share volume divided by float shares and is exposed as percentage points without recomputation. total/main net inflow fields are yuan amounts and their signs must not be used to rewrite DDX. Live quote, current price, total market value, and industry syscols are not present in JSN and are not fabricated.";
    return result;
}

}  // namespace tdx
