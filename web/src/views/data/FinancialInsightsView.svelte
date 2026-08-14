<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, money, percent } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { FinancialInsightRecord, MarketFinancialInsightsDocument } from '../../types';

  type View = MarketFinancialInsightsDocument['view'];
  type Sort = MarketFinancialInsightsDocument['sort'];
  const VIEWS = [
    { id: 'all', label: '全部' }, { id: 'buffett-quality', label: '巴菲特' },
    { id: 'high-bonus-potential', label: '高送转' }, { id: 'investment-property', label: '房地产' },
    { id: 'low-price-sales', label: '低PS' }, { id: 'dividend-shortfall', label: '分红不足' },
    { id: 'equity-investment', label: '股权投资' }, { id: 'cash-above-market-cap', label: '现金破市值' },
    { id: 'high-receivables', label: '高应收' }, { id: 'profit-warning', label: '利润预警' },
    { id: 'cash-flow-quality', label: '现金流' }, { id: 'earnings-reversal', label: '业绩反转' },
    { id: 'steady-growth', label: '稳健成长' }, { id: 'quality-growth', label: '质量增长' },
    { id: 'profit-breakout', label: '利润突破' }, { id: 'dividend-plan', label: '分红方案' }
  ];
  const SORTS = [
    { id: 'signal', label: '信号' }, { id: 'amount', label: '金额' },
    { id: 'ratio', label: '占比' }, { id: 'pe', label: 'PE' }, { id: 'roe', label: 'ROE' }
  ];
  let view = $state<View>('all');
  let sort = $state<Sort>('signal');
  let order = $state<'asc' | 'desc'>('desc');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketFinancialInsightsDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.record_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived.by<Stat[]>(() => doc ? [
    { label: '特色线索', value: count(doc.match_count) },
    { label: '覆盖股票', value: count(doc.summary.unique_securities) },
    { label: '利润预警 / 反转', value: count(doc.summary['profit-warning'] + doc.summary['earnings-reversal']) },
    { label: '新增四类', value: count(doc.summary['steady-growth'] + doc.summary['quality-growth'] + doc.summary['profit-breakout'] + doc.summary['dividend-plan']) }
  ] : []);
  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/financial-insights?${queryString({ view, sort, order, q: query.trim(), limit: 5000, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  function switchView(next: string) { view = next as View; load(); }
  function switchSort(next: string) { sort = next as Sort; load(); }
  function toggleOrder() { order = order === 'desc' ? 'asc' : 'desc'; load(); }
  function name(row: FinancialInsightRecord) { return row.security.name || row.security.code; }
  function primary(row: FinancialInsightRecord) {
    switch (row.kind) {
      case 'buffett-quality': return `ROE ${percent(row.roe_pct)}`;
      case 'high-bonus-potential': return `资本公积 ${fixed(row.capital_reserve_per_share_yuan)} 元/股`;
      case 'investment-property': return money(row.investment_property_yuan);
      case 'low-price-sales': return `PS ${fixed(row.price_to_sales)}`;
      case 'dividend-shortfall': return `三年分红 ${percent(row.dividend_to_average_profit_3y_pct)}`;
      case 'equity-investment': return money(row.investment_total_yuan);
      case 'cash-above-market-cap': return `超额 ${money(row.cash_excess_yuan)}`;
      case 'high-receivables': return percent(row.receivables_to_market_cap_pct);
      case 'profit-warning': return `实际利润 ${money(row.actual_profit_yuan)}`;
      case 'cash-flow-quality': return `每股经营现金 ${fixed(row.operating_cash_flow_per_share_yuan)} 元`;
      case 'earnings-reversal': return `利润增长 ${delta(row.profit_growth_pct)}`;
      case 'steady-growth': return `累计分红 ${money(row.cumulative_dividend_yuan)}`;
      case 'quality-growth': return `营收增长 ${delta(row.revenue_growth_t_pct)}`;
      case 'profit-breakout': return `利润 ${money(row.latest_or_forecast_profit_yuan)}`;
      default: return `每十股派 ${fixed(row.cash_dividend_per_10_shares_yuan)} 元`;
    }
  }
  function primarySub(row: FinancialInsightRecord) {
    switch (row.kind) {
      case 'buffett-quality': return `${count(row.roe_qualified_years)} 年达标 · PE ${fixed(row.pe)}`;
      case 'high-bonus-potential': return `未分配 ${fixed(row.undistributed_profit_per_share_yuan)} 元/股`;
      case 'investment-property': return `同比 ${delta(row.investment_property_yoy_pct)}`;
      case 'low-price-sales': return `营收预期 ${delta(row.expected_revenue_cagr_3y_pct)}`;
      case 'dividend-shortfall': return `研发占比 ${percent(row.rd_to_revenue_3y_pct)}`;
      case 'equity-investment': return `收益 ${money(row.investment_return_yuan)}`;
      case 'cash-above-market-cap': return `现金/市值 ${percent(row.cash_to_market_cap_pct)}`;
      case 'high-receivables': return `应收 ${money(row.receivables_yuan)}`;
      case 'profit-warning': return row.has_forecast ? `预告中值 ${money(row.forecast_profit_midpoint_yuan)} · ${date(row.forecast_disclosure_date)}` : '尚无下一报告期预告';
      case 'cash-flow-quality': return `现金/净利润 ${percent(row.operating_cash_to_net_profit_pct)} · 自由现金 ${money(row.free_cash_flow_yuan)}`;
      case 'earnings-reversal': return `预测净利润 ${money(row.forecast_net_profit_yuan)} · ${count(row.consensus_institutions)} 家机构`;
      case 'steady-growth': return `分红率 ${percent(row.dividend_payout_pct)} · Beta ${fixed(row.beta)}`;
      case 'quality-growth': return `毛利率 ${percent(row.gross_margin_t_pct)} · 四项条件${row.screen_criteria_complete ? '全部成立' : '未完整成立'}`;
      case 'profit-breakout': return `较基准 ${delta(row.profit_breakout_growth_pct)} · 同比 ${delta(row.profit_yoy_growth_pct)}`;
      default: return `${row.plan_stage || '方案待定'} · 除息 ${date(row.ex_dividend_date)} · 三月 ${delta(row.return_3m_pct)}`;
    }
  }
  function amount(row: FinancialInsightRecord) { return money(row.amount_value_yuan); }
  function ratio(row: FinancialInsightRecord) { return percent(row.ratio_value_pct); }
  function openStock() { if (selected) router.go(stockPath(selected.security.market, selected.security.code, 'financial-insights')); }
  const columns: Column<FinancialInsightRecord>[] = [
    { key: 'security', label: '股票', width: '160px', value: name, sub: (row) => row.security.security_id },
    { key: 'kind', label: '线索 / 报告期', width: '175px', value: (row) => row.kind_label, sub: (row) => date(row.report_period) },
    { key: 'primary', label: '核心指标', align: 'right', num: true, value: primary, sub: primarySub },
    { key: 'amount', label: '金额口径', align: 'right', num: true, value: amount },
    { key: 'ratio', label: '比例口径', align: 'right', num: true, value: ratio },
    { key: 'profit', label: '利润 / 增长', align: 'right', num: true, value: (row) => money(row.latest_or_forecast_profit_yuan ?? row.forecast_net_profit_yuan ?? row.actual_profit_yuan ?? row.parent_net_profit_yuan ?? row.latest_net_profit_yuan ?? row.net_profit_yuan), sub: (row) => delta(row.profit_breakout_growth_pct ?? row.revenue_growth_t_pct ?? row.profit_growth_pct ?? row.parent_net_profit_yoy_pct) }
  ];
  const detailStats = $derived.by<Stat[]>(() => selected ? [
    { label: '信号值', value: count(selected.signal_value) },
    { label: '金额口径', value: amount(selected) },
    { label: '比例口径', value: ratio(selected) },
    { label: '报告期', value: date(selected.report_period) }
  ] : []);
  onMount(() => load());
</script>

<PageHeader eyebrow="15 CFG-CALIBRATED SCREENS" title="特色财务线索" description="十五类客户端财务筛选，包含稳健成长、连续质量增长、利润突破和当前分红方案；金额、比例与增长公式均按 CFG 原始口径保留。" {stats}>
  {#snippet actions()}
    <Button onclick={toggleOrder}>{order === 'desc' ? '降序 ↓' : '升序 ↑'}</Button>
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="380px">
  {#snippet main()}
    <Panel title="财务线索表" subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.sources.length)} 个资源` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前条件下没有线索。" flush scroll>
      {#snippet toolbar()}
        <TextInput bind:value={query} icon="search" width="210px" label="检索股票" placeholder="代码或名称" onEnter={() => load()} />
        <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="财务线索类型" />
        <Segmented options={SORTS} value={sort} onChange={switchSort} ariaLabel="线索排序" />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.record_id} onRowClick={(row) => (selectedId = row.record_id)} isActive={(row) => row.record_id === selected?.record_id} stickyFirst numbered minWidth="1060px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.kind_label ?? 'FINANCIAL INSIGHT'} title={selected ? name(selected) : '线索详情'} subtitle={selected ? `${selected.security.security_id} · ${date(selected.report_period)}` : '选择左侧股票'} empty={!selected && !resource.busy}>
      {#if selected}
        <button type="button" onclick={openStock}>在个股工作台打开</button>
        <StatGrid stats={detailStats} inline />
        <p>{primary(selected)}；{primarySub(selected)}。所有换算字段均保留源值，可由 API 的 raw 模式审计。</p>
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  button { min-width: 72px; height: 28px; padding: 0 var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  p { margin-top: var(--sp-3); font-size: var(--fs-xs); line-height: 1.7; color: var(--fg-mute); }
</style>
