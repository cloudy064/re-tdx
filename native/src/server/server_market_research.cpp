#include "server_market_research_internal.hpp"

#include "tdx/block_trades.hpp"
#include "tdx/common.hpp"
#include "tdx/consensus.hpp"
#include "tdx/funds.hpp"
#include "tdx/futures_issuance.hpp"
#include "tdx/industry_profile.hpp"
#include "tdx/lhb.hpp"
#include "tdx/research.hpp"
#include "tdx/roadshows.hpp"
#include "tdx/unlocks.hpp"
#include "tdx/valuation.hpp"

namespace tdx::server_detail {

Json query_intraday_funds(const ApiState& state, const RequestTarget& target) {
    if (!state.funds_service) throw Error("intraday funds service is unavailable");
    IntradayFundsQuery query;
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.industry = trim(query_value(target, "industry"));
    query.all_industries = query_bool(target, "all_industries");
    query.refresh = query_bool(target, "refresh");
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "15"),
        "cache_ttl_seconds", 0, 3600);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.funds_service->query(query);
}

Json query_lhb(const ApiState& state, const RequestTarget& target) {
    if (!state.lhb_service) throw Error("LHB service is unavailable");
    LhbQuery query;
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.event_id = trim(query_value(target, "event_id"));
    query.include_details = query_bool(target, "include_details",
        !query.event_id.empty() || (!query.market.empty() && !query.code.empty()));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "10"), "limit", 1, 50);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "60"),
        "master_cache_ttl_seconds", 0, 3600);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "300"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "10000"),
                                     "timeout_ms", 100, 60000);
    return state.lhb_service->query(query);
}

Json query_market_valuation(const ApiState& state, const RequestTarget& target) {
    if (!state.valuation_service) throw Error("valuation service is unavailable");
    ValuationQuery query;
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.detail_id = trim(query_value(target, "detail_id"));
    query.start_date = trim(query_value(target, "start_date"));
    query.end_date = trim(query_value(target, "end_date"));
    query.include_details = query_bool(target, "include_details",
        !query.market.empty() || !query.code.empty() || !query.detail_id.empty());
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "4000"),
                                "limit", 1, 10000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.valuation_service->query(query);
}

Json query_market_futures_issuance(const ApiState& state, const RequestTarget& target) {
    if (!state.futures_issuance_service)
        throw Error("futures/issuance service is unavailable");
    FuturesIssuanceQuery query;
    query.section = lower_ascii(trim(query_value(target, "section", "all")));
    query.contract_key = trim(query_value(target, "contract_key"));
    query.year = trim(query_value(target, "year"));
    query.industry_key = trim(query_value(target, "industry_key"));
    query.placement_status = lower_ascii(trim(query_value(target, "placement_status", "all")));
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.search = trim(query_value(target, "q"));
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.futures_issuance_service->query(query);
}

