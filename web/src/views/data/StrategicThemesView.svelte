<script lang="ts">
  /** 26 个主题投资大类 → 内部主题 → 股票与逐股入选逻辑。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, text } from '../../lib/fmt';
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
    MarketStrategicThemesDocument,
    StrategicThemeDetail,
    StrategicThemeRecord,
    ValuationSecurity
  } from '../../types';

  interface MemberRow {
    security: ValuationSecurity;
    logic: string;
    description: string;
  }

  let category = $state('');
  let query = $state('');
  const listing = new Resource<MarketStrategicThemesDocument>();
  const detail = new Resource<MarketStrategicThemesDocument>();
  const doc = $derived(listing.data);
  const selected = $derived(detail.data?.selected_theme ?? null);
  const categoryOptions = $derived([
    { id: '', label: '全部 26 大类' },
    ...(doc?.categories ?? []).map((row) => ({ id: row.block_id, label: `${row.name} · ${row.theme_count}` }))
  ]);
  const memberRows = $derived.by<MemberRow[]>(() => {
    if (detail.data?.details.length) {
      return detail.data.details.map((row: StrategicThemeDetail) => ({
        security: row.security,
        logic: row.logic,
        description: row.description
      }));
    }
    return (detail.data?.members ?? []).map((security) => ({ security, logic: '', description: '' }));
  });

  const stats = $derived.by<Stat[]>(() => [
    { label: '主题大类', value: count(doc?.summary.category_count), note: '客户端主题投资树' },
    { label: '唯一主题', value: count(doc?.summary.theme_count), note: `${count(doc?.summary.category_theme_memberships)} 条大类关系` },
    { label: '主题—股票', value: count(doc?.summary.theme_stock_memberships), note: '同票可属于多个主题' },
    { label: '覆盖证券', value: count(doc?.summary.security_count), note: '沪 / 深 / 北' },
    { label: '主表重复', value: count(doc?.summary.duplicate_master_memberships), note: '校验后去重保留' },
    { label: '对账异常', value: count(doc?.summary.count_mismatch_count), note: 'S_NUM 与原始成员串' }
  ]);

  async function load(refresh = false) {
    const result = await listing.load(`/api/v1/market/strategic-themes?${queryString({
      view: 'catalog', category, q: query.trim(), sort: 'name', order: 'asc', limit: 1000,
      refresh: refresh ? 1 : 0
    })}`);
    detail.reset();
    if (result?.themes.length) open(result.themes[0]);
  }

  function open(theme: StrategicThemeRecord) {
    void detail.load(`/api/v1/market/strategic-themes?${queryString({
      view: 'theme', theme_id: theme.theme_id, include_detail: 1, limit: 5000
    })}`);
  }

  const themeColumns: Column<StrategicThemeRecord>[] = [
    { key: 'name', label: '主题', width: '180px', wrap: true, value: (row) => row.name, sub: (row) => `${row.theme_id} · ${row.categories.join(' / ')}` },
    { key: 'members', label: '股票', width: '72px', align: 'right', num: true, value: (row) => count(row.master_member_count), sortValue: (row) => row.master_member_count },
    { key: 'categories', label: '大类数', width: '70px', align: 'right', num: true, value: (row) => count(row.categories.length), sub: (row) => row.master_duplicate_member_count ? `去重 ${row.master_duplicate_member_count}` : '无重复' }
  ];

  const memberColumns: Column<MemberRow>[] = [
    { key: 'security', label: '证券', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'logic', label: '入选逻辑', width: '260px', wrap: true, value: (row) => text(row.logic), sub: (row) => row.logic ? 'zttzty 动态详情' : '主表成员关系' },
    { key: 'description', label: '完整说明', wrap: true, value: (row) => text(row.description) }
  ];

  onMount(() => void load());
</script>

<PageHeader
  eyebrow="STRATEGIC THEMES · 26 → THEMES → STOCKS"
  title="战略主题关系"
  description="还原通达信主题投资的三级关系，并补入军工公司系与互联网+旧主题树；选中主题时按需读取逐股入选逻辑，而不是把主题 ID 当成股票代码。"
  {stats}
>
  {#snippet actions()}<Button icon="refresh" busy={listing.busy || detail.busy} onclick={() => void load(true)}>刷新 26 张主表</Button>{/snippet}
</PageHeader>

<div class="controls">
  <Select options={categoryOptions} value={category} width="220px" label="主题大类" onChange={(next) => { category = next; void load(); }} />
  <TextInput bind:value={query} icon="search" width="260px" label="检索" placeholder="5G、机器人、光模块" onEnter={() => void load()} />
</div>

<Split asideWidth="650px">
  {#snippet main()}
    <Panel title="主题目录" subtitle={`${count(doc?.counts.matched)} 个 · 大类相同主题会合并但保留多重归属`} busy={listing.busy} error={listing.error} onRetry={() => void load()} empty={listing.loaded && (doc?.themes.length ?? 0) === 0} emptyText="当前筛选没有战略主题。" flush scroll fill>
      <DataTable numbered columns={themeColumns} rows={doc?.themes ?? []} rowKey={(row) => row.theme_id} onRowClick={open} isActive={(row) => row.theme_id === selected?.theme_id} sortKey="name" minWidth="620px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel title={selected?.name ?? '主题成分'} eyebrow={selected?.theme_id ?? 'ZTTZTY'} subtitle={selected ? `${count(selected.member_count ?? selected.master_member_count)} 只 · ${selected.detail_available ? '已取得逐股逻辑' : '当前使用主表成员'}` : '从左侧选择主题'} busy={detail.busy} error={detail.error} empty={!selected} emptyText="点击左侧主题读取成分与逐股入选逻辑。" flush scroll fill>
      <DataTable columns={memberColumns} rows={memberRows} rowKey={(row) => row.security.security_id} onRowClick={(row) => router.go(stockPath(row.security.market, row.security.code, 'blocks'))} minWidth="900px" />
    </Panel>
  {/snippet}
</Split>

<style>
  .controls { display: flex; align-items: center; gap: var(--sp-3); margin-bottom: var(--sp-3); }
</style>
