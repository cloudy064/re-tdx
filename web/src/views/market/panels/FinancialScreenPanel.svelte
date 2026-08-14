<script lang="ts">
  import { queryString } from '../../../api';
  import { date, delta, fixed, money, percent } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { FinancialScreenRecord, MarketFinancialScreenDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';
  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketFinancialScreenDocument>();
  const growthResource = new Resource<MarketFinancialScreenDocument>();
  const rows = $derived(resource.data?.records ?? []);
  const growthRows = $derived(growthResource.data?.records ?? []);
  function load(refresh = false) {
    void resource.load(`/api/v1/market/financial-screen?${queryString({ market, code, limit: 10, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
    void growthResource.load(`/api/v1/market/financial-screen?${queryString({ dataset: 'small-cap-growth', market, code, sort: 'profit-cagr', limit: 10, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  const columns: Column<FinancialScreenRecord>[] = [
    { key: 'period', label: '板块 / 报告期', width: '160px', value: (row) => row.board_label, sub: (row) => date(row.report_period) },
    { key: 'cap', label: '市值 / 股息率', align: 'right', num: true, value: (row) => money(row.market_cap_yuan), sub: (row) => percent(row.dividend_yield_pct) },
    { key: 'valuation', label: 'PE / PB / PEG', align: 'right', num: true, value: (row) => `${fixed(row.pe_ttm)} / ${fixed(row.pb_mrq)}`, sub: (row) => `PEG ${fixed(row.peg)}` },
    { key: 'growth', label: '利润 / 营收同比', align: 'right', num: true, value: (row) => delta(row.net_profit_yoy_pct), sub: (row) => delta(row.revenue_yoy_pct) },
    { key: 'quality', label: 'ROE / 毛利率', align: 'right', num: true, value: (row) => percent(row.roe_pct), sub: (row) => percent(row.gross_margin_pct) },
    { key: 'holding', label: '机构持仓', align: 'right', num: true, value: (row) => percent(row.institution_float_holding_pct) }
  ];
  const growthColumns: Column<FinancialScreenRecord>[] = [
    { key: 'period', label: '成长池 / 报告期', width: '155px', value: (row) => row.board_label, sub: (row) => date(row.report_period) },
    { key: 'profit', label: '扣非净利同比', align: 'right', num: true, value: (row) => delta(row.adjusted_net_profit_yoy_pct) },
    { key: 'profit-cagr', label: '利润三年复合', align: 'right', num: true, value: (row) => percent(row.adjusted_net_profit_cagr_3y_pct) },
    { key: 'revenue', label: '营收同比', align: 'right', num: true, value: (row) => delta(row.revenue_yoy_pct) },
    { key: 'revenue-cagr', label: '营收三年复合', align: 'right', num: true, value: (row) => percent(row.revenue_cagr_3y_pct) }
  ];
  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 财务筛选快照`} eyebrow="CWZB + XPCZ · PUBLIC CROSS-SECTION" subtitle="五板财务横截面与小盘成长股票池关联" busy={resource.busy || growthResource.busy} error={resource.error || growthResource.error} onRetry={() => load()} empty={resource.loaded && growthResource.loaded && rows.length === 0 && growthRows.length === 0} emptyText="该证券不在当前财务快照或小盘成长池中。" flush scroll>
  <div class="stacks">
    {#if rows.length}<DataTable {columns} {rows} rowKey={(row) => row.record_id} minWidth="1050px" />{/if}
    {#if growthRows.length}
      <div class="dataset-label">通达信小盘成长池</div>
      <DataTable columns={growthColumns} rows={growthRows} rowKey={(row) => row.record_id} minWidth="760px" />
    {/if}
  </div>
</Panel>

<style>
  .stacks { display: grid; gap: var(--sp-3); }
  .dataset-label { padding: 0 var(--sp-3); color: var(--fg-mute); font-size: var(--fs-micro); }
</style>
