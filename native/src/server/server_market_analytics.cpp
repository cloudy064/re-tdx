#include "server_market_analytics_internal.hpp"

#include "tdx/abnormal_details.hpp"
#include "tdx/abnormal_moves.hpp"
#include "tdx/anomaly_risk.hpp"
#include "tdx/block_backtest.hpp"
#include "tdx/common.hpp"
#include "tdx/equity_valuation.hpp"
#include "tdx/factors.hpp"
#include "tdx/flow_followup.hpp"
#include "tdx/fund_analytics.hpp"
#include "tdx/index_volatility.hpp"
#include "tdx/intelligence.hpp"
#include "tdx/leverage.hpp"
#include "tdx/profit_gaps.hpp"
#include "tdx/relative_valuation.hpp"
#include "tdx/technical_signals.hpp"
#include "tdx/total_return_gap.hpp"

#include <cmath>
#include <stdexcept>

namespace tdx::server_detail {

Json query_market_margin(const ApiState& state, const RequestTarget& target) {
    if (!state.leverage_service) throw Error("leverage service is unavailable");
    MarginQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "market")));
    query.category = lower_ascii(trim(query_value(target, "category", "balance")));
    query.query = trim(query_value(target, "q"));
    query.date = trim(query_value(target, "date"));
    query.group_id = trim(query_value(target, "group_id"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "2000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.leverage_service->query_margin(query);
}

Json query_market_stock_connect(const ApiState& state,
                                const RequestTarget& target) {
    if (!state.leverage_service) throw Error("leverage service is unavailable");
    StockConnectQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "flows")));
    query.category = lower_ascii(trim(
        query_value(target, "category", "northbound-total")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.date = trim(query_value(target, "date"));
    query.channel = lower_ascii(trim(query_value(target, "channel")));
    query.group_id = trim(query_value(target, "group_id"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "3000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.leverage_service->query_stock_connect(query);
}

Json query_market_intelligence(const ApiState& state,
                               const RequestTarget& target) {
    if (!state.intelligence_service) throw Error("intelligence service is unavailable");
    IntelligenceQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "attention")));
    query.category = lower_ascii(trim(query_value(target, "category", "all")));
    query.highlight_type = trim(query_value(target, "type"));
    query.sort = lower_ascii(trim(query_value(target, "sort")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.query = trim(query_value(target, "q"));
    query.topic_id = trim(query_value(target, "topic_id"));
    query.event_id = trim(query_value(target, "event_id"));
    query.category_id = trim(query_value(target, "category_id"));
    const bool has_market = !trim(query_value(target, "market")).empty();
    const bool has_code = !trim(query_value(target, "code")).empty();
    if (query.view == "security" || has_market || has_code) {
        const auto [market, code] = query_security(target);
        query.market = market;
        query.code = code;
    }
    query.refresh = query_bool(target, "refresh");
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 10000);
    query.member_limit = parse_bounded(
        query_value(target, "member_limit", "3000"),
        "member_limit", 1, 20000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.intelligence_service->query(query);
}

Json query_market_technical_signals(const ApiState& state,
                                    const RequestTarget& target) {
    if (!state.technical_signals_service)
        throw Error("technical-signals service is unavailable");
    const auto market = lower_ascii(trim(query_value(target, "market")));
    const auto code = trim(query_value(target, "code"));
    if (!market.empty() || !code.empty()) {
        if (market.empty() || code.empty())
            throw Error("market and code must be provided together");
        TechnicalSignalSecurityQuery query;
        query.market = market;
        query.code = code;
        query.apply_client_filters = !query_bool(target, "raw");
        query.refresh = query_bool(target, "refresh");
        query.cache_ttl_seconds = parse_bounded(
            query_value(target, "cache_ttl_seconds", "15"),
            "cache_ttl_seconds", 0, 3600);
        query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                         "timeout_ms", 100, 60000);
        return state.technical_signals_service->query_security(query);
    }
    TechnicalSignalsQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "nine-turn")));
    query.direction = lower_ascii(trim(query_value(target, "direction", "all")));
    query.board = lower_ascii(trim(query_value(target, "board", "main")));
    const auto default_rps = query.view == "rps-block" ? "85" : "90";
    query.duration1 = parse_bounded(query_value(target, "duration1", "10"),
                                    "duration1", 1, 10000);
    query.rps1 = parse_bounded(query_value(target, "rps1", default_rps),
                              "rps1", 0, 100);
    query.duration2 = parse_bounded(query_value(target, "duration2", "20"),
                                    "duration2", 1, 10000);
    query.rps2 = parse_bounded(query_value(target, "rps2", default_rps),
                              "rps2", 0, 100);
    query.duration3 = parse_bounded(query_value(target, "duration3", "60"),
                                    "duration3", 1, 10000);
    query.rps3 = parse_bounded(query_value(target, "rps3", default_rps),
                              "rps3", 0, 100);
    query.index_period = parse_bounded(query_value(target, "index_period", "2"),
                                       "index_period", 1, 10000);
    query.history_period = parse_bounded(query_value(target, "history_period", "120"),
                                         "history_period", 1, 10000);
    query.retracement = parse_bounded(query_value(target, "retracement", "5"),
                                      "retracement", 0, 100);
    query.sideways_period = parse_bounded(query_value(target, "sideways_period", "20"),
                                          "sideways_period", 1, 10000);
    query.amplitude = parse_bounded(query_value(target, "amplitude", "10"),
                                    "amplitude", 0, 100);
    query.breakout_period = parse_bounded(query_value(target, "breakout_period", "2"),
                                          "breakout_period", 1, 10000);
    query.apply_client_filters = !query_bool(target, "raw");
    query.enrich_quotes = query_bool(target, "quotes");
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "15"),
        "cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.technical_signals_service->query(query);
}

