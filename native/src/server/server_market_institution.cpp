#include "server_market_institution_internal.hpp"

#include "tdx/active_lhb.hpp"
#include "tdx/common.hpp"
#include "tdx/foreign_alerts.hpp"
#include "tdx/institution.hpp"
#include "tdx/institution_lhb.hpp"
#include "tdx/ratings.hpp"
#include "tdx/state_owned_reform.hpp"

namespace tdx::server_detail {

Json query_market_institution_lhb(const ApiState& state,
                                  const RequestTarget& target) {
    if (!state.institution_lhb_service)
        throw Error("institution-LHB service is unavailable");
    InstitutionLhbQuery query;
    query.period = lower_ascii(trim(query_value(target, "period", "week")));
    query.direction = lower_ascii(trim(query_value(target, "direction", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.include_details = query_bool(target, "include_details",
        !query.market.empty() || !query.code.empty());
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "3000"),
                                "limit", 1, 5000);
    query.detail_limit = parse_bounded(
        query_value(target, "detail_limit", "1000"),
        "detail_limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.institution_lhb_service->query(query);
}

Json query_market_active_lhb(const ApiState& state,
                             const RequestTarget& target) {
    if (!state.active_lhb_service) throw Error("active-LHB service is unavailable");
    ActiveLhbQuery query;
    query.period = lower_ascii(trim(query_value(target, "period", "5d")));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.direction = lower_ascii(trim(query_value(target, "direction", "all")));
    query.sort = lower_ascii(trim(query_value(target, "sort", "events")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.include_details = query_bool(target, "include_details", true);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.detail_limit = parse_bounded(query_value(target, "detail_limit", "500"),
                                       "detail_limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "300"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.active_lhb_service->query(query);
}

Json query_market_state_owned_reform(const ApiState& state,
                                     const RequestTarget& target) {
    if (!state.state_owned_reform_service)
        throw Error("state-owned reform service is unavailable");
    StateOwnedReformQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "groups")));
    query.dimension = lower_ascii(trim(query_value(target, "dimension", "industry")));
    query.group_id = trim(query_value(target, "group_id"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.sort = lower_ascii(trim(query_value(target, "sort",
        query.view == "restructuring" ? "date" : "count")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.include_details = query_bool(target, "include_details", true);
    query.include_quotes = query_bool(target, "include_quotes", true);
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.detail_limit = parse_bounded(query_value(target, "detail_limit", "500"),
                                       "detail_limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "300"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.quote_cache_ttl_seconds = parse_bounded(
        query_value(target, "quote_cache_ttl_seconds", "5"),
        "quote_cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.state_owned_reform_service->query(query);
}

Json query_market_ratings(const ApiState& state,
                          const RequestTarget& target) {
    if (!state.rating_service)
        throw Error("rating service is unavailable");
    RatingQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "hong-kong")));
    query.stance = lower_ascii(trim(query_value(target, "stance", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.industry = trim(query_value(target, "industry"));
    query.include_details = query_bool(target, "include_details",
        !query.code.empty() || !query.industry.empty());
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "2000"),
                                "limit", 1, 5000);
    query.detail_limit = parse_bounded(
        query_value(target, "detail_limit", "1000"),
        "detail_limit", 1, 10000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.rating_service->query(query);
}

Json query_market_foreign_alerts(const ApiState& state,
                                 const RequestTarget& target) {
    if (!state.foreign_alert_service)
        throw Error("foreign-alert service is unavailable");
    ForeignAlertQuery query;
    query.status = lower_ascii(trim(query_value(target, "status", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.include_details = query_bool(target, "include_details",
        !query.market.empty() || !query.code.empty());
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "1000"),
                                "limit", 1, 5000);
    query.detail_limit = parse_bounded(
        query_value(target, "detail_limit", "2000"),
        "detail_limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.foreign_alert_service->query(query);
}

Json query_holder(const ApiState& state, const RequestTarget& target) {
    if (!state.institution_service) throw Error("institution service is unavailable");
    HolderQuery query;
    query.holder_id = trim(query_value(target, "holder_id"));
    query.variant_id = trim(query_value(target, "variant_id"));
    query.holder_name = trim(query_value(target, "holder_name"));
    query.reference_code = trim(query_value(target, "reference_code"));
    query.stock_code = trim(query_value(target, "stock_code"));
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "100"),
                                "limit", 1, 500);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.institution_service->query_holder(query);
}

}  // namespace tdx::server_detail
