<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, fixed, money, percent, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { CorporateOrderRecord, MarketCorporateOrdersDocument } from '../../types';

  const VIEWS = [
    { id: 'all', label: '全部' }, { id: 'tenders', label: '招投标 / 中标' },
    { id: 'contracts', label: '重大合同' }
  ];
  const SORTS = [
    { id: 'date', label: '公告日' }, { id: 'amount', label: '金额' },
    { id: 'revenue-share', label: '营收占比' }
  ];
  let view = $state<'all' | 'tenders' | 'contracts'>('all');
  let sort = $state('date');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketCorporateOrdersDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.record_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived(doc ? [
    { label: '招投标 / 中标', value: count(doc.summary.tenders) },
    { label: '重大合同', value: count(doc.summary.major_contracts) },
    { label: '涉及公司', value: count(doc.summary.unique_securities) },
    { label: '占比公式偏差', value: count(doc.summary.formula_mismatches), note: `${count(doc.summary.formula_checked)} 条已核验` },
    { label: '最新公告', value: date(doc.summary.latest_date) }
  ] : []);

  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/corporate-orders?${queryString({
      view, sort, order: 'desc', q: query.trim(), include_raw: 0, limit: 10000,
      refresh: refresh ? 1 : 0
    })}`);
  }
  function name(row: CorporateOrderRecord) { return row.security.name || row.security.code; }
  function openStock() {
    if (selected) router.go(stockPath(selected.security.market, selected.security.code, 'corporate-orders'));
  }
  const columns: Column<CorporateOrderRecord>[] = [
    { key: 'date', label: '公告日', width: '92px', num: true, value: (row) => date(row.announcement_date), sortValue: (row) => row.announcement_date },
    { key: 'security', label: '公司', width: '150px', value: name, sub: (row) => row.security.security_id },
    { key: 'kind', label: '类型 / 阶段', width: '118px', slot: true, sortValue: (row) => row.kind },
    { key: 'title', label: '事项', value: (row) => text(row.title), wrap: true },
    { key: 'amount', label: '订单 / 合同金额', align: 'right', num: true, value: (row) => money(row.amount_yuan), sortValue: (row) => row.amount_yuan ?? 0 },
    { key: 'share', label: '营收占比', align: 'right', num: true, value: (row) => percent(row.calculated_revenue_share_pct), sub: (row) => row.revenue_share_formula_matches === false ? '源值与复算不一致' : '已复算' }
  ];
  const detailStats = $derived(selected ? [
    { label: '金额', value: money(selected.amount_yuan) },
    { label: '营业收入', value: money(selected.revenue_yuan) },
    { label: '营收占比', value: percent(selected.calculated_revenue_share_pct) },
    { label: '非经常性损益', value: money(selected.non_recurring_profit_yuan) },
    { label: '公告日期', value: date(selected.announcement_date) }
  ] : []);
  onMount(() => load());
</script>

<PageHeader eyebrow="ZB101 · ZDHT101" title="招投标、中标与重大合同" description="把公司订单、合同金额、营业收入占比和公告原文统一到可检索主表；占比由金额与营收再次复算。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>{/snippet}
</PageHeader>

<Split asideWidth="390px">
  {#snippet main()}
    <Panel title="公司订单主表" subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.sources.length)} 个来源` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选下没有订单或合同。" flush scroll>
      {#snippet toolbar()}
        <Segmented options={VIEWS} value={view} onChange={(next) => { view = next as typeof view; load(); }} ariaLabel="订单类型" />
        <TextInput bind:value={query} icon="search" width="250px" label="检索" placeholder="公司、代码、标题或阶段" onEnter={() => load()} />
        <Segmented options={SORTS} value={sort} onChange={(next) => { sort = next; load(); }} ariaLabel="排序" />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.record_id} onRowClick={(row) => (selectedId = row.record_id)} isActive={(row) => row.record_id === selected?.record_id} stickyFirst numbered minWidth="1050px">
        {#snippet cell({ row })}<Badge tone={row.kind === 'major-contract' ? 'focus' : 'neutral'}>{row.kind_label} · {row.stage}</Badge>{/snippet}
      </DataTable>
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.kind_label ?? 'CORPORATE ORDERS'} title={selected ? name(selected) : '订单详情'} subtitle={selected ? `${selected.security.security_id} · ${selected.source_resource}` : '点击左侧记录查看'} empty={!selected && !resource.busy} emptyText="选择一条订单或合同。" scroll>
      {#if selected}
        <button type="button" onclick={openStock}>在个股工作台打开</button>
        <StatGrid stats={detailStats} inline />
        <h3>{selected.title}</h3>
        <p>阶段：{text(selected.stage)}；源占比 {fixed(selected.source_revenue_share_pct, 4)}%，复算占比 {fixed(selected.calculated_revenue_share_pct, 4)}%。</p>
        {#if selected.source_url}<a href={selected.source_url} target="_blank" rel="noreferrer">打开公告原文 ↗</a>{/if}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  button { width: 100%; height: 28px; margin-bottom: var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  h3 { margin: var(--sp-4) 0 var(--sp-2); font-size: var(--fs-xs); line-height: 1.6; }
  p { color: var(--fg-mute); font-size: var(--fs-xs); line-height: 1.7; }
  a { display: inline-block; margin-top: var(--sp-3); color: var(--focus); text-decoration: none; }
</style>
