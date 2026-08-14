<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, money, percent, price, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { ExchangeFundRecord, MarketExchangeFundsDocument } from '../../types';

  type View = MarketExchangeFundsDocument['view'];
  const VIEWS = [
    { id: 'all', label: '全部' },
    { id: 'etf-performance', label: 'ETF表现' },
    { id: 'etf-share-ranking', label: 'ETF份额榜' },
    { id: 'etf-scale-flow', label: 'ETF规模' },
    { id: 'commodity-etf', label: '商品ETF' },
    { id: 'lof', label: 'LOF' },
    { id: 'closed-fund', label: '封闭基金' },
    { id: 'cash-arbitrage', label: '货币ETF套利' },
    { id: 'cash-yield', label: '货基收益' },
    { id: 'cash-management-calendar', label: '现金管理' },
    { id: 'reits-issued', label: '已发行REITs' },
    { id: 'reits-pipeline', label: '待发行REITs' }
  ];

  let view = $state<View>('etf-performance');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketExchangeFundsDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);

  const stats = $derived.by<Stat[]>(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return [
      { label: 'ETF表现', value: count(summary.etf_performance) },
      { label: 'ETF份额榜', value: count(summary.etf_share_ranking), note: `${count(summary.etf_share_ranking_nonzero_net_inflow)} 条非零净流入` },
      { label: 'ETF规模', value: count(summary.etf_scale_flow) },
      { label: 'LOF', value: count(summary.lof) },
      { label: '封闭基金', value: count(summary.closed_fund) },
      { label: '货币ETF套利', value: count(summary.cash_arbitrage) },
      { label: '货基收益', value: count(summary.cash_yield) },
      { label: '已发行REITs', value: count(summary.reits_issued) },
      { label: '覆盖证券', value: count(summary.unique_securities) },
      { label: '明确空表', value: count(summary.configured_empty_sources), note: '待发行REITs' }
    ];
  });

  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/exchange-funds?${queryString({
      view,
      q: query.trim(),
      include_quotes: view === 'cash-arbitrage' || view === 'etf-share-ranking' ? 1 : 0,
      limit: 5000,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function switchView(next: string) {
    view = next as View;
    load();
  }

  function name(row: ExchangeFundRecord): string {
    return row.security.name || row.security.code;
  }

  function gotoWorkbench() {
    if (selected) router.go(stockPath(
      selected.security.market,
      selected.security.code,
      'exchange-funds'
    ));
  }

  function kindTone(row: ExchangeFundRecord): 'focus' | 'warn' | 'neutral' {
    if (row.kind === 'cash-arbitrage') return 'warn';
    if (row.kind === 'reit-issued' || row.kind === 'reit-pipeline') return 'focus';
    return 'neutral';
  }

  function primaryMetric(row: ExchangeFundRecord): string {
    if (row.kind === 'etf-performance') return delta(row.change_5d_pct);
    if (row.kind === 'etf-share-ranking') return money(row.net_inflow_yuan);
    if (row.kind === 'etf-scale-flow' || row.kind === 'commodity-etf') return money(row.latest_scale_yuan);
    if (row.kind === 'lof') return fixed(row.nav_per_unit, 4);
    if (row.kind === 'closed-fund') return percent(row.discount_pct);
    if (row.kind === 'cash-management-calendar') return date(row.capital_available_date);
    if (row.kind === 'cash-arbitrage') return delta(row.buy_redeem_annualized_pct, 3);
    if (row.kind === 'cash-yield') return percent(row.seven_day_annualized_pct, 3);
    return price(row.subscription_price);
  }

  function secondaryMetric(row: ExchangeFundRecord): string {
    if (row.kind === 'etf-performance') return money(row.turnover_5d_yuan);
    if (row.kind === 'etf-share-ranking') return compact(row.latest_shares, '份');
    if (row.kind === 'cash-arbitrage') return delta(row.subscribe_sell_annualized_pct, 3);
    if (row.kind === 'cash-yield') return fixed(row.per_10k_yield, 4);
    if (row.kind === 'etf-scale-flow' || row.kind === 'commodity-etf') return compact(row.latest_shares, '份');
    if (row.kind === 'lof') return percent(row.equity_ratio_pct);
    if (row.kind === 'closed-fund') return date(row.maturity_date);
    if (row.kind === 'cash-management-calendar') return `${count(row.holding_days)} 天`;
    return compact(row.offering_total_units, '份');
  }

  const allColumns: Column<ExchangeFundRecord>[] = [
    { key: 'kind', label: '类型', width: '118px', slot: true, sortValue: (row) => row.kind },
    { key: 'security', label: '证券 / 项目', width: '158px', value: name, sub: (row) => row.security.security_id },
    { key: 'date', label: '快照 / 询价日', width: '90px', num: true, value: (row) => date(row.snapshot_date), sortValue: (row) => row.snapshot_date },
    { key: 'primary', label: '核心指标', align: 'right', num: true, value: primaryMetric },
    { key: 'secondary', label: '辅助指标', align: 'right', num: true, value: secondaryMetric },
    { key: 'source', label: '原始资源', width: '210px', value: (row) => row.source_resource }
  ];

  const etfColumns: Column<ExchangeFundRecord>[] = [
    { key: 'security', label: 'ETF', width: '158px', value: name, sub: (row) => row.security.security_id },
    { key: 'close', label: '快照收盘', align: 'right', num: true, value: (row) => fixed(row.close_price, 3), sub: (row) => date(row.snapshot_date) },
    { key: '5d', label: '5日', align: 'right', num: true, value: (row) => delta(row.change_5d_pct), tone: (row) => tone(row.change_5d_pct), sortValue: (row) => row.change_5d_pct ?? -Infinity },
    { key: '20d', label: '20日', align: 'right', num: true, value: (row) => delta(row.change_20d_pct), tone: (row) => tone(row.change_20d_pct), sortValue: (row) => row.change_20d_pct ?? -Infinity },
    { key: '60d', label: '60日', align: 'right', num: true, value: (row) => delta(row.change_60d_pct), tone: (row) => tone(row.change_60d_pct), sortValue: (row) => row.change_60d_pct ?? -Infinity },
    { key: 'month', label: '本月', align: 'right', num: true, value: (row) => delta(row.change_month_pct), tone: (row) => tone(row.change_month_pct) },
    { key: 'ytd', label: '年初至今', align: 'right', num: true, value: (row) => delta(row.change_ytd_pct), tone: (row) => tone(row.change_ytd_pct) },
    { key: 'day-amount', label: '当日成交额', align: 'right', num: true, value: (row) => money(row.turnover_yuan), sortValue: (row) => row.turnover_yuan ?? 0 },
    { key: '5d-amount', label: '5日成交额', align: 'right', num: true, value: (row) => money(row.turnover_5d_yuan), sortValue: (row) => row.turnover_5d_yuan ?? 0 }
  ];

  const arbitrageColumns: Column<ExchangeFundRecord>[] = [
    { key: 'security', label: '货币ETF', width: '158px', value: name, sub: (row) => row.security.security_id },
    { key: 'current', label: '现价 / 理论净值', align: 'right', num: true, value: (row) => `${fixed(row.current_quote?.last_price, 3)} / ${fixed(row.theoretical_nav, 4)}`, sub: (row) => delta(row.premium_pct, 3), tone: (row) => tone(row.premium_pct) },
    { key: 'buy', label: '买入赎回年化', align: 'right', num: true, value: (row) => delta(row.buy_redeem_annualized_pct, 3), tone: (row) => tone(row.buy_redeem_annualized_pct), sortValue: (row) => row.buy_redeem_annualized_pct ?? -Infinity },
    { key: 'sell', label: '申购卖出年化', align: 'right', num: true, value: (row) => delta(row.subscribe_sell_annualized_pct, 3), tone: (row) => tone(row.subscribe_sell_annualized_pct), sortValue: (row) => row.subscribe_sell_annualized_pct ?? -Infinity },
    { key: '7d', label: '七日年化', align: 'right', num: true, value: (row) => percent(row.seven_day_annualized_pct, 3) },
    { key: 'days', label: '占款天数', align: 'right', num: true, value: (row) => `${count(row.capital_tieup_days)} 天` },
    { key: 'settle', label: '到账日', width: '88px', num: true, value: (row) => date(row.settlement_date) }
  ];

  const shareRankingColumns: Column<ExchangeFundRecord>[] = [
    { key: 'security', label: 'ETF', width: '158px', value: name, sub: (row) => row.security.security_id },
    { key: 'reference', label: '跟踪指数', width: '118px', value: (row) => row.reference_instrument?.name || row.reference_instrument?.code || '—', sub: (row) => row.reference_instrument?.security_id ?? '' },
    { key: 'flow', label: '净流入', align: 'right', num: true, value: (row) => money(row.net_inflow_yuan), tone: (row) => tone(row.net_inflow_yuan), sortValue: (row) => row.net_inflow_yuan ?? 0 },
    { key: 'shares', label: '最新份额 / 日变化', align: 'right', num: true, value: (row) => compact(row.latest_shares, '份'), sub: (row) => delta(row.share_change), sortValue: (row) => row.latest_shares ?? 0 },
    { key: 'changes', label: '周 / 月份额变化', align: 'right', num: true, value: (row) => delta(row.weekly_share_change), sub: (row) => delta(row.monthly_share_change) },
    { key: 'scale', label: '实时规模 / 日变化', align: 'right', num: true, value: (row) => money(row.latest_scale_yuan), sub: (row) => money(row.daily_scale_change_yuan), sortValue: (row) => row.latest_scale_yuan ?? 0 },
    { key: 'premium', label: '现价 / IOPV', align: 'right', num: true, value: (row) => `${fixed(row.current_quote?.last_price, 3)} / ${fixed(row.iopv, 4)}`, sub: (row) => delta(row.premium_pct, 3), tone: (row) => tone(row.premium_pct) },
    { key: 'unit', label: '最小申赎单位', align: 'right', num: true, value: (row) => compact(row.subscription_unit_shares, '份'), sub: (row) => date(row.snapshot_date) }
  ];

  const yieldColumns: Column<ExchangeFundRecord>[] = [
    { key: 'security', label: '场内货基', width: '158px', value: name, sub: (row) => `${row.security.security_id} · 源市场${row.source_market_id}` },
    { key: 'type', label: '计息类型', width: '110px', value: (row) => text(row.interest_calculation_type) },
    { key: 'per10k', label: '万份收益', align: 'right', num: true, value: (row) => fixed(row.per_10k_yield, 4), sortValue: (row) => row.per_10k_yield ?? 0 },
    { key: '7d', label: '七日年化', align: 'right', num: true, value: (row) => percent(row.seven_day_annualized_pct, 3), sortValue: (row) => row.seven_day_annualized_pct ?? 0 },
    { key: 'month', label: '月均七日年化', align: 'right', num: true, value: (row) => percent(row.monthly_average_seven_day_pct, 3) },
    { key: 'year', label: '年均七日年化', align: 'right', num: true, value: (row) => percent(row.yearly_average_seven_day_pct, 3) },
    { key: 'shares', label: '最新份额', align: 'right', num: true, value: (row) => `${fixed(row.latest_shares_100m, 2)} 亿份` }
  ];

  const scaleColumns: Column<ExchangeFundRecord>[] = [
    { key: 'security', label: 'ETF', width: '158px', value: name, sub: (row) => row.security.security_id },
    { key: 'nav', label: '单位净值', align: 'right', num: true, value: (row) => fixed(row.nav_per_unit, 4), sub: (row) => date(row.snapshot_date) },
    { key: 'shares', label: '最新份额 / 变化', align: 'right', num: true, value: (row) => compact(row.latest_shares, '份'), sub: (row) => delta(row.share_change) },
    { key: 'week', label: '周 / 月份额变化', align: 'right', num: true, value: (row) => delta(row.weekly_share_change), sub: (row) => delta(row.monthly_share_change) },
    { key: 'scale', label: '最新 / 上期规模', align: 'right', num: true, value: (row) => money(row.latest_scale_yuan), sub: (row) => money(row.prior_scale_yuan) },
    { key: 'reference', label: '跟踪标的', value: (row) => row.reference_instrument?.name || row.reference_instrument?.code || '—', sub: (row) => delta(row.reference_change_pct) },
    { key: 'fees', label: '申购 / 赎回上限', align: 'right', num: true, value: (row) => percent(row.max_subscription_fee_pct), sub: (row) => percent(row.max_redemption_fee_pct) }
  ];

  const lofColumns: Column<ExchangeFundRecord>[] = [
    { key: 'security', label: 'LOF', width: '158px', value: name, sub: (row) => row.security.security_id },
    { key: 'nav', label: '单位净值', align: 'right', num: true, value: (row) => fixed(row.nav_per_unit, 4), sub: (row) => date(row.snapshot_date) },
    { key: 'shares', label: '当前 / 新增份额', align: 'right', num: true, value: (row) => compact(row.current_shares, '份'), sub: (row) => compact(row.new_shares, '份') },
    { key: 'allocation', label: '股票 / 债券占比', align: 'right', num: true, value: (row) => percent(row.equity_ratio_pct), sub: (row) => percent(row.bond_ratio_pct) },
    { key: 'status', label: '申赎状态', value: (row) => text(row.subscription_status) },
    { key: 'fees', label: '申购 / 赎回上限', align: 'right', num: true, value: (row) => percent(row.max_subscription_fee_pct), sub: (row) => percent(row.max_redemption_fee_pct) }
  ];

  const closedColumns: Column<ExchangeFundRecord>[] = [
    { key: 'security', label: '封闭基金', width: '158px', value: name, sub: (row) => row.security.security_id },
    { key: 'nav', label: '单位净值', align: 'right', num: true, value: (row) => fixed(row.nav_per_unit, 4), sub: (row) => date(row.snapshot_date) },
    { key: 'discount', label: '折溢价率', align: 'right', num: true, value: (row) => percent(row.discount_pct), tone: (row) => tone(row.discount_pct) },
    { key: 'maturity', label: '到期日 / 剩余年限', width: '140px', value: (row) => date(row.maturity_date), sub: (row) => `${fixed(row.remaining_years, 2)} 年` },
    { key: 'allocation', label: '股票 / 债券占比', align: 'right', num: true, value: (row) => percent(row.equity_ratio_pct), sub: (row) => percent(row.bond_ratio_pct) }
  ];

  const cashManagementColumns: Column<ExchangeFundRecord>[] = [
    { key: 'security', label: '现金管理品种', width: '168px', value: name, sub: (row) => row.security.security_id },
    { key: 'holding', label: '持有 / 自然日', align: 'right', num: true, value: (row) => `${count(row.holding_days)} 天`, sub: (row) => `${count(row.calendar_days)} 天` },
    { key: 'available', label: '资金可用', width: '110px', num: true, value: (row) => date(row.capital_available_date), sub: (row) => `${count(row.available_days)} 天` },
    { key: 'settle', label: '市场结算', width: '100px', num: true, value: (row) => date(row.market_settlement_date) },
    { key: 'withdraw', label: '资金可取', width: '110px', num: true, value: (row) => date(row.capital_withdrawable_date), sub: (row) => `${count(row.withdrawable_days)} 天` },
    { key: 'fee', label: '费率', align: 'right', num: true, value: (row) => percent(row.fee_pct) }
  ];

  const reitColumns: Column<ExchangeFundRecord>[] = [
    { key: 'security', label: 'REITs项目', width: '180px', value: name, sub: (row) => row.security.security_id },
    { key: 'status', label: '状态', width: '86px', value: (row) => text(row.status) },
    { key: 'inquiry', label: '询价日 / 区间', width: '150px', value: (row) => date(row.dates?.inquiry), sub: (row) => text(row.inquiry_price_range) },
    { key: 'price', label: '认购价', align: 'right', num: true, value: (row) => fixed(row.subscription_price, 3), sub: (row) => text(row.term) },
    { key: 'total', label: '发售总规模', align: 'right', num: true, value: (row) => compact(row.offering_total_units, '份'), sortValue: (row) => row.offering_total_units ?? 0 },
    { key: 'strategic', label: '战略配售', align: 'right', num: true, value: (row) => compact(row.strategic_placement_units, '份') },
    { key: 'profit', label: '项目净利润', align: 'right', num: true, value: (row) => money(row.project_net_profit_yuan), tone: (row) => tone(row.project_net_profit_yuan) }
  ];

  const columns = $derived.by<Column<ExchangeFundRecord>[]>(() => {
    if (view === 'etf-performance') return etfColumns;
    if (view === 'etf-share-ranking') return shareRankingColumns;
    if (view === 'etf-scale-flow' || view === 'commodity-etf') return scaleColumns;
    if (view === 'lof') return lofColumns;
    if (view === 'closed-fund') return closedColumns;
    if (view === 'cash-arbitrage') return arbitrageColumns;
    if (view === 'cash-yield') return yieldColumns;
    if (view === 'cash-management-calendar') return cashManagementColumns;
    if (view === 'reits-issued' || view === 'reits-pipeline') return reitColumns;
    return allColumns;
  });

  const selectedStats = $derived.by<Stat[]>(() => {
    if (!selected) return [];
    if (selected.kind === 'etf-performance') return [
      { label: '5日', value: delta(selected.change_5d_pct), tone: tone(selected.change_5d_pct) },
      { label: '20日', value: delta(selected.change_20d_pct), tone: tone(selected.change_20d_pct) },
      { label: '60日', value: delta(selected.change_60d_pct), tone: tone(selected.change_60d_pct) },
      { label: '5日成交额', value: money(selected.turnover_5d_yuan) }
    ];
    if (selected.kind === 'etf-share-ranking') return [
      { label: '净流入', value: money(selected.net_inflow_yuan), tone: tone(selected.net_inflow_yuan) },
      { label: '实时规模', value: money(selected.latest_scale_yuan) },
      { label: '周规模变化', value: money(selected.weekly_scale_change_yuan), tone: tone(selected.weekly_scale_change_yuan) },
      { label: '月规模变化', value: money(selected.monthly_scale_change_yuan), tone: tone(selected.monthly_scale_change_yuan) },
      { label: 'IOPV折溢价', value: delta(selected.premium_pct, 3), tone: tone(selected.premium_pct) }
    ];
    if (selected.kind === 'cash-arbitrage') return [
      { label: '理论净值', value: fixed(selected.theoretical_nav, 4) },
      { label: '溢价率', value: delta(selected.premium_pct, 3), tone: tone(selected.premium_pct) },
      { label: '买入赎回', value: delta(selected.buy_redeem_annualized_pct, 3), tone: tone(selected.buy_redeem_annualized_pct) },
      { label: '申购卖出', value: delta(selected.subscribe_sell_annualized_pct, 3), tone: tone(selected.subscribe_sell_annualized_pct) }
    ];
    if (selected.kind === 'cash-yield') return [
      { label: '万份收益', value: fixed(selected.per_10k_yield, 4) },
      { label: '七日年化', value: percent(selected.seven_day_annualized_pct, 3) },
      { label: '月均七日', value: percent(selected.monthly_average_seven_day_pct, 3) },
      { label: '最新份额', value: `${fixed(selected.latest_shares_100m, 2)} 亿份` }
    ];
    if (selected.kind === 'etf-scale-flow' || selected.kind === 'commodity-etf') return [
      { label: '单位净值', value: fixed(selected.nav_per_unit, 4) },
      { label: '最新份额', value: compact(selected.latest_shares, '份') },
      { label: '最新规模', value: money(selected.latest_scale_yuan) },
      { label: '月变化', value: delta(selected.monthly_share_change), tone: tone(selected.monthly_share_change) }
    ];
    if (selected.kind === 'lof' || selected.kind === 'closed-fund') return [
      { label: '单位净值', value: fixed(selected.nav_per_unit, 4) },
      { label: '股票占比', value: percent(selected.equity_ratio_pct) },
      { label: '债券占比', value: percent(selected.bond_ratio_pct) },
      { label: '折溢价', value: percent(selected.discount_pct), tone: tone(selected.discount_pct) }
    ];
    if (selected.kind === 'cash-management-calendar') return [
      { label: '持有天数', value: `${count(selected.holding_days)} 天` },
      { label: '资金可用', value: date(selected.capital_available_date) },
      { label: '资金可取', value: date(selected.capital_withdrawable_date) },
      { label: '费率', value: percent(selected.fee_pct) }
    ];
    return [
      { label: '认购价', value: fixed(selected.subscription_price, 3) },
      { label: '发售规模', value: compact(selected.offering_total_units, '份') },
      { label: '战略配售', value: compact(selected.strategic_placement_units, '份') },
      { label: '项目净利润', value: money(selected.project_net_profit_yuan), tone: tone(selected.project_net_profit_yuan) }
    ];
  });

  onMount(() => load());
</script>

<PageHeader
  eyebrow="ETFHQ · TLF EYXETF · ETF / LOF / FBJJ / XJGL · REITS"
  title="场内基金全景、套利与REITs"
  description="ETF表现、份额排名与实时规模、跟踪标的、LOF配置与申赎、封闭基金折溢价、现金管理日历、货币ETF套利及REITs项目。"
  {stats}
>
  {#snippet actions()}
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="场内基金视图" />
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="410px">
  {#snippet main()}
    <Panel title="场内基金主表" subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.sources.length)} 个原始资源` : '本地JSN优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText={view === 'reits-pipeline' ? '待发行REITs上游当前是一行全空占位，已规范化为正常空关系。' : '当前筛选下没有记录。'} flush scroll>
      {#snippet toolbar()}
        <TextInput bind:value={query} icon="search" width="260px" label="检索场内基金" placeholder="代码、名称、状态或项目简介" onEnter={() => load()} />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedId = row.event_id)} isActive={(row) => row.event_id === selected?.event_id} stickyFirst numbered minWidth={view === 'etf-performance' || view === 'etf-share-ranking' ? '1120px' : '940px'}>
        {#snippet cell({ row })}<Badge tone={kindTone(row)}>{row.kind_label}</Badge>{/snippet}
      </DataTable>
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.kind_label ?? 'EXCHANGE FUND'} title={selected ? name(selected) : '场内基金详情'} subtitle={selected ? `${selected.security.security_id} · ${selected.source_resource}` : '点击左侧记录展开'} empty={!selected && !resource.busy} emptyText="选择一条记录查看客户端口径和原始字段。" scroll>
      {#if selected}
        <button class="jump" type="button" onclick={gotoWorkbench}>在证券工作台打开</button>
        <StatGrid stats={selectedStats} inline />
        {#if selected.kind === 'cash-arbitrage'}
          <h3>到账与计息</h3>
          <p>T日 {date(selected.trade_date)}，T+1日 {date(selected.next_trade_date)}，资金到账 {date(selected.settlement_date)}，占款 {count(selected.capital_tieup_days)} 天。</p>
          <p class="note">理论净值及两种年化严格复现 ETFJJ103 客户端公式；现价来自公开L1，行情失败不影响原始收益参数。</p>
        {:else if selected.kind === 'etf-performance'}
          <p class="note">5/20/60日、本月和年初至今均由快照收盘价与客户端隐藏参考收盘价复算，不用实时价替换历史基准。</p>
        {:else if selected.kind === 'etf-share-ranking'}
          <p>跟踪指数：{selected.reference_instrument?.name || selected.reference_instrument?.code || '未提供'}；最小申赎单位 {compact(selected.subscription_unit_shares, '份')}（原表 {fixed(selected.subscription_unit_10k_shares, 2)} 万份）。</p>
          <p>日 / 周 / 月规模变化：{money(selected.daily_scale_change_yuan)} · {money(selected.weekly_scale_change_yuan)} · {money(selected.monthly_scale_change_yuan)}</p>
          <p class="note">份额和历史规模来自 ETF份额榜；实时规模、规模变化与折溢价由公开L1现价、IOPV按客户端公式本地复算。净流入保留上游原值，不用份额变化伪造。</p>
        {:else if selected.kind === 'cash-yield'}
          <p>计息方式：{text(selected.interest_calculation_type)}</p>
          <p class="note">源市场号34按证券目录恢复交易市场；159前缀归深圳、519前缀归上海，同时保留原市场号。</p>
        {:else if selected.kind === 'etf-scale-flow' || selected.kind === 'commodity-etf'}
          <p>跟踪标的：{selected.reference_instrument?.name || selected.reference_instrument?.code || '未提供'}；最新申赎单位 {compact(selected.subscription_unit_shares, '份')}。</p>
          <p class="note">规模、份额及周/月变化均保留客户端原始单位；非A股跟踪标的保留扩展市场号。</p>
        {:else if selected.kind === 'lof'}
          <p>申赎状态：{text(selected.subscription_status)}；当前份额 {compact(selected.current_shares, '份')}，新增 {compact(selected.new_shares, '份')}。</p>
        {:else if selected.kind === 'closed-fund'}
          <p>到期日 {date(selected.maturity_date)}，剩余 {fixed(selected.remaining_years, 2)} 年，折溢价 {percent(selected.discount_pct)}。</p>
        {:else if selected.kind === 'cash-management-calendar'}
          <p>资金可用 {date(selected.capital_available_date)}，市场结算 {date(selected.market_settlement_date)}，资金可取 {date(selected.capital_withdrawable_date)}。</p>
        {:else}
          <h3>认购与配售</h3>
          <p>网上认购：{date(selected.dates?.online_subscription_start)} — {date(selected.dates?.online_subscription_end)}</p>
          <p>网下 {compact(selected.offline_offering_units, '份')} · 网上 {compact(selected.online_offering_units, '份')} · 原始权益人 {compact(selected.original_owner_subscription_units, '份')}</p>
          <h3>项目简介</h3><p class="copy">{text(selected.project_description)}</p>
        {/if}
        {#each doc?.quote_errors ?? [] as failure}<p class="warn">L1：{failure.message}</p>{/each}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .jump { width: 100%; height: 24px; margin-bottom: var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  h3 { margin: var(--sp-4) 0 var(--sp-2); font-size: var(--fs-micro); color: var(--fg-dim); }
  p { font-size: var(--fs-xs); line-height: 1.65; }
  .copy { white-space: pre-wrap; }
  .note { margin-top: var(--sp-3); color: var(--fg-mute); font-size: 10px; }
  .warn { margin-top: var(--sp-2); color: var(--warn); }
</style>
