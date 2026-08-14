#include "server_market_catalog_internal.hpp"

#include "tdx/benchmark_analysis.hpp"
#include "tdx/common.hpp"
#include "tdx/equity_performance.hpp"
#include "tdx/financial_insights.hpp"
#include "tdx/financial_screen.hpp"
#include "tdx/global_performance.hpp"
#include "tdx/overview_factors.hpp"
#include "tdx/specialized_metrics.hpp"

namespace tdx::server_detail {

Json query_market_specialized_metrics(const ApiState& state,
                                      const RequestTarget& target) {
    if (!state.specialized_metrics_service)
        throw Error("specialized-metrics service is unavailable");
    SpecializedMetricsQuery query;
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
    return state.specialized_metrics_service->query(query);
}

Json query_market_financial_screen(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.financial_screen_service)
        throw Error("financial-screen service is unavailable");
    FinancialScreenQuery query;
    query.dataset = lower_ascii(trim(query_value(target, "dataset", "snapshot")));
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.sort = lower_ascii(trim(query_value(
        target, "sort", query.dataset == "small-cap-growth" ? "profit-cagr" : "market-cap")));
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
    return state.financial_screen_service->query(query);
}

Json query_market_financial_insights(const ApiState& state,
                                     const RequestTarget& target) {
    if (!state.financial_insights_service)
        throw Error("financial-insights service is unavailable");
    FinancialInsightsQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
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
    return state.financial_insights_service->query(query);
}

Json query_market_equity_performance(const ApiState& state,
                                     const RequestTarget& target) {
    if (!state.equity_performance_service)
        throw Error("equity-performance service is unavailable");
    EquityPerformanceQuery query;
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "return-5d")));
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
    return state.equity_performance_service->query(query);
}

Json query_market_global_performance(const ApiState& state,
                                     const RequestTarget& target) {
    if (!state.global_performance_service)
        throw Error("global-performance service is unavailable");
    GlobalPerformanceQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "all")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.sort = lower_ascii(trim(query_value(target, "sort", "return-5d")));
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
    return state.global_performance_service->query(query);
}

Json query_market_overview_factors(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.overview_factors_service)
        throw Error("overview-factors service is unavailable");
    OverviewFactorsQuery query;
    query.query = trim(query_value(target, "q"));
    query.signal = lower_ascii(trim(query_value(target, "signal", "all")));
    query.include_raw = query_bool(target, "include_raw", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "100"),
                                "limit", 1, 1000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.overview_factors_service->query(query);
}

Json query_market_benchmark_analysis(const ApiState& state,
                                     const RequestTarget& target) {
    if (!state.benchmark_analysis_service)
        throw Error("benchmark-analysis service is unavailable");
    BenchmarkAnalysisQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "stocks")));
    query.stage = trim(query_value(target, "stage"));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.include_raw = query_bool(target, "include_raw", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 50000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "900"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.benchmark_analysis_service->query(query);
}

}  // namespace tdx::server_detail
