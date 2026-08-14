<script lang="ts">
  /** Route-level lazy loader for the data center. */
  import type { Component } from 'svelte';

  interface Props { view: string; }
  type ViewModule = { default: Component };
  type ViewLoader = () => Promise<ViewModule>;

  const { view }: Props = $props();

  const loaders: Record<string, ViewLoader> = {
    ownership: () => import('./OwnershipView.svelte'),
    repurchases: () => import('./RepurchasesView.svelte'),
    'tender-offers': () => import('./TenderOffersView.svelte'),
    unlocks: () => import('./UnlocksView.svelte'),
    'block-trades': () => import('./BlockTradesView.svelte'),
    'block-rotation': () => import('./BlockRotationView.svelte'),
    'limit-ladder': () => import('./LimitLadderView.svelte'),
    'block-backtest': () => import('./BlockBacktestView.svelte'),
    'institution-lhb': () => import('./InstitutionLhbView.svelte'),
    'active-lhb': () => import('./ActiveLhbView.svelte'),
    'state-owned-reform': () => import('./StateOwnedReformView.svelte'),
    'institution-analysis': () => import('./InstitutionAnalysisView.svelte'),
    'foreign-alerts': () => import('./ForeignAlertsView.svelte'),
    'active-funds': () => import('./ActiveFundsView.svelte'),
    'etf-flows': () => import('./EtfFlowsView.svelte'),
    securities: () => import('./SecurityDirectoryView.svelte'),
    'convertible-bonds': () => import('./ConvertibleBondsView.svelte'),
    'bond-reference': () => import('./BondReferenceView.svelte'),
    'economic-indicators': () => import('./EconomicIndicatorsView.svelte'),
    'strategic-themes': () => import('./StrategicThemesView.svelte'),
    'theme-library': () => import('./ThemeLibraryView.svelte'),
    'thematic-opportunities': () => import('./ThematicOpportunitiesView.svelte'),
    calendar: () => import('./CalendarView.svelte'),
    employees: () => import('./EmployeesView.svelte'),
    'hk-events': () => import('./HkEventsView.svelte'),
    'hk-reference': () => import('./HkLocalReferenceView.svelte'),
    'special-situations': () => import('./SpecialSituationsView.svelte'),
    'special-attention': () => import('./SpecialAttentionView.svelte'),
    'exchange-funds': () => import('./ExchangeFundsView.svelte'),
    'fund-statistics': () => import('./FundStatisticsView.svelte'),
    'specialized-metrics': () => import('./SpecializedMetricsView.svelte'),
    'company-changes': () => import('./CompanyChangesView.svelte'),
    'financial-screen': () => import('./FinancialScreenView.svelte'),
    'financial-insights': () => import('./FinancialInsightsView.svelte'),
    gdr: () => import('./GdrView.svelte'),
    'fund-calendar': () => import('./FundCalendarView.svelte'),
    'equity-performance': () => import('./EquityPerformanceView.svelte'),
    'corporate-orders': () => import('./CorporateOrdersView.svelte'),
    'event-impact': () => import('./EventImpactView.svelte'),
    'global-performance': () => import('./GlobalPerformanceView.svelte'),
    'shareholder-signals': () => import('./ShareholderSignalsView.svelte'),
    'recent-watch': () => import('./RecentWatchView.svelte'),
    'patent-statistics': () => import('./PatentStatisticsView.svelte'),
    'overview-factors': () => import('./OverviewFactorsView.svelte'),
    'benchmark-analysis': () => import('./BenchmarkAnalysisView.svelte'),
    'curated-data': () => import('./CuratedDataView.svelte'),
    consensus: () => import('./ConsensusView.svelte'),
    'factor-signals': () => import('./FactorSignalsView.svelte'),
    'fund-analytics': () => import('./FundAnalyticsView.svelte'),
    'valuation-research': () => import('./ValuationResearchView.svelte'),
    'anomaly-risk': () => import('./AnomalyRiskView.svelte'),
    'hot-history': () => import('./HotHistoryView.svelte'),
    'flow-followup': () => import('./FlowFollowupView.svelte'),
    forecasts: () => import('./ForecastsView.svelte'),
    research: () => import('./ResearchView.svelte'),
    roadshows: () => import('./RoadshowsView.svelte'),
    ratings: () => import('./RatingsView.svelte'),
    valuation: () => import('./ValuationView.svelte'),
    'limit-quality': () => import('./LimitQualityView.svelte'),
    'limit-review': () => import('./LimitReviewView.svelte'),
    'session-turnover': () => import('./SessionTurnoverView.svelte'),
    margin: () => import('./MarginView.svelte'),
    'stock-connect': () => import('./StockConnectView.svelte'),
    intelligence: () => import('./IntelligenceView.svelte'),
    'threshold-stocks': () => import('./ThresholdStocksView.svelte'),
    'capital-strength': () => import('./CapitalStrengthView.svelte'),
    'strong-stocks': () => import('./StrongStocksView.svelte'),
    'commodity-links': () => import('./CommodityLinksView.svelte'),
    'announcement-signals': () => import('./AnnouncementSignalsView.svelte'),
    'reverse-repo': () => import('./ReverseRepoView.svelte'),
    'exchange-supervision': () => import('./ExchangeSupervisionView.svelte'),
    'futures-issuance': () => import('./FuturesIssuanceView.svelte'),
    options: () => import('./OptionsView.svelte'),
    'expansion-market': () => import('./ExpansionMarketView.svelte'),
    'local-reference': () => import('./LocalReferenceView.svelte')
  };

  const loader = $derived(loaders[view] ?? loaders.ownership);
</script>

{#key view}
  {#await loader()}
    <div class="route-state">正在加载数据模块…</div>
  {:then loaded}
    {@const View = loaded.default}
    <View />
  {:catch error}
    <div class="route-state error">数据模块加载失败：{error instanceof Error ? error.message : String(error)}</div>
  {/await}
{/key}

<style>
  .route-state {
    display: grid;
    flex: 1;
    place-items: center;
    min-height: 120px;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }
  .route-state.error { color: var(--negative); }
</style>
