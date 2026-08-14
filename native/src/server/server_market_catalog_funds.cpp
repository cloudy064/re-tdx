#include "server_market_catalog_internal.hpp"
#include "server_local_resource_paths_internal.hpp"

#include "tdx/active_funds.hpp"
#include "tdx/bond_reference.hpp"
#include "tdx/common.hpp"
#include "tdx/convertible_bonds.hpp"
#include "tdx/etf_flows.hpp"
#include "tdx/exchange_funds.hpp"
#include "tdx/fund_calendar.hpp"
#include "tdx/fund_reference.hpp"
#include "tdx/fund_statistics.hpp"

namespace tdx::server_detail {

Json query_market_active_funds(const ApiState& state,
                               const RequestTarget& target) {
    if (!state.active_fund_service)
        throw Error("active-fund service is unavailable");
    ActiveFundQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "securities")));
    query.direction = lower_ascii(trim(query_value(target, "direction", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "2000"),
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
    return state.active_fund_service->query(query);
}

Json query_market_etf_flows(const ApiState& state,
                            const RequestTarget& target) {
    if (!state.etf_flow_service) throw Error("ETF-flow service is unavailable");
    EtfFlowQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "stocks")));
    query.direction = lower_ascii(trim(query_value(target, "direction", "all")));
    query.sort = lower_ascii(trim(query_value(target, "sort", "total-flow")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.industry = trim(query_value(target, "industry"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.etf_flow_service->query(query);
}

Json query_market_convertible_bonds(const ApiState& state,
                                    const RequestTarget& target) {
    if (!state.convertible_bond_service)
        throw Error("convertible-bond service is unavailable");
    ConvertibleBondQuery query;
    query.view = trim(query_value(target, "view", "listed"));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.sort = trim(query_value(target, "sort"));
    query.order = trim(query_value(target, "order", "desc"));
    query.refresh = query_bool(target, "refresh");
    query.include_details = query_bool(target, "include_details",
                                       !query.code.empty());
    query.include_quotes = query_bool(target, "include_quotes", true);
    query.active_only = query_bool(target, "active_only", true);
    query.limit = parse_bounded(query_value(target, "limit", "2000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.quote_cache_ttl_seconds = parse_bounded(
        query_value(target, "quote_cache_ttl_seconds", "5"),
        "quote_cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.convertible_bond_service->query(query);
}

Json query_market_bond_reference(const ApiState& state,
                                 const RequestTarget& target) {
    if (!state.bond_reference_service)
        throw Error("bond-reference service is unavailable");
    BondReferenceQuery query;
    query.group = lower_ascii(trim(query_value(target, "group", "rating")));
    query.bucket = lower_ascii(trim(query_value(target, "bucket", "aa-plus")));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "maturity")));
    query.order = lower_ascii(trim(query_value(target, "order", "asc")));
    query.refresh = query_bool(target, "refresh");
    query.include_projections = query_bool(target, "include_projections");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "30000"),
                                     "timeout_ms", 100, 120000);
    return state.bond_reference_service->query(query);
}

Json query_market_exchange_funds(const ApiState& state,
                                 const RequestTarget& target) {
    if (!state.exchange_fund_service)
        throw Error("exchange-fund service is unavailable");
    ExchangeFundQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.include_quotes = query_bool(target, "include_quotes", false);
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
    return state.exchange_fund_service->query(query);
}

Json query_market_fund_reference(const ApiState& state,
                                 const RequestTarget& target) {
    FundReferenceQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.as_of_date = trim(query_value(target, "as_of_date"));
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    auto document = load_local_fund_reference(
        state.root, query, state.block_data.securities);
    project_local_catalog_resource_paths(document, state.root);
    return document;
}

Json query_market_fund_statistics(const ApiState& state,
                                  const RequestTarget& target) {
    if (!state.fund_statistics_service)
        throw Error("fund-statistics service is unavailable");
    FundStatisticsQuery query;
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
    return state.fund_statistics_service->query(query);
}

Json query_market_fund_calendar(const ApiState& state,
                                const RequestTarget& target) {
    if (!state.fund_calendar_service)
        throw Error("fund-calendar service is unavailable");
    FundCalendarQuery query;
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.event_type = trim(query_value(target, "event_type"));
    query.category = trim(query_value(target, "category"));
    query.date_from = trim(query_value(target, "date_from"));
    query.date_to = trim(query_value(target, "date_to"));
    query.order = lower_ascii(trim(query_value(target, "order", "asc")));
    query.include_raw = query_bool(target, "include_raw", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.fund_calendar_service->query(query);
}

}  // namespace tdx::server_detail
