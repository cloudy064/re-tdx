<script lang="ts">
  import { queryString } from '../../../api';
  import { date, delta, money, percent } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { MarketRecentWatchDocument, RecentWatchRecord } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';
  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketRecentWatchDocument>(); const rows = $derived(resource.data?.records ?? []);
  function load(refresh = false) { void resource.load(`/api/v1/market/recent-watch?${queryString({ market, code, limit: 100, include_raw: 0, refresh: refresh ? 1 : 0 })}`); }
  function primary(row: RecentWatchRecord) {
    if (row.kind === 'foreign-business') return `境外收入 ${money(row.foreign_revenue_yuan)} · ${percent(row.foreign_revenue_pct)}`;
    if (row.kind === 'st-turnaround') return `预告利润 ${money(row.profit_lower_yuan)} ～ ${money(row.profit_upper_yuan)}`;
    return `${row.forecast_type || '业绩预告'} · 同比 ${delta(row.forecast_profit_yoy_pct)}`;
  }
  const columns: Column<RecentWatchRecord>[] = [
    { key: 'kind', label: '类型', width: '145px', value: (row) => row.kind_label, sub: (row) => row.source_resource },
    { key: 'date', label: '日期', width: '105px', num: true, value: (row) => date(row.event_date) },
    { key: 'primary', label: '核心数据', width: '430px', value: primary },
    { key: 'return', label: '公告至今 / 影响', width: '220px', value: (row) => row.effect || delta(row.return_since_announcement_pct) }
  ];
  $effect(() => { void market; void code; load(); });
</script>
<Panel title={`${name} · 近期关注`} eyebrow="JQGZ · PUBLIC WATCHLIST" subtitle="绩价背离、涉外经营与 ST 业绩预盈" busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="该证券不在当前三类关注表中。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.event_id} minWidth="900px" />
</Panel>
