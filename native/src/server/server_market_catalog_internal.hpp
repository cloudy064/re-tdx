#pragma once

#include "server_formula_internal.hpp"
#include "server_state_internal.hpp"

namespace tdx::server_detail {

Json query_market_active_funds(const ApiState&, const RequestTarget&);
Json query_market_etf_flows(const ApiState&, const RequestTarget&);
Json query_market_securities(const ApiState&, const RequestTarget&);
Json query_market_convertible_bonds(const ApiState&, const RequestTarget&);
Json query_market_bond_reference(const ApiState&, const RequestTarget&);
Json query_market_economic_indicators(const ApiState&, const RequestTarget&);
Json query_market_strategic_themes(const ApiState&, const RequestTarget&);
Json query_market_theme_library(const ApiState&, const RequestTarget&);
Json query_market_thematic_opportunities(const ApiState&, const RequestTarget&);
Json query_market_hot_history(const ApiState&, const RequestTarget&);
Json query_market_historical_securities(const ApiState&, const RequestTarget&);
Json query_market_index_events(const ApiState&, const RequestTarget&);
Json query_market_calendar(const ApiState&, const RequestTarget&);
Json query_market_employees(const ApiState&, const RequestTarget&);
Json query_market_hk_events(const ApiState&, const RequestTarget&);
Json query_market_hk_actions(const ApiState&, const RequestTarget&);
Json query_market_hk_finance(const ApiState&, const RequestTarget&);
Json query_market_hk_short_history(const ApiState&, const RequestTarget&);
Json query_market_special_situations(const ApiState&, const RequestTarget&);
Json query_market_exchange_funds(const ApiState&, const RequestTarget&);
Json query_market_fund_reference(const ApiState&, const RequestTarget&);
Json query_market_curated_data(const ApiState&, const RequestTarget&);
Json query_market_special_attention(const ApiState&, const RequestTarget&);
Json query_market_fund_statistics(const ApiState&, const RequestTarget&);
Json query_market_fund_calendar(const ApiState&, const RequestTarget&);
Json query_market_specialized_metrics(const ApiState&, const RequestTarget&);
Json query_market_company_changes(const ApiState&, const RequestTarget&);
Json query_market_financial_screen(const ApiState&, const RequestTarget&);
Json query_market_financial_insights(const ApiState&, const RequestTarget&);
Json query_market_gdr(const ApiState&, const RequestTarget&);
Json query_market_equity_performance(const ApiState&, const RequestTarget&);
Json query_market_corporate_orders(const ApiState&, const RequestTarget&);
Json query_market_event_impact(const ApiState&, const RequestTarget&);
Json query_market_global_performance(const ApiState&, const RequestTarget&);
Json query_market_shareholder_signals(const ApiState&, const RequestTarget&);
Json query_market_recent_watch(const ApiState&, const RequestTarget&);
Json query_market_patent_statistics(const ApiState&, const RequestTarget&);
Json query_market_overview_factors(const ApiState&, const RequestTarget&);
Json query_market_benchmark_analysis(const ApiState&, const RequestTarget&);

}  // namespace tdx::server_detail
