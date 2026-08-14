<script lang="ts">
  /** 通达信 QSZJ 页：五周期 DDX 前百资金强势榜与跨周期共振。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, percent, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    CapitalStrengthConfluenceRecord,
    CapitalStrengthPeriod,
    CapitalStrengthRecord,
    MarketCapitalStrengthDocument,
    ValuationSecurity
  } from '../../types';

  const VIEWS = [
    { id: 'ranking', label: '单周期前100' },
    { id: 'confluence', label: '跨周期共振' }
  ];
  const PERIODS = [
    { id: '5d', label: '5日资金强势' },
    { id: '10d', label: '10日资金强势' },
    { id: '20d', label: '20日资金强势' },
    { id: '30d', label: '30日资金强势' },
    { id: '3m', label: '近3月资金强势' }
  ];
  const MIN_PERIODS = [2, 3, 4, 5].map((value) => ({ id: String(value), label: `至少 ${value} 个周期` }));

  let view = $state<'ranking' | 'confluence'>('ranking');
  let period = $state<CapitalStrengthPeriod>('5d');
  let minPeriods = $state('2');
  let query = $state('');
  const resource = new Resource<MarketCapitalStrengthDocument>();
  const doc = $derived(resource.data);
  const rankingRows = $derived((view === 'ranking' ? doc?.records ?? [] : []) as CapitalStrengthRecord[]);
  const confluenceRows = $derived((view === 'confluence' ? doc?.records ?? [] : []) as CapitalStrengthConfluenceRecord[]);

  const stats = $derived.by<Stat[]>(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    if (view === 'confluence') return [
      { label: '五表原始行', value: count(summary.source_rows), note: '5 个周期 × 前100' },
      { label: '去重股票', value: count(summary.unique_securities) },
      { label: '至少双周期', value: count(summary.at_least_two_periods) },
      { label: '五周期共振', value: count(summary.all_five_periods), tone: Number(summary.all_five_periods) > 0 ? 'up' : '' },
      { label: '当前筛选', value: count(doc?.counts.matched), note: `至少 ${minPeriods} 个周期` },
      { label: '真实来源', value: count(doc?.counts.sources), note: doc?.availability === 'stale-cache' ? '陈旧缓存' : '在线' }
    ];
    return [
      { label: '榜单股票', value: count(summary.source_rows), note: PERIODS.find((item) => item.id === period)?.label },
      { label: '统计日期', value: date(summary.statistics_dates?.[0]) },
      { label: '名称解析', value: `${count(summary.names_resolved)} / ${count(summary.source_rows)}` },
      { label: 'DDX 顺序', value: summary.source_ddx_descending ? '严格降序' : '待核验' },
      { label: '总净流入为负', value: count(summary.negative_total_net_inflow_rows), note: '不等同于 DDX 方向' },
      { label: '来源', value: count(doc?.counts.sources), note: doc?.sources[0]?.resource ?? '' }
    ];
  });

  function load(refresh = false) {
    void resource.load(`/api/v1/market/capital-strength?${queryString({
      view,
      period: view === 'ranking' ? period : '',
      min_periods: view === 'confluence' ? minPeriods : '',
      q: query.trim(),
      sort: view === 'ranking' ? 'ddx' : 'period-count',
      order: 'desc', limit: 1000, refresh: refresh ? 1 : 0
    })}`);
  }

  function openSecurity(security: ValuationSecurity) {
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code, 'capital-strength'));
  }

  const rankingColumns: Column<CapitalStrengthRecord>[] = [
    { key: 'security', label: '股票', width: '138px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'rank', label: '源排名', width: '70px', align: 'right', num: true, value: (row) => count(row.source_rank), sortValue: (row) => row.source_rank },
    { key: 'ddx', label: 'DDX', width: '90px', align: 'right', num: true, value: (row) => `${fixed(row.ddx_float_share_pct, 4)}%`, tone: (row) => tone(row.ddx_float_share_pct), sortValue: (row) => row.ddx_float_share_pct ?? 0 },
    { key: 'return', label: '周期涨幅', width: '92px', align: 'right', num: true, value: (row) => percent(row.period_return_pct), tone: (row) => tone(row.period_return_pct), sortValue: (row) => row.period_return_pct ?? 0 },
    { key: 'main', label: '主力净流入', width: '120px', align: 'right', num: true, value: (row) => compact(row.main_net_inflow_yuan, '元'), tone: (row) => tone(row.main_net_inflow_yuan), sortValue: (row) => row.main_net_inflow_yuan ?? 0 },
    { key: 'total', label: '总净流入', width: '120px', align: 'right', num: true, value: (row) => compact(row.total_net_inflow_yuan, '元'), tone: (row) => tone(row.total_net_inflow_yuan), sortValue: (row) => row.total_net_inflow_yuan ?? 0 },
    { key: 'float', label: '流通股本', width: '110px', align: 'right', num: true, value: (row) => compact(row.float_shares, '股'), sortValue: (row) => row.float_shares ?? 0 },
    { key: 'date', label: '统计日期', width: '92px', num: true, value: (row) => date(row.statistics_date), sortValue: (row) => row.statistics_date }
  ];

  const confluenceColumns: Column<CapitalStrengthConfluenceRecord>[] = [
    { key: 'security', label: '股票', width: '138px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'periods', label: '命中周期', width: '190px', value: (row) => row.periods.map((item) => PERIODS.find((periodItem) => periodItem.id === item)?.label.replace('资金强势', '') ?? item).join(' / '), sub: (row) => `${count(row.period_count)} / 5` },
    { key: 'average', label: '平均 DDX', width: '100px', align: 'right', num: true, value: (row) => `${fixed(row.average_ddx_float_share_pct, 4)}%`, tone: (row) => tone(row.average_ddx_float_share_pct), sortValue: (row) => row.average_ddx_float_share_pct ?? 0 },
    { key: 'maximum', label: '最高 DDX', width: '100px', align: 'right', num: true, value: (row) => `${fixed(row.maximum_ddx_float_share_pct, 4)}%`, sortValue: (row) => row.maximum_ddx_float_share_pct ?? 0 },
    { key: 'rank', label: '最佳排名', width: '82px', align: 'right', num: true, value: (row) => count(row.best_source_rank), sortValue: (row) => row.best_source_rank ?? 999 },
    { key: 'date', label: '最新统计日', width: '96px', num: true, value: (row) => date(row.latest_statistics_date), sortValue: (row) => row.latest_statistics_date }
  ];

  onMount(() => load());
</script>

<PageHeader eyebrow="709/1721 · QSZJ101—105" title="DDX 资金强势" description="还原通达信个股资金流向页的五个 DDX 前百榜，并识别同时进入多个周期的资金强势共振股票。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新榜单</Button>{/snippet}
</PageHeader>

<Panel title={view === 'ranking' ? PERIODS.find((item) => item.id === period)?.label ?? '资金强势' : '跨周期共振'} subtitle={doc ? `${count(doc.counts.matched)} 只命中 · 点击股票进入单票反查` : ''} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && doc?.records.length === 0} emptyText="当前筛选没有命中股票。" flush scroll fill>
  {#snippet toolbar()}
    <Select options={VIEWS} value={view} width="150px" label="视图" onChange={(next) => { view = next as 'ranking' | 'confluence'; query = ''; load(); }} />
    {#if view === 'ranking'}
      <Select options={PERIODS} value={period} width="165px" label="统计周期" onChange={(next) => { period = next as CapitalStrengthPeriod; load(); }} />
    {:else}
      <Select options={MIN_PERIODS} value={minPeriods} width="155px" label="共振门槛" onChange={(next) => { minPeriods = next; load(); }} />
    {/if}
    <TextInput bind:value={query} icon="search" width="240px" label="检索" placeholder="股票代码 / 名称" onEnter={() => load()} />
  {/snippet}
  {#if view === 'ranking'}
    <DataTable columns={rankingColumns} rows={rankingRows} stickyFirst numbered rowKey={(row) => `${row.period}:${row.security.security_id}`} onRowClick={(row) => openSecurity(row.security)} sortKey="ddx" />
  {:else}
    <DataTable columns={confluenceColumns} rows={confluenceRows} stickyFirst numbered rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} sortKey="periods" />
  {/if}
</Panel>
