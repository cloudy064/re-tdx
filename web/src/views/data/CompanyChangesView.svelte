<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, money, percent, price } from '../../lib/fmt';
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
  import type { CompanyChangeKind, CompanyChangeRecord, MarketCompanyChangesDocument } from '../../types';

  type View = MarketCompanyChangesDocument['view'];
  const VIEWS = [
    { id: 'all', label: '全部' },
    { id: 'security-renames', label: '证券更名' },
    { id: 'company-renames', label: '公司更名' },
    { id: 'mainland-index', label: '指数调整' },
    { id: 'major-equity', label: '重大股权' },
    { id: 'controllers', label: '实控人' },
    { id: 'industries', label: '行业变更' },
    { id: 'equity-transfers', label: '股权转让' },
    { id: 'hk-index', label: '港股指数' },
    { id: 'neeq-index', label: '新三板指数' }
  ];

  let view = $state<View>('all');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketCompanyChangesDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);

  const stats = $derived.by<Stat[]>(() => doc ? [
    { label: '匹配事件', value: count(doc.match_count) },
    { label: '覆盖证券', value: count(doc.summary.unique_securities) },
    { label: '指数调整', value: count(doc.summary['mainland-index'] + doc.summary['hk-index'] + doc.summary['neeq-index']) },
    { label: '股权事项', value: count(doc.summary['major-equity'] + doc.summary['equity-transfers']) },
    { label: '最新日期', value: date(doc.summary.latest_date) }
  ] : []);

  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/company-changes?${queryString({
      view, q: query.trim(), limit: 20000, include_raw: 0, refresh: refresh ? 1 : 0
    })}`);
  }
  function switchView(next: string) { view = next as View; load(); }
  function name(row: CompanyChangeRecord) { return row.security.name || row.security.code; }
  function relation(row: CompanyChangeRecord) {
    if (row.kind === 'security-renames' || row.kind === 'company-renames') return `${row.old_name || '—'} → ${row.new_name || name(row)}`;
    if (row.kind.endsWith('index')) return `${row.direction || '调整'} · ${row.index_name || '—'}`;
    if (row.kind === 'controllers') return `${row.before_controller || '—'} → ${row.after_controller || '—'}`;
    if (row.kind === 'industries') return `${row.before_industry || '—'} → ${row.after_industry || '—'}`;
    return `${row.seller || '—'} → ${row.buyer || '—'}`;
  }
  function scale(row: CompanyChangeRecord) {
    if (row.kind === 'major-equity') return `${count(row.shares)} 股 · ${percent(row.total_share_pct)}`;
    if (row.kind === 'equity-transfers') return `${money(row.total_amount_yuan)} · ${price(row.transfer_price_yuan)}/股`;
    return row.status || row.cross_industry || '';
  }
  function canOpen(row: CompanyChangeRecord) { return ['sz', 'sh', 'bj'].includes(row.security.market); }
  function openStock() {
    if (selected && canOpen(selected)) router.go(stockPath(selected.security.market, selected.security.code, 'company-changes'));
  }

  const columns: Column<CompanyChangeRecord>[] = [
    { key: 'kind', label: '变更类型', width: '145px', value: (row) => row.kind_label, sub: (row) => row.source_resource.replace('list/func_', '').replace('_1.jsn', '') },
    { key: 'security', label: '证券', width: '155px', value: name, sub: (row) => row.security.security_id },
    { key: 'date', label: '事件日期', width: '105px', num: true, value: (row) => date(row.event_date) },
    { key: 'relation', label: '变更关系 / 指数', width: '410px', value: relation },
    { key: 'scale', label: '规模 / 状态', width: '220px', value: scale }
  ];

  const detailStats = $derived.by<Stat[]>(() => selected ? [
    { label: '事件日期', value: date(selected.event_date) },
    { label: '公告日期', value: date(selected.announcement_date) },
    { label: '占总股本', value: percent(selected.total_share_pct) },
    { label: '转让总额', value: money(selected.total_amount_yuan) }
  ] : []);

  onMount(() => load());
</script>

<PageHeader eyebrow="ZQBG · 9 LIVE SOURCES" title="证券与公司变更库" description="证券/公司更名、境内/港股/新三板指数调整、重大股权、实控人、行业和股权转让九类事实。静态源不伪造宿主实时涨幅。" {stats}>
  {#snippet actions()}
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="公司变更视图" />
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="390px">
  {#snippet main()}
    <Panel title="变更事实" subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.sources.length)} 个客户端资源` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选下没有变更记录。" flush scroll>
      {#snippet toolbar()}<TextInput bind:value={query} icon="search" width="280px" label="检索" placeholder="代码、公司、股东、指数或行业" onEnter={() => load()} />{/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedId = row.event_id)} isActive={(row) => row.event_id === selected?.event_id} stickyFirst numbered minWidth="1040px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.kind_label ?? 'COMPANY CHANGES'} title={selected ? name(selected) : '变更详情'} subtitle={selected?.source_resource ?? '选择左侧事件'} empty={!selected && !resource.busy}>
      {#if selected}
        {#if canOpen(selected)}<button type="button" onclick={openStock}>在个股工作台打开</button>{/if}
        <StatGrid stats={detailStats} inline />
        <h4>{relation(selected)}</h4>
        {#if selected.details}<p>{selected.details}</p>{/if}
        {#if selected.ownership_chain}<p>{selected.ownership_chain}</p>{/if}
        {#if selected.history}<pre>{selected.history}</pre>{/if}
        {#if selected.business_change}<p>主营变更：{selected.business_change}</p>{/if}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  button { width: 100%; height: 28px; margin-bottom: var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  h4 { margin: var(--sp-3) 0; font-size: var(--fs-sm); color: var(--fg); }
  p, pre { margin: var(--sp-2) 0 0; white-space: pre-wrap; font: inherit; font-size: var(--fs-xs); line-height: 1.7; color: var(--fg-mute); max-height: 360px; overflow: auto; }
</style>
