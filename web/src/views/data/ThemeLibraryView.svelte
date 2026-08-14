<script lang="ts">
  /** ZTTZ 五类来源快照 → 主题 → 股票、逐股纳入原因与主题指数。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import ThemeIndexChart from '../../charts/ThemeIndexChart.svelte';
  import { count, date, fixed, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    MarketThemeLibraryDocument,
    ThemeLibraryDetail,
    ThemeLibraryRecord,
    ValuationSecurity
  } from '../../types';

  interface MemberRow {
    security: ValuationSecurity;
    relation: string;
    leaderCount: number | null;
    reference3d: number | null;
    description: string;
  }

  let source = $state('general');
  let query = $state('');
  let selectedRecordId = $state('');
  const listing = new Resource<MarketThemeLibraryDocument>();
  const detail = new Resource<MarketThemeLibraryDocument>();
  const doc = $derived(listing.data);
  const selected = $derived(detail.data?.selected_theme ?? null);
  const sourceOptions = $derived([
    { id: 'general', label: '统一主题 · 850' },
    ...(doc?.source_options ?? [])
      .filter((row) => row.id !== 'general')
      .map((row) => ({ id: row.id, label: `${row.name} · ${row.theme_count}` })),
    { id: 'all', label: '全部来源快照 · 1102' }
  ]);
  const memberRows = $derived.by<MemberRow[]>(() => {
    if (detail.data?.details.length) {
      return detail.data.details.map((row: ThemeLibraryDetail) => ({
        security: row.security,
        relation: row.relation_strength,
        leaderCount: row.leader_count,
        reference3d: row.reference_prices['3d'],
        description: row.description
      }));
    }
    return (detail.data?.members ?? []).map((security) => ({
      security, relation: '', leaderCount: null, reference3d: null, description: ''
    }));
  });

  const stats = $derived.by<Stat[]>(() => [
    { label: '来源快照', value: count(doc?.summary.snapshot_count), note: '五张客户端主表保真' },
    { label: '唯一主题 ID', value: count(doc?.summary.unique_theme_id_count), note: '跨来源去重后' },
    { label: '当前目录', value: count(doc?.counts.matched), note: source === 'general' ? '统一主题主表' : '当前来源筛选' },
    { label: '主题—股票', value: count(doc?.summary.theme_stock_memberships), note: '来源快照口径' },
    { label: '覆盖证券', value: count(doc?.summary.security_count), note: '沪 / 深 / 北' },
    { label: '对账异常', value: count(doc?.summary.count_mismatch_count), note: 'gpsl 与原始成员串' }
  ]);

  async function load(refresh = false) {
    const result = await listing.load(`/api/v1/market/theme-library?${queryString({
      view: 'catalog', source, q: query.trim(), sort: 'created', order: 'desc', limit: 1200,
      refresh: refresh ? 1 : 0
    })}`);
    detail.reset();
    selectedRecordId = '';
    if (result?.themes.length) open(result.themes[0]);
  }

  function open(theme: ThemeLibraryRecord) {
    selectedRecordId = theme.record_id;
    void detail.load(`/api/v1/market/theme-library?${queryString({
      view: 'theme', source: theme.source, theme_id: theme.theme_id,
      include_detail: 1, include_chart: 1, limit: 5000
    })}`);
  }

  const themeColumns: Column<ThemeLibraryRecord>[] = [
    { key: 'name', label: '主题', width: '190px', wrap: true, value: (row) => row.name, sub: (row) => `${row.theme_id} · ${row.type || row.source_name}` },
    { key: 'created', label: '创建', width: '88px', num: true, value: (row) => date(row.created_date), sub: (row) => `${count(row.age_days)} 天`, sortValue: (row) => row.created_date },
    { key: 'members', label: '股票', width: '65px', align: 'right', num: true, value: (row) => count(row.member_count), sub: (row) => row.limit_up_count == null ? row.source_name : `涨停 ${row.limit_up_count}`, sortValue: (row) => row.member_count },
    { key: 'description', label: '主题说明', wrap: true, value: (row) => text(row.description) }
  ];

  const memberColumns: Column<MemberRow>[] = [
    { key: 'security', label: '证券', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'relation', label: '关联度', width: '72px', value: (row) => text(row.relation), sub: (row) => row.leaderCount == null ? '' : `领涨 ${row.leaderCount}` },
    { key: 'reference', label: '3日前复权价', width: '90px', align: 'right', num: true, value: (row) => fixed(row.reference3d, 2) },
    { key: 'description', label: '逐股纳入原因', wrap: true, value: (row) => text(row.description), sub: (row) => row.description ? 'zttz 动态详情' : '主表成员关系' }
  ];

  onMount(() => void load());
</script>

<PageHeader
  eyebrow="ZTTZ · SOURCES → THEMES → STOCKS + INDEX"
  title="统一主题库"
  description="保留通达信统一主题、区域经济、国企系、公司系与参股持股五类来源快照；主题详情按需读取逐股纳入原因和独立主题指数，不把来源重叠误判为父子关系。"
  {stats}
>
  {#snippet actions()}<Button icon="refresh" busy={listing.busy || detail.busy} onclick={() => void load(true)}>刷新五张主表</Button>{/snippet}
</PageHeader>

<div class="controls">
  <Select options={sourceOptions} value={source} width="220px" label="主题来源" onChange={(next) => { source = next; void load(); }} />
  <TextInput bind:value={query} icon="search" width="280px" label="检索" placeholder="超节点、实验猴、区域、集团" onEnter={() => void load()} />
</div>

<Split asideWidth="610px">
  {#snippet main()}
    <div class="stage">
      <Panel title={`${selected?.name ?? '主题'} · 指数走势`} eyebrow={selected?.chart_resource ?? 'ZTTZ1'} subtitle={selected ? `${date(selected.created_date)} 创建 · ${count(detail.data?.counts.chart_points)} 个指数点 · 基准 1000` : '从右侧选择主题'} busy={detail.busy} error={detail.error} onRetry={() => selected && open(selected)} empty={detail.loaded && !detail.busy && (detail.data?.chart.length ?? 0) === 0} emptyText="该主题当前没有返回指数历史。" fill>
        {#if detail.data?.chart.length}<ThemeIndexChart points={detail.data.chart} title={selected?.name ?? '主题指数'} />{/if}
      </Panel>
      <Panel title="主题成分与纳入原因" eyebrow={selected?.detail_resource ?? 'ZTTZ'} subtitle={selected ? `${count(selected.active_member_count ?? selected.member_count)} 只 · ${selected.detail_available ? '动态详情覆盖主表成员' : '使用主表成员快照'}` : ''} busy={detail.busy} empty={!selected} emptyText="点击右侧主题读取成分与逐股纳入原因。" flush scroll>
        <DataTable columns={memberColumns} rows={memberRows} rowKey={(row) => row.security.security_id} onRowClick={(row) => router.go(stockPath(row.security.market, row.security.code, 'blocks'))} minWidth="760px" />
      </Panel>
    </div>
  {/snippet}
  {#snippet aside()}
    <Panel title="主题目录" subtitle={`${count(doc?.counts.matched)} 条 · 按创建日倒序`} busy={listing.busy} error={listing.error} onRetry={() => void load()} empty={listing.loaded && (doc?.themes.length ?? 0) === 0} emptyText="当前筛选没有主题。" flush scroll fill>
      <DataTable numbered columns={themeColumns} rows={doc?.themes ?? []} rowKey={(row) => row.record_id} onRowClick={open} isActive={(row) => row.record_id === selectedRecordId} sortKey="created" minWidth="760px" />
    </Panel>
  {/snippet}
</Split>

<style>
  .controls { display: flex; align-items: center; gap: var(--sp-3); margin-bottom: var(--sp-3); }
  .stage { display: grid; grid-template-rows: minmax(230px, 0.8fr) minmax(280px, 1.2fr); gap: var(--sp-2); flex: 1; min-height: 0; }
</style>
