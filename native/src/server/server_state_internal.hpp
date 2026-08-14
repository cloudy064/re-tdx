#pragma once

#include "tdx/blocks.hpp"
#include "tdx/formula_render_profile.hpp"
#include "tdx/formulas.hpp"
#include "tdx/json.hpp"

#include <cstddef>
#include <ctime>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace tdx {

class AbnormalDetailsService;
class AbnormalMovesService;
class ActiveFundService;
class ActiveLhbService;
class AnnouncementSignalsService;
class AnomalyRiskService;
class BenchmarkAnalysisService;
class BlockBacktestService;
class BlockRotationService;
class BlockTradeService;
class BondReferenceService;
class CalendarService;
class CapitalStrengthService;
class CommodityLinksService;
class CompanyChangesService;
class ConsensusService;
class ConvertibleBondService;
class CorporateOrdersService;
class CuratedDataService;
class DisclosureService;
class EconomicIndicatorService;
class EmployeeService;
class EquityPerformanceService;
class EquityValuationService;
class EtfFlowService;
class EventImpactService;
class ExchangeFundService;
class ExchangeSupervisionService;
class FactorService;
class FinancialInsightsService;
class FinancialScreenService;
class FlowFollowupService;
class ForecastService;
class ForeignAlertService;
class FundAnalyticsService;
class FundCalendarService;
class FundStatisticsService;
class FuturesIssuanceService;
class GdrService;
class GlobalPerformanceService;
class HkEventService;
class IndexVolatilityService;
class IndustryProfileService;
class InstitutionAnalysisService;
class InstitutionLhbService;
class InstitutionService;
class IntelligenceService;
class IntradayFundsService;
class JsnIndex;
class LeverageService;
class LhbService;
class LimitLadderService;
class LimitReviewService;
class MarketStreamHub;
class OverviewFactorsService;
class OwnershipService;
class PanoramaService;
class PatentStatisticsService;
class ProfitGapsService;
class RatingService;
class RecentWatchService;
class RelativeValuationService;
class RepurchaseService;
class ResearchService;
class ReverseRepoService;
class RoadshowService;
class SecurityDirectoryService;
class SessionTurnoverService;
class ShareholderSignalsService;
class SpecialAttentionService;
class SpecializedMetricsService;
class SpecialSituationService;
class StateOwnedReformService;
class StrategicThemeService;
class StrongStocksService;
class TdxStatsResource;
class TechnicalSignalsService;
class TenderOfferService;
class ThematicOpportunityService;
class ThemeLibraryService;
class ThresholdStocksService;
class TotalReturnGapService;
class UnlockService;
class ValuationService;

