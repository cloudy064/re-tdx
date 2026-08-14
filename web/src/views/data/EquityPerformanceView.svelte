<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, delta, fixed, money, price } from '../../lib/fmt';
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
  import type { EquityPerformanceRecord, EquityPerformanceSort, MarketEquityPerformanceDocument } from '../../types';

  const SORTS = [
    { id: 'return-5d', label: '5日' }, { id: 'return-20d', label: '20日' },
    { id: 'return-60d', label: '60日' }, { id: 'month', label: '本月' },
    { id: 'ytd', label: '年内' }, { id: 'turnover-day', label: '成交额' },
    { id: 'pe', label: 'PE' }
  ];
  let sort = $state<EquityPerformanceSort>('return-5d');
  let order = $state<'asc' | 'desc'>('desc');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketEquityPerformanceDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.record_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived.by<Stat[]>(() => doc ? [
    { label: '股票总数', value: count(doc.match_count) },
    { label: '沪市', value: count(doc.summary.sh) },
    { label: '深市', value: count(doc.summary.sz) },
    { label: '北证', value: count(doc.summary.bj) },
    { label: '行情日期', value: date(doc.summary.latest_quote_date) }
  ] : []);
  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/equity-performance?${queryString({
      sort, order, q: query.trim(), limit: 10000, include_raw: 0,
      refresh: refresh ? 1 : 0
    })}`);
  }
  function switchSort(next: string) { sort = next as EquityPerformanceSort; load(); }
  function toggleOrder() { order = order === 'desc' ? 'asc' : 'desc'; load(); }
  function name(row: EquityPerformanceRecord) { return row.security.name || row.security.code; }
  function openStock() {
    if (selected) router.go(stockPath(selected.security.market, selected.security.code, 'equity-performance'));
  }
  const columns: Column<EquityPerformanceRecord>[] = [
    { key: 'security', label: '股票', width: '160px', value: name, sub: (row) => row.security.security_id },
    { key: 'close', label: '收盘 / PE', align: 'right', num: true, value: (row) => price(row.close), sub: (row) => `PE ${fixed(row.pe)}` },
    { key: 'r5', label: '5日涨幅', align: 'right', num: true, value: (row) => delta(row.return_5d_pct) },
    { key: 'r20', label: '20日涨幅', align: 'right', num: true, value: (row) => delta(row.return_20d_pct) },
    { key: 'r60', label: '60日涨幅', align: 'right', num: true, value: (row) => delta(row.return_60d_pct) },
    { key: 'month', label: '本月 / 年内', align: 'right', num: true, value: (row) => delta(row.month_to_date_pct), sub: (row) => delta(row.year_to_date_pct) },
    { key: 'turnover', label: '当日 / 5日成交额', align: 'right', num: true, value: (row) => money(row.daily_turnover_yuan), sub: (row) => money(row.five_day_turnover_yuan) },
    { key: 'date', label: '行情日期', align: 'right', num: true, value: (row) => date(row.quote_date) }
  ];
  const detailStats = $derived.by<Stat[]>(() => selected ? [
    { label: '5日', value: delta(selected.return_5d_pct) },
    { label: '20日', value: delta(selected.return_20d_pct) },
    { label: '60日', value: delta(selected.return_60d_pct) },
    { label: '本月以来', value: delta(selected.month_to_date_pct) },
    { label: '年初至今', value: delta(selected.year_to_date_pct) },
    { label: '当日成交额', value: money(selected.daily_turnover_yuan) }
  ] : []);
  onMount(() => load());
</script>

<PageHeader eyebrow="AGHQ · 5,539 A-SHARES" title="A股多周期表现" description="通达信客户端 A 股表现横截面：收盘价、成交额、5/20/60 日、本月及年初至今涨跌；本月涨幅按客户端 CFG 原公式复算。" {stats}>
  {#snippet actions()}
    <Button onclick={toggleOrder}>{order === 'desc' ? '降序 ↓' : '升序 ↑'}</Button>
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="360px">
  {#snippet main()}
    <Panel title="行情表现横截面" subtitle={doc ? `${count(doc.match_count)} 只 · ${date(doc.summary.latest_quote_date)}` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前条件下没有股票。" flush scroll>
      {#snippet toolbar()}
        <TextInput bind:value={query} icon="search" width="220px" label="检索股票" placeholder="代码或名称" onEnter={() => load()} />
        <Segmented options={SORTS} value={sort} onChange={switchSort} ariaLabel="表现排序" />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.record_id} onRowClick={(row) => (selectedId = row.record_id)} isActive={(row) => row.record_id === selected?.record_id} stickyFirst numbered minWidth="1080px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow="AGHQ PERFORMANCE" title={selected ? name(selected) : '表现详情'} subtitle={selected ? `${selected.security.security_id} · ${date(selected.quote_date)}` : '选择左侧股票'} empty={!selected && !resource.busy}>
      {#if selected}
        <button type="button" onclick={openStock}>在个股工作台打开</button>
        <StatGrid stats={detailStats} inline />
        <p>收盘 {price(selected.close)}，上月底收盘 {price(selected.prior_month_close)}，5日成交额 {money(selected.five_day_turnover_yuan)}，PE {fixed(selected.pe)}。</p>
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  button { min-width: 72px; height: 28px; padding: 0 var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  p { margin-top: var(--sp-3); font-size: var(--fs-xs); line-height: 1.7; color: var(--fg-mute); }
</style>
