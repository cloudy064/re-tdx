<script lang="ts">
  import { queryString } from '../../../api';
  import { count, date, fixed, money, percent } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { FinancialInsightRecord, MarketFinancialInsightsDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';
  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketFinancialInsightsDocument>();
  const rows = $derived(resource.data?.records ?? []);
  function load(refresh = false) {
    void resource.load(`/api/v1/market/financial-insights?${queryString({ market, code, limit: 20, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  const columns: Column<FinancialInsightRecord>[] = [
    { key: 'kind', label: '线索 / 报告期', width: '190px', value: (row) => row.kind_label, sub: (row) => date(row.report_period) },
    { key: 'signal', label: '信号值', align: 'right', num: true, value: (row) => count(row.signal_value) },
    { key: 'amount', label: '金额口径', align: 'right', num: true, value: (row) => money(row.amount_value_yuan) },
    { key: 'ratio', label: '比例口径', align: 'right', num: true, value: (row) => percent(row.ratio_value_pct) },
    { key: 'pe', label: 'PE / ROE', align: 'right', num: true, value: (row) => fixed(row.pe), sub: (row) => percent(row.roe_pct) }
  ];
  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 特色财务线索`} eyebrow="15 CLIENT SCREENS" subtitle="质量、利润预警、现金流、稳健成长、利润突破与分红方案等十五类客户端筛选" busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="该证券未命中当前十五类特色财务筛选。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.record_id} minWidth="820px" />
</Panel>