Json query_market_consensus(const ApiState& state, const RequestTarget& target) {
    if (!state.consensus_service) throw Error("consensus service is unavailable");
    ConsensusQuery query;
    query.category = lower_ascii(trim(query_value(target, "category", "latest")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.include_details = query_bool(target, "include_details",
        !query.market.empty() || !query.code.empty());
    query.include_report_text = query_bool(target, "include_report_text", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.report_limit = parse_bounded(query_value(target, "report_limit", "100"),
                                       "report_limit", 1, 500);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.consensus_service->query(query);
}

Json query_market_research(const ApiState& state, const RequestTarget& target) {
    if (!state.research_service) throw Error("research service is unavailable");
    ResearchQuery query;
    query.category = lower_ascii(trim(query_value(
        target, "category", "institution-research")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.detail_id = trim(query_value(target, "detail_id"));
    query.include_details = query_bool(target, "include_details",
        !query.market.empty() || !query.code.empty() || !query.detail_id.empty());
    query.include_text = query_bool(target, "include_text", true);
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.detail_limit = parse_bounded(query_value(target, "detail_limit", "100"),
                                       "detail_limit", 1, 1000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.research_service->query(query);
}

Json query_market_roadshows(const ApiState& state, const RequestTarget& target) {
    if (!state.roadshow_service) throw Error("roadshow service is unavailable");
    RoadshowQuery query;
    query.market = lower_ascii(trim(query_value(target, "market")));
    query.code = trim(query_value(target, "code"));
    query.query = trim(query_value(target, "q"));
    query.type = trim(query_value(target, "type"));
    query.status = trim(query_value(target, "status"));
    query.start_date = trim(query_value(target, "start"));
    query.end_date = trim(query_value(target, "end"));
    query.offset = parse_bounded(query_value(target, "offset", "0"),
                                 "offset", 0, 1000000);
    query.limit = parse_bounded(query_value(target, "limit", "100"),
                                "limit", 1, 10000);
    query.refresh = query_bool(target, "refresh");
    query.cache_ttl_seconds = parse_bounded(
        query_value(target, "cache_ttl_seconds", "300"),
        "cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 600000);
    return state.roadshow_service->query(query);
}

Json query_market_industry_profile(const ApiState& state, const RequestTarget& target) {
    if (!state.industry_profile_service)
        throw Error("industry profile service is unavailable");
    IndustryProfileQuery query;
    query.industry = trim(query_value(target, "industry"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.include_catalog = query_bool(target, "include_catalog",
        query.industry.empty() && query.market.empty() && query.code.empty());
    query.refresh = query_bool(target, "refresh");
    query.detail_limit = parse_bounded(query_value(target, "detail_limit", "1000"),
                                       "detail_limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.industry_profile_service->query(query);
}

Json query_market_unlocks(const ApiState& state, const RequestTarget& target) {
    if (!state.unlock_service) throw Error("unlock service is unavailable");
    UnlockQuery query;
    query.view = trim(query_value(target, "view", "calendar"));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.detail_id = trim(query_value(target, "detail_id"));
    query.start_date = trim(query_value(target, "start_date"));
    query.end_date = trim(query_value(target, "end_date"));
    query.progress = trim(query_value(target, "progress"));
    query.reason = trim(query_value(target, "reason"));
    query.include_details = query_bool(target, "include_details",
        !query.market.empty() || !query.code.empty() || !query.detail_id.empty());
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "500"),
                                "limit", 1, 5000);
    query.detail_limit = parse_bounded(query_value(target, "detail_limit", "1000"),
                                       "detail_limit", 1, 5000);
    query.master_cache_ttl_seconds = parse_bounded(
        query_value(target, "master_cache_ttl_seconds", "300"),
        "master_cache_ttl_seconds", 0, 86400);
    query.detail_cache_ttl_seconds = parse_bounded(
        query_value(target, "detail_cache_ttl_seconds", "900"),
        "detail_cache_ttl_seconds", 0, 86400);
    query.timeout_ms = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                     "timeout_ms", 100, 60000);
    return state.unlock_service->query(query);
}

Json query_market_block_trades(const ApiState& state,
                               const RequestTarget& target) {
    if (!state.block_trade_service)
        throw Error("block-trade service is unavailable");
    BlockTradeQuery query;
    query.view = lower_ascii(trim(query_value(target, "view", "trades")));
    query.query = trim(query_value(target, "q"));
    query.market = trim(query_value(target, "market"));
    query.code = trim(query_value(target, "code"));
    query.broker_id = trim(query_value(target, "broker_id"));
    query.period = lower_ascii(trim(query_value(target, "period", "1m")));
    query.month = trim(query_value(target, "month"));
    query.industry = trim(query_value(target, "industry"));
    query.include_details = query_bool(target, "include_details",
        !query.market.empty() || !query.code.empty() ||
        !query.broker_id.empty() || !query.industry.empty());
    query.refresh = query_bool(target, "refresh");
    query.limit = parse_bounded(query_value(target, "limit", "500"),
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
    return state.block_trade_service->query(query);
}

}  // namespace tdx::server_detail
