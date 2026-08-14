<script lang="ts">
  /** KPHPHJY：A 股与 ETF 的统计日开盘、盘后及总成交排行。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, delta, money, percent, price, tone } from '../../lib/fmt';
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
  import type {
    MarketSessionTurnoverDocument,
    SessionTurnoverActivity,
    SessionTurnoverRecord,
    SessionTurnoverSort,
    SessionTurnoverUniverse
  } from '../../types';

  const UNIVERSES = [
    { id: 'a', label: 'A 股' },
    { id: 'etf', label: 'ETF' }
  ];
  const ACTIVITIES = [
    { id: 'all', label: '全部记录' },
    { id: 'after-hours', label: '有盘后成交' },
    { id: 'opening', label: '有开盘成交' },
    { id: 'both', label: '开盘与盘后均有' }
  ];
  const SORTS = [
    { id: 'after-hours', label: '盘后成交额' },
    { id: 'opening', label: '开盘成交额' },
    { id: 'total', label: '总成交额' },
    { id: 'after-hours-share', label: '盘后占比' },
    { id: 'opening-share', label: '开盘占比' },
    { id: 'close-change', label: '收盘涨跌幅' },
    { id: 'open-change', label: '开盘涨跌幅' }
  ];
  const ORDERS = [
    { id: 'desc', label: '降序' },
    { id: 'asc', label: '升序' }
  ];
  const MARKETS = [
    { id: '', label: '全部市场' },
    { id: 'sz', label: '深圳' },
    { id: 'sh', label: '上海' },
    { id: 'bj', label: '北京' }
  ];
  const PAGE_SIZE = 200;

  let universe = $state<SessionTurnoverUniverse>('a');
  let activity = $state<SessionTurnoverActivity>('all');
  let sort = $state<SessionTurnoverSort>('after-hours');
  let order = $state<'asc' | 'desc'>('desc');
  let market = $state('');
  let query = $state('');
  let offset = $state(0);

  const resource = new Resource<MarketSessionTurnoverDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const staleError = $derived(doc && resource.error ? resource.error : '');
  const canPrevious = $derived(offset > 0 && !resource.busy);
  const canNext = $derived(Boolean(doc && offset + doc.counts.returned < doc.counts.matched) && !resource.busy);

  const stats = $derived.by<Stat[]>(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return [
      { label: '证券', value: count(summary.available), note: `${count(doc.counts.matched)} 个命中` },
      { label: '开盘活跃', value: count(summary.opening_active) },
      { label: '盘后活跃', value: count(summary.after_hours_active) },
      { label: '总成交额', value: money(summary.total_turnover_yuan) },
      { label: '开盘成交 / 占比', value: money(summary.opening_turnover_yuan), note: percent(summary.opening_share_total_pct) },
      { label: '盘后成交 / 占比', value: money(summary.after_hours_turnover_yuan), note: percent(summary.after_hours_share_total_pct) },
      { label: '统计日期', value: date(doc.statistics_date), note: doc.availability === 'stale-cache' ? '陈旧缓存' : '在线源' }
    ];
  });

  const columns: Column<SessionTurnoverRecord>[] = [
    {
      key: 'security', label: '证券', width: '150px',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => row.security.security_id
    },
    { key: 'date', label: '统计日', width: '92px', num: true, value: (row) => date(row.statistics_date) },
    {
      key: 'open', label: '开盘价 / 涨跌', align: 'right', num: true,
      value: (row) => price(row.open_price),
      sub: (row) => delta(row.open_change_pct),
      tone: (row) => tone(row.open_change_pct)
    },
    {
      key: 'close', label: '收盘价 / 涨跌', align: 'right', num: true,
      value: (row) => price(row.close_price),
      sub: (row) => delta(row.close_change_pct),
      tone: (row) => tone(row.close_change_pct)
    },
    { key: 'total', label: '总成交额', align: 'right', num: true, value: (row) => money(row.total_turnover_yuan) },
    {
      key: 'opening', label: '开盘成交 / 占比', align: 'right', num: true,
      value: (row) => money(row.opening_turnover_yuan),
      sub: (row) => percent(row.opening_share_total_pct)
    },
    {
      key: 'after-hours', label: '盘后成交 / 占比', align: 'right', num: true,
      value: (row) => money(row.after_hours_turnover_yuan),
      sub: (row) => percent(row.after_hours_share_total_pct)
    }
  ];

  function canOpen(row: SessionTurnoverRecord): boolean {
    return ['sz', 'sh', 'bj'].includes(row.security.market) && /^\d{6}$/.test(row.security.code);
  }

  function open(row: SessionTurnoverRecord) {
    if (!canOpen(row)) return;
    app.setStock({ market: row.security.market, code: row.security.code, name: row.security.name });
    router.go(stockPath(row.security.market, row.security.code));
  }

  function load(refresh = false) {
    void resource.load(`/api/v1/market/session-turnover?${queryString({
      universe,
      activity,
      sort,
      order,
      market,
      q: query.trim(),
      offset,
      limit: PAGE_SIZE,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function runQuery() {
    offset = 0;
    resource.reset();
    load();
  }

  function goPage(nextOffset: number) {
    offset = Math.max(0, nextOffset);
    resource.reset();
    load();
  }

  onMount(() => load());
</script>

<PageHeader
  eyebrow="KPHPHJY · PHCJE101 / 103 / 104"
  title="开盘与盘后成交"
  description="A 股与 ETF 的统计日开盘、盘后及总成交排行；仅展示上游已报告字段，不补造实时行情、市值或行业列。"
  {stats}
>
  {#snippet actions()}
    {#if staleError}<Badge tone="warn">刷新失败</Badge>{:else if doc?.availability === 'stale-cache'}<Badge tone="warn">陈旧缓存</Badge>{:else if doc}<Badge tone="up">在线</Badge>{/if}
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Panel
  flush
  scroll
  fill
  title={`${universe === 'a' ? 'A 股' : 'ETF'}开盘与盘后成交排行`}
  subtitle={staleError || (doc
    ? `${count(doc.counts.matched)} 个命中 · 第 ${count(offset + 1)}—${count(offset + doc.counts.returned)} 条 · ${doc.cache.refreshed ? '已刷新上游' : `缓存 ${count(doc.cache.age_seconds)} 秒`}`
    : '服务端筛选、排序与分页')}
  busy={resource.busy}
  error={doc ? '' : resource.error}
  onRetry={() => load()}
  empty={resource.loaded && !resource.busy && rows.length === 0}
  emptyText="当前市场、活跃状态和检索条件下没有成交记录。"
>
  {#snippet toolbar()}
    <Segmented options={UNIVERSES} value={universe} ariaLabel="证券范围" onChange={(value) => { universe = value as SessionTurnoverUniverse; runQuery(); }} />
    <Select options={ACTIVITIES} value={activity} width="150px" label="活跃状态" onChange={(value) => { activity = value as SessionTurnoverActivity; runQuery(); }} />
    <Select options={SORTS} value={sort} width="140px" label="服务端排序" onChange={(value) => { sort = value as SessionTurnoverSort; runQuery(); }} />
    <Select options={ORDERS} value={order} width="84px" label="顺序" onChange={(value) => { order = value as 'asc' | 'desc'; runQuery(); }} />
    <Select options={MARKETS} value={market} width="108px" label="市场" onChange={(value) => { market = value; runQuery(); }} />
    <TextInput bind:value={query} icon="search" width="190px" label="检索" placeholder="证券代码或名称" onEnter={runQuery} />
    <Button icon="search" onclick={runQuery}>查询</Button>
    <Button variant="ghost" disabled={!canPrevious} onclick={() => goPage(offset - PAGE_SIZE)}>上一页</Button>
    <Button variant="ghost" disabled={!canNext} onclick={() => goPage(offset + PAGE_SIZE)}>下一页</Button>
  {/snippet}

  <DataTable
    {columns}
    {rows}
    numbered
    stickyFirst
    minWidth="1040px"
    rowKey={(row) => row.security.security_id}
    onRowClick={open}
  />
</Panel>