Json query_market_factors(const ApiState& state,
                          const RequestTarget& target) {
    if (!state.factor_service) throw Error("factor service is unavailable");
    FactorQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "catalog")));
    query.factor_id = trim(query_value(target, "factor_id"));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.include_patterns = query_bool(target, "include_patterns");
    const auto all_pages = trim(query_value(target, "all_pages"));
    query.all_pages = all_pages.empty() ? true : query_bool(target, "all_pages");
    if (query_bool(target, "first_page")) query.all_pages = false;
    query.enrich_quotes = query_bool(target, "quotes");
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "10000"),
                                "limit", 1, 30000);
    query.max_pages = parse_bounded(query_value(target, "max_pages", "100"),
                                    "max_pages", 1, 1000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "60"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.factor_service->query(query);
}

Json query_market_block_backtest(const ApiState& state,
                                 const RequestTarget& target) {
    if (!state.block_backtest_service)
        throw Error("block-backtest service is unavailable");
    BlockBacktestQuery query;
    query.category = lower_ascii(trim(query_value(target, "category", "all")));
    query.block_code = trim(query_value(target, "block_code"));
    query.begin_date = trim(query_value(target, "begin"));
    query.end_date = trim(query_value(target, "end"));
    query.adjustment = lower_ascii(trim(query_value(target, "adjustment", "forward")));
    query.sort = lower_ascii(trim(query_value(target, "sort", "return")));
    query.order = lower_ascii(trim(query_value(target, "order", "desc")));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 20000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.block_backtest_service->query(query);
}

Json query_market_equity_valuation(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.equity_valuation_service)
        throw Error("equity-valuation service is unavailable");
    EquityValuationQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "pe-industries")));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.industry_code = trim(query_value(target, "industry"));
    query.industry_market = trim(query_value(target, "industry_market", "1"));
    query.start_date = trim(query_value(target, "start"));
    query.end_date = trim(query_value(target, "end"));
    query.pe_type = lower_ascii(trim(query_value(target, "pe_type", "ttm")));
    const auto return_rate = trim(query_value(target, "required_return_rate", "3"));
    try {
        std::size_t used = 0;
        query.required_return_rate_pct = std::stod(return_rate, &used);
        if (used != return_rate.size() || !std::isfinite(query.required_return_rate_pct) ||
            query.required_return_rate_pct < 0 || query.required_return_rate_pct > 100)
            throw std::invalid_argument("range");
    } catch (...) {
        throw Error("required_return_rate must be a finite percentage in 0..100");
    }
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "4000"),
                                "limit", 1, 20000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.equity_valuation_service->query(query);
}

Json query_market_relative_valuation(const ApiState& state,
                                     const RequestTarget& target) {
    if (!state.relative_valuation_service)
        throw Error("relative-valuation service is unavailable");
    RelativeValuationQuery query;
    query.code = trim(query_value(target, "code"));
    query.index_type = lower_ascii(trim(query_value(target, "index_type", "broad")));
    query.benchmark = trim(query_value(target, "benchmark", "000001"));
    query.method = lower_ascii(trim(query_value(target, "method", "pe-ttm")));
    query.start_date = trim(query_value(target, "start"));
    query.end_date = trim(query_value(target, "end"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "4000"),
                                "limit", 1, 20000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.relative_valuation_service->query(query);
}

Json query_market_flow_followup(const ApiState& state,
                                const RequestTarget& target) {
    if (!state.flow_followup_service)
        throw Error("flow-followup service is unavailable");
    FlowFollowupQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "margin")));
    query.start_date = trim(query_value(target, "start"));
    query.end_date = trim(query_value(target, "end"));
    query.available_only = query_bool(target, "available_only");
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 20000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "30000"),
                                     "timeout_ms", 100, 60000);
    return state.flow_followup_service->query(query);
}