namespace server_detail {

struct HttpResponse {
    int status{200};
    std::string reason{"OK"};
    std::string content_type{"application/json; charset=utf-8"};
    std::string body;
};

inline constexpr std::size_t maximum_post_request_bytes = 1024 * 1024;

struct ApiState {
    std::filesystem::path root;
    BlockData block_data;
    Json formulas;
    FormulaIconSprite formula_icons;
    std::filesystem::path web_root;
    std::filesystem::path serving_executable;
    std::string serving_executable_sha256;
    std::string web_index_sha256;
    std::filesystem::path jsn_root;
    std::shared_ptr<JsnIndex> jsn_index;
    mutable std::shared_ptr<Json> jsn_discovery_cache;
    mutable std::map<std::string, std::shared_ptr<Json>> jsn_candidate_cache;
    mutable std::shared_ptr<std::mutex> jsn_discovery_mutex;
    mutable std::shared_ptr<TdxStatsResource> stats_cache;
    mutable std::string stats_endpoint;
    mutable std::string stats_server_name;
    mutable std::size_t stats_archive_size{};
    mutable Json stats_transport;
    mutable std::time_t stats_cache_time{};
    std::shared_ptr<IntradayFundsService> funds_service;
    std::shared_ptr<InstitutionService> institution_service;
    std::shared_ptr<InstitutionAnalysisService> institution_analysis_service;
    std::shared_ptr<LimitReviewService> limit_review_service;
    std::shared_ptr<SessionTurnoverService> session_turnover_service;
    std::shared_ptr<BlockRotationService> block_rotation_service;
    std::shared_ptr<LimitLadderService> limit_ladder_service;
    std::shared_ptr<ThresholdStocksService> threshold_stocks_service;
    std::shared_ptr<CapitalStrengthService> capital_strength_service;
    std::shared_ptr<StrongStocksService> strong_stocks_service;
    std::shared_ptr<CommodityLinksService> commodity_links_service;
    std::shared_ptr<AnnouncementSignalsService> announcement_signals_service;
    std::shared_ptr<ReverseRepoService> reverse_repo_service;
    std::shared_ptr<ExchangeSupervisionService> exchange_supervision_service;
    std::shared_ptr<PanoramaService> panorama_service;
    std::shared_ptr<IndustryProfileService> industry_profile_service;
    std::shared_ptr<UnlockService> unlock_service;
    std::shared_ptr<LhbService> lhb_service;
    std::shared_ptr<ConsensusService> consensus_service;
    std::shared_ptr<ResearchService> research_service;
    std::shared_ptr<RoadshowService> roadshow_service;
    std::shared_ptr<ValuationService> valuation_service;
    std::shared_ptr<RelativeValuationService> relative_valuation_service;
    std::shared_ptr<EquityValuationService> equity_valuation_service;
    std::shared_ptr<FlowFollowupService> flow_followup_service;
    std::shared_ptr<AbnormalMovesService> abnormal_moves_service;
    std::shared_ptr<AbnormalDetailsService> abnormal_details_service;
    std::shared_ptr<AnomalyRiskService> anomaly_risk_service;
    std::shared_ptr<ProfitGapsService> profit_gaps_service;
    std::shared_ptr<IndexVolatilityService> index_volatility_service;
    std::shared_ptr<TotalReturnGapService> total_return_gap_service;
    std::shared_ptr<FundAnalyticsService> fund_analytics_service;
    std::shared_ptr<FuturesIssuanceService> futures_issuance_service;
    std::shared_ptr<BlockTradeService> block_trade_service;
    std::shared_ptr<RepurchaseService> repurchase_service;
    std::shared_ptr<TenderOfferService> tender_offer_service;
    std::shared_ptr<OwnershipService> ownership_service;
    std::shared_ptr<ForecastService> forecast_service;
    std::shared_ptr<DisclosureService> disclosure_service;
    std::shared_ptr<ActiveFundService> active_fund_service;
    std::shared_ptr<EtfFlowService> etf_flow_service;
    std::shared_ptr<SecurityDirectoryService> security_directory_service;
    std::shared_ptr<ConvertibleBondService> convertible_bond_service;
    std::shared_ptr<BondReferenceService> bond_reference_service;
    std::shared_ptr<EconomicIndicatorService> economic_indicator_service;
    std::shared_ptr<StrategicThemeService> strategic_theme_service;
    std::shared_ptr<ThemeLibraryService> theme_library_service;
    std::shared_ptr<ThematicOpportunityService> thematic_opportunity_service;
    std::shared_ptr<CalendarService> calendar_service;
    std::shared_ptr<EmployeeService> employee_service;
    std::shared_ptr<HkEventService> hk_event_service;
    std::shared_ptr<SpecialSituationService> special_situation_service;
    std::shared_ptr<ExchangeFundService> exchange_fund_service;
    std::shared_ptr<CuratedDataService> curated_data_service;
    std::shared_ptr<SpecialAttentionService> special_attention_service;
    std::shared_ptr<FundStatisticsService> fund_statistics_service;
    std::shared_ptr<FundCalendarService> fund_calendar_service;
    std::shared_ptr<SpecializedMetricsService> specialized_metrics_service;
    std::shared_ptr<CompanyChangesService> company_changes_service;
    std::shared_ptr<FinancialScreenService> financial_screen_service;
    std::shared_ptr<FinancialInsightsService> financial_insights_service;
    std::shared_ptr<GdrService> gdr_service;
    std::shared_ptr<EquityPerformanceService> equity_performance_service;
    std::shared_ptr<CorporateOrdersService> corporate_orders_service;
    std::shared_ptr<EventImpactService> event_impact_service;
    std::shared_ptr<GlobalPerformanceService> global_performance_service;
    std::shared_ptr<ShareholderSignalsService> shareholder_signals_service;
    std::shared_ptr<RecentWatchService> recent_watch_service;
    std::shared_ptr<PatentStatisticsService> patent_statistics_service;
    std::shared_ptr<OverviewFactorsService> overview_factors_service;
    std::shared_ptr<BenchmarkAnalysisService> benchmark_analysis_service;
    std::shared_ptr<InstitutionLhbService> institution_lhb_service;
    std::shared_ptr<ActiveLhbService> active_lhb_service;
    std::shared_ptr<StateOwnedReformService> state_owned_reform_service;
    std::shared_ptr<RatingService> rating_service;
    std::shared_ptr<ForeignAlertService> foreign_alert_service;
    std::shared_ptr<LeverageService> leverage_service;
    std::shared_ptr<IntelligenceService> intelligence_service;
    std::shared_ptr<TechnicalSignalsService> technical_signals_service;
    std::shared_ptr<FactorService> factor_service;
    std::shared_ptr<BlockBacktestService> block_backtest_service;
    std::shared_ptr<MarketStreamHub> market_stream_hub;
    mutable std::shared_ptr<Json> special_limits_cache;
    mutable std::time_t special_limits_cache_time{};
    mutable std::string special_limits_cache_source;
    mutable std::shared_ptr<Json> limit_quality_cache;
    mutable std::time_t limit_quality_cache_time{};
    mutable int limit_quality_cache_limit{};
    mutable int limit_quality_cache_auction_limit{};
};

ApiState build_api_state(const std::filesystem::path& root,
                         const std::filesystem::path& web_root,
                         const std::filesystem::path& jsn_root,
                         const std::filesystem::path& cache_root,
                         bool include_user_formulas = false);

}  // namespace server_detail
}  // namespace tdx
