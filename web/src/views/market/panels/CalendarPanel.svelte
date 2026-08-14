<script lang="ts">
  import { queryString } from '../../../api';
  import { count, date, percent, text, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { CalendarRecord, MarketCalendarDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';
  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketCalendarDocument>();
  const rows = $derived(resource.data?.rows ?? []);
  const meetings = $derived(resource.data?.related_meetings ?? []);
  function load(refresh = false) { void resource.load(`/api/v1/market/calendar?${queryString({ market, code, view: 'all', refresh: refresh ? 1 : 0 })}`); }
  const columns: Column<CalendarRecord>[] = [
    { key: 'date', label: '日期', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'type', label: '类型', value: (row) => text(row.event_type || (row.kind === 'meeting' ? '热点会议' : row.kind)) },
    { key: 'title', label: '事件', wrap: true, value: (row) => row.title, sub: (row) => row.content_excerpt || row.content || row.industry },
    { key: 'return', label: '上市涨幅', align: 'right', num: true, value: (row) => percent(row.listing_return_pct), tone: (row) => tone(row.listing_return_pct) }
  ];
  $effect(() => { void market; void code; load(); });
</script>
<Panel title={`${name} · 公司与重大事件`} eyebrow="GSRL · DSJTX · XGRL" subtitle={`${count(rows.length)} 条公司事件 / 重大提醒 / 次新与板块资讯`} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="当前日历没有关联记录。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row, index) => `${row.date}:${row.event_type}:${index}`} sortKey="date" />
</Panel>
<Panel title="相关热点会议" eyebrow="CJRL · EVENT MEMBERS" subtitle={`${count(meetings.length)} 场`} busy={resource.busy} empty={!resource.busy && meetings.length === 0} emptyText="当前会议窗口没有关联到该股票。" flush scroll>
  <DataTable {columns} rows={meetings} rowKey={(row) => row.event_id} sortKey="date" />
</Panel>
