<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { MarketOverviewFactorsDocument, OverviewFactorRecord, OverviewFactorSignal } from '../../types';

  const SIGNALS = [
    { id: 'all', label: '全部' }, { id: 'positive', label: '利好' },
    { id: 'neutral', label: '中性' }, { id: 'negative', label: '利空' },
    { id: 'unrated', label: '未评级' }
  ];
  let signal = $state<MarketOverviewFactorsDocument['signal']>('all');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketOverviewFactorsDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.factor_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived.by<Stat[]>(() => doc ? [
    { label: '观察因素', value: count(doc.summary.positive + doc.summary.neutral + doc.summary.negative + doc.summary.unrated) },
    { label: '利好', value: count(doc.summary.positive), tone: 'up' },
    { label: '中性', value: count(doc.summary.neutral) },
    { label: '利空', value: count(doc.summary.negative), tone: 'down' },
    { label: '未评级', value: count(doc.summary.unrated) }
  ] : []);
  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/overview-factors?${queryString({
      signal, q: query.trim(), include_raw: 0, limit: 100, refresh: refresh ? 1 : 0
    })}`);
  }
  function switchSignal(value: string) {
    signal = value as MarketOverviewFactorsDocument['signal'];
    load();
  }
  function badgeTone(value: OverviewFactorSignal): 'up' | 'down' | 'neutral' | 'warn' {
    if (value === 'positive') return 'up';
    if (value === 'negative') return 'down';
    return value === 'unrated' ? 'warn' : 'neutral';
  }
  const columns: Column<OverviewFactorRecord>[] = [
    { key: 'rank', label: '#', width: '52px', num: true, value: (row) => count(row.source_rank) },
    { key: 'name', label: '大盘因素', width: '190px', value: (row) => row.name, sub: (row) => row.factor_id },
    { key: 'signal', label: '源判断', width: '84px', value: (row) => row.signal_label },
    { key: 'indicator', label: '对应观察指标', width: '220px', value: (row) => text(row.chart_indicator) },
    { key: 'description', label: '客户端快照说明', value: (row) => text(row.description), wrap: true }
  ];
  onMount(() => load());
</script>

<PageHeader eyebrow="DPFX · MARKET FACTORS" title="大盘影响因素" description="还原通达信大盘分析中的 17 项观察因素与源判断。文字带有各自观测日期，是客户端分析快照，不等同于实时行情。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>{/snippet}
</PageHeader>
<Split asideWidth="390px">
  {#snippet main()}
    <Panel title="影响因素快照" subtitle={doc ? `${count(doc.match_count)} 项 · ${doc.cache.refreshed ? '已更新' : '本地缓存'}` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选下没有因素。" flush scroll>
      {#snippet toolbar()}<div class="toolbar"><Segmented options={SIGNALS} value={signal} onChange={switchSignal} ariaLabel="影响方向" /><TextInput bind:value={query} icon="search" width="250px" label="检索因素" placeholder="名称、指标或说明" onEnter={() => load()} /></div>{/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.factor_id} onRowClick={(row) => (selectedId = row.factor_id)} isActive={(row) => row.factor_id === selected?.factor_id} stickyFirst minWidth="900px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.factor_id ?? 'OVERVIEW'} title={selected?.name ?? '因素详情'} subtitle={selected?.chart_indicator || '选择左侧因素'} empty={!selected && !resource.busy}>
      {#if selected}
        <div class="signal"><Badge tone={badgeTone(selected.signal)} solid={selected.signal === 'positive' || selected.signal === 'negative'}>{selected.signal_label}</Badge></div>
        <StatGrid stats={[{ label: '源序号', value: count(selected.source_rank) }, { label: '源判断', value: selected.signal_label }]} inline />
        <h4>客户端说明</h4><p>{text(selected.description)}</p>
        <h4>口径提示</h4><p>同一批因素的描述日期可能不同；页面原样保留，不把旧描述包装为当前实时结论。</p>
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .toolbar { display: flex; align-items: center; gap: var(--sp-2); flex-wrap: wrap; }
  .signal { margin-bottom: var(--sp-3); }
  h4 { margin: var(--sp-4) 0 0; font-size: var(--fs-sm); }
  p { margin-top: var(--sp-2); font-size: var(--fs-xs); line-height: 1.75; color: var(--fg-mute); }
</style>
