<script lang="ts">
  /** 个股反查 GQGG 四类分组、控制人逻辑与重组预期。 */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, percent, price, text, tone } from '../../../lib/fmt';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    MarketStateOwnedReformDocument,
    StateOwnedReformDetail,
    StateOwnedReformGroup,
    StateOwnedRestructuring
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();
  const reform = new Resource<MarketStateOwnedReformDocument>();
  const doc = $derived(reform.data);

  function load(refresh = false) {
    void reform.load(`/api/v1/market/state-owned-reform?${queryString({
      view: 'security', market, code, include_details: 1, include_quotes: 1,
      refresh: refresh ? 1 : 0
    })}`);
  }

  const stats = $derived.by<Stat[]>(() => [
    { label: '改革分组', value: count(doc?.groups.length), note: '行业 / 地区 / 整合 / 公司系' },
    { label: '逻辑明细', value: count(doc?.details.length), note: `${count(doc?.errors.length)} 个动态源暂不可用` },
    { label: '重组预期', value: count(doc?.restructuring.length), note: '独立资料表命中' }
  ]);

  const groupColumns: Column<StateOwnedReformGroup>[] = [
    { key: 'dimension', label: '分类', width: '88px', value: (row) => row.dimension_label },
    { key: 'name', label: '细分组', width: '180px', value: (row) => row.name, sub: (row) => row.group_id },
    { key: 'members', label: '组内证券', width: '88px', align: 'right', num: true, value: (row) => count(row.member_count) }
  ];

  const detailColumns: Column<StateOwnedReformDetail>[] = [
    { key: 'group', label: '细分组', width: '145px', value: (row) => row.group?.name ?? '国企改革', sub: (row) => row.group?.dimension_label ?? '' },
    { key: 'controller', label: '实际控制人', width: '185px', wrap: true, value: (row) => text(row.actual_controller), sub: (row) => `控股 ${percent(row.controlling_stake_pct)}` },
    { key: 'price', label: '现价 / 涨幅', width: '86px', align: 'right', num: true, value: (row) => price(row.last_price), sub: (row) => percent(row.change_pct), tone: (row) => tone(row.change_pct) },
    { key: '3d', label: '近3日', width: '72px', align: 'right', num: true, value: (row) => percent(row.returns_pct['3d']), tone: (row) => tone(row.returns_pct['3d']) },
    { key: '20d', label: '近20日', width: '72px', align: 'right', num: true, value: (row) => percent(row.returns_pct['20d']), tone: (row) => tone(row.returns_pct['20d']) },
    { key: 'logic', label: '投资逻辑', wrap: true, value: (row) => text(row.logic) }
  ];

  const restructuringColumns: Column<StateOwnedRestructuring>[] = [
    { key: 'date', label: '资料日', width: '88px', num: true, value: (row) => date(row.as_of_date) },
    { key: 'operation', label: '资本运作', width: '105px', value: (row) => text(row.capital_operation) },
    { key: 'controller', label: '实际控制人', width: '145px', wrap: true, value: (row) => text(row.actual_controller), sub: (row) => `控股 ${percent(row.controlling_stake_pct)}` },
    { key: 'profit', label: '净利润', width: '95px', align: 'right', num: true, value: (row) => row.net_profit_10k_yuan == null ? '—' : compact(row.net_profit_10k_yuan * 10000, '元') },
    { key: 'explanation', label: '重组与改革说明', wrap: true, value: (row) => text(row.explanation) }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="国企改革关系"
  eyebrow="GQGG · 25701—25704"
  subtitle="四类细分归属、实际控制人、控股比例与投资逻辑；不同于宽泛的国企改革指数"
  busy={reform.busy}
  error={reform.error}
  onRetry={load}
  empty={reform.loaded && !reform.busy && (doc?.counts.matched ?? 0) === 0}
  emptyText="这只股票未进入当前国企改革分组或重组预期表"
  flush
  scroll
>
  {#snippet actions()}
    {#if doc?.errors.length}<Badge tone="warn">{doc.errors.length} 个细分详情暂缺</Badge>{/if}
    <Button icon="refresh" busy={reform.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}
  {#if doc && doc.counts.matched}
    <div class="pad"><StatGrid stats={stats} columns={3} /></div>
    <DataTable columns={groupColumns} rows={doc.groups} rowKey={(row) => row.group_id} maxHeight="190px" />
    {#if doc.details.length}
      <DataTable columns={detailColumns} rows={doc.details} rowKey={(row, index) => `${row.group?.group_id ?? 'detail'}-${index}`} minWidth="830px" maxHeight="310px" />
    {/if}
  {/if}
</Panel>

{#if doc?.restructuring.length}
  <Panel title="重组预期" eyebrow="GQGG · UNIT 6" subtitle="客户端独立资料表命中，不等同于已公告或必然实施" flush scroll>
    <DataTable columns={restructuringColumns} rows={doc.restructuring} rowKey={(row) => row.security.security_id} minWidth="780px" />
  </Panel>
{/if}

<style>
  .pad { padding: var(--sp-3); border-bottom: 1px solid var(--line); }
</style>
