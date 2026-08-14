<script lang="ts">
  import { queryString } from '../../../api';
  import { compact, date, delta, fixed, price } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { GdrRecord, MarketGdrDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';
  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketGdrDocument>();
  const rows = $derived(resource.data?.records ?? []);
  function load(refresh = false) { void resource.load(`/api/v1/market/gdr?${queryString({ market, code, limit: 20, include_raw: 0, refresh: refresh ? 1 : 0 })}`); }
  const columns: Column<GdrRecord>[] = [
    { key: 'gdr', label: 'GDR / 市场', width: '210px', value: (row) => row.gdr_name, sub: (row) => `${row.currency} · ${row.listing_location}` },
    { key: 'price', label: 'GDR / A股', align: 'right', num: true, value: (row) => price(row.gdr_price), sub: (row) => price(row.underlying_price) },
    { key: 'premium', label: '折算价 / 溢价', align: 'right', num: true, value: (row) => price(row.converted_gdr_price), sub: (row) => delta(row.premium_pct) },
    { key: 'terms', label: '发行量 / 兑换比', align: 'right', num: true, value: (row) => compact(row.issuance_units), sub: (row) => `1 : ${fixed(row.conversion_ratio)}` },
    { key: 'date', label: '行情日', align: 'right', num: true, value: (row) => date(row.quote_date) }
  ];
  $effect(() => { void market; void code; load(); });
</script>
<Panel title={`${name} · GDR映射`} eyebrow="GDR · CROSS-MARKET" subtitle="报价、折算价、溢价与兑换条款" busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="该证券当前没有 GDR 映射。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.record_id} minWidth="900px" />
</Panel>
