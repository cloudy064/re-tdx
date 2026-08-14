<script lang="ts">
  import { queryString } from '../../../api';
  import { date, delta, fixed, money, price } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { EquityPerformanceRecord, MarketEquityPerformanceDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';
  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketEquityPerformanceDocument>();
  const rows = $derived(resource.data?.records ?? []);
  function load(refresh = false) {
    void resource.load(`/api/v1/market/equity-performance?${queryString({ market, code, limit: 1, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  const columns: Column<EquityPerformanceRecord>[] = [
    { key: 'date', label: '行情日 / 收盘', width: '155px', value: (row) => date(row.quote_date), sub: (row) => price(row.close) },
    { key: 'r5', label: '5日 / 20日', align: 'right', num: true, value: (row) => delta(row.return_5d_pct), sub: (row) => delta(row.return_20d_pct) },
    { key: 'r60', label: '60日 / 本月', align: 'right', num: true, value: (row) => delta(row.return_60d_pct), sub: (row) => delta(row.month_to_date_pct) },
    { key: 'ytd', label: '年初至今', align: 'right', num: true, value: (row) => delta(row.year_to_date_pct) },
    { key: 'turnover', label: '当日 / 5日成交额', align: 'right', num: true, value: (row) => money(row.daily_turnover_yuan), sub: (row) => money(row.five_day_turnover_yuan) },
    { key: 'pe', label: 'PE', align: 'right', num: true, value: (row) => fixed(row.pe) }
  ];
  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 多周期行情表现`} eyebrow="AGHQ · PUBLIC SNAPSHOT" subtitle="5/20/60 日、本月、年初至今与成交额" busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="该证券不在当前 A 股表现快照中。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.record_id} minWidth="900px" />
</Panel>
