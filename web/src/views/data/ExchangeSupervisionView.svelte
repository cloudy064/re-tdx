<script lang="ts">
  /** JYJGFX：交易所当前监管观察期、历史区间及公告原文。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, tone } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type { ExchangeSupervisionRecord, MarketExchangeSupervisionDocument } from '../../types';

  const SECTIONS = [
    { id: 'current', label: '当前监管' },
    { id: 'history', label: '历史记录' }
  ];
  const PDF_FILTERS = [
    { id: 'all', label: '全部记录' },
    { id: 'pdf', label: '有公告原文' }
  ];

  let section = $state('current');
  let query = $state('');
  let pdfFilter = $state('all');
  const resource = new Resource<MarketExchangeSupervisionDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);

  const stats = $derived.by<Stat[]>(() => [
    { label: section === 'current' ? '当前监管证券' : '历史监管区间', value: count(doc?.summary.rows), note: `${count(doc?.summary.unique_securities)} 个证券` },
    { label: '公告原文', value: count(doc?.summary.pdf_links), note: '上游 PDF 链接' },
    { label: '行情覆盖', value: section === 'current' ? `${count(doc?.summary.quoted_rows)} / ${count(doc?.summary.rows)}` : '历史起止价', note: section === 'current' ? '公开 L1 快照' : '不混入当前行情' },
    { label: '最早开始', value: date(doc?.summary.first_start_date) },
    { label: '最晚结束', value: date(doc?.summary.latest_end_date) },
    { label: '数据状态', value: doc?.availability === 'records-only' ? '仅监管表' : doc?.availability === 'live' ? '在线' : doc?.availability ?? '—', note: `${count(doc?.counts.warnings)} 条上游警告` }
  ]);

  async function load(refresh = false) {
    await resource.load(`/api/v1/market/exchange-supervision?${queryString({
      view: section, q: query.trim(), pdf_only: pdfFilter === 'pdf' ? 1 : 0,
      sort: section === 'current' ? 'end-date' : 'end-date', order: 'desc', limit: 500,
      quote_cache_ttl_seconds: 5, refresh: refresh ? 1 : 0
    })}`);
  }

  function switchSection(value: string) {
    section = value;
    void load();
  }

  function openSecurity(row: ExchangeSupervisionRecord) {
    router.go(stockPath(row.security.market, row.security.code, 'intelligence'));
  }

  const columns: Column<ExchangeSupervisionRecord>[] = [
    { key: 'security', label: '证券', width: '135px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'kind', label: '状态', width: '72px', value: (row) => row.record_kind === 'current' ? '当前监管' : '历史记录' },
    { key: 'start', label: '监管开始', width: '92px', num: true, value: (row) => date(row.start_date), sortValue: (row) => row.start_date },
    { key: 'end', label: '监管结束', width: '92px', num: true, value: (row) => date(row.end_date), sortValue: (row) => row.end_date },
    { key: 'start-price', label: '起始价', width: '76px', align: 'right', num: true, value: (row) => fixed(row.start_price, 3), sortValue: (row) => row.start_price ?? -9999 },
    { key: 'last-price', label: '当前 / 结束价', width: '94px', align: 'right', num: true, value: (row) => fixed(row.record_kind === 'current' ? row.last_price : row.end_price, 3), sub: (row) => row.record_kind === 'current' ? `当日 ${row.quote_change_pct == null ? '—' : `${fixed(row.quote_change_pct, 2)}%`}` : '监管结束价' },
    { key: 'return', label: '监管期表现', width: '92px', align: 'right', num: true, value: (row) => { const value = row.record_kind === 'current' ? row.since_start_return_pct : row.period_return_pct; return value == null ? '—' : `${fixed(value, 2)}%`; }, tone: (row) => tone(row.record_kind === 'current' ? row.since_start_return_pct : row.period_return_pct), sortValue: (row) => (row.record_kind === 'current' ? row.since_start_return_pct : row.period_return_pct) ?? -9999 },
    { key: 'pe', label: 'PE(TTM)', width: '78px', align: 'right', num: true, value: (row) => fixed(row.pe_ttm, 2), sortValue: (row) => row.pe_ttm ?? -9999 },
    { key: 'turnover', label: '成交额', width: '105px', align: 'right', num: true, value: (row) => row.record_kind === 'current' ? compact(row.turnover_amount_yuan, '元') : '—' },
    { key: 'pdf', label: '异动公告', width: '82px', slot: true }
  ];

  onMount(() => void load());
</script>

<PageHeader eyebrow="709/1721 · JYSJK101—102 · JYJGFX" title="交易所监管观察期" description="还原客户端“当前监管个股 / 历史监管记录”，关联监管起止价、区间表现和公告 PDF；它不同于异常波动阈值和监管处分。" {stats}>
  {#snippet actions()}
    <Segmented options={SECTIONS} value={section} onChange={switchSection} ariaLabel="监管数据分区" />
    <Button icon="refresh" busy={resource.busy} onclick={() => void load(true)}>刷新监管表与行情</Button>
  {/snippet}
</PageHeader>

<Panel title={section === 'current' ? '当前监管个股' : '历史监管记录'} subtitle={section === 'current' ? '区间表现复现客户端 (现价−起始价) / 起始价；基金和转债也按原表保留' : '历史表现复现客户端 (结束价−起始价) / 起始价'} busy={resource.busy} error={resource.error} onRetry={() => void load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选没有监管记录。" flush scroll fill>
  {#snippet toolbar()}
    <Select options={PDF_FILTERS} bind:value={pdfFilter} width="130px" label="公告" onChange={() => void load()} />
    <TextInput bind:value={query} icon="search" width="230px" label="检索" placeholder="股票代码或名称" onEnter={() => void load()} />
    <Button icon="search" onclick={() => void load()}>查询</Button>
  {/snippet}
  <DataTable columns={columns} rows={rows} rowKey={(row) => row.supervision_id} onRowClick={openSecurity} stickyFirst sortKey="end" minWidth="1050px">
    {#snippet cell({ row, column })}
      {#if column.key === 'pdf'}
        {#if row.announcement_url}
          <a href={row.announcement_url} target="_blank" rel="noreferrer" onclick={(event) => event.stopPropagation()}>打开 PDF</a>
        {:else}
          <span class="mute">—</span>
        {/if}
      {/if}
    {/snippet}
  </DataTable>
</Panel>

<style>
  a { color: var(--focus); text-decoration: none; }
  a:hover { text-decoration: underline; }
  .mute { color: var(--fg-mute); }
</style>
