<script lang="ts">
  /** GQGG：国企改革分组、控制人逻辑、公司系与重组预期。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, percent, price, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    MarketStateOwnedReformDocument,
    StateOwnedReformDetail,
    StateOwnedReformGroup,
    StateOwnedRestructuring
  } from '../../types';

  type Dimension = 'industry' | 'region' | 'integration' | 'company' | 'restructuring';
  const DIMENSIONS = [
    { id: 'industry', label: '按行业' },
    { id: 'region', label: '按地区' },
    { id: 'integration', label: '整合预期' },
    { id: 'company', label: '公司系' },
    { id: 'restructuring', label: '重组预期' }
  ];

  let dimension = $state<Dimension>('integration');
  let query = $state('');
  const listing = new Resource<MarketStateOwnedReformDocument>();
  const detail = new Resource<MarketStateOwnedReformDocument>();
  const doc = $derived(listing.data);
  const selected = $derived(detail.data?.groups[0] ?? null);

  const stats = $derived.by<Stat[]>(() => [
    { label: '细分组', value: count(doc?.summary.group_count), note: '行业 / 地区 / 整合 / 公司系' },
    { label: '分组关系', value: count(doc?.summary.group_relationships), note: '同票可进入多个主题组' },
    { label: '去重证券', value: count(doc?.summary.unique_grouped_securities), note: '显著宽于本地 100 股指数' },
    { label: '重组预期', value: count(doc?.summary.restructuring_rows), note: '独立于四类分组' },
    { label: '关系对账', value: count(doc?.summary.member_count_mismatches), note: '主表声明数不一致' }
  ]);

  function load(refresh = false) {
    const params: Record<string, string | number> = dimension === 'restructuring'
      ? { view: 'restructuring', q: query.trim(), sort: 'date', order: 'desc', limit: 500, refresh: refresh ? 1 : 0 }
      : { view: 'groups', dimension, q: query.trim(), sort: 'count', order: 'desc', limit: 500, include_quotes: 0, refresh: refresh ? 1 : 0 };
    void listing.load(`/api/v1/market/state-owned-reform?${queryString(params)}`);
    detail.reset();
  }

  function switchDimension(value: string) {
    dimension = value as Dimension;
    load();
  }

  function open(group: StateOwnedReformGroup) {
    void detail.load(`/api/v1/market/state-owned-reform?${queryString({ view: 'group', group_id: group.group_id, detail_limit: 500 })}`);
  }

  const groupColumns: Column<StateOwnedReformGroup>[] = [
    { key: 'name', label: '细分组', width: '150px', value: (row) => row.name, sub: (row) => `${row.dimension_label} · ${row.group_id}` },
    { key: 'count', label: '证券数', width: '76px', align: 'right', num: true, value: (row) => count(row.member_count), sortValue: (row) => row.member_count },
    {
      key: 'members', label: '成员预览', wrap: true,
      value: (row) => (row.members ?? []).slice(0, 8).map((item) => item.name || item.code).join(' · '),
      sub: (row) => row.member_count > 8 ? `另有 ${row.member_count - 8} 只，点击展开控制人和逻辑` : '点击展开控制人和逻辑'
    }
  ];

  const detailColumns: Column<StateOwnedReformDetail>[] = [
    { key: 'security', label: '证券', width: '115px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'price', label: '现价', width: '70px', align: 'right', num: true, value: (row) => price(row.last_price), sub: (row) => percent(row.change_pct), tone: (row) => tone(row.change_pct) },
    { key: '3d', label: '近3日', width: '72px', align: 'right', num: true, value: (row) => percent(row.returns_pct['3d']), tone: (row) => tone(row.returns_pct['3d']) },
    { key: '20d', label: '近20日', width: '72px', align: 'right', num: true, value: (row) => percent(row.returns_pct['20d']), tone: (row) => tone(row.returns_pct['20d']) },
    { key: 'controller', label: '实际控制人', width: '180px', wrap: true, value: (row) => text(row.actual_controller), sub: (row) => `控股 ${percent(row.controlling_stake_pct)}` },
    { key: 'logic', label: '投资逻辑', wrap: true, value: (row) => text(row.logic) }
  ];

  const restructuringColumns: Column<StateOwnedRestructuring>[] = [
    { key: 'date', label: '资料日', width: '88px', num: true, value: (row) => date(row.as_of_date) },
    { key: 'security', label: '证券', width: '125px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'operation', label: '资本运作', width: '105px', value: (row) => text(row.capital_operation) },
    { key: 'controller', label: '实际控制人', width: '145px', wrap: true, value: (row) => text(row.actual_controller), sub: (row) => `控股 ${percent(row.controlling_stake_pct)}` },
    { key: 'profit', label: '净利润', width: '95px', align: 'right', num: true, value: (row) => row.net_profit_10k_yuan == null ? '—' : compact(row.net_profit_10k_yuan * 10000, '元') },
    { key: 'price', label: '现价 / 涨幅', width: '88px', align: 'right', num: true, value: (row) => price(row.last_price), sub: (row) => percent(row.change_pct), tone: (row) => tone(row.change_pct) },
    { key: 'explanation', label: '重组与改革说明', wrap: true, value: (row) => text(row.explanation) }
  ];

  onMount(load);
</script>

<PageHeader
  eyebrow="GQGG · 709/1721"
  title="国企改革关系"
  description="按行业、地区、整合预期和公司系查看国企股票关系，并展开实际控制人、控股比例、投资逻辑与独立重组预期；不是本地宽泛概念板块的重复副本。"
  {stats}
>
  {#snippet actions()}<Button icon="refresh" busy={listing.busy} onclick={() => load(true)}>刷新五张主表</Button>{/snippet}
</PageHeader>

<div class="controls">
  <Segmented options={DIMENSIONS} value={dimension} onChange={switchDimension} ariaLabel="国企改革分类" />
  <TextInput bind:value={query} icon="search" width="230px" label="检索" placeholder="分组、股票或控制人" onEnter={load} />
</div>

{#if dimension === 'restructuring'}
  <Panel title="重组预期" subtitle="独立 49 行资料表；空市场号按证券代码规则恢复，行情来自公开 L1" busy={listing.busy} error={listing.error} onRetry={load} empty={listing.loaded && (doc?.restructuring.length ?? 0) === 0} emptyText="当前筛选没有重组预期记录。" flush scroll fill>
    <DataTable columns={restructuringColumns} rows={doc?.restructuring ?? []} rowKey={(row) => row.security.security_id} onRowClick={(row) => router.go(stockPath(row.security.market, row.security.code, 'blocks'))} minWidth="1080px" />
  </Panel>
{:else}
  <Split asideWidth="650px">
    {#snippet main()}
      <Panel title="改革细分组" subtitle={`${DIMENSIONS.find((item) => item.id === dimension)?.label ?? ''} · 主表直接携带完整成员关系`} busy={listing.busy} error={listing.error} onRetry={load} empty={listing.loaded && (doc?.groups.length ?? 0) === 0} emptyText="当前筛选没有细分组。" flush scroll fill>
        <DataTable numbered columns={groupColumns} rows={doc?.groups ?? []} rowKey={(row) => row.group_id} onRowClick={open} isActive={(row) => row.group_id === selected?.group_id} sortKey="count" minWidth="620px" />
      </Panel>
    {/snippet}
    {#snippet aside()}
      <Panel title={selected?.name ?? '分组详情'} eyebrow="GQGG DETAIL" subtitle={selected ? `${count(selected.member_count)} 只 · 控制人与收益按需读取` : '从左侧选择细分组'} busy={detail.busy} error={detail.error} empty={!selected} emptyText="点击左侧分组读取控制人、控股比例与逻辑。" flush scroll fill>
        <DataTable columns={detailColumns} rows={detail.data?.details ?? []} rowKey={(row) => row.security.security_id} onRowClick={(row) => router.go(stockPath(row.security.market, row.security.code, 'blocks'))} minWidth="850px" />
      </Panel>
    {/snippet}
  </Split>
{/if}

<style>
  .controls { display: flex; align-items: center; gap: var(--sp-3); margin-bottom: var(--sp-3); }
</style>
