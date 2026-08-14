<script lang="ts">
  /** 单票当前公告精选/风险提示及 ggjx 历史。 */
  import { queryString } from '../../../api';
  import { count, date, percent, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import { router } from '../../../lib/router.svelte';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid from '../../../ui/StatGrid.svelte';
  import type { AnnouncementSignalRecord, MarketAnnouncementSignalsDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketAnnouncementSignalsDocument>();
  const rows = $derived(resource.data?.records ?? []);
  const current = $derived(rows.filter((row) => row.record_kind !== 'history'));
  const history = $derived(rows.filter((row) => row.record_kind === 'history'));

  function load(refresh = false) {
    void resource.load(`/api/v1/market/announcement-signals?${queryString({
      view: 'security', market, code, include_history: 1,
      sort: 'date', order: 'desc', limit: 300, refresh: refresh ? 1 : 0
    })}`);
  }

  const currentColumns: Column<AnnouncementSignalRecord>[] = [
    { key: 'date', label: '日期', width: '88px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'source', label: '来源', width: '74px', value: (row) => row.record_kind === 'risk' ? '风险提示' : '公告精选' },
    { key: 'direction', label: '多空', width: '58px', slot: true },
    { key: 'type', label: '公告类型', width: '150px', wrap: true, value: (row) => row.announcement_type },
    { key: 'title', label: '公告标题', width: '430px', wrap: true, slot: true },
    { key: '3d', label: '近 3 日', width: '76px', align: 'right', num: true, value: (row) => percent(row.recent_3d_return_pct), tone: (row) => tone(row.recent_3d_return_pct) },
    { key: '10d', label: '近 10 日', width: '78px', align: 'right', num: true, value: (row) => percent(row.recent_10d_return_pct), tone: (row) => tone(row.recent_10d_return_pct) }
  ];
  const historyColumns: Column<AnnouncementSignalRecord>[] = [
    { key: 'date', label: '日期', width: '88px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'direction', label: '多空', width: '58px', slot: true },
    { key: 'type', label: '公告类型', width: '150px', wrap: true, value: (row) => row.announcement_type },
    { key: 'title', label: '历史公告', width: '430px', wrap: true, slot: true },
    { key: 'pre', label: '公告前 3 日', width: '90px', align: 'right', num: true, value: (row) => percent(row.pre_3d_return_pct), tone: (row) => tone(row.pre_3d_return_pct) },
    { key: 'post', label: '公告后 3 日', width: '90px', align: 'right', num: true, value: (row) => percent(row.post_3d_return_pct), tone: (row) => tone(row.post_3d_return_pct) }
  ];

  $effect(() => { market; code; void load(); });
</script>

<div class="stack">
  <Panel title={`${name} · 公告信号`} subtitle="当前未入选主榜是正常状态；个股历史仍会独立查询" busy={resource.busy} error={resource.error} onRetry={() => load()}>
    {#snippet actions()}
      <Button onclick={() => router.go('/data/announcement-signals')}>打开公告中心</Button>
      <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新</Button>
    {/snippet}
    <StatGrid inline stats={[
      { label: '当前精选 / 风险', value: count(current.length) },
      { label: '历史公告', value: count(history.length) },
      { label: '历史利好', value: count(history.filter((row) => row.direction === 'bullish').length) },
      { label: '历史利空', value: count(history.filter((row) => row.direction === 'bearish').length) },
      { label: 'PDF 原文', value: count(rows.filter((row) => row.pdf_url).length) },
      { label: '来源', value: count(resource.data?.counts.sources) }
    ]} />
  </Panel>

  <Panel title="当前公告信号" subtitle="近 3/10 日为主榜近期行情口径" busy={resource.busy} empty={resource.loaded && current.length === 0} emptyText="当前证券不在公告精选或风险提示主榜中。" flush scroll>
    <DataTable columns={currentColumns} rows={current} rowKey={(row) => row.signal_id} sortKey="date" minWidth="1120px">
      {#snippet cell({ row, column })}
        {#if column.key === 'direction'}<Badge tone={row.direction === 'bullish' ? 'up' : row.direction === 'bearish' ? 'down' : 'neutral'}>{row.direction_raw}</Badge>
        {:else if row.pdf_url}<a href={row.pdf_url} target="_blank" rel="noreferrer">{row.title}</a>{:else}{row.title}{/if}
      {/snippet}
    </DataTable>
  </Panel>

  <Panel title="公告精选历史" subtitle="公告前/后 3 日口径；最近公告的后验收益可能为空" busy={resource.busy} empty={resource.loaded && history.length === 0} emptyText="该证券暂没有公告精选历史。" flush scroll fill>
    <DataTable columns={historyColumns} rows={history} rowKey={(row) => row.signal_id} sortKey="date" minWidth="1040px">
      {#snippet cell({ row, column })}
        {#if column.key === 'direction'}<Badge tone={row.direction === 'bullish' ? 'up' : row.direction === 'bearish' ? 'down' : 'neutral'}>{row.direction_raw}</Badge>
        {:else if row.pdf_url}<a href={row.pdf_url} target="_blank" rel="noreferrer">{row.title}</a>{:else}{row.title}{/if}
      {/snippet}
    </DataTable>
  </Panel>
</div>

<style>
  .stack { display: flex; min-height: 0; flex: 1; flex-direction: column; gap: var(--sp-2); overflow: hidden; }
  .stack > :last-child { min-height: 0; flex: 1; }
  a { color: var(--focus); text-decoration: none; }
  a:hover { text-decoration: underline; }
</style>
