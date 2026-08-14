<script lang="ts">
  import { queryString } from '../../../api';
  import { date, money, percent, text } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { CorporateOrderRecord, MarketCorporateOrdersDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';
  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketCorporateOrdersDocument>();
  const rows = $derived(resource.data?.records ?? []);
  function load(refresh = false) {
    void resource.load(`/api/v1/market/corporate-orders?${queryString({ market, code, limit: 1000, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  const columns: Column<CorporateOrderRecord>[] = [
    { key: 'date', label: '公告日', width: '92px', num: true, value: (row) => date(row.announcement_date) },
    { key: 'kind', label: '类型 / 阶段', width: '130px', value: (row) => `${row.kind_label} · ${text(row.stage)}` },
    { key: 'title', label: '事项', value: (row) => text(row.title), wrap: true },
    { key: 'amount', label: '金额', align: 'right', num: true, value: (row) => money(row.amount_yuan) },
    { key: 'share', label: '营收占比', align: 'right', num: true, value: (row) => percent(row.calculated_revenue_share_pct) },
    { key: 'link', label: '公告', width: '70px', value: (row) => row.source_url ? '有原文' : '—' }
  ];
  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 公司订单与重大合同`} eyebrow="ZB101 · ZDHT101" subtitle="招投标、中标、重大合同及营收占比" busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="该证券当前没有招投标或重大合同记录。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.record_id} minWidth="900px" />
</Panel>
