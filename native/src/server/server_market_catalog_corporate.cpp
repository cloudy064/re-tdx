#include "server_market_catalog_internal.hpp"
#include "server_local_resource_paths_internal.hpp"

#include "tdx/common.hpp"
#include "tdx/company_changes.hpp"
#include "tdx/corporate_orders.hpp"
#include "tdx/curated_data.hpp"
#include "tdx/event_impact.hpp"
#include "tdx/gdr.hpp"
#include "tdx/hk_actions.hpp"
#include "tdx/hk_events.hpp"
#include "tdx/hk_finance.hpp"
#include "tdx/patent_statistics.hpp"
#include "tdx/recent_watch.hpp"
#include "tdx/shareholder_signals.hpp"
#include "tdx/special_attention.hpp"
#include "tdx/special_situations.hpp"

namespace tdx::server_detail {

Json query_market_hk_actions(const ApiState& state,
                             const RequestTarget& target) {
    HkActionQuery query;
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.kind = lower_ascii(trim(query_value(target, "kind", "all")));
    query.date_from = trim(query_value(target, "from"));
    query.date_to = trim(query_value(target, "to"));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "200"),
                                "limit", 1, 2000);
    auto document = load_local_hk_actions(state.root, query);
    project_local_catalog_resource_paths(document, state.root);
    return document;
}

Json query_market_hk_finance(const ApiState& state,
                             const RequestTarget& target) {
    HkFinanceQuery query;
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.classification = trim(query_value(target, "classification"));
    query.report_from = trim(query_value(target, "from"));
    query.report_to = trim(query_value(target, "to"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "code")));
    query.order = lower_ascii(trim(query_value(target, "order", "asc")));
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "200"),
                                "limit", 1, 2000);
    auto document = load_local_hk_finance(state.root, query);
    project_local_catalog_resource_paths(document, state.root);
    return document;
}

Json query_market_hk_events(const ApiState& state,
                            const RequestTarget& target) {
    if (!state.hk_event_service) throw Error("HK event service is unavailable");
    HkEventQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.code = trim(query_value(target, "code"));
    query.date_from = trim(query_value(target, "from"));
    query.date_to = trim(query_value(target, "to"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "10000"),
                                "limit", 1, 20000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.hk_event_service->query(query);
}

Json query_market_hk_short_history(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.hk_event_service) throw Error("HK event service is unavailable");
    HkShortHistoryQuery query;
    query.market = trim(query_value(target, "market", "31"));
    query.code = trim(query_value(target, "code"));
    query.refresh = query_bool(target, "refresh");
    query.pages = parse_bounded(query_value(target, "pages", "1"),
                                "pages", 1, 20);
    query.page_size = parse_bounded(query_value(target, "page_size", "800"),
                                    "page_size", 1, 800);
    query.start = parse_bounded(query_value(target, "start", "0"),
                                "start", 0, 65535);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    if (query.code.empty()) throw Error("code is required");
    return state.hk_event_service->query_short_history(query);
}

Json query_market_special_situations(const ApiState& state,
                                     const RequestTarget& target) {
    if (!state.special_situation_service)
        throw Error("special-situation service is unavailable");
    SpecialSituationQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.include_quotes = query_bool(target, "include_quotes", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.quote_cache_ttl_seconds = parse_bounded(
        query_value(target, "quote_cache_ttl_seconds", "15"),
        "quote_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.special_situation_service->query(query);
}

Json query_market_curated_data(const ApiState& state,
                               const RequestTarget& target) {
    if (!state.curated_data_service)
        throw Error("curated-data service is unavailable");
    CuratedDataQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.curated_data_service->query(query);
}

Json query_market_special_attention(const ApiState& state,
                                    const RequestTarget& target) {
    if (!state.special_attention_service)
        throw Error("special-attention service is unavailable");
    SpecialAttentionQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.special_attention_service->query(query);
}

Json query_market_company_changes(const ApiState& state,
                                  const RequestTarget& target) {
    if (!state.company_changes_service)
        throw Error("company-changes service is unavailable");
    CompanyChangesQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.include_raw = query_bool(target, "include_raw", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "10000"),
                                "limit", 1, 20000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.company_changes_service->query(query);
}

Json query_market_gdr(const ApiState& state, const RequestTarget& target) {
    if (!state.gdr_service) throw Error("GDR service is unavailable");
    GdrQuery query;
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "date")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.include_raw = query_bool(target, "include_raw", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "1000"),
                                "limit", 1, 5000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.gdr_service->query(query);
}

Json query_market_corporate_orders(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.corporate_orders_service)
        throw Error("corporate-orders service is unavailable");
    CorporateOrdersQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "date")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.include_raw = query_bool(target, "include_raw", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "10000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.corporate_orders_service->query(query);
}

Json query_market_event_impact(const ApiState& state,
                               const RequestTarget& target) {
    if (!state.event_impact_service)
        throw Error("event-impact service is unavailable");
    EventImpactQuery query;
    query.benchmark = lower_ascii(trim(query_value(target, "benchmark", "all")));
    query.type = trim(query_value(target, "type"));
    query.query = trim(query_value(target, "q"));
    query.date_from = trim(query_value(target, "from"));
    query.date_to = trim(query_value(target, "to"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "date")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.include_raw = query_bool(target, "include_raw", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.event_impact_service->query(query);
}

Json query_market_shareholder_signals(const ApiState& state,
                                      const RequestTarget& target) {
    if (!state.shareholder_signals_service)
        throw Error("shareholder-signals service is unavailable");
    ShareholderSignalsQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.investor_id = trim(query_value(target, "investor_id"));
    query.investor_query = trim(query_value(target, "investor_q"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "signal")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.include_raw = query_bool(target, "include_raw", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.shareholder_signals_service->query(query);
}

Json query_market_recent_watch(const ApiState& state,
                               const RequestTarget& target) {
    if (!state.recent_watch_service) throw Error("recent-watch service is unavailable");
    RecentWatchQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.include_raw = query_bool(target, "include_raw", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"), "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(query_value(target, "cache_ttl_seconds", "900"), "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"), "timeout_ms", 100, 60000);
    return state.recent_watch_service->query(query);
}

Json query_market_patent_statistics(const ApiState& state,
                                    const RequestTarget& target) {
    if (!state.patent_statistics_service)
        throw Error("patent-statistics service is unavailable");
    PatentStatisticsQuery query;
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.report_from = trim(query_value(target, "report_from"));
    query.report_to = trim(query_value(target, "report_to"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "cumulative-total")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.include_raw = query_bool(target, "include_raw", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "10000"),
                                "limit", 1, 20000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.patent_statistics_service->query(query);
}

}  // namespace tdx::server_detail
