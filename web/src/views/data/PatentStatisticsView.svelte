<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date } from '../../lib/fmt';
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
  import type { MarketPatentStatisticsDocument, PatentStatisticsRecord } from '../../types';

  const MARKETS = [
    { id: '', label: '全部' }, { id: 'sz', label: '深市' },
    { id: 'sh', label: '沪市' }, { id: 'bj', label: '北市' }
  ];
  const SORTS = [
    { id: 'cumulative-total', label: '累计专利' },
    { id: 'cumulative-invention', label: '发明专利' },
    { id: 'period-grants', label: '本期授权' },
    { id: 'period-applications', label: '本期申请' },
    { id: 'report-date', label: '报告日期' }
  ];
  let market = $state('');
  let sort = $state<MarketPatentStatisticsDocument['sort']>('cumulative-total');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketPatentStatisticsDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.security.security_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived.by<Stat[]>(() => doc ? [
    { label: '覆盖公司', value: count(doc.summary.unique_securities) },
    { label: '深市', value: count(doc.summary.sz) },
    { label: '沪市', value: count(doc.summary.sh) },
    { label: '北市', value: count(doc.summary.bj) }
  ] : []);
  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/patent-statistics?${queryString({
      market, q: query.trim(), sort, order: 'desc', limit: 10000,
      include_raw: 0, refresh: refresh ? 1 : 0
    })}`);
  }
  function switchMarket(value: string) { market = value; load(); }
  function switchSort(value: string) { sort = value as MarketPatentStatisticsDocument['sort']; load(); }
  function name(row: PatentStatisticsRecord) { return row.security.name || row.security.code; }
  function breakdown(invention: number | null, utility: number | null, design: number | null) {
    return `发明 ${count(invention)} · 实用 ${count(utility)} · 外观 ${count(design)}`;
  }
  function openStock() {
    if (selected) router.go(stockPath(selected.security.market, selected.security.code, 'patent-statistics'));
  }
  const columns: Column<PatentStatisticsRecord>[] = [
    { key: 'security', label: '公司', width: '170px', value: name, sub: (row) => row.security.security_id },
    { key: 'report', label: '截止日期', width: '110px', num: true, value: (row) => date(row.report_date) },
    { key: 'applications', label: '本期申请', width: '265px', num: true, value: (row) => count(row.period_application_total), sub: (row) => breakdown(row.period_application_invention, row.period_application_utility_model, row.period_application_design) },
    { key: 'grants', label: '本期获得', width: '265px', num: true, value: (row) => count(row.period_grant_total), sub: (row) => breakdown(row.period_grant_invention, row.period_grant_utility_model, row.period_grant_design) },
    { key: 'cumulative', label: '累计获得', width: '265px', num: true, value: (row) => count(row.cumulative_grant_total), sub: (row) => breakdown(row.cumulative_grant_invention, row.cumulative_grant_utility_model, row.cumulative_grant_design) },
    { key: 'delta', label: '总数－三类合计', width: '145px', num: true, value: (row) => count(row.cumulative_grant_total_delta) }
  ];
  const detailStats = $derived.by<Stat[]>(() => selected ? [
    { label: '累计授权', value: count(selected.cumulative_grant_total) },
    { label: '累计发明', value: count(selected.cumulative_grant_invention) },
    { label: '本期申请', value: count(selected.period_application_total) },
    { label: '本期授权', value: count(selected.period_grant_total) }
  ] : []);
  onMount(() => load());
</script>

<PageHeader eyebrow="GSZL · 3,970 COMPANIES" title="上市公司专利统计" description="客户端公开专利截面：本期申请、本期获得及累计获得。源总数可能包含客户端未单列的专利类别，因此保留三类合计与差值，不强行改写。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>{/snippet}
</PageHeader>
<Split asideWidth="390px">
  {#snippet main()}
    <Panel title="公司专利横截面" subtitle={doc ? `${count(doc.match_count)} 家 · ${date(doc.summary.first_report_date)} 至 ${date(doc.summary.last_report_date)}` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选下没有专利记录。" flush scroll>
      {#snippet toolbar()}<div class="toolbar"><Segmented options={MARKETS} value={market} onChange={switchMarket} ariaLabel="交易市场" /><Segmented options={SORTS} value={sort} onChange={switchSort} ariaLabel="专利排序" /><TextInput bind:value={query} icon="search" width="230px" label="检索" placeholder="代码或公司名称" onEnter={() => load()} /></div>{/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.security.security_id} onRowClick={(row) => (selectedId = row.security.security_id)} isActive={(row) => row.security.security_id === selected?.security.security_id} stickyFirst numbered minWidth="1220px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow="PATENT SNAPSHOT" title={selected ? name(selected) : '专利详情'} subtitle={selected ? `${selected.security.security_id} · ${date(selected.report_date)}` : '选择左侧公司'} empty={!selected && !resource.busy}>
      {#if selected}<button type="button" onclick={openStock}>在个股工作台打开</button><StatGrid stats={detailStats} inline /><h4>累计授权分类</h4><p>{breakdown(selected.cumulative_grant_invention, selected.cumulative_grant_utility_model, selected.cumulative_grant_design)}</p><h4>口径差值</h4><p>来源总数－客户端三类合计：{count(selected.cumulative_grant_total_delta)}。正数通常代表源总数还包含未在当前表单列的类别；负数保持原样供审计。</p>{/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .toolbar { display: flex; align-items: center; gap: var(--sp-2); flex-wrap: wrap; }
  button { width: 100%; height: 28px; margin-bottom: var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  h4 { margin: var(--sp-3) 0 0; font-size: var(--fs-sm); }
  p { margin-top: var(--sp-2); font-size: var(--fs-xs); line-height: 1.7; color: var(--fg-mute); }
</style>
