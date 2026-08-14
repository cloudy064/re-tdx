<script lang="ts">
  import { queryString } from '../../../api';
  import { count, date, delta, money, percent } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { MarketSpecializedMetricsDocument, SpecializedMetricsRecord } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketSpecializedMetricsDocument>();
  const rows = $derived(resource.data?.records ?? []);
  function load(refresh = false) {
    void resource.load(`/api/v1/market/specialized-metrics?${queryString({ view: 'all', market, code, limit: 10, refresh: refresh ? 1 : 0 })}`);
  }
  function primary(row: SpecializedMetricsRecord) {
    if (row.kind === 'banks') return `资本充足率 ${percent(row.capital_adequacy_ratio_pct)} · 不良 ${percent(row.nonperforming_loan_ratio_pct)}`;
    if (row.kind === 'securities') return `月营收 ${money(row.monthly_revenue_yuan)} · 同比 ${delta(row.monthly_revenue_yoy_pct)}`;
    return `内含价值 ${money(row.embedded_value_yuan)} · 核心偿付率 ${percent(row.core_solvency_adequacy_ratio_pct)}`;
  }
  function secondary(row: SpecializedMetricsRecord) {
    if (row.kind === 'banks') return `净息差 ${percent(row.net_interest_margin_ratio_pct)} · 拨备覆盖 ${percent(row.provision_coverage_ratio_pct)}`;
    if (row.kind === 'securities') return `月净利润 ${money(row.monthly_net_profit_yuan)} · 同比 ${delta(row.monthly_net_profit_yoy_pct)}`;
    return `综合偿付率 ${percent(row.combined_solvency_adequacy_ratio_pct)} · 净投资收益率 ${percent(row.net_investment_yield_ratio_pct)}`;
  }
  const columns: Column<SpecializedMetricsRecord>[] = [
    { key: 'kind', label: '专用指标', width: '150px', value: (row) => row.kind_label, sub: (row) => row.source_resource },
    { key: 'date', label: '报告期', width: '110px', num: true, value: (row) => date(row.date) },
    { key: 'primary', label: '核心指标', width: '330px', value: primary },
    { key: 'secondary', label: '质量 / 收益指标', width: '330px', value: secondary },
    { key: 'fields', label: '原始字段', align: 'right', num: true, value: (row) => count(Object.keys(row.raw).length) }
  ];
  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 金融行业专项指标`} eyebrow="HYJYFX · PUBLIC SNAPSHOT" subtitle={`${count(rows.length)} 条银行 / 券商 / 保险专用指标`} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="该证券不是当前银行、券商或保险专项表成分。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.event_id} minWidth="930px" />
</Panel>
