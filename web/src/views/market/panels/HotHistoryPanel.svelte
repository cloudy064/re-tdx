<script lang="ts">
  /** 当前证券的本地 K 线热点区间；不触发网络请求或行情图层。 */
  import { queryString } from '../../../api';
  import { count, date, delta, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { HotHistoryRecord, MarketHotHistoryDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketHotHistoryDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);

  function load() {
    void resource.load(`/api/v1/market/hot-history?${queryString({
      market,
      code,
      sort: 'start-date',
      order: 'desc',
      offset: 0,
      limit: 500
    })}`);
  }

  const columns: Column<HotHistoryRecord>[] = [
    {
      key: 'range', label: '热点区间', width: '176px', num: true,
      value: (row) => `${date(row.start_date)} → ${date(row.end_date)}`,
      sortValue: (row) => row.start_date
    },
    { key: 'days', label: '交易日', width: '68px', align: 'right', num: true, value: (row) => count(row.trading_days), sortValue: (row) => row.trading_days },
    { key: 'theme', label: '主题', width: '190px', wrap: true, value: (row) => row.theme || '—', sortValue: (row) => row.theme },
    { key: 'return', label: '区间收益', width: '90px', align: 'right', num: true, value: (row) => delta(row.interval_return_pct), tone: (row) => tone(row.interval_return_pct), sortValue: (row) => row.interval_return_pct },
    { key: 'peak', label: '峰值收益', width: '90px', align: 'right', num: true, value: (row) => delta(row.peak_return_pct), tone: (row) => tone(row.peak_return_pct), sortValue: (row) => row.peak_return_pct },
    // DataTable 使用普通文本插值；即使分析中包含分隔符，也不解释成 HTML。
    { key: 'analysis', label: '完整分析', width: '460px', wrap: true, value: (row) => row.analysis || '—' }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  eyebrow="speczshot.txt · 本地只读"
  title={`${name} · K 线历史热点`}
  subtitle={doc
    ? `${count(doc.match_count)} 个历史区间 · ${date(doc.summary.earliest_start_date)} 至 ${date(doc.summary.latest_end_date)} · 最大区间 / 峰值 ${delta(doc.summary.maximum_interval_return_pct)} / ${delta(doc.summary.maximum_peak_return_pct)}`
    : '固定按当前市场与代码反查；未命中是正常关系'}
  busy={resource.busy}
  error={resource.error}
  onRetry={load}
  empty={resource.loaded && rows.length === 0}
  emptyText="当前证券没有进入通达信本地 K 线历史热点区间。"
  flush
  scroll
  fill
>
  {#snippet actions()}
    <Button icon="refresh" busy={resource.busy} onclick={load}>重新读取</Button>
  {/snippet}
  <DataTable
    {columns}
    {rows}
    minWidth="1050px"
    rowKey={(row) => `${row.start_date}:${row.end_date}:${row.source_line}`}
    sortKey="range"
  />
</Panel>
