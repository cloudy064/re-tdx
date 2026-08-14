<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, delta, money, percent } from '../../lib/fmt';
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
  import type { MarketRecentWatchDocument, RecentWatchRecord } from '../../types';

  type View = MarketRecentWatchDocument['view'];
  const VIEWS = [
    { id: 'all', label: '全部' }, { id: 'earnings-divergence', label: '绩价背离' },
    { id: 'foreign-business', label: '涉外经营' }, { id: 'st-turnaround', label: 'ST预盈' }
  ];
  let view = $state<View>('all'); let query = $state(''); let selectedId = $state('');
  const resource = new Resource<MarketRecentWatchDocument>();
  const doc = $derived(resource.data); const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived.by<Stat[]>(() => doc ? [
    { label: '绩价背离', value: count(doc.summary['earnings-divergence']) },
    { label: '涉外经营', value: count(doc.summary['foreign-business']) },
    { label: 'ST业绩预盈', value: count(doc.summary['st-turnaround']) },
    { label: '覆盖证券', value: count(doc.summary.unique_securities) }
  ] : []);
  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/recent-watch?${queryString({ view, q: query.trim(), limit: 5000, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  function switchView(next: string) { view = next as View; load(); }
  function name(row: RecentWatchRecord) { return row.security.name || row.security.code; }
  function primary(row: RecentWatchRecord) {
    if (row.kind === 'foreign-business') return `${money(row.foreign_revenue_yuan)} · 占比 ${percent(row.foreign_revenue_pct)}`;
    if (row.kind === 'st-turnaround') return `${money(row.profit_lower_yuan)} ～ ${money(row.profit_upper_yuan)}`;
    return `${row.forecast_type || '业绩预告'} · 预告同比 ${delta(row.forecast_profit_yoy_pct)}`;
  }
  function secondary(row: RecentWatchRecord) {
    if (row.kind === 'foreign-business') return `${row.effect || '—'} · 境外利润 ${money(row.foreign_profit_yuan)}`;
    if (row.kind === 'st-turnaround') return `增速 ${delta(row.growth_lower_pct)} ～ ${delta(row.growth_upper_pct)}`;
    return `预告至今 ${delta(row.return_since_announcement_pct)} · 实际 ${delta(row.actual_profit_yoy_pct)}`;
  }
  function openStock() { if (selected) router.go(stockPath(selected.security.market, selected.security.code, 'recent-watch')); }
  const columns: Column<RecentWatchRecord>[] = [
    { key: 'kind', label: '关注类型', width: '140px', value: (row) => row.kind_label, sub: (row) => row.source_resource },
    { key: 'security', label: '股票', width: '165px', value: name, sub: (row) => row.security.security_id },
    { key: 'date', label: '日期 / 报告期', width: '115px', num: true, value: (row) => date(row.event_date), sub: (row) => date(row.report_period) },
    { key: 'primary', label: '核心数据', width: '350px', value: primary },
    { key: 'secondary', label: '变化 / 敞口', width: '350px', value: secondary }
  ];
  const detailStats = $derived.by<Stat[]>(() => selected ? [
    { label: '事件日期', value: date(selected.event_date) },
    { label: '公告至今', value: delta(selected.return_since_announcement_pct) },
    { label: '境外收入占比', value: percent(selected.foreign_revenue_pct) },
    { label: '预告利润下限', value: money(selected.profit_lower_yuan) }
  ] : []);
  onMount(() => load());
</script>

<PageHeader eyebrow="JQGZ · 3 LIVE SOURCES" title="近期业绩与涉外经营关注" description="补齐客户端绩价背离、人民币波动相关涉外经营敞口和 ST 业绩预盈。公告后涨幅是静态源快照，不冒充实时行情。" {stats}>
  {#snippet actions()}<Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="近期关注视图" /><Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>{/snippet}
</PageHeader>
<Split asideWidth="390px">
  {#snippet main()}
    <Panel title="近期关注线索" subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.sources.length)} 个资源` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选下没有线索。" flush scroll>
      {#snippet toolbar()}<TextInput bind:value={query} icon="search" width="270px" label="检索" placeholder="代码、名称、原因或类型" onEnter={() => load()} />{/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedId = row.event_id)} isActive={(row) => row.event_id === selected?.event_id} stickyFirst numbered minWidth="1100px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.kind_label ?? 'RECENT WATCH'} title={selected ? name(selected) : '线索详情'} subtitle={selected?.source_resource ?? '选择左侧线索'} empty={!selected && !resource.busy}>
      {#if selected}<button type="button" onclick={openStock}>在个股工作台打开</button><StatGrid stats={detailStats} inline /><h4>{primary(selected)}</h4><p>{selected.reason || secondary(selected)}</p>{/if}
    </Panel>
  {/snippet}
</Split>
<style>
  button { width: 100%; height: 28px; margin-bottom: var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  h4 { margin: var(--sp-3) 0 0; font-size: var(--fs-sm); } p { margin-top: var(--sp-2); white-space: pre-wrap; font-size: var(--fs-xs); line-height: 1.7; color: var(--fg-mute); }
</style>
