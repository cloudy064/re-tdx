<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, delta, fixed, money, percent } from '../../lib/fmt';
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
  import type { FinancialScreenRecord, MarketFinancialScreenDocument } from '../../types';

  type View = MarketFinancialScreenDocument['view'];
  type Sort = MarketFinancialScreenDocument['sort'];
  type Dataset = MarketFinancialScreenDocument['dataset'];
  const DATASETS = [
    { id: 'snapshot', label: '五板快照' },
    { id: 'small-cap-growth', label: '小盘成长' }
  ];
  const VIEWS = [
    { id: 'all', label: '全部' }, { id: 'sh-main', label: '沪主板' },
    { id: 'sz-main', label: '深主板' }, { id: 'chinext', label: '创业板' },
    { id: 'star', label: '科创板' }, { id: 'beijing', label: '北证A股' }
  ];
  const SNAPSHOT_SORTS = [
    { id: 'market-cap', label: '市值' }, { id: 'pe', label: 'PE' },
    { id: 'pb', label: 'PB' }, { id: 'roe', label: 'ROE' },
    { id: 'revenue-growth', label: '营收增长' },
    { id: 'profit-growth', label: '利润增长' },
    { id: 'dividend-yield', label: '股息率' }
  ];
  const GROWTH_SORTS = [
    { id: 'profit-cagr', label: '利润复合增速' },
    { id: 'revenue-cagr', label: '营收复合增速' },
    { id: 'profit-growth', label: '本期利润同比' },
    { id: 'revenue-growth', label: '本期营收同比' }
  ];

  let dataset = $state<Dataset>('snapshot');
  let view = $state<View>('all');
  let sort = $state<Sort>('market-cap');
  let order = $state<'asc' | 'desc'>('desc');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketFinancialScreenDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const sorts = $derived(dataset === 'small-cap-growth' ? GROWTH_SORTS : SNAPSHOT_SORTS);
  const selected = $derived(rows.find((row) => row.record_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived.by<Stat[]>(() => !doc ? [] : dataset === 'small-cap-growth' ? [
    { label: '成长股票池', value: count(doc.match_count) },
    { label: '沪深主板', value: count(doc.summary['sh-main'] + doc.summary['sz-main']) },
    { label: '创业板', value: count(doc.summary.chinext) },
    { label: '科创板', value: count(doc.summary.star) },
    { label: '报告期', value: date(doc.summary.latest_report_period) }
  ] : [
    { label: '筛选股票', value: count(doc.match_count) },
    { label: '沪深主板', value: count(doc.summary['sh-main'] + doc.summary['sz-main']) },
    { label: '创业 / 科创', value: count(doc.summary.chinext + doc.summary.star) },
    { label: '北证A股', value: count(doc.summary.beijing) },
    { label: '最新报告期', value: date(doc.summary.latest_report_period) }
  ]);
  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/financial-screen?${queryString({
      dataset, view, sort, order, q: query.trim(), limit: 10000, include_raw: 0,
      refresh: refresh ? 1 : 0
    })}`);
  }
  function switchView(next: string) { view = next as View; load(); }
  function switchDataset(next: string) {
    dataset = next as Dataset;
    view = 'all';
    sort = dataset === 'small-cap-growth' ? 'profit-cagr' : 'market-cap';
    load();
  }
  function switchSort(next: string) { sort = next as Sort; load(); }
  function toggleOrder() { order = order === 'desc' ? 'asc' : 'desc'; load(); }
  function name(row: FinancialScreenRecord) { return row.security.name || row.security.code; }
  function openStock() { if (selected) router.go(stockPath(selected.security.market, selected.security.code, 'financial-screen')); }
  const columns: Column<FinancialScreenRecord>[] = [
    { key: 'security', label: '股票', width: '160px', value: name, sub: (row) => `${row.security.security_id} · ${row.board_label}` },
    { key: 'period', label: '报告期', width: '105px', num: true, value: (row) => date(row.report_period) },
    { key: 'cap', label: '市值', align: 'right', num: true, value: (row) => money(row.market_cap_yuan), sub: (row) => `股息 ${percent(row.dividend_yield_pct)}` },
    { key: 'valuation', label: 'PE / PB / PEG', align: 'right', num: true, value: (row) => `${fixed(row.pe_ttm)} / ${fixed(row.pb_mrq)}`, sub: (row) => `PEG ${fixed(row.peg)}` },
    { key: 'growth', label: '利润 / 营收同比', align: 'right', num: true, value: (row) => delta(row.net_profit_yoy_pct), sub: (row) => delta(row.revenue_yoy_pct) },
    { key: 'quality', label: 'ROE / 毛利率', align: 'right', num: true, value: (row) => percent(row.roe_pct), sub: (row) => percent(row.gross_margin_pct) },
    { key: 'holding', label: '机构持仓 / 负债率', align: 'right', num: true, value: (row) => percent(row.institution_float_holding_pct), sub: (row) => percent(row.debt_ratio_pct) }
  ];
  const growthColumns: Column<FinancialScreenRecord>[] = [
    { key: 'security', label: '股票', width: '160px', value: name, sub: (row) => `${row.security.security_id} · ${row.board_label}` },
    { key: 'period', label: '报告期 T', width: '105px', num: true, value: (row) => date(row.report_period) },
    { key: 'profit-current', label: '扣非净利 T', align: 'right', num: true, value: (row) => delta(row.adjusted_net_profit_yoy_pct), sortValue: (row) => row.adjusted_net_profit_yoy_pct ?? 0 },
    { key: 'profit-history', label: '扣非净利 T-1 / T-2 / T-3', width: '205px', align: 'right', num: true, value: (row) => `${delta(row.adjusted_net_profit_yoy_t_minus_1_pct)} / ${delta(row.adjusted_net_profit_yoy_t_minus_2_pct)}`, sub: (row) => `T-3 ${delta(row.adjusted_net_profit_yoy_t_minus_3_pct)}` },
    { key: 'profit-cagr', label: '利润三年复合', align: 'right', num: true, value: (row) => percent(row.adjusted_net_profit_cagr_3y_pct), sortValue: (row) => row.adjusted_net_profit_cagr_3y_pct ?? 0 },
    { key: 'revenue-current', label: '营收 T', align: 'right', num: true, value: (row) => delta(row.revenue_yoy_pct), sortValue: (row) => row.revenue_yoy_pct ?? 0 },
    { key: 'revenue-history', label: '营收 T-1 / T-2 / T-3', width: '205px', align: 'right', num: true, value: (row) => `${delta(row.revenue_yoy_t_minus_1_pct)} / ${delta(row.revenue_yoy_t_minus_2_pct)}`, sub: (row) => `T-3 ${delta(row.revenue_yoy_t_minus_3_pct)}` },
    { key: 'revenue-cagr', label: '营收三年复合', align: 'right', num: true, value: (row) => percent(row.revenue_cagr_3y_pct), sortValue: (row) => row.revenue_cagr_3y_pct ?? 0 }
  ];
  const activeColumns = $derived(dataset === 'small-cap-growth' ? growthColumns : columns);
  const detailStats = $derived.by<Stat[]>(() => !selected ? [] : dataset === 'small-cap-growth' ? [
    { label: '扣非净利 T', value: delta(selected.adjusted_net_profit_yoy_pct) },
    { label: '利润三年复合', value: percent(selected.adjusted_net_profit_cagr_3y_pct) },
    { label: '营收 T', value: delta(selected.revenue_yoy_pct) },
    { label: '营收三年复合', value: percent(selected.revenue_cagr_3y_pct) },
    { label: '扣非净利 T-1', value: delta(selected.adjusted_net_profit_yoy_t_minus_1_pct) },
    { label: '扣非净利 T-2', value: delta(selected.adjusted_net_profit_yoy_t_minus_2_pct) },
    { label: '营收 T-1', value: delta(selected.revenue_yoy_t_minus_1_pct) },
    { label: '营收 T-2', value: delta(selected.revenue_yoy_t_minus_2_pct) }
  ] : [
    { label: '市值', value: money(selected.market_cap_yuan) },
    { label: 'PE / PB', value: `${fixed(selected.pe_ttm)} / ${fixed(selected.pb_mrq)}` },
    { label: 'ROE', value: percent(selected.roe_pct) },
    { label: '毛利率', value: percent(selected.gross_margin_pct) },
    { label: '净利润同比', value: delta(selected.net_profit_yoy_pct) },
    { label: '营收同比', value: delta(selected.revenue_yoy_pct) },
    { label: '研发占营收', value: percent(selected.rd_to_revenue_pct) },
    { label: '股息率', value: percent(selected.dividend_yield_pct) }
  ]);
  onMount(() => load());
</script>

<PageHeader eyebrow={dataset === 'small-cap-growth' ? 'XPCZ · 3 LIVE SOURCES' : 'CWZB · 5 LIVE SOURCES'} title={dataset === 'small-cap-growth' ? '小盘成长股票池' : '五板财务筛选'} description={dataset === 'small-cap-growth' ? '通达信预筛的小盘成长股票池，保留扣非净利润与营收的四期同比和客户端给出的三年复合增速。' : '覆盖沪深主板、创业板、科创板与北证A股的估值、成长、质量、费用、机构持仓和股息横截面；全部字段按客户端 CFG 校准。'} {stats}>
  {#snippet actions()}
    <Segmented options={DATASETS} value={dataset} onChange={switchDataset} ariaLabel="财务数据集" />
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="板块视图" />
    <Button onclick={toggleOrder}>{order === 'desc' ? '降序 ↓' : '升序 ↑'}</Button>
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="390px">
  {#snippet main()}
    <Panel title={dataset === 'small-cap-growth' ? '小盘成长预筛池' : '财务横截面'} subtitle={doc ? `${count(doc.match_count)} 只 · ${count(doc.sources.length)} 个板块资源` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选下没有股票。" flush scroll>
      {#snippet toolbar()}
        <TextInput bind:value={query} icon="search" width="240px" label="检索股票" placeholder="代码或名称" onEnter={() => load()} />
        <Segmented options={sorts} value={sort} onChange={switchSort} ariaLabel="排序指标" />
      {/snippet}
      <DataTable columns={activeColumns} {rows} rowKey={(row) => row.record_id} onRowClick={(row) => (selectedId = row.record_id)} isActive={(row) => row.record_id === selected?.record_id} stickyFirst numbered minWidth="1160px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.board_label ?? 'FINANCIAL SCREEN'} title={selected ? name(selected) : '财务详情'} subtitle={selected ? `${selected.security.security_id} · ${date(selected.report_period)}` : '选择左侧股票'} empty={!selected && !resource.busy}>
      {#if selected}
        <button type="button" onclick={openStock}>在个股工作台打开</button>
        <StatGrid stats={detailStats} inline />
        {#if dataset === 'small-cap-growth'}
          <p>客户端小盘成长口径：T 为 {date(selected.report_period)}；T-1/T-2/T-3 保留源表年度同比，三年复合值直接采用 jlfh / ysfh，不由网页重算。</p>
        {:else}
          <p>合同负债 {money(selected.contract_liability_yuan)}，同比 {delta(selected.contract_liability_yoy_pct)}；机构占流通A股 {percent(selected.institution_float_holding_pct)}。</p>
        {/if}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  button { min-width: 72px; height: 28px; padding: 0 var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  p { margin-top: var(--sp-3); font-size: var(--fs-xs); line-height: 1.7; color: var(--fg-mute); }
</style>
