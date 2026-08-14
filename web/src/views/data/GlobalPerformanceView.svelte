<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, price, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { EquityPerformanceSort, GlobalPerformanceRecord, MarketGlobalPerformanceDocument } from '../../types';

  const VIEWS = [
    { id: 'all', label: '全部' }, { id: 'major-indices', label: '主要指数' },
    { id: 'overseas-china', label: '海外中资' }
  ];
  const SORTS = [
    { id: 'return-5d', label: '5日' }, { id: 'return-20d', label: '20日' },
    { id: 'return-60d', label: '60日' }, { id: 'month', label: '本月' },
    { id: 'ytd', label: '年内' }, { id: 'turnover-day', label: '成交' },
    { id: 'pe', label: 'PE' }
  ];
  let view = $state<'all' | 'major-indices' | 'overseas-china'>('major-indices');
  let sort = $state<EquityPerformanceSort>('return-5d');
  let order = $state<'asc' | 'desc'>('desc');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketGlobalPerformanceDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.record_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived(doc ? [
    { label: '主要指数', value: count(doc.summary.major_indices) },
    { label: '海外中资证券', value: count(doc.summary.overseas_china) },
    { label: '未解析名称', value: count(doc.summary.unresolved_names) },
    { label: '最新行情日', value: date(doc.summary.latest_quote_date) }
  ] : []);
  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/global-performance?${queryString({
      view, sort, order, q: query.trim(), include_raw: 0, limit: 10000,
      refresh: refresh ? 1 : 0
    })}`);
  }
  const columns: Column<GlobalPerformanceRecord>[] = [
    { key: 'instrument', label: '指数 / 证券', width: '175px', value: (row) => row.instrument.name || row.instrument.code, sub: (row) => `${row.instrument.market_id}:${row.instrument.code}` },
    { key: 'kind', label: '市场类别', width: '105px', slot: true },
    { key: 'close', label: '收盘 / PE', align: 'right', num: true, value: (row) => price(row.close), sub: (row) => `PE ${fixed(row.pe)}` },
    { key: 'r5', label: '5日', align: 'right', num: true, value: (row) => delta(row.return_5d_pct), sortValue: (row) => row.return_5d_pct ?? -Infinity },
    { key: 'r20', label: '20日', align: 'right', num: true, value: (row) => delta(row.return_20d_pct) },
    { key: 'r60', label: '60日', align: 'right', num: true, value: (row) => delta(row.return_60d_pct) },
    { key: 'month', label: '本月 / 年内', align: 'right', num: true, value: (row) => delta(row.month_to_date_pct), sub: (row) => delta(row.year_to_date_pct) },
    { key: 'turnover', label: '当日 / 5日成交', align: 'right', num: true, value: (row) => compact(row.daily_turnover), sub: (row) => compact(row.five_day_turnover) },
    { key: 'date', label: '行情日', width: '90px', num: true, value: (row) => date(row.quote_date) }
  ];
  const detailStats = $derived(selected ? [
    { label: '5日', value: delta(selected.return_5d_pct) },
    { label: '20日', value: delta(selected.return_20d_pct) },
    { label: '60日', value: delta(selected.return_60d_pct) },
    { label: '本月', value: delta(selected.month_to_date_pct) },
    { label: '年内', value: delta(selected.year_to_date_pct) },
    { label: 'PE', value: fixed(selected.pe) }
  ] : []);
  onMount(() => load());
</script>

<PageHeader eyebrow="ZYZH101 · ZGGHQ101 · EXPANSION DIRECTORY" title="主要指数与海外中资表现" description="13 个主要指数和海外中资证券的 5/20/60 日、本月及年内表现；名称由 7727 扩展市场目录补齐。" {stats}>
  {#snippet actions()}
    <Button onclick={() => { order = order === 'desc' ? 'asc' : 'desc'; load(); }}>{order === 'desc' ? '降序 ↓' : '升序 ↑'}</Button>
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>
<Split asideWidth="350px">
  {#snippet main()}
    <Panel title="全球表现横截面" subtitle={doc ? `${count(doc.match_count)} 个指数 / 证券` : '本地 JSN 与扩展目录'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选下没有记录。" flush scroll>
      {#snippet toolbar()}
        <Segmented options={VIEWS} value={view} onChange={(next) => { view = next as typeof view; load(); }} ariaLabel="市场范围" />
        <TextInput bind:value={query} icon="search" width="220px" label="检索" placeholder="代码或名称" onEnter={() => load()} />
        <Segmented options={SORTS} value={sort} onChange={(next) => { sort = next as EquityPerformanceSort; load(); }} ariaLabel="排序指标" />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.record_id} onRowClick={(row) => (selectedId = row.record_id)} isActive={(row) => row.record_id === selected?.record_id} stickyFirst numbered minWidth="1100px">
        {#snippet cell({ row })}<Badge tone={row.kind === 'major-index' ? 'focus' : 'neutral'}>{row.kind_label}</Badge>{/snippet}
      </DataTable>
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.kind_label ?? 'GLOBAL PERFORMANCE'} title={selected?.instrument.name || selected?.instrument.code || '表现详情'} subtitle={selected ? `${selected.instrument.market_id}:${selected.instrument.code} · ${date(selected.quote_date)}` : '选择左侧记录'} empty={!selected && !resource.busy}>
      {#if selected}
        <StatGrid stats={detailStats} inline />
        <p>收盘 {price(selected.close)}；当日成交 {compact(selected.daily_turnover)}，5日成交 {compact(selected.five_day_turnover)}。</p>
        <p class="note">成交额保留各市场原始币种 / 单位，不跨市场汇总。{text(doc?.semantics)}</p>
      {/if}
    </Panel>
  {/snippet}
</Split>
<style>
  p { margin-top: var(--sp-4); font-size: var(--fs-xs); line-height: 1.7; }
  .note { color: var(--fg-mute); }
</style>
