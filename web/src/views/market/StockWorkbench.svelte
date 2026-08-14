<script lang="ts" module>
  export interface PanelProps {
    market: string;
    code: string;
    name: string;
  }

  /** 个股页签按语义收成 5 组；机构动向同时容纳公开基金与 ETF 口径。 */
  export const TAB_GROUPS = [
    {
      id: 'trading',
      label: '盘口交易',
      items: [
        { id: 'quote', label: '行情图表' },
        { id: 'speed', label: '涨速盘口' },
        { id: 'auction', label: '集合竞价' },
        { id: 'trades', label: '成交明细' },
        { id: 'funds', label: '分时资金' },
        { id: 'capital-strength', label: 'DDX强势' },
        { id: 'strong-stocks', label: '强势生命周期' },
        { id: 'stats', label: '交易统计' },
        { id: 'equity-performance', label: '多周期表现' }
      ]
    },
    {
      id: 'holding',
      label: '筹码股东',
      items: [
        { id: 'holders', label: '股东与机构' },
        { id: 'ownership', label: '股权变动' },
        { id: 'unlocks', label: '限售解禁' },
        { id: 'private-placements', label: '定向增发' },
        { id: 'repurchases', label: '股份回购' },
        { id: 'tender-offers', label: '要约收购' },
        { id: 'corporate-orders', label: '订单合同' },
        { id: 'corporate', label: '财务股本' },
        { id: 'professional', label: '专业序列' },
        { id: 'convertible-bonds', label: '关联转债' },
        { id: 'employees', label: '员工高管' }
      ]
    },
    {
      id: 'institution',
      label: '机构动向',
      items: [
        { id: 'lhb', label: '龙虎榜' },
        { id: 'institution-lhb', label: '机构龙虎' },
        { id: 'institution-holdings', label: '机构持仓' },
        { id: 'shareholder-signals', label: '牛散机构信号' },
        { id: 'active-funds', label: '主动基金' },
        { id: 'etf-flows', label: 'ETF 资金' },
        { id: 'exchange-funds', label: '场内基金' },
        { id: 'foreign-alerts', label: '外资预警' },
        { id: 'block-trades', label: '大宗交易' }
      ]
    },
    {
      id: 'research',
      label: '研究预期',
      items: [
        { id: 'consensus', label: '预期研报' },
        { id: 'forecasts', label: '业绩预告' },
        { id: 'disclosures', label: '披露日历' },
        { id: 'panorama', label: '数据全景' },
        { id: 'research', label: '机构调研' },
        { id: 'roadshows', label: '公司路演' },
        { id: 'ratings', label: '行业评级' },
        { id: 'intelligence', label: '关注与事件' },
        { id: 'commodity-links', label: '商股联动' },
        { id: 'announcement-signals', label: '公告信号' },
        { id: 'special-situations', label: '并购与转板' },
        { id: 'special-attention', label: '特别关注' },
        { id: 'specialized-metrics', label: '金融专项' },
        { id: 'company-changes', label: '公司变更' },
        { id: 'financial-screen', label: '财务筛选' },
        { id: 'financial-insights', label: '财务线索' },
        { id: 'gdr', label: 'GDR映射' },
        { id: 'recent-watch', label: '近期关注' },
        { id: 'patent-statistics', label: '专利统计' },
        { id: 'benchmark-analysis', label: '基准分析' },
        { id: 'hot-history', label: '历史热点' },
        { id: 'limit-review', label: '涨跌停复盘' },
        { id: 'curated-data', label: '客户端精选' },
        { id: 'calendar', label: '公司日历' }
      ]
    },
    {
      id: 'source',
      label: '归属与源',
      items: [
        { id: 'blocks', label: '行业与板块' },
        { id: 'leverage', label: '两融与陆股通' },
        { id: 'raw', label: '数据关系' }
      ]
    }
  ];
</script>

