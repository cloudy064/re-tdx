#include "state_owned_reform_internal.hpp"

#include "tdx/jsn.hpp"

namespace tdx::state_owned_detail {

Json compose_response(const StateOwnedReformQuery& options,
                      QueryOutput output) {
    const auto health = jsn_sources_health(output.sources);
    const auto returned = output.groups.size() + output.restructuring.size();
    Json result = Json::object();
    result["schema"] = "tdx-market-state-owned-reform-native-v1";
    result["generated_at"] = now_text();
    result["view"] = options.view;
    result["dimension"] = options.dimension;
    result["availability"] = health.at("stale").as_bool() ? "stale-cache"
        : output.errors.size() ? "partial"
        : returned || output.details.size() ? "live" : "empty";
    result["summary"] = std::move(output.summary);
    result["groups"] = std::move(output.groups);
    result["details"] = std::move(output.details);
    result["restructuring"] = std::move(output.restructuring);
    result["errors"] = std::move(output.errors);
    Json counts = Json::object();
    counts["matched"] = output.matched;
    counts["returned"] = static_cast<std::uint64_t>(returned);
    counts["details"] =
        static_cast<std::uint64_t>(result.at("details").size());
    result["counts"] = std::move(counts);
    Json filters = Json::object();
    filters["dimension"] = options.dimension;
    filters["group_id"] = options.group_id.empty()
        ? Json(nullptr) : Json(options.group_id);
    filters["market"] = options.market.empty()
        ? Json(nullptr) : Json(options.market);
    filters["code"] = options.code.empty()
        ? Json(nullptr) : Json(options.code);
    filters["query"] = options.query;
    filters["sort"] = options.sort;
    filters["order"] = options.order;
    result["filters"] = std::move(filters);
    result["sources"] = std::move(output.sources);
    result["upstream_health"] = health;
    result["quote_source"] = std::move(output.quote_source);
    Json cache = Json::object();
    cache["master_refreshed"] = output.master_refreshed;
    cache["master_age_seconds"] = output.master_age;
    cache["detail_refreshed"] = output.detail_refreshed;
    cache["detail_age_seconds"] = output.detail_age;
    cache["quote_refreshed"] = output.quote_refreshed;
    cache["quote_age_seconds"] = output.quote_age;
    result["cache"] = std::move(cache);
    result["semantics"] =
        "TDX GQGG is a state-owned-enterprise reform relationship surface: industry and region subsets, integration expectations, company groups, per-group controller/control-ratio/logic details, plus a separate restructuring-expectation list. It is materially broader and more structured than the local 100-member generic reform index. Main-table performance columns are host fields; only detail returns reproduced from public 0x054C L1 quotes and upstream reference closes are emitted.";
    return result;
}

}  // namespace tdx::state_owned_detail