Json query_market_abnormal_moves(const ApiState& state,
                                 const RequestTarget& target) {
    if (!state.abnormal_moves_service) throw Error("abnormal-moves service is unavailable");
    AbnormalMovesQuery query;
    query.board = lower_ascii(trim(query_value(target, "board", "all")));
    query.type = trim(query_value(target, "type", "all"));
    query.date = trim(query_value(target, "date"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"), "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(query_value(target, "cache_ttl_seconds", "60"), "cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"), "timeout_ms", 100, 60000);
    return state.abnormal_moves_service->query(query);
}

Json query_market_abnormal_details(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.abnormal_details_service)
        throw Error("abnormal-details service is unavailable");
    AbnormalDetailsQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "summary")));
    query.board = lower_ascii(trim(query_value(target, "board", "all")));
    query.type = trim(query_value(target, "type", "all"));
    query.date = trim(query_value(target, "date"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.refresh = query_bool(target, "refresh");
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "60"),
        "cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.abnormal_details_service->query(query);
}

Json query_market_anomaly_risk(const ApiState& state,
                               const RequestTarget& target) {
    if (!state.anomaly_risk_service)
        throw Error("anomaly-risk service is unavailable");
    AnomalyRiskQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "statistics")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.warning = lower_ascii(trim(query_value(target, "warning", "all")));
    query.warnings_only = query_bool(target, "warnings_only");
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.anomaly_risk_service->query(query);
}

Json query_market_profit_gaps(const ApiState& state,
                              const RequestTarget& target) {
    if (!state.profit_gaps_service) throw Error("profit-gaps service is unavailable");
    ProfitGapsQuery query;
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.minimum_safety = parse_bounded(
        query_value(target, "minimum_safety", "60"), "minimum_safety", 0, 100);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "5000"),
                                "limit", 1, 10000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.profit_gaps_service->query(query);
}

Json query_market_index_volatility(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.index_volatility_service)
        throw Error("index-volatility service is unavailable");
    IndexVolatilityQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "catalog")));
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.start_date = trim(query_value(target, "start"));
    query.end_date = trim(query_value(target, "end"));
    query.window_days = parse_bounded(query_value(target, "window", "5"),
                                      "window", 1, 250);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "10000"),
                                "limit", 1, 20000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.index_volatility_service->query(query);
}

Json query_market_total_return_gap(const ApiState& state,
                                   const RequestTarget& target) {
    if (!state.total_return_gap_service)
        throw Error("total-return-gap service is unavailable");
    TotalReturnGapQuery query;
    query.query = trim(query_value(target, "q"));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    const auto year = trim(query_value(target, "year"));
    query.year = year.empty() ? 0 : parse_bounded(year, "year", 1999, 2049);
    query.month = parse_bounded(query_value(target, "month", "0"),
                                "month", 0, 12);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "1000"),
                                "limit", 1, 5000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.total_return_gap_service->query(query);
}

Json query_market_fund_analytics(const ApiState& state,
                                 const RequestTarget& target) {
    if (!state.fund_analytics_service)
        throw Error("fund-analytics service is unavailable");
    FundAnalyticsQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "risk")));
    query.query = trim(query_value(target, "q"));
    query.fund_code = trim(query_value(target, "fund_code"));
    query.style = trim(query_value(target, "style", "005001"));
    query.fund_size = parse_bounded(query_value(target, "fund_size", "0"),
                                    "fund_size", 0, 6);
    query.fund_age = parse_bounded(query_value(target, "fund_age", "0"),
                                   "fund_age", 0, 6);
    query.benchmark = parse_bounded(query_value(target, "benchmark", "0"),
                                    "benchmark", 0, 2);
    query.start_date = trim(query_value(target, "start"));
    query.end_date = trim(query_value(target, "end"));
    query.report_date = trim(query_value(target, "report_date"));
    query.estimate_date = trim(query_value(target, "estimate_date"));
    const auto risk_free = trim(query_value(target, "risk_free_rate", "3"));
    try {
        std::size_t used = 0;
        query.risk_free_rate = std::stod(risk_free, &used);
        if (used != risk_free.size() || !std::isfinite(query.risk_free_rate))
            throw std::invalid_argument("value");
    } catch (...) { throw Error("risk_free_rate must be numeric"); }
    query.all_pages = !query_bool(target, "first_page");
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "10000"),
                                "limit", 1, 20000);
    query.max_pages = parse_bounded(query_value(target, "max_pages", "100"),
                                    "max_pages", 1, 100);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.fund_analytics_service->query(query);
}

}  // namespace tdx::server_detail
