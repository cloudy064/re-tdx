#include "tdx/cloud_resilience.hpp"
#include "tdx/common.hpp"
#include "tdx/pbrpc.hpp"
#include "tdx/tqlex.hpp"

#include "server_formula_internal.hpp"
#include "server_http_internal.hpp"
#include "server_market_analytics_internal.hpp"
#include "server_market_catalog_internal.hpp"
#include "server_market_events_internal.hpp"
#include "server_market_institution_internal.hpp"
#include "server_market_internal.hpp"
#include "server_market_realtime_internal.hpp"
#include "server_market_research_internal.hpp"
#include "server_state_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
namespace tdx::server_detail {

std::map<std::string, std::string> query_json_string_map(
    const RequestTarget& target, const std::string& key) {
    const auto raw = trim(query_value(target, key));
    if (raw.empty()) return {};
    const auto value = Json::parse(raw);
    if (!value.is_object()) throw Error(key + " must be a JSON object");
    std::map<std::string, std::string> result;
    for (const auto& [name, item] : value.as_object()) {
        if (item.is_string()) result[name] = item.as_string();
        else if (item.is_number() || item.is_bool()) result[name] = item.dump(-1);
        else if (item.is_null()) result[name] = "";
        else throw Error(key + " values must be strings, numbers, booleans or null");
    }
    return result;
}

TqlexHttpQueryPlan tqlex_http_query_plan(const RequestTarget& target) {
    TqlexHttpQueryPlan result;
    result.page = parse_bounded(query_value(target, "page", "0"),
                                "page", 0, 1000000);
    result.page_size = parse_bounded(query_value(target, "page_size", "20"),
                                     "page_size", 1, 5000);
    result.requested_max_pages = parse_bounded(
        query_value(target, "max_pages", "20"), "max_pages", 1, 20);
    result.all_pages = query_bool(target, "all_pages");
    if (result.all_pages &&
        result.requested_max_pages > 50000 / result.page_size)
        throw Error("TQLEX all_pages page_size * max_pages must not exceed 50000 rows");
    result.effective_max_pages = result.all_pages
        ? result.requested_max_pages : 1;
    return result;
}

Json query_tqlex_api(const ApiState& state, const RequestTarget& target) {
    const auto request_id = trim(query_value(target, "req_id"));
    if (request_id.empty() || request_id.size() > 12 ||
        !std::all_of(request_id.begin(), request_id.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("req_id must contain 1..12 digits");
    const auto query_plan = tqlex_http_query_plan(target);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                      "timeout_ms", 100, 30000);
    std::vector<std::string> body_contains;
    const auto body_needle = query_value(target, "body_contains");
    if (!body_needle.empty()) body_contains.push_back(body_needle);
    int attempts = 0;
    auto document = detail::retry_cloud_json([&] {
        return execute_tqlex_config(state.root, request_id,
            query_json_string_map(target, "set"), query_json_string_map(target, "param"),
            trim(query_value(target, "entry")), trim(query_value(target, "source_file")),
            body_contains, query_plan.all_pages, query_plan.page,
            query_plan.page_size, query_plan.effective_max_pages,
            cloud_endpoints::tqlex, timeout);
    }, detail::is_transient_tqlex_error, attempts, 3, 250);
    document["attempts"] = attempts;
    document["max_attempts"] = 3;
    return document;
}

Json query_pbrpc_api(const ApiState& state, const RequestTarget& target) {
    const auto request_id = trim(query_value(target, "req_id"));
    if (request_id.empty() || request_id.size() > 12 ||
        !std::all_of(request_id.begin(), request_id.end(), [](char ch) {
            return ch >= '0' && ch <= '9';
        })) throw Error("req_id must contain 1..12 digits");
    const int max_rounds = parse_bounded(query_value(target, "max_rounds", "32"),
                                         "max_rounds", 1, 100);
    const int timeout = parse_bounded(query_value(target, "timeout_ms", "15000"),
                                      "timeout_ms", 100, 30000);
    const int retry_delay = parse_bounded(
        query_value(target, "retry_delay_ms", "150"), "retry_delay_ms", 0, 5000);
    const int max_assembled_bytes = parse_bounded(
        query_value(target, "max_assembled_bytes", "134217728"),
        "max_assembled_bytes", 1,
        static_cast<int>(pbrpc_maximum_assembled_bytes));
    std::vector<std::string> body_contains;
    const auto body_needle = query_value(target, "body_contains");
    if (!body_needle.empty()) body_contains.push_back(body_needle);
    int attempts = 0;
    auto document = detail::retry_cloud_json([&] {
        return execute_pbrpc_config(state.root, request_id,
            query_json_string_map(target, "set"), query_json_string_map(target, "param"),
            trim(query_value(target, "entry")), trim(query_value(target, "module")),
            trim(query_value(target, "source_file")), body_contains,
            cloud_endpoints::tqlex, timeout, max_rounds,
            retry_delay, static_cast<std::size_t>(max_assembled_bytes));
    }, detail::is_transient_pbrpc_error, attempts, 3, 250);
    document["attempts"] = attempts;
    document["max_attempts"] = 3;
    return document;
}

enum class UpstreamErrorPolicy : unsigned {
    none = 0,
    known_transient = 1U << 0,
    tqlex = 1U << 1,
    pbrpc = 1U << 2,
    rpc_id_minus_one = 1U << 3,
};

constexpr UpstreamErrorPolicy operator|(UpstreamErrorPolicy left,
                                        UpstreamErrorPolicy right) {
    return static_cast<UpstreamErrorPolicy>(
        static_cast<unsigned>(left) | static_cast<unsigned>(right));
}

bool has_policy(UpstreamErrorPolicy value, UpstreamErrorPolicy flag) {
    return (static_cast<unsigned>(value) & static_cast<unsigned>(flag)) != 0;
}

bool policy_accepts_upstream_error(UpstreamErrorPolicy policy,
                                   const std::string& message) {
    if (has_policy(policy, UpstreamErrorPolicy::known_transient) &&
        upstream_unavailable_error(message))
        return true;
    if (has_policy(policy, UpstreamErrorPolicy::tqlex) &&
        (message.rfind("TQLEX HTTP status", 0) == 0 ||
         message.rfind("TQLEX server returned ErrorCode", 0) == 0))
        return true;
    if (has_policy(policy, UpstreamErrorPolicy::pbrpc) &&
        (message.rfind("PBRPC HTTP status", 0) == 0 ||
         message.rfind("PBRPC business ErrorCode", 0) == 0))
        return true;
    return has_policy(policy, UpstreamErrorPolicy::rpc_id_minus_one) &&
           message.find("RpcID -1") != std::string::npos;
}

bool retryable_upstream_error(const std::string& message) {
    for (const auto* token : {"ErrorCode 4", "RpcID -1", "status 429",
                              "status 502", "status 503", "status 504"})
        if (message.find(token) != std::string::npos) return true;
    return false;
}

HttpResponse classified_upstream_response(UpstreamErrorPolicy policy,
                                          const std::string& message) {
    if (has_policy(policy, UpstreamErrorPolicy::known_transient))
        return upstream_unavailable_response(message);
    Json body = Json::object();
    body["error"] = "upstream_unavailable";
    body["message"] = message;
    body["retryable"] = retryable_upstream_error(message);
    return json_response(std::move(body), 503, "Service Unavailable");
}

using ApiJsonHandler = Json (*)(const ApiState&, const RequestTarget&);

struct ApiJsonRoute {
    std::string_view path;
    ApiJsonHandler handler;
    UpstreamErrorPolicy upstream_policy{UpstreamErrorPolicy::none};
};

constexpr ApiJsonRoute market_json_routes[]{
    {"/api/v1/market/snapshot", query_market_snapshot},
    {"/api/v1/market/speed", query_market_speed},
    {"/api/v1/market/instruments", query_market_instruments_route},
    {"/api/v1/market/options", query_market_options_route},
    {"/api/v1/market/option-expiry", query_market_option_expiry},
    {"/api/v1/market/option-chain", query_market_option_chain},
    {"/api/v1/market/option-volatility", query_market_option_volatility},
    {"/api/v1/market/expansion-timeline", query_market_expansion_timeline_route},
    {"/api/v1/market/expansion-trades", query_market_expansion_trades_route},
    {"/api/v1/market/depth", query_market_depth},
    {"/api/v1/market/finance", query_market_finance},
    {"/api/v1/market/capital", query_market_capital},
    {"/api/v1/market/limits", query_market_limits},
    {"/api/v1/market/funds", query_intraday_funds,
     UpstreamErrorPolicy::known_transient},
    {"/api/v1/market/holder", query_holder},
    {"/api/v1/market/lhb", query_lhb},
    {"/api/v1/market/consensus", query_market_consensus},
    {"/api/v1/market/research", query_market_research},
    {"/api/v1/market/roadshows", query_market_roadshows},
    {"/api/v1/market/industry-profile", query_market_industry_profile},
    {"/api/v1/market/unlocks", query_market_unlocks},
    {"/api/v1/market/block-trades", query_market_block_trades},
    {"/api/v1/market/ownership", query_market_ownership},
    {"/api/v1/market/forecasts", query_market_forecasts},
    {"/api/v1/market/disclosures", query_market_disclosures},
    {"/api/v1/market/foreign-alerts", query_market_foreign_alerts},
    {"/api/v1/market/active-funds", query_market_active_funds,
     UpstreamErrorPolicy::known_transient},
    {"/api/v1/market/etf-flows", query_market_etf_flows},
    {"/api/v1/market/securities", query_market_securities},
    {"/api/v1/market/convertible-bonds", query_market_convertible_bonds},
    {"/api/v1/market/bond-reference", query_market_bond_reference},
    {"/api/v1/market/economic-indicators", query_market_economic_indicators},
    {"/api/v1/market/strategic-themes", query_market_strategic_themes},
    {"/api/v1/market/theme-library", query_market_theme_library},
    {"/api/v1/market/thematic-opportunities", query_market_thematic_opportunities},
    {"/api/v1/market/hot-history", query_market_hot_history},
    {"/api/v1/market/historical-securities", query_market_historical_securities},
    {"/api/v1/market/index-events", query_market_index_events},
    {"/api/v1/market/calendar", query_market_calendar},
    {"/api/v1/market/employees", query_market_employees},
    {"/api/v1/market/hk-events", query_market_hk_events},
    {"/api/v1/market/hk-actions", query_market_hk_actions},
    {"/api/v1/market/hk-finance", query_market_hk_finance},
    {"/api/v1/market/hk-short-history", query_market_hk_short_history},
    {"/api/v1/market/special-situations", query_market_special_situations},
    {"/api/v1/market/exchange-funds", query_market_exchange_funds},
    {"/api/v1/market/fund-reference", query_market_fund_reference},
    {"/api/v1/market/curated-data", query_market_curated_data},
    {"/api/v1/market/special-attention", query_market_special_attention},
    {"/api/v1/market/fund-statistics", query_market_fund_statistics},
    {"/api/v1/market/fund-calendar", query_market_fund_calendar},
    {"/api/v1/market/specialized-metrics", query_market_specialized_metrics},
    {"/api/v1/market/company-changes", query_market_company_changes},
    {"/api/v1/market/financial-screen", query_market_financial_screen},
    {"/api/v1/market/financial-insights", query_market_financial_insights},
    {"/api/v1/market/gdr", query_market_gdr},
    {"/api/v1/market/equity-performance", query_market_equity_performance},
    {"/api/v1/market/corporate-orders", query_market_corporate_orders},
    {"/api/v1/market/event-impact", query_market_event_impact},
    {"/api/v1/market/global-performance", query_market_global_performance},
    {"/api/v1/market/shareholder-signals", query_market_shareholder_signals},
    {"/api/v1/market/recent-watch", query_market_recent_watch},
    {"/api/v1/market/patent-statistics", query_market_patent_statistics},
    {"/api/v1/market/overview-factors", query_market_overview_factors},
    {"/api/v1/market/benchmark-analysis", query_market_benchmark_analysis},
    {"/api/v1/market/margin", query_market_margin},
    {"/api/v1/market/stock-connect", query_market_stock_connect},
    {"/api/v1/market/intelligence", query_market_intelligence},
    {"/api/v1/market/panorama", query_market_panorama},
    {"/api/v1/market/institution-analysis", query_market_institution_analysis},
    {"/api/v1/market/limit-review", query_market_limit_review},
    {"/api/v1/market/session-turnover", query_market_session_turnover},
    {"/api/v1/market/block-rotation", query_market_block_rotation},
    {"/api/v1/market/limit-ladder", query_market_limit_ladder},
    {"/api/v1/market/threshold-stocks", query_market_threshold_stocks},
    {"/api/v1/market/capital-strength", query_market_capital_strength},
    {"/api/v1/market/strong-stocks", query_market_strong_stocks},
    {"/api/v1/market/commodity-links", query_market_commodity_links},
    {"/api/v1/market/announcement-signals", query_market_announcement_signals},
    {"/api/v1/market/reverse-repo", query_market_reverse_repo},
    {"/api/v1/market/exchange-supervision", query_market_exchange_supervision},
    {"/api/v1/market/factors", query_market_factors,
     UpstreamErrorPolicy::tqlex | UpstreamErrorPolicy::rpc_id_minus_one},
    {"/api/v1/market/technical-signals", query_market_technical_signals,
     UpstreamErrorPolicy::tqlex | UpstreamErrorPolicy::pbrpc |
         UpstreamErrorPolicy::rpc_id_minus_one},
    {"/api/v1/market/block-backtest", query_market_block_backtest,
     UpstreamErrorPolicy::pbrpc},
    {"/api/v1/market/equity-valuation", query_market_equity_valuation,
     UpstreamErrorPolicy::tqlex | UpstreamErrorPolicy::pbrpc |
         UpstreamErrorPolicy::rpc_id_minus_one},
    {"/api/v1/market/relative-valuation", query_market_relative_valuation,
     UpstreamErrorPolicy::tqlex | UpstreamErrorPolicy::pbrpc |
         UpstreamErrorPolicy::rpc_id_minus_one},
    {"/api/v1/market/flow-followup", query_market_flow_followup,
     UpstreamErrorPolicy::tqlex | UpstreamErrorPolicy::pbrpc |
         UpstreamErrorPolicy::rpc_id_minus_one},
    {"/api/v1/market/abnormal-moves", query_market_abnormal_moves},
    {"/api/v1/market/abnormal-details", query_market_abnormal_details,
     UpstreamErrorPolicy::tqlex},
    {"/api/v1/market/anomaly-risk", query_market_anomaly_risk,
     UpstreamErrorPolicy::tqlex},
    {"/api/v1/market/profit-gaps", query_market_profit_gaps},
    {"/api/v1/market/index-volatility", query_market_index_volatility},
    {"/api/v1/market/total-return-gap", query_market_total_return_gap},
    {"/api/v1/market/fund-analytics", query_market_fund_analytics},
    {"/api/v1/market/institution-lhb", query_market_institution_lhb},
    {"/api/v1/market/active-lhb", query_market_active_lhb},
    {"/api/v1/market/state-owned-reform", query_market_state_owned_reform},
    {"/api/v1/market/ratings", query_market_ratings},
    {"/api/v1/market/repurchases", query_market_repurchases},
    {"/api/v1/market/tender-offers", query_market_tender_offers},
    {"/api/v1/market/valuation", query_market_valuation},
    {"/api/v1/market/futures-issuance", query_market_futures_issuance},
    {"/api/v1/market/auction", query_market_auction},
    {"/api/v1/market/ranking", query_market_ranking},
    {"/api/v1/market/limit-quality", query_market_limit_quality},
    {"/api/v1/market/stats", query_market_stats},
    {"/api/v1/market/professional", query_market_professional_route},
    {"/api/v1/market/trades", query_market_trades},
};

constexpr bool market_route_paths_are_unique() {
    for (std::size_t left = 0; left < std::size(market_json_routes); ++left)
        for (std::size_t right = left + 1;
             right < std::size(market_json_routes); ++right)
            if (market_json_routes[left].path == market_json_routes[right].path)
                return false;
    return true;
}

static_assert(std::size(market_json_routes) == 108,
              "update the audited market route count when adding an endpoint");
static_assert(market_route_paths_are_unique(),
              "market route paths must be unique");

const std::unordered_map<std::string_view, const ApiJsonRoute*>&
market_json_route_index() {
    static const auto index = [] {
        std::unordered_map<std::string_view, const ApiJsonRoute*> routes;
        routes.reserve(std::size(market_json_routes));
        for (const auto& route : market_json_routes)
            routes.emplace(route.path, &route);
        return routes;
    }();
    return index;
}

std::optional<HttpResponse> route_registered_market_api(
    const ApiState& state, const RequestTarget& target) {
    const auto& index = market_json_route_index();
    const auto found = index.find(std::string_view(target.path));
    if (found == index.end()) return std::nullopt;
    const auto& route = *found->second;
    try {
        return json_response(route.handler(state, target));
    } catch (const Error& error) {
        const std::string message = error.what();
        if (!policy_accepts_upstream_error(route.upstream_policy, message)) throw;
        return classified_upstream_response(route.upstream_policy, message);
    }
}

}  // namespace tdx::server_detail

