<script lang="ts">
  /** 通达信 ZXJX/SJQD4/JYFX：公告精选、风险提示与单票公告历史。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, percent, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type { AnnouncementSignalRecord, MarketAnnouncementSignalsDocument } from '../../types';

  const SECTIONS = [
    { id: 'selected', label: '公告精选' },
    { id: 'risks', label: '风险提示' }
  ];
  const DIRECTIONS = [
    { id: 'all', label: '全部多空' },
    { id: 'bullish', label: '利好' },
    { id: 'bearish', label: '利空' }
  ];

  let section = $state<'selected' | 'risks'>('selected');
  let direction = $state('all');
  let query = $state('');
  let selected = $state<AnnouncementSignalRecord | null>(null);
  const masterResource = new Resource<MarketAnnouncementSignalsDocument>();
  const historyResource = new Resource<MarketAnnouncementSignalsDocument>();
  const doc = $derived(masterResource.data);
  const rows = $derived(doc?.records ?? []);
  const history = $derived(historyResource.data?.records ?? []);

  const stats = $derived.by<Stat[]>(() => [
    { label: section === 'selected' ? '精选公告' : '风险公告', value: count(doc?.summary.rows), note: `${date(doc?.summary.first_date)} 至 ${date(doc?.summary.latest_date)}` },
    { label: '覆盖股票', value: count(doc?.summary.unique_securities) },
    { label: '利好', value: count(doc?.summary.bullish) },
    { label: '利空', value: count(doc?.summary.bearish) },
    { label: '公告类型', value: count(doc?.summary.announcement_types) },
    { label: 'PDF 可用', value: `${count(doc?.summary.pdf_links)} / ${count(doc?.summary.rows)}` }
  ]);

  async function load(refresh = false) {
    historyResource.reset();
    const result = await masterResource.load(`/api/v1/market/announcement-signals?${queryString({
      view: section, direction, q: query.trim(), sort: 'date', order: 'desc',
      limit: 1000, refresh: refresh ? 1 : 0
    })}`);
    const next = result?.records ?? [];
    selected = next[0] ?? null;
    if (selected) void loadHistory(selected, refresh);
  }

  function switchSection(value: string) {
    section = value as typeof section;
    direction = section === 'risks' ? 'bearish' : 'all';
    query = '';
    void load();
  }

  function loadHistory(row: AnnouncementSignalRecord, refresh = false) {
    selected = row;
    void historyResource.load(`/api/v1/market/announcement-signals?${queryString({
      view: 'history', market: row.security.market, code: row.security.code,
      sort: 'date', order: 'desc', limit: 200, refresh: refresh ? 1 : 0
    })}`);
  }

  function openSecurity() {
    if (!selected) return;
    const security = selected.security;
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code, 'announcement-signals'));
  }

  const mainColumns: Column<AnnouncementSignalRecord>[] = [
    { key: 'security', label: '股票', width: '122px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'date', label: '日期', width: '88px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'direction', label: '多空', width: '58px', slot: true },
    { key: 'type', label: '公告类型', width: '146px', wrap: true, value: (row) => row.announcement_type },
    { key: 'title', label: '公告标题', width: '360px', wrap: true, value: (row) => row.title },
    { key: '3d', label: '近 3 日', width: '74px', align: 'right', num: true, value: (row) => percent(row.recent_3d_return_pct), tone: (row) => tone(row.recent_3d_return_pct), sortValue: (row) => row.recent_3d_return_pct ?? -9999 },
    { key: '10d', label: '近 10 日', width: '76px', align: 'right', num: true, value: (row) => percent(row.recent_10d_return_pct), tone: (row) => tone(row.recent_10d_return_pct), sortValue: (row) => row.recent_10d_return_pct ?? -9999 }
  ];
  const historyColumns: Column<AnnouncementSignalRecord>[] = [
    { key: 'date', label: '日期', width: '88px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'direction', label: '多空', width: '58px', slot: true },
    { key: 'type', label: '类型', width: '136px', wrap: true, value: (row) => row.announcement_type },
    { key: 'title', label: '历史公告', width: '340px', wrap: true, value: (row) => row.title },
    { key: 'pre', label: '公告前 3 日', width: '88px', align: 'right', num: true, value: (row) => percent(row.pre_3d_return_pct), tone: (row) => tone(row.pre_3d_return_pct), sortValue: (row) => row.pre_3d_return_pct ?? -9999 },
    { key: 'post', label: '公告后 3 日', width: '88px', align: 'right', num: true, value: (row) => percent(row.post_3d_return_pct), tone: (row) => tone(row.post_3d_return_pct), sortValue: (row) => row.post_3d_return_pct ?? -9999 }
  ];

  onMount(() => void load());
</script>

<PageHeader eyebrow="709/1721 · ZXJX101—103" title="公告精选与风险提示" description="还原通达信公告精选：主榜多空信号、公告原文 PDF、近期表现，以及单票公告前后 3 日表现历史。" {stats}>
  {#snippet actions()}
    <Segmented options={SECTIONS} value={section} onChange={switchSection} ariaLabel="公告信号分区" />
    <Button icon="refresh" busy={masterResource.busy} onclick={() => void load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<div class="workspace">
  <Panel title={section === 'selected' ? '公告精选主表' : '风险提示公告'} subtitle={`${count(doc?.counts.matched)} 条 · 点击查看该票历史`} busy={masterResource.busy} error={masterResource.error} onRetry={() => void load()} empty={masterResource.loaded && rows.length === 0} emptyText="当前筛选没有公告信号。" flush scroll fill>
    {#snippet toolbar()}
      <Select options={DIRECTIONS} bind:value={direction} width="126px" label="多空" onChange={() => void load()} />
      <TextInput bind:value={query} icon="search" width="240px" label="检索" placeholder="股票 / 标题 / 公告类型" onEnter={() => void load()} />
      <Button icon="search" onclick={() => void load()}>查询</Button>
    {/snippet}
    <DataTable columns={mainColumns} rows={rows} rowKey={(row) => row.signal_id} onRowClick={(row) => loadHistory(row)} isActive={(row) => row.signal_id === selected?.signal_id} stickyFirst sortKey="date" minWidth="1020px">
      {#snippet cell({ row })}<Badge tone={row.direction === 'bullish' ? 'up' : row.direction === 'bearish' ? 'down' : 'neutral'}>{row.direction_raw || '未知'}</Badge>{/snippet}
    </DataTable>
  </Panel>

  <div class="detail">
    <Panel title={selected ? `${selected.security.name || selected.security.code} · 公告原文` : '公告原文'} subtitle={selected ? `${date(selected.date)} · ${selected.announcement_type}` : '从左侧选择一条公告'} empty={!selected} emptyText="选择公告后可打开原始 PDF，并查看该票历史。">
      {#snippet actions()}
        <Button disabled={!selected} onclick={openSecurity}>打开个股</Button>
        {#if selected?.pdf_url}<a class="pdf-link" href={selected.pdf_url} target="_blank" rel="noreferrer">打开 PDF</a>{/if}
      {/snippet}
      {#if selected}<p class="headline">{selected.title}</p>{/if}
    </Panel>
    <Panel title="个股公告精选历史" subtitle="这里的收益口径是公告前 / 后 3 日；空值表示尚未形成后验区间" busy={historyResource.busy} error={historyResource.error} onRetry={selected ? () => loadHistory(selected!, true) : undefined} empty={!selected || (historyResource.loaded && history.length === 0)} emptyText="该证券暂没有公告精选历史；这不代表接口错误。" flush scroll fill>
      <DataTable columns={historyColumns} rows={history} rowKey={(row) => row.signal_id} sortKey="date" minWidth="850px">
        {#snippet cell({ row })}<Badge tone={row.direction === 'bullish' ? 'up' : row.direction === 'bearish' ? 'down' : 'neutral'}>{row.direction_raw || '未知'}</Badge>{/snippet}
      </DataTable>
    </Panel>
  </div>
</div>

<style>
  .workspace { display: grid; min-height: 0; flex: 1; grid-template-columns: minmax(650px, 1.2fr) minmax(440px, .8fr); gap: var(--sp-2); }
  .detail { display: flex; min-height: 0; flex-direction: column; gap: var(--sp-2); overflow: hidden; }
  .detail > :last-child { min-height: 0; flex: 1; }
  .headline { margin: 0; color: var(--fg); font-size: var(--fs-body); line-height: 1.65; }
  .pdf-link { display: inline-flex; align-items: center; height: var(--h-control); padding: 0 var(--sp-3); color: var(--on-solid); font-size: var(--fs-micro); text-decoration: none; background: var(--focus); border: 1px solid var(--focus); border-radius: var(--radius); }
  @media (max-width: 1120px) { .workspace { grid-template-columns: 1fr; grid-template-rows: minmax(420px, 1fr) minmax(360px, 1fr); } }
</style>
