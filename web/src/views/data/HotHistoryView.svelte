<script lang="ts">
  /** 通达信本地 speczshot.txt：多年 K 线热点区间与完整分析。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, delta, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    HotHistoryMarket,
    HotHistoryRecord,
    HotHistorySort,
    MarketHotHistoryDocument
  } from '../../types';

  const MARKETS = [
    { id: 'all', label: '全部市场' },
    { id: 'sz', label: '深市' },
    { id: 'sh', label: '沪市' },
    { id: 'bj', label: '北交所' }
  ];
  const SORTS = [
    { id: 'start-date', label: '开始日期' },
    { id: 'end-date', label: '结束日期' },
    { id: 'return', label: '区间收益' },
    { id: 'peak', label: '峰值收益' },
    { id: 'days', label: '交易日数' },
    { id: 'code', label: '证券代码' }
  ];
  const ORDERS = [
    { id: 'desc', label: '降序' },
    { id: 'asc', label: '升序' }
  ];
  const LIMITS = [50, 100, 200, 500, 1000].map((value) => ({
    id: String(value), label: `${value} 条 / 页`
  }));
  const VALID_MARKETS = new Set<HotHistoryMarket>(['sz', 'sh', 'bj']);

  let market = $state('all');
  let query = $state('');
  let dateFrom = $state('');
  let dateTo = $state('');
  let sort = $state<HotHistorySort>('start-date');
  let order = $state<'asc' | 'desc'>('desc');
  let limit = $state('200');
  let offset = $state(0);

  const resource = new Resource<MarketHotHistoryDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const pageStart = $derived(doc?.returned ? doc.filters.offset + 1 : 0);
  const pageEnd = $derived(doc ? doc.filters.offset + doc.returned : 0);

  const stats = $derived.by<Stat[]>(() => [
    {
      label: '匹配区间',
      value: count(doc?.match_count),
      note: doc ? `${count(doc.returned)} 条在当前页` : '本地多年记录'
    },
    { label: '覆盖证券', value: count(doc?.summary.security_count) },
    {
      label: '时间范围',
      value: `${date(doc?.summary.earliest_start_date)} — ${date(doc?.summary.latest_end_date)}`
    },
    {
      label: '最大区间收益',
      value: delta(doc?.summary.maximum_interval_return_pct),
      tone: tone(doc?.summary.maximum_interval_return_pct)
    },
    {
      label: '最大峰值收益',
      value: delta(doc?.summary.maximum_peak_return_pct),
      tone: tone(doc?.summary.maximum_peak_return_pct)
    },
    {
      label: '当前目录可打开',
      value: count(doc?.summary.native_host_eligible_records),
      note: doc?.summary.historical_name_fallback_records
        ? `${count(doc.summary.historical_name_fallback_records)} 条仅有历史名称`
        : undefined
    }
  ]);

  function canOpenSecurity(row: HotHistoryRecord): boolean {
    return row.native_host_eligible
      && VALID_MARKETS.has(row.security.market)
      && /^\d{6}$/.test(row.security.code);
  }

  function openSecurity(row: HotHistoryRecord) {
    if (!canOpenSecurity(row)) return;
    const { security } = row;
    app.setStock({
      market: security.market,
      code: security.code,
      name: security.name || security.security_id
    });
    router.go(stockPath(security.market, security.code, 'hot-history'));
  }

  function load(resetPage = true) {
    if (resetPage) offset = 0;
    return resource.load(`/api/v1/market/hot-history?${queryString({
      market: market === 'all' ? '' : market,
      q: query.trim(),
      from: dateFrom.trim(),
      to: dateTo.trim(),
      sort,
      order,
      offset,
      limit: Number(limit)
    })}`);
  }

  function previousPage() {
    const pageSize = doc?.filters.limit ?? Number(limit);
    offset = Math.max(0, offset - pageSize);
    void load(false);
  }

  function nextPage() {
    if (doc?.next_offset === null || doc?.next_offset === undefined) return;
    offset = doc.next_offset;
    void load(false);
  }

  const columns: Column<HotHistoryRecord>[] = [
    { key: 'security', label: '股票', width: '138px', slot: true },
    {
      key: 'range', label: '热点区间', width: '174px', num: true,
      value: (row) => `${date(row.start_date)} → ${date(row.end_date)}`
    },
    { key: 'days', label: '交易日', width: '66px', align: 'right', num: true, value: (row) => count(row.trading_days) },
    { key: 'theme', label: '主题', width: '170px', wrap: true, value: (row) => row.theme || '—' },
    { key: 'return', label: '区间收益', width: '88px', align: 'right', num: true, value: (row) => delta(row.interval_return_pct), tone: (row) => tone(row.interval_return_pct) },
    { key: 'peak', label: '峰值收益', width: '88px', align: 'right', num: true, value: (row) => delta(row.peak_return_pct), tone: (row) => tone(row.peak_return_pct) },
    // value() 交给 Svelte 文本插值，绝不把本地分析串当作 HTML 注入。
    { key: 'analysis', label: '完整分析', width: '420px', wrap: true, value: (row) => row.analysis || '—' }
  ];

  onMount(() => void load());
</script>

<PageHeader
  eyebrow="speczshot.txt · LOCAL ONLY"
  title="K 线历史热点"
  description="读取通达信本地多年热点区间，按证券、主题、日期及收益排序复盘；日期筛选采用区间重叠口径。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={resource.busy} onclick={() => void load(true)}>重新读取本地文件</Button>
  {/snippet}
</PageHeader>

<Panel
  title="历史热点区间"
  subtitle={doc ? `${count(pageStart)}—${count(pageEnd)} / ${count(doc.match_count)} · 仅“当前目录”证券可打开` : '完整分析按纯文本展示'}
  busy={resource.busy}
  error={resource.error}
  onRetry={() => void load(false)}
  empty={resource.loaded && rows.length === 0}
  emptyText="当前筛选没有历史热点区间。"
  flush
  scroll
  fill
>
  {#snippet toolbar()}
    <Select options={MARKETS} value={market} width="136px" label="市场" onChange={(value) => { market = value; void load(); }} />
    <TextInput bind:value={dateFrom} width="116px" label="开始日期" placeholder="开始 YYYYMMDD" onEnter={() => void load()} />
    <TextInput bind:value={dateTo} width="116px" label="结束日期" placeholder="结束 YYYYMMDD" onEnter={() => void load()} />
    <Select options={SORTS} value={sort} width="136px" label="排序" onChange={(value) => { sort = value as HotHistorySort; void load(); }} />
    <Select options={ORDERS} value={order} width="84px" label="顺序" onChange={(value) => { order = value as 'asc' | 'desc'; void load(); }} />
    <Select options={LIMITS} value={limit} width="112px" label="分页" onChange={(value) => { limit = value; void load(); }} />
    <TextInput bind:value={query} icon="search" width="220px" label="检索" placeholder="代码 / 名称 / 主题 / 分析" onEnter={() => void load()} />
    <Button icon="search" onclick={() => void load()}>查询</Button>
    <div class="pager">
      <Button icon="chevron-left" disabled={resource.busy || offset === 0} title="上一页" onclick={previousPage} />
      <span>第 {count(offset / Number(limit) + 1)} 页</span>
      <Button icon="chevron-right" disabled={resource.busy || !doc?.has_more} title="下一页" onclick={nextPage} />
    </div>
  {/snippet}

  <DataTable
    {columns}
    {rows}
    numbered
    stickyFirst
    minWidth="1180px"
    rowKey={(row) => `${row.security.security_id}:${row.start_date}:${row.source_line}`}
  >
    {#snippet cell({ row, column })}
      {#if column.key === 'security'}
        {#if canOpenSecurity(row)}
          <button class="security-link" type="button" title="在个股工作台打开" onclick={() => openSecurity(row)}>
            <span>{row.security.name || row.security.code}</span>
            <small>{row.security.security_id}</small>
          </button>
        {:else}
          <span class="security-static" title="仅有历史兼容名称或证券身份不可解析，不能打开个股工作台">
            <span>{row.security.name || row.security.code}</span>
            <small>{row.security.security_id} · 历史记录</small>
          </span>
        {/if}
      {/if}
    {/snippet}
  </DataTable>
</Panel>

<style>
  .pager { display: inline-flex; align-items: center; gap: var(--sp-2); margin-left: auto; }
  .pager span { min-width: 54px; font-family: var(--font-num); font-size: var(--fs-micro); color: var(--fg-mute); text-align: center; }
  .security-link, .security-static { display: inline-flex; min-width: 0; flex-direction: column; align-items: flex-start; line-height: 1.2; text-align: left; }
  .security-link { color: var(--focus); }
  .security-link:hover span:first-child { text-decoration: underline; }
  .security-link small, .security-static small { margin-top: 1px; font-family: var(--font-num); font-size: 9px; color: var(--fg-mute); }
  .security-static { color: var(--fg-dim); }
  @media (max-width: 1100px) { .pager { margin-left: 0; } }
</style>
