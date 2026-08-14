#include "tdx/server.hpp"

#include "tdx/active_funds.hpp"
#include "tdx/abnormal_details.hpp"
#include "tdx/abnormal_moves.hpp"
#include "tdx/anomaly_risk.hpp"
#include "tdx/announcement_signals.hpp"
#include "tdx/active_lhb.hpp"
#include "tdx/exchange_supervision.hpp"
#include "tdx/blocks.hpp"
#include "tdx/auction.hpp"
#include "tdx/block_backtest.hpp"
#include "tdx/block_rotation.hpp"
#include "tdx/block_trades.hpp"
#include "tdx/bond_reference.hpp"
#include "tdx/benchmark_analysis.hpp"
#include "tdx/calendar.hpp"
#include "tdx/capital_strength.hpp"
#include "tdx/cloud_workflow.hpp"
#include "tdx/cloud_resilience.hpp"
#include "tdx/cloud_routes.hpp"
#include "tdx/cloud_variants.hpp"
#include "tdx/cloud_calc.hpp"
#include "tdx/common.hpp"
#include "tdx/commodity_links.hpp"
#include "tdx/company_changes.hpp"
#include "tdx/consensus.hpp"
#include "tdx/convertible_bonds.hpp"
#include "tdx/corporate.hpp"
#include "tdx/corporate_orders.hpp"
#include "tdx/curated_data.hpp"
#include "tdx/disclosures.hpp"
#include "tdx/economic_indicators.hpp"
#include "tdx/etf_flows.hpp"
#include "tdx/employees.hpp"
#include "tdx/event_impact.hpp"
#include "tdx/exchange_funds.hpp"
#include "tdx/hk_events.hpp"
#include "tdx/equity_valuation.hpp"
#include "tdx/equity_performance.hpp"
#include "tdx/factors.hpp"
#include "tdx/financial_screen.hpp"
#include "tdx/financial_insights.hpp"
#include "tdx/formulas.hpp"
#include "tdx/formula_calc.hpp"
#include "tdx/formula_context.hpp"
#include "tdx/formula_engine.hpp"
#include "tdx/formula_render_profile.hpp"
#include "tdx/formula_strategy.hpp"
#include "tdx/foreign_alerts.hpp"
#include "tdx/flow_followup.hpp"
#include "tdx/forecasts.hpp"
#include "tdx/funds.hpp"
#include "tdx/fund_analytics.hpp"
#include "tdx/fund_statistics.hpp"
#include "tdx/fund_calendar.hpp"
#include "tdx/specialized_metrics.hpp"
#include "tdx/futures_issuance.hpp"
#include "tdx/gdr.hpp"
#include "tdx/global_performance.hpp"
#include "tdx/institution.hpp"
#include "tdx/institution_analysis.hpp"
#include "tdx/institution_lhb.hpp"
#include "tdx/industry_profile.hpp"
#include "tdx/index_volatility.hpp"
#include "tdx/intelligence.hpp"
#include "tdx/jsn.hpp"
#include "tdx/jsn_data.hpp"
#include "tdx/jsn_variants.hpp"
#include "tdx/json.hpp"
#include "tdx/leverage.hpp"
#include "tdx/level2.hpp"
#include "tdx/lhb.hpp"
#include "tdx/limit_quality.hpp"
#include "tdx/limit_ladder.hpp"
#include "tdx/limit_review.hpp"
#include "tdx/market.hpp"
#include "tdx/market_stream.hpp"
#include "tdx/minute.hpp"
#include "tdx/options.hpp"
#include "tdx/overview_factors.hpp"
#include "tdx/ownership.hpp"
#include "tdx/patent_statistics.hpp"
#include "tdx/panorama.hpp"
#include "tdx/pbrpc.hpp"
#include "tdx/professional_data.hpp"
#include "tdx/profit_gaps.hpp"
#include "tdx/ranking.hpp"
#include "tdx/recent_watch.hpp"
#include "tdx/ratings.hpp"
#include "tdx/registry.hpp"
#include "tdx/recon.hpp"
#include "tdx/relative_valuation.hpp"
#include "tdx/repurchases.hpp"
#include "tdx/reverse_repo.hpp"
#include "tdx/session_audit.hpp"
#include "tdx/research.hpp"
#include "tdx/roadshows.hpp"
#include "tdx/security_directory.hpp"
#include "tdx/state_owned_reform.hpp"
#include "tdx/strategic_themes.hpp"
#include "tdx/theme_library.hpp"
#include "tdx/session_turnover.hpp"
#include "tdx/shareholder_signals.hpp"
#include "tdx/special_attention.hpp"
#include "tdx/special_situations.hpp"
#include "tdx/stats.hpp"
#include "tdx/strong_stocks.hpp"
#include "tdx/technical_signals.hpp"
#include "tdx/tender_offers.hpp"
#include "tdx/thematic_opportunities.hpp"
#include "tdx/threshold_stocks.hpp"
#include "tdx/trades.hpp"
#include "tdx/tqlex.hpp"
#include "tdx/total_return_gap.hpp"
#include "tdx/tpool.hpp"
#include "tdx/unlocks.hpp"
#include "tdx/valuation.hpp"

