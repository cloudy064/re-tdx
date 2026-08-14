<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, fixed, percent, price, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { EventImpactRecord, MarketEventImpactDocument } from '../../types';

  const BENCHMARKS = [
    { id: 'all', label: '全部指数' }, { id: 'shanghai-composite', label: '上证指数' },
    { id: 'hang-seng', label: '恒生指数' }, { id: 'nasdaq-composite', label: '纳斯达克' }
  ];
  let benchmark = $state('all');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketEventImpactDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.record_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived(doc ? [
    { label: '上证事件', value: count(doc.summary.shanghai_composite) },
    { label: '恒生事件', value: count(doc.summary.hang_seng) },
    { label: '纳指事件', value: count(doc.summary.nasdaq_composite) },
    { label: '事件类型', value: count(doc.summary.event_types) },
    { label: '覆盖区间', value: `${date(doc.summary.earliest_date)} — ${date(doc.summary.latest_date)}` }
  ] : []);
  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/event-impact?${queryString({
      benchmark, q: query.trim(), sort: 'date', order: 'desc', include_raw: 0,
      limit: 5000, refresh: refresh ? 1 : 0
    })}`);
  }
  const columns: Column<EventImpactRecord>[] = [
    { key: 'date', label: '事件日期', width: '96px', num: true, value: (row) => date(row.start_date), sub: (row) => row.end_date ? date(row.end_date) : '' },
    { key: 'benchmark', label: '对照指数', width: '112px', value: (row) => row.benchmark_label },
    { key: 'type', label: '事件类型', width: '90px', value: (row) => text(row.event_type) },
    { key: 'event', label: '事件', value: (row) => text(row.description), wrap: true },
    { key: 'before', label: '前一周至开始', align: 'right', num: true, value: (row) => percent(row.impacts.week_before_start_pct), tone: (row) => tone(row.impacts.week_before_start_pct) },
    { key: 'interval', label: '事件期间', align: 'right', num: true, value: (row) => percent(row.impacts.event_interval_pct), tone: (row) => tone(row.impacts.event_interval_pct) },
    { key: 'after', label: '结束后一周', align: 'right', num: true, value: (row) => percent(row.impacts.week_after_end_pct), tone: (row) => tone(row.impacts.week_after_end_pct) }
  ];
  const detailStats = $derived(selected ? [
    { label: '前一周', value: percent(selected.impacts.week_before_start_pct), tone: tone(selected.impacts.week_before_start_pct) },
    { label: '开始当日', value: percent(selected.impacts.start_day_pct), tone: tone(selected.impacts.start_day_pct) },
    { label: '事件期间', value: percent(selected.impacts.event_interval_pct), tone: tone(selected.impacts.event_interval_pct) },
    { label: '结束次日', value: percent(selected.impacts.day_after_end_pct), tone: tone(selected.impacts.day_after_end_pct) },
    { label: '后一周', value: percent(selected.impacts.week_after_end_pct), tone: tone(selected.impacts.week_after_end_pct) }
  ] : []);
  onMount(() => load());
</script>

<PageHeader eyebrow="ZDSJ101 / 102 / 103" title="重大事件对指数的冲击" description="将节假日、政治经济与市场事件对齐上证、恒生和纳斯达克指数，观察事件前一周、事件期间及结束后一周表现。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新事件库</Button>{/snippet}
</PageHeader>
<Split asideWidth="380px">
  {#snippet main()}
    <Panel title="事件冲击主表" subtitle={doc ? `${count(doc.match_count)} 条事件—指数关系` : '三地指数'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前条件下没有事件。" flush scroll>
      {#snippet toolbar()}
        <Segmented options={BENCHMARKS} value={benchmark} onChange={(next) => { benchmark = next; load(); }} ariaLabel="对照指数" />
        <TextInput bind:value={query} icon="search" width="240px" label="检索事件" placeholder="类型或事件描述" onEnter={() => load()} />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.record_id} onRowClick={(row) => (selectedId = row.record_id)} isActive={(row) => row.record_id === selected?.record_id} stickyFirst numbered minWidth="980px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.benchmark_label ?? 'EVENT IMPACT'} title={selected?.description ?? '事件详情'} subtitle={selected ? `${date(selected.start_date)} · ${selected.event_type}` : '选择左侧事件'} empty={!selected && !resource.busy} scroll>
      {#if selected}
        <StatGrid stats={detailStats} inline />
        <p>事件前周收盘 {price(selected.closes.week_before_start)}，开始日 {price(selected.closes.start_day)}，结束日 {price(selected.closes.end_day)}，结束后周收盘 {price(selected.closes.week_after_end)}。</p>
        <p class="note">不同交易所的日期口径按客户端原表保留；空值表示该窗口没有可比交易日，不补造收益。</p>
      {/if}
    </Panel>
  {/snippet}
</Split>
<style>
  p { margin-top: var(--sp-4); font-size: var(--fs-xs); line-height: 1.7; }
  .note { color: var(--fg-mute); }
</style>
