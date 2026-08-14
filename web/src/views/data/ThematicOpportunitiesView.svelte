<script lang="ts">
  /** 一带一路行业/区域机会组、遗留客户端主题，以及 RDHS 近期爆炒复盘。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, delta, price, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Button from '../../ui/Button.svelte';
  import Badge from '../../ui/Badge.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    ActiveHypeRecord,
    CompletedHypeRecord,
    MarketThematicOpportunitiesDocument,
    ThematicOpportunityDetail,
    ThematicOpportunityGroup
  } from '../../types';

  type Mode = 'groups' | 'hype-completed' | 'hype-active';

  let mode = $state<Mode>('groups');
  let type = $state('all');
  let query = $state('');
  const listing = new Resource<MarketThematicOpportunitiesDocument>();
  const detail = new Resource<MarketThematicOpportunitiesDocument>();
  const doc = $derived(listing.data);
  const selected = $derived(detail.data?.selected_group ?? null);

  const stats = $derived.by<Stat[]>(() => [
    { label: '机会组', value: count(doc?.summary.group_count), note: `${count(doc?.summary.industry_group_count)} 行业 · ${count(doc?.summary.region_group_count)} 区域 · ${count(doc?.summary.legacy_client_theme_count)} 遗留主题` },
    { label: '组—股票', value: count(doc?.summary.group_memberships), note: '同票可属于多个组' },
    { label: '覆盖证券', value: count(doc?.summary.security_count), note: '主表成员串去重' },
    { label: '近期已爆炒', value: count(doc?.summary.completed_hype_count), note: '板块与龙头复盘' },
    { label: '当前爆炒', value: count(doc?.summary.active_hype_count), note: '个股相对上证表现' }
  ]);

  async function load(refresh = false) {
    const apiView = mode === 'groups' ? 'catalog' : mode;
    const result = await listing.load(`/api/v1/market/thematic-opportunities?${queryString({
      view: apiView,
      type: mode === 'groups' ? type : 'all',
      q: query.trim(),
      sort: 'name',
      order: mode === 'groups' ? 'asc' : 'desc',
      limit: 5000,
      refresh: refresh ? 1 : 0
    })}`);
    detail.reset();
    if (mode === 'groups' && result?.groups.length) openGroup(result.groups[0]);
  }

  function openGroup(group: ThematicOpportunityGroup) {
    void detail.load(`/api/v1/market/thematic-opportunities?${queryString({
      view: 'group', group_id: group.group_id, include_detail: 1, limit: 5000
    })}`);
  }

  function switchMode(next: string) {
    mode = next as Mode;
    void load();
  }

  const groupColumns: Column<ThematicOpportunityGroup>[] = [
    { key: 'name', label: '机会组', width: '170px', wrap: true, value: (row) => row.name, sub: (row) => `${row.group_id} · ${row.type_name}` },
    { key: 'category', label: '上级大类', width: '120px', value: (row) => text(row.category) },
    { key: 'members', label: '股票', width: '72px', align: 'right', num: true, value: (row) => count(row.member_count), sortValue: (row) => row.member_count },
    { key: 'source', label: '动态明细', width: '160px', value: (row) => row.detail_resource }
  ];

  const detailColumns: Column<ThematicOpportunityDetail>[] = [
    { key: 'security', label: '证券', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'logic', label: '投资逻辑', width: '300px', wrap: true, value: (row) => text(row.logic) },
    { key: 'description', label: '详细说明', width: '300px', wrap: true, value: (row) => text(row.description) },
    { key: 'threeDay', label: '3日参考收盘', width: '95px', align: 'right', num: true, value: (row) => price(row.reference_close_3d) },
    { key: 'fiveDay', label: '5日参考收盘', width: '95px', align: 'right', num: true, value: (row) => price(row.reference_close_5d) },
    { key: 'twentyDay', label: '20日参考收盘', width: '100px', align: 'right', num: true, value: (row) => price(row.reference_close_20d) },
    { key: 'threeMonth', label: '近三月参考收盘', width: '110px', align: 'right', num: true, value: (row) => price(row.three_month_adjusted_close) },
    { key: 'yearStart', label: '年初参考收盘', width: '110px', align: 'right', num: true, value: (row) => price(row.year_start_adjusted_close) },
    { key: 'limitUps', label: '连涨次数', width: '80px', align: 'right', num: true, value: (row) => count(row.limit_up_count) }
  ];

  const completedColumns: Column<CompletedHypeRecord>[] = [
    { key: 'block', label: '爆炒板块', width: '130px', value: (row) => row.block_name, sub: (row) => row.block_code },
    { key: 'leader', label: '龙头股', width: '130px', value: (row) => row.leader.name || row.leader.code, sub: (row) => row.leader.security_id },
    { key: 'window', label: '炒作区间', width: '180px', value: (row) => `${date(row.start_date)} → ${date(row.end_date)}` },
    { key: 'pattern', label: '区间统计', width: '100px', value: (row) => text(row.limit_pattern) },
    { key: 'return', label: '龙头涨幅', width: '90px', align: 'right', num: true, value: (row) => delta(row.interval_return_pct), tone: (row) => tone(row.interval_return_pct) },
    { key: 'analysis', label: '炒作分析', width: '420px', wrap: true, value: (row) => text(row.analysis) }
  ];

  const activeColumns: Column<ActiveHypeRecord>[] = [
    { key: 'security', label: '股票', width: '140px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'window', label: '统计区间', width: '180px', value: (row) => `${date(row.start_date)} → ${date(row.end_date)}` },
    { key: 'stat', label: '区间统计', width: '110px', value: (row) => text(row.interval_stat) },
    { key: 'stockReturn', label: '个股涨幅', width: '90px', align: 'right', num: true, value: (row) => delta(row.stock_return_pct), tone: (row) => tone(row.stock_return_pct) },
    { key: 'indexReturn', label: '上证涨幅', width: '90px', align: 'right', num: true, value: (row) => delta(row.shanghai_index_return_pct), tone: (row) => tone(row.shanghai_index_return_pct) },
    { key: 'relative', label: '相对收益', width: '90px', align: 'right', num: true, value: (row) => delta(row.relative_return_pct), tone: (row) => tone(row.relative_return_pct) }
  ];

  onMount(() => void load());
</script>

<PageHeader
  eyebrow="YDYL + RDHS · NATIVE RELATION CHAINS"
  title="主题机会与爆炒复盘"
  description="还原一带一路细分行业 / 核心区域和遗留客户端主题 → 股票 → 逐股投资逻辑，同时区分 RDHS 已爆炒板块复盘与当前正在爆炒股票；页面标签冲突与参考收盘价均按真实语义保留。"
  {stats}
>
  {#snippet actions()}<Button icon="refresh" busy={listing.busy || detail.busy} onclick={() => void load(true)}>刷新原生资源</Button>{/snippet}
</PageHeader>

<div class="controls">
  <Segmented
    options={[
      { id: 'groups', label: '行业 / 区域机会' },
      { id: 'hype-completed', label: '近期已爆炒' },
      { id: 'hype-active', label: '当前爆炒' }
    ]}
    value={mode}
    onChange={switchMode}
    ariaLabel="主题机会视图"
  />
  {#if mode === 'groups'}
    <Select
      options={[
        { id: 'all', label: '全部机会组' },
        { id: 'industry', label: '细分行业' },
        { id: 'region', label: '核心区域' },
        { id: 'legacy-client-theme', label: '遗留客户端主题' }
      ]}
      value={type}
      width="150px"
      label="分组类型"
      onChange={(next) => { type = next; void load(); }}
    />
  {/if}
  <TextInput bind:value={query} icon="search" width="260px" label="检索" placeholder="行业、区域、股票或炒作分析" onEnter={() => void load()} />
</div>

{#if mode === 'groups'}
  <Split asideWidth="690px">
    {#snippet main()}
      <Panel title="行业、区域与遗留主题机会组" subtitle={`${count(doc?.counts.matched)} 组 · 不是普通行业板块`} busy={listing.busy} error={listing.error} onRetry={() => void load()} empty={listing.loaded && (doc?.groups.length ?? 0) === 0} emptyText="当前筛选没有机会组。" flush scroll fill>
        <DataTable numbered columns={groupColumns} rows={doc?.groups ?? []} rowKey={(row) => row.group_id} onRowClick={openGroup} isActive={(row) => row.group_id === selected?.group_id} minWidth="620px" />
      </Panel>
    {/snippet}
    {#snippet aside()}
      <Panel title={selected?.name ?? '机会组成分'} eyebrow={selected?.group_id ?? 'YDYL1'} subtitle={selected ? `${count(detail.data?.counts.matched)} 只 · ${selected.type_name} · 已取得逐股逻辑` : '从左侧选择机会组'} busy={detail.busy} error={detail.error} empty={!selected} emptyText="点击左侧机会组读取股票与投资逻辑。" flush scroll fill>
        {#if selected?.semantic_mismatch}
          <div class="semantic-warning"><Badge tone="warn">页面标签冲突</Badge><span>{selected.semantic_note}</span></div>
        {/if}
        <DataTable columns={detailColumns} rows={detail.data?.details ?? []} rowKey={(row) => row.security.security_id} onRowClick={(row) => router.go(stockPath(row.security.market, row.security.code, 'blocks'))} minWidth="1380px" />
      </Panel>
    {/snippet}
  </Split>
{:else if mode === 'hype-completed'}
  <Panel title="近期已爆炒板块" subtitle={`${count(doc?.counts.matched)} 条 · 板块、龙头、区间表现与炒作分析`} busy={listing.busy} error={listing.error} onRetry={() => void load()} empty={listing.loaded && (doc?.completed_hype.length ?? 0) === 0} emptyText="当前没有已爆炒复盘记录。" flush scroll fill>
    <DataTable numbered columns={completedColumns} rows={doc?.completed_hype ?? []} rowKey={(row) => row.record_id} onRowClick={(row) => router.go(stockPath(row.leader.market, row.leader.code, 'overview'))} minWidth="1200px" />
  </Panel>
{:else}
  <Panel title="近期正在爆炒股票" subtitle={`${count(doc?.counts.matched)} 只 · 个股区间收益与上证指数对照`} busy={listing.busy} error={listing.error} onRetry={() => void load()} empty={listing.loaded && (doc?.active_hype.length ?? 0) === 0} emptyText="当前没有正在爆炒记录。" flush scroll fill>
    <DataTable numbered columns={activeColumns} rows={doc?.active_hype ?? []} rowKey={(row) => row.record_id} onRowClick={(row) => router.go(stockPath(row.security.market, row.security.code, 'overview'))} minWidth="900px" />
  </Panel>
{/if}

<style>
  .controls { display: flex; align-items: center; gap: var(--sp-3); margin-bottom: var(--sp-3); flex-wrap: wrap; }
  .semantic-warning { display: flex; align-items: center; gap: var(--sp-2); padding: var(--sp-2) var(--sp-3); color: var(--fg-dim); font-size: var(--fs-micro); border-bottom: 1px solid var(--line); }
</style>
