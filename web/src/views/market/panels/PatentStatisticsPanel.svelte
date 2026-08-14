<script lang="ts">
  import { queryString } from '../../../api';
  import { count, date } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { MarketPatentStatisticsDocument, PatentStatisticsRecord } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketPatentStatisticsDocument>();
  const rows = $derived(resource.data?.records ?? []);
  function load(refresh = false) {
    void resource.load(`/api/v1/market/patent-statistics?${queryString({ market, code, limit: 10, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  const columns: Column<PatentStatisticsRecord>[] = [
    { key: 'date', label: '截止日期', width: '110px', num: true, value: (row) => date(row.report_date) },
    { key: 'applications', label: '本期申请', width: '135px', num: true, value: (row) => count(row.period_application_total), sub: (row) => `发明 ${count(row.period_application_invention)}` },
    { key: 'grants', label: '本期授权', width: '135px', num: true, value: (row) => count(row.period_grant_total), sub: (row) => `发明 ${count(row.period_grant_invention)}` },
    { key: 'cumulative', label: '累计授权', width: '150px', num: true, value: (row) => count(row.cumulative_grant_total), sub: (row) => `发明 ${count(row.cumulative_grant_invention)}` },
    { key: 'delta', label: '其他类别/调整', width: '145px', num: true, value: (row) => count(row.cumulative_grant_total_delta) }
  ];
  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 专利统计`} eyebrow="GSZL · PUBLIC PATENTS" subtitle="本期申请、授权与累计授权数量" busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="该证券不在当前专利统计表中。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => `${row.security.security_id}:${row.report_date}`} minWidth="760px" />
</Panel>
