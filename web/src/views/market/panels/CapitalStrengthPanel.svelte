<script lang="ts">
  /** 单票反查五周期 QSZJ DDX 前百榜命中。 */
  import { queryString } from '../../../api';
  import { compact, count, date, fixed, percent, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid from '../../../ui/StatGrid.svelte';
  import type { CapitalStrengthRecord, MarketCapitalStrengthDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketCapitalStrengthDocument>();
  const doc = $derived(resource.data);
  const rows = $derived((doc?.records ?? []) as CapitalStrengthRecord[]);
  const averageDdx = $derived(rows.length ? rows.reduce((sum, row) => sum + (row.ddx_float_share_pct ?? 0), 0) / rows.length : null);
  const maximumDdx = $derived(rows.length ? Math.max(...rows.map((row) => row.ddx_float_share_pct ?? Number.NEGATIVE_INFINITY)) : null);

  function load(refresh = false) {
    void resource.load(`/api/v1/market/capital-strength?${queryString({
      view: 'security', market, code, sort: 'period', order: 'asc', limit: 10,
      refresh: refresh ? 1 : 0
    })}`);
  }

  const columns: Column<CapitalStrengthRecord>[] = [
    { key: 'period', label: '周期', width: '82px', value: (row) => row.period_label },
    { key: 'rank', label: '源排名', width: '76px', align: 'right', num: true, value: (row) => count(row.source_rank), sortValue: (row) => row.source_rank },
    { key: 'ddx', label: 'DDX', width: '96px', align: 'right', num: true, value: (row) => `${fixed(row.ddx_float_share_pct, 4)}%`, tone: (row) => tone(row.ddx_float_share_pct), sortValue: (row) => row.ddx_float_share_pct ?? 0 },
    { key: 'return', label: '周期涨幅', width: '96px', align: 'right', num: true, value: (row) => percent(row.period_return_pct), tone: (row) => tone(row.period_return_pct), sortValue: (row) => row.period_return_pct ?? 0 },
    { key: 'main', label: '主力净流入', width: '120px', align: 'right', num: true, value: (row) => compact(row.main_net_inflow_yuan, '元'), tone: (row) => tone(row.main_net_inflow_yuan), sortValue: (row) => row.main_net_inflow_yuan ?? 0 },
    { key: 'total', label: '总净流入', width: '120px', align: 'right', num: true, value: (row) => compact(row.total_net_inflow_yuan, '元'), tone: (row) => tone(row.total_net_inflow_yuan), sortValue: (row) => row.total_net_inflow_yuan ?? 0 },
    { key: 'float', label: '流通股本', width: '110px', align: 'right', num: true, value: (row) => compact(row.float_shares, '股') },
    { key: 'date', label: '统计日期', width: '92px', num: true, value: (row) => date(row.statistics_date) }
  ];

  $effect(() => {
    market; code;
    load();
  });
</script>

<div class="stack">
  <Panel title={`${name} · DDX 资金强势`} subtitle="反查五个周期的前100榜；未命中是正常关系" busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="当前证券未进入 5日、10日、20日、30日或近3月 DDX 前100榜。">
    {#snippet toolbar()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新五表</Button>{/snippet}
    {#if rows.length}
      <StatGrid inline stats={[
        { label: '命中周期', value: `${count(rows.length)} / 5`, note: rows.map((row) => row.period_label).join('、') },
        { label: '平均 DDX', value: `${fixed(averageDdx, 4)}%` },
        { label: '最高 DDX', value: `${fixed(maximumDdx, 4)}%` },
        { label: '最新统计日', value: date(rows[rows.length - 1]?.statistics_date), note: 'DDX 与金额方向不强制相同' }
      ]} />
    {/if}
  </Panel>

  <Panel title="周期命中明细" subtitle={`${count(rows.length)} 条真实榜单记录`} empty={!resource.busy && Boolean(doc) && rows.length === 0} emptyText="暂无周期命中。" flush scroll fill>
    <DataTable {columns} {rows} rowKey={(row) => row.period} sortKey="period" />
  </Panel>
</div>

<style>
  .stack { display: flex; min-height: 0; flex: 1; flex-direction: column; gap: var(--sp-2); overflow: auto; }
</style>