#include "server_state_internal.hpp"

#include <filesystem>
#include <memory>
#include <set>
#include <utility>

namespace fs = std::filesystem;

namespace tdx::server_detail {

ApiState build_api_state(const fs::path& root, const fs::path& web_root,
                         const fs::path& jsn_root, const fs::path& cache_root,
                         bool include_user_formulas) {
    auto blocks = load_blocks(root, {"industry", "research-industry", "concept", "style", "index"});
    auto formulas = analyze_formula_library_document(
        load_bundled_installed_formula_library_document(
            root, include_user_formulas));
    auto formula_icons = load_bundled_formula_icon_sprite();
    std::shared_ptr<JsnIndex> jsn_index;
    if (!jsn_root.empty()) jsn_index = std::make_shared<JsnIndex>(jsn_root);
    ApiState state{};
    state.root = root;
    state.block_data = std::move(blocks);
    state.formulas = std::move(formulas);
    state.formula_icons = std::move(formula_icons);
    state.web_root = web_root;
    state.jsn_root = jsn_root;
    state.jsn_index = std::move(jsn_index);
    state.jsn_discovery_mutex = std::make_shared<std::mutex>();
    state.funds_service = std::make_shared<IntradayFundsService>(root, state.block_data);
    state.institution_service = std::make_shared<InstitutionService>(
        state.block_data.securities, TqlexTransport{}, cache_root / "institution");
    state.institution_analysis_service =
        std::make_shared<InstitutionAnalysisService>(state.block_data.securities);
    state.limit_review_service =
        std::make_shared<LimitReviewService>(state.block_data.securities);
    state.session_turnover_service =
        std::make_shared<SessionTurnoverService>(state.block_data.securities);
    state.block_rotation_service =
        std::make_shared<BlockRotationService>(state.block_data);
    state.limit_ladder_service =
        std::make_shared<LimitLadderService>(state.block_data);
    state.threshold_stocks_service =
        std::make_shared<ThresholdStocksService>(state.block_data.securities);
    state.capital_strength_service =
        std::make_shared<CapitalStrengthService>(state.block_data.securities);
    state.strong_stocks_service =
        std::make_shared<StrongStocksService>(state.block_data.securities);
    state.commodity_links_service =
        std::make_shared<CommodityLinksService>(state.block_data.securities);
    state.announcement_signals_service =
        std::make_shared<AnnouncementSignalsService>(state.block_data.securities);
    state.reverse_repo_service =
        std::make_shared<ReverseRepoService>(root, state.block_data.securities);
    state.exchange_supervision_service =
        std::make_shared<ExchangeSupervisionService>(root, state.block_data.securities);
    state.panorama_service = std::make_shared<PanoramaService>(state.block_data.securities);
    state.industry_profile_service =
        std::make_shared<IndustryProfileService>(state.block_data);
    state.unlock_service = std::make_shared<UnlockService>(
        state.block_data.securities, state.jsn_root);
    state.block_trade_service =
        std::make_shared<BlockTradeService>(state.block_data.securities);
    state.repurchase_service =
        std::make_shared<RepurchaseService>(state.block_data.securities,
                                            state.jsn_root);
    state.tender_offer_service =
        std::make_shared<TenderOfferService>(state.block_data.securities);
    state.ownership_service =
        std::make_shared<OwnershipService>(state.block_data.securities);
    state.forecast_service =
        std::make_shared<ForecastService>(state.block_data);
    state.disclosure_service =
        std::make_shared<DisclosureService>(state.block_data.securities);
    state.active_fund_service =
        std::make_shared<ActiveFundService>(state.block_data);
    state.etf_flow_service = std::make_shared<EtfFlowService>(state.block_data);
    state.security_directory_service = std::make_shared<SecurityDirectoryService>();
    state.convertible_bond_service =
        std::make_shared<ConvertibleBondService>(root, state.block_data.securities);
    state.bond_reference_service =
        std::make_shared<BondReferenceService>(root, state.jsn_root);
    state.economic_indicator_service =
        std::make_shared<EconomicIndicatorService>(root, state.block_data.securities);
    state.strategic_theme_service =
        std::make_shared<StrategicThemeService>(root, state.block_data.securities);
    state.theme_library_service =
        std::make_shared<ThemeLibraryService>(root, state.block_data.securities);
    state.thematic_opportunity_service =
        std::make_shared<ThematicOpportunityService>(state.block_data.securities);
    state.calendar_service = std::make_shared<CalendarService>(
        state.block_data.securities, state.jsn_root);
    state.employee_service = std::make_shared<EmployeeService>(
        state.block_data.securities, state.jsn_root);
    state.hk_event_service = std::make_shared<HkEventService>(state.jsn_root);
    state.special_situation_service = std::make_shared<SpecialSituationService>(
        root, state.block_data.securities, state.jsn_root);
    state.exchange_fund_service = std::make_shared<ExchangeFundService>(
        root, state.block_data.securities, state.jsn_root);
    state.curated_data_service = std::make_shared<CuratedDataService>(
        root, state.block_data.securities, state.jsn_root);
    state.special_attention_service = std::make_shared<SpecialAttentionService>(
        root, state.block_data.securities, state.jsn_root);
    state.fund_statistics_service =
        std::make_shared<FundStatisticsService>(state.jsn_root);
    state.fund_calendar_service =
        std::make_shared<FundCalendarService>(state.jsn_root);
    state.specialized_metrics_service = std::make_shared<SpecializedMetricsService>(
        root, state.block_data.securities, state.jsn_root);
    state.company_changes_service = std::make_shared<CompanyChangesService>(
        root, state.block_data.securities, state.jsn_root);
    state.financial_screen_service = std::make_shared<FinancialScreenService>(
        root, state.block_data.securities, state.jsn_root);
    state.financial_insights_service = std::make_shared<FinancialInsightsService>(
        root, state.block_data.securities, state.jsn_root);
    state.gdr_service = std::make_shared<GdrService>(
        root, state.block_data.securities, state.jsn_root);
    state.equity_performance_service = std::make_shared<EquityPerformanceService>(
        root, state.block_data.securities, state.jsn_root);
    state.corporate_orders_service = std::make_shared<CorporateOrdersService>(
        state.block_data.securities, state.jsn_root);
    state.event_impact_service = std::make_shared<EventImpactService>(state.jsn_root);
    state.global_performance_service = std::make_shared<GlobalPerformanceService>(
        state.block_data.securities, state.jsn_root);
    state.shareholder_signals_service = std::make_shared<ShareholderSignalsService>(
        root, state.block_data.securities, state.jsn_root);
    state.recent_watch_service = std::make_shared<RecentWatchService>(
        root, state.block_data.securities, state.jsn_root);
    state.patent_statistics_service = std::make_shared<PatentStatisticsService>(
        state.block_data.securities, state.jsn_root);
    state.overview_factors_service =
        std::make_shared<OverviewFactorsService>(state.jsn_root);
    state.benchmark_analysis_service = std::make_shared<BenchmarkAnalysisService>(
        root, state.block_data.securities, state.jsn_root);
    state.institution_lhb_service =
        std::make_shared<InstitutionLhbService>(state.block_data.securities);
    state.active_lhb_service =
        std::make_shared<ActiveLhbService>(state.block_data.securities);
    state.state_owned_reform_service =
        std::make_shared<StateOwnedReformService>(root, state.block_data.securities);
    state.rating_service =
        std::make_shared<RatingService>(root, state.block_data);
    state.foreign_alert_service =
        std::make_shared<ForeignAlertService>(state.block_data.securities);
    state.leverage_service = std::make_shared<LeverageService>(state.block_data);
    state.intelligence_service = std::make_shared<IntelligenceService>(state.block_data);
    state.technical_signals_service =
        std::make_shared<TechnicalSignalsService>(root, state.block_data);
    state.factor_service = std::make_shared<FactorService>(root, state.block_data);
    state.market_stream_hub = std::make_shared<MarketStreamHub>(root, state.block_data);
    state.block_backtest_service =
        std::make_shared<BlockBacktestService>(root, state.block_data);
    state.equity_valuation_service =
        std::make_shared<EquityValuationService>(root, state.block_data);
    state.flow_followup_service = std::make_shared<FlowFollowupService>(root);
    state.abnormal_moves_service = std::make_shared<AbnormalMovesService>(root, state.block_data);
    state.abnormal_details_service =
        std::make_shared<AbnormalDetailsService>(root, state.block_data);
    state.anomaly_risk_service = std::make_shared<AnomalyRiskService>(root, state.block_data);
    state.profit_gaps_service = std::make_shared<ProfitGapsService>(root, state.block_data);
    state.index_volatility_service =
        std::make_shared<IndexVolatilityService>(root, state.block_data);
    state.total_return_gap_service = std::make_shared<TotalReturnGapService>(root);
    state.fund_analytics_service = std::make_shared<FundAnalyticsService>(root);
    state.lhb_service = std::make_shared<LhbService>(state.block_data.securities);
    state.consensus_service = std::make_shared<ConsensusService>(state.block_data.securities);
    std::map<std::string, std::string> industry_names;
    for (const auto& block : state.block_data.blocks)
        if (!block.block_code.empty()) industry_names.emplace(block.block_code, block.name);
    state.research_service = std::make_shared<ResearchService>(
        state.block_data.securities, std::move(industry_names));
    state.roadshow_service = std::make_shared<RoadshowService>(root, state.block_data);
    state.valuation_service = std::make_shared<ValuationService>(state.block_data.securities);
    state.relative_valuation_service =
        std::make_shared<RelativeValuationService>(root, state.block_data.securities);
    state.futures_issuance_service =
        std::make_shared<FuturesIssuanceService>(state.block_data.securities);
    return state;
}

}  // namespace tdx::server_detail