<script lang="ts">
  /**
   * 个股工作台。
   *
   * 重构前个股详情是一个 slide-over 抽屉，19 个页签平铺成一行横向滚动，
   * 且没有路由——刷新丢状态、后退键直接退出应用。现在它是一级页面：
   * 位置进 URL，页签按语义分两级，盘口常驻右栏不再占页签位。
   */
  import { onDestroy, onMount } from 'svelte';
  import { queryString } from '../../api';
  import { Resource } from '../../lib/resource.svelte';
  import { compact, delta, fixed, price, tone, volume } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import type { DepthDocument, DepthRecord } from '../../types';

  import OrderBook from './panels/OrderBook.svelte';
  import QuotePanel from './panels/QuotePanel.svelte';
  import SpeedPanel from './panels/SpeedPanel.svelte';
  import AuctionPanel from './panels/AuctionPanel.svelte';
  import TradesPanel from './panels/TradesPanel.svelte';
  import FundsPanel from './panels/FundsPanel.svelte';
  import CapitalStrengthPanel from './panels/CapitalStrengthPanel.svelte';
  import StrongStocksPanel from './panels/StrongStocksPanel.svelte';
  import SpecializedMetricsPanel from './panels/SpecializedMetricsPanel.svelte';
  import CompanyChangesPanel from './panels/CompanyChangesPanel.svelte';
  import FinancialScreenPanel from './panels/FinancialScreenPanel.svelte';
  import FinancialInsightsPanel from './panels/FinancialInsightsPanel.svelte';
  import GdrPanel from './panels/GdrPanel.svelte';
  import EquityPerformancePanel from './panels/EquityPerformancePanel.svelte';
  import ShareholderSignalsPanel from './panels/ShareholderSignalsPanel.svelte';
  import RecentWatchPanel from './panels/RecentWatchPanel.svelte';
  import PatentStatisticsPanel from './panels/PatentStatisticsPanel.svelte';
  import BenchmarkAnalysisPanel from './panels/BenchmarkAnalysisPanel.svelte';
  import HotHistoryPanel from './panels/HotHistoryPanel.svelte';
  import LimitReviewPanel from './panels/LimitReviewPanel.svelte';
  import StatsPanel from './panels/StatsPanel.svelte';
  import HoldersPanel from './panels/HoldersPanel.svelte';
  import OwnershipPanel from './panels/OwnershipPanel.svelte';
  import UnlocksPanel from './panels/UnlocksPanel.svelte';
  import PrivatePlacementsPanel from './panels/PrivatePlacementsPanel.svelte';
  import RepurchasesPanel from './panels/RepurchasesPanel.svelte';
  import TenderOffersPanel from './panels/TenderOffersPanel.svelte';
  import CorporateOrdersPanel from './panels/CorporateOrdersPanel.svelte';
  import CorporatePanel from './panels/CorporatePanel.svelte';
  import ProfessionalPanel from './panels/ProfessionalPanel.svelte';
  import ConvertibleBondsPanel from './panels/ConvertibleBondsPanel.svelte';
  import EmployeesPanel from './panels/EmployeesPanel.svelte';
  import ActiveLhbPanel from './panels/ActiveLhbPanel.svelte';
  import LhbPanel from './panels/LhbPanel.svelte';
  import InstitutionLhbPanel from './panels/InstitutionLhbPanel.svelte';
  import InstitutionHoldingsPanel from './panels/InstitutionHoldingsPanel.svelte';
  import ActiveFundsPanel from './panels/ActiveFundsPanel.svelte';
  import EtfFlowsPanel from './panels/EtfFlowsPanel.svelte';
  import ExchangeFundsPanel from './panels/ExchangeFundsPanel.svelte';
  import ForeignAlertsPanel from './panels/ForeignAlertsPanel.svelte';
  import BlockTradesPanel from './panels/BlockTradesPanel.svelte';
  import ConsensusPanel from './panels/ConsensusPanel.svelte';
  import ForecastsPanel from './panels/ForecastsPanel.svelte';
  import DisclosurePanel from './panels/DisclosurePanel.svelte';
  import PanoramaPanel from './panels/PanoramaPanel.svelte';
  import ResearchPanel from './panels/ResearchPanel.svelte';
  import RoadshowsPanel from './panels/RoadshowsPanel.svelte';
  import RatingsPanel from './panels/RatingsPanel.svelte';
  import BlocksPanel from './panels/BlocksPanel.svelte';
  import StateOwnedReformPanel from './panels/StateOwnedReformPanel.svelte';
  import RawPanel from './panels/RawPanel.svelte';
  import LeveragePanel from './panels/LeveragePanel.svelte';
  import IntelligencePanel from './panels/IntelligencePanel.svelte';
  import CommodityLinksPanel from './panels/CommodityLinksPanel.svelte';
  import AnnouncementSignalsPanel from './panels/AnnouncementSignalsPanel.svelte';
  import SpecialSituationsPanel from './panels/SpecialSituationsPanel.svelte';
  import SpecialAttentionPanel from './panels/SpecialAttentionPanel.svelte';
  import CuratedDataPanel from './panels/CuratedDataPanel.svelte';
  import CalendarPanel from './panels/CalendarPanel.svelte';

  const market = $derived(router.segment(1) || app.stock.market);
  const code = $derived(router.segment(2) || app.stock.code);
  const tab = $derived(router.segment(3) || 'quote');

  const group = $derived(
    TAB_GROUPS.find((item) => item.items.some((entry) => entry.id === tab)) ?? TAB_GROUPS[0]
  );

  const depth = new Resource<DepthDocument>();
  const quote = $derived(depth.data?.records[0] ?? null);
  const name = $derived(quote?.name || app.stock.name || code);

  interface StreamEvent {
    type: 'snapshot' | 'change' | 'heartbeat' | 'missing' | 'reconnect';
    generated_at: string;
    record: DepthRecord | null;
    source?: { endpoint?: string; server_name?: string };
  }

  let timer: ReturnType<typeof setInterval> | undefined;
  let stream: EventSource | undefined;
  let streamHealthy = false;

  function loadDepth(silent = false) {
    if (!market || !code) return;
    void depth.load(`/api/v1/market/depth?${queryString({ market, code })}`, { silent });
  }

  function consumeStream(message: MessageEvent<string>) {
    try {
      const event = JSON.parse(message.data) as StreamEvent;
      if (!event.record) return;
      depth.data = {
        generated_at: event.generated_at,
        endpoint: event.source?.endpoint ?? depth.data?.endpoint ?? '',
        server_name: event.source?.server_name ?? depth.data?.server_name ?? '',
        received: 1,
        records: [event.record]
      };
      depth.busy = false;
      depth.error = '';
      depth.loaded = true;
      streamHealthy = true;
    } catch {
      // Ignore a malformed individual event; EventSource remains connected and
      // the bounded fallback request below can still repair the view.
    }
  }

  // 标的变化时先取一次盘口，再切换到共享 SSE 增量流。
  $effect(() => {
    const selectedMarket = market;
    const selectedCode = code;
    depth.reset();
    loadDepth();
    streamHealthy = false;
    stream?.close();
    if (typeof EventSource === 'undefined' || !selectedMarket || !selectedCode) return;
    const source = new EventSource(
      `/api/v1/market/stream?${queryString({ market: selectedMarket, code: selectedCode })}`
    );
    stream = source;
    source.onopen = () => (streamHealthy = true);
    source.onerror = () => {
      streamHealthy = false;
      if (!document.hidden) loadDepth(true);
    };
    for (const type of ['snapshot', 'change', 'heartbeat']) {
      source.addEventListener(type, consumeStream as EventListener);
    }
    return () => {
      source.close();
      if (stream === source) stream = undefined;
    };
  });

  // 名称回填全局，供顶栏上下文与最近访问列表使用
  $effect(() => {
    const record = depth.data?.records[0];
    if (!record) return;
    if (app.stock.market !== market || app.stock.code !== code || app.stock.name !== record.name) {
      app.setStock({ market, code, name: record.name });
    }
  });

  function selectTab(next: string) {
    // 切页签不值得占一条历史记录，用 replace
    router.replace(stockPath(market, code, next));
  }

  function selectGroup(nextGroup: string) {
    const target = TAB_GROUPS.find((item) => item.id === nextGroup);
    if (target) selectTab(target.items[0].id);
  }

  onMount(() => {
    // EventSource 不可用或正在重连时保留低频兜底；正常流式状态不再轮询。
    timer = setInterval(() => {
      if (!document.hidden && !streamHealthy) loadDepth(true);
    }, 15000);
  });

  onDestroy(() => {
    clearInterval(timer);
    stream?.close();
  });
</script>

<header class="head">
  <div class="ident">
    <h1>{name}</h1>
    <span class="ticker num">{market.toUpperCase()}{code}</span>
    {#if quote && quote.change_pct !== null}
      <Badge tone={tone(quote.change_pct)} solid>{delta(quote.change_pct)}</Badge>
    {/if}
  </div>

  {#if quote}
    <div class="price {tone(quote.change_pct)}">
      <span class="last num">{price(quote.last_price)}</span>
      <span class="diff num">
        {quote.last_price - quote.pre_close_price >= 0 ? '+' : ''}{fixed(
          quote.last_price - quote.pre_close_price
        )}
      </span>
    </div>

    <dl class="quick">
      <div>
        <dt>今开</dt>
        <dd class="num {tone(quote.open_price - quote.pre_close_price)}">{price(quote.open_price)}</dd>
      </div>
      <div>
        <dt>最高</dt>
        <dd class="num {tone(quote.high_price - quote.pre_close_price)}">{price(quote.high_price)}</dd>
      </div>
      <div>
        <dt>最低</dt>
        <dd class="num {tone(quote.low_price - quote.pre_close_price)}">{price(quote.low_price)}</dd>
      </div>
      <div><dt>昨收</dt><dd class="num">{price(quote.pre_close_price)}</dd></div>
      <div><dt>成交额</dt><dd class="num">{compact(quote.amount, '元')}</dd></div>
      <div><dt>成交量</dt><dd class="num">{volume(quote.total_hand)}</dd></div>
      <div>
        <dt>内/外盘</dt>
        <dd class="num">{compact(quote.inside_dish)} / {compact(quote.outer_disc)}</dd>
      </div>
    </dl>
  {/if}

  {#if app.recent.length > 1}
    <nav class="recent" aria-label="最近访问">
      {#each app.recent.slice(0, 6) as item (item.market + item.code)}
        <button
          type="button"
          class:on={item.market === market && item.code === code}
          onclick={() => router.go(stockPath(item.market, item.code))}
        >{item.name}</button>
      {/each}
    </nav>
  {/if}
</header>

<div class="tabs">
  <Segmented
    size="md"
    options={TAB_GROUPS.map((item) => ({ id: item.id, label: item.label }))}
    value={group.id}
    onChange={selectGroup}
    ariaLabel="个股资料分组"
  />
  <Segmented options={group.items} value={tab} onChange={selectTab} ariaLabel="资料页签" />
</div>

<div class="stage">
  <div class="content">
    {#key `${market}${code}${tab}`}
      {#if tab === 'auction'}
        <AuctionPanel {market} {code} {name} />
      {:else if tab === 'speed'}
        <SpeedPanel {market} {code} {name} />
      {:else if tab === 'trades'}
        <TradesPanel {market} {code} {name} />
      {:else if tab === 'funds'}
        <FundsPanel {market} {code} {name} />
      {:else if tab === 'capital-strength'}
        <CapitalStrengthPanel {market} {code} {name} />
      {:else if tab === 'strong-stocks'}
        <StrongStocksPanel {market} {code} {name} />
      {:else if tab === 'stats'}
        <StatsPanel {market} {code} {name} />
      {:else if tab === 'holders'}
        <HoldersPanel {market} {code} {name} />
      {:else if tab === 'ownership'}
        <OwnershipPanel {market} {code} {name} />
      {:else if tab === 'unlocks'}
        <UnlocksPanel {market} {code} {name} />
      {:else if tab === 'private-placements'}
        <PrivatePlacementsPanel {market} {code} {name} />
      {:else if tab === 'repurchases'}
        <RepurchasesPanel {market} {code} {name} />
      {:else if tab === 'tender-offers'}
        <TenderOffersPanel {market} {code} {name} />
      {:else if tab === 'corporate-orders'}
        <CorporateOrdersPanel {market} {code} {name} />
      {:else if tab === 'corporate'}
        <CorporatePanel {market} {code} {name} />
      {:else if tab === 'professional'}
        <ProfessionalPanel {market} {code} {name} />
      {:else if tab === 'convertible-bonds'}
        <ConvertibleBondsPanel {market} {code} {name} />
      {:else if tab === 'employees'}
        <EmployeesPanel {market} {code} {name} />
      {:else if tab === 'lhb'}
        <ActiveLhbPanel {market} {code} {name} />
        <LhbPanel {market} {code} {name} />
      {:else if tab === 'institution-lhb'}
        <InstitutionLhbPanel {market} {code} {name} />
      {:else if tab === 'institution-holdings'}
        <InstitutionHoldingsPanel {market} {code} {name} />
      {:else if tab === 'active-funds'}
        <ActiveFundsPanel {market} {code} {name} />
      {:else if tab === 'etf-flows'}
        <EtfFlowsPanel {market} {code} {name} />
      {:else if tab === 'exchange-funds'}
        <ExchangeFundsPanel {market} {code} {name} />
      {:else if tab === 'foreign-alerts'}
        <ForeignAlertsPanel {market} {code} {name} />
      {:else if tab === 'block-trades'}
        <BlockTradesPanel {market} {code} {name} />
      {:else if tab === 'consensus'}
        <ConsensusPanel {market} {code} {name} />
      {:else if tab === 'forecasts'}
        <ForecastsPanel {market} {code} {name} />
      {:else if tab === 'disclosures'}
        <DisclosurePanel {market} {code} {name} />
      {:else if tab === 'panorama'}
        <PanoramaPanel {market} {code} {name} />
      {:else if tab === 'research'}
        <ResearchPanel {market} {code} {name} />
      {:else if tab === 'roadshows'}
        <RoadshowsPanel {market} {code} {name} />
      {:else if tab === 'ratings'}
        <RatingsPanel {market} {code} {name} />
      {:else if tab === 'blocks'}
        <StateOwnedReformPanel {market} {code} {name} />
        <BlocksPanel {market} {code} {name} />
      {:else if tab === 'raw'}
        <RawPanel {market} {code} {name} />
      {:else if tab === 'leverage'}
        <LeveragePanel {market} {code} {name} />
      {:else if tab === 'intelligence'}
        <IntelligencePanel {market} {code} {name} />
      {:else if tab === 'commodity-links'}
        <CommodityLinksPanel {market} {code} {name} />
      {:else if tab === 'announcement-signals'}
        <AnnouncementSignalsPanel {market} {code} {name} />
      {:else if tab === 'special-situations'}
        <SpecialSituationsPanel {market} {code} {name} />
      {:else if tab === 'special-attention'}
        <SpecialAttentionPanel {market} {code} {name} />
      {:else if tab === 'specialized-metrics'}
        <SpecializedMetricsPanel {market} {code} {name} />
      {:else if tab === 'company-changes'}
        <CompanyChangesPanel {market} {code} {name} />
      {:else if tab === 'financial-screen'}
        <FinancialScreenPanel {market} {code} {name} />
      {:else if tab === 'financial-insights'}
        <FinancialInsightsPanel {market} {code} {name} />
      {:else if tab === 'gdr'}
        <GdrPanel {market} {code} {name} />
      {:else if tab === 'equity-performance'}
        <EquityPerformancePanel {market} {code} {name} />
      {:else if tab === 'shareholder-signals'}
        <ShareholderSignalsPanel {market} {code} {name} />
      {:else if tab === 'recent-watch'}
        <RecentWatchPanel {market} {code} {name} />
      {:else if tab === 'patent-statistics'}
        <PatentStatisticsPanel {market} {code} {name} />
      {:else if tab === 'benchmark-analysis'}
        <BenchmarkAnalysisPanel {market} {code} {name} />
      {:else if tab === 'hot-history'}
        <HotHistoryPanel {market} {code} {name} />
      {:else if tab === 'limit-review'}
        <LimitReviewPanel {market} {code} {name} />
      {:else if tab === 'curated-data'}
        <CuratedDataPanel {market} {code} {name} />
      {:else if tab === 'calendar'}
        <CalendarPanel {market} {code} {name} />
      {:else}
        <QuotePanel {market} {code} {name} live={quote} />
      {/if}
    {/key}
  </div>

  <!-- 盘口常驻右栏：研判任何一个页签时都需要它作上下文，
       做成页签反而会被切走 -->
  <aside class="rail">
    <OrderBook record={quote} busy={depth.busy} error={depth.error} onRetry={() => loadDepth()} />
  </aside>
</div>

<style>
  .head {
    display: flex;
    flex: none;
    flex-wrap: wrap;
    align-items: center;
    gap: var(--sp-3) var(--sp-5);
    padding: var(--sp-2) var(--sp-4);
    background: var(--bg-panel);
    border: 1px solid var(--line);
    border-radius: var(--radius-lg);
  }

  .ident {
    display: flex;
    align-items: baseline;
    gap: var(--sp-2);
  }

  h1 {
    font-size: var(--fs-lead);
    font-weight: 600;
    line-height: var(--lh-tight);
  }

  .ticker {
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  .price {
    display: flex;
    align-items: baseline;
    gap: var(--sp-2);
  }

  .last {
    font-size: var(--fs-hero);
    font-weight: 600;
    line-height: 1;
  }

  .diff {
    font-size: var(--fs-body);
  }

  .quick {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-1) var(--sp-5);
    margin: 0;
  }

  .quick div {
    display: flex;
    align-items: baseline;
    gap: var(--sp-2);
  }

  dt {
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  dd {
    margin: 0;
    font-size: var(--fs-micro);
    color: var(--fg);
  }

  .recent {
    display: flex;
    gap: var(--sp-1);
    margin-left: auto;
  }

  .recent button {
    height: 20px;
    padding: 0 var(--sp-2);
    font-size: var(--fs-micro);
    color: var(--fg-dim);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .recent button:hover {
    color: var(--fg);
    background: var(--bg-hover);
  }

  .recent button.on {
    color: var(--fg);
    background: var(--bg-active);
    border-color: var(--line-strong);
  }

  .tabs {
    display: flex;
    flex: none;
    flex-wrap: wrap;
    align-items: center;
    gap: var(--sp-3);
  }

  .stage {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 244px;
    gap: var(--sp-2);
    flex: 1;
    min-height: 0;
  }

  .content,
  .rail {
    display: flex;
    flex-direction: column;
    gap: var(--sp-2);
    min-width: 0;
    min-height: 0;
  }

  @media (max-width: 1100px) {
    .stage {
      grid-template-columns: minmax(0, 1fr);
      grid-template-rows: minmax(0, 1fr) auto;
    }
  }
</style>
