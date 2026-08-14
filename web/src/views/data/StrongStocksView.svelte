<script lang="ts">
  /** 通达信 QSGFX：历史强势股生命周期及区间逐日涨停原因。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, percent, tone } from '../../lib/fmt';
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
    MarketStrongStocksDocument,
    StrongStockDetailRecord,
    StrongStockIntervalRecord
  } from '../../types';

  const MARKETS = [
    { id: 'all', label: '全部市场' },
    { id: 'sz', label: '深市' },
    { id: 'sh', label: '沪市' },
    { id: 'bj', label: '北交所' }
  ];
  const BOARDS = [0, 3, 5, 7, 10].map((value) => ({
    id: String(value), label: value ? `至少 ${value} 板` : '不限连板数'
  }));

  let market = $state('all');
  let minBoards = $state('0');
  let query = $state('');
  let selected = $state<StrongStockIntervalRecord | null>(null);
  const intervalsResource = new Resource<MarketStrongStocksDocument>();
  const detailResource = new Resource<MarketStrongStocksDocument>();
  const doc = $derived(intervalsResource.data);
  const intervals = $derived((doc?.records ?? []) as StrongStockIntervalRecord[]);
  const detail = $derived((detailResource.data?.records ?? []) as StrongStockDetailRecord[]);

  const stats = $derived.by<Stat[]>(() => [
    { label: '强势区间', value: count(doc?.summary.source_rows), note: `${date(doc?.summary.first_start_date)} 至 ${date(doc?.summary.latest_end_date)}` },
    { label: '覆盖股票', value: count(doc?.summary.unique_securities) },
    { label: '多次入选', value: count(doc?.summary.securities_with_multiple_intervals) },
    { label: '名称解析', value: `${count(doc?.summary.names_resolved)} / ${count(doc?.summary.source_rows)}` },
    { label: '收益待结算', value: count(doc?.summary.return_pending_rows), note: '空值不按 0 处理' },
    { label: '当前命中', value: count(doc?.counts.matched), note: doc?.availability === 'stale-cache' ? '陈旧缓存' : '在线主表' }
  ]);

  async function load(refresh = false) {
    const result = await intervalsResource.load(`/api/v1/market/strong-stocks?${queryString({
      view: 'intervals', market: market === 'all' ? '' : market,
      q: query.trim(), min_limit_up_days: minBoards, sort: 'end-date', order: 'desc',
      limit: 1000, refresh: refresh ? 1 : 0
    })}`);
    const rows = (result?.records ?? []) as StrongStockIntervalRecord[];
    if (!selected || !rows.some((row) => row.interval_id === selected?.interval_id)) {
      selected = rows[0] ?? null;
      if (selected) void loadDetail(selected, refresh);
      else detailResource.reset();
    }
  }

  function loadDetail(row: StrongStockIntervalRecord, refresh = false) {
    selected = row;
    void detailResource.load(`/api/v1/market/strong-stocks?${queryString({
      view: 'detail', interval_id: row.interval_id, limit: 100,
      sort: 'date', order: 'asc', refresh: refresh ? 1 : 0
    })}`);
  }

  function openSecurity() {
    if (!selected) return;
    const security = selected.security;
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code, 'strong-stocks'));
  }

  const intervalColumns: Column<StrongStockIntervalRecord>[] = [
    { key: 'security', label: '股票', width: '128px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'end', label: '强势区间', width: '168px', num: true, value: (row) => `${date(row.start_date)} → ${date(row.end_date)}`, sortValue: (row) => row.end_date },
    { key: 'boards', label: '区间统计', width: '82px', value: (row) => row.interval_statistics, sortValue: (row) => row.limit_up_days ?? 0 },
    { key: 'return', label: '个股涨幅', width: '86px', align: 'right', num: true, value: (row) => percent(row.stock_return_pct), tone: (row) => tone(row.stock_return_pct), sortValue: (row) => row.stock_return_pct ?? -9999 },
    { key: 'index', label: '上证涨幅', width: '86px', align: 'right', num: true, value: (row) => percent(row.index_return_pct), tone: (row) => tone(row.index_return_pct), sortValue: (row) => row.index_return_pct ?? -9999 },
    { key: 'excess', label: '超额收益', width: '86px', align: 'right', num: true, value: (row) => percent(row.excess_return_pct), tone: (row) => tone(row.excess_return_pct), sortValue: (row) => row.excess_return_pct ?? -9999 },
    { key: 'state', label: '收益状态', width: '74px', value: (row) => row.return_finalized ? '已结算' : '进行中' }
  ];

  const detailColumns: Column<StrongStockDetailRecord>[] = [
    { key: 'date', label: '日期', width: '88px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'return', label: '个股涨幅', width: '82px', align: 'right', num: true, value: (row) => percent(row.stock_return_pct), tone: (row) => tone(row.stock_return_pct), sortValue: (row) => row.stock_return_pct ?? 0 },
    { key: 'amount', label: '成交额', width: '108px', align: 'right', num: true, value: (row) => compact(row.turnover_amount_yuan, '元'), sortValue: (row) => row.turnover_amount_yuan ?? 0 },
    { key: 'reason', label: '涨停原因', width: '360px', wrap: true, value: (row) => row.limit_up_reason || '—' },
    { key: 'temperature', label: '涨停 / 炸板 / 跌停', width: '126px', align: 'right', num: true, value: (row) => `${count(row.market_limit_up_count)} / ${count(row.market_broken_limit_count)} / ${count(row.market_limit_down_count)}` },
    { key: 'seal', label: '封板率', width: '76px', align: 'right', num: true, value: (row) => percent(row.market_seal_success_pct), sortValue: (row) => row.market_seal_success_pct ?? 0 },
    { key: 'index', label: '上证涨幅', width: '82px', align: 'right', num: true, value: (row) => percent(row.index_return_pct), tone: (row) => tone(row.index_return_pct) }
  ];

  onMount(() => void load());
</script>

<PageHeader eyebrow="709/1721 · QSGFX / YGZL101—102" title="强势股生命周期" description="还原通达信强势股分析：历史连板区间、区间超额收益，以及逐日涨停原因和全市场涨跌停温度。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={intervalsResource.busy} onclick={() => void load(true)}>刷新主表</Button>{/snippet}
</PageHeader>

<div class="workspace">
  <Panel title="历史强势区间" subtitle={doc ? `${count(doc.counts.matched)} 个区间 · 点击查看逐日原因` : ''} busy={intervalsResource.busy} error={intervalsResource.error} onRetry={() => void load()} empty={intervalsResource.loaded && intervals.length === 0} emptyText="当前筛选没有强势股区间。" flush scroll fill>
    {#snippet toolbar()}
      <Select options={MARKETS} bind:value={market} width="142px" label="市场" onChange={() => void load()} />
      <Select options={BOARDS} bind:value={minBoards} width="148px" label="强度" onChange={() => void load()} />
      <TextInput bind:value={query} icon="search" width="220px" label="检索" placeholder="代码 / 名称" onEnter={() => void load()} />
    {/snippet}
    <DataTable columns={intervalColumns} rows={intervals} stickyFirst numbered rowKey={(row) => row.interval_id} onRowClick={(row) => loadDetail(row)} isActive={(row) => row.interval_id === selected?.interval_id} sortKey="end" />
  </Panel>

  <Panel title={selected ? `${selected.security.name || selected.security.code} · ${selected.interval_statistics}` : '区间逐日明细'} subtitle={selected ? `${date(selected.start_date)} 至 ${date(selected.end_date)} · ${selected.interval_id}` : '从左侧选择一个强势区间'} busy={detailResource.busy} error={detailResource.error} onRetry={selected ? () => loadDetail(selected!, true) : undefined} empty={!selected || (detailResource.loaded && detail.length === 0)} emptyText={selected ? '该区间没有逐日明细。' : '选择左侧区间后查看涨停原因与市场温度。'} flush scroll fill>
    {#snippet actions()}
      <Button icon="external" disabled={!selected} onclick={openSecurity}>打开个股</Button>
      <Button icon="refresh" busy={detailResource.busy} disabled={!selected} onclick={() => selected && loadDetail(selected, true)} title="刷新区间明细" />
    {/snippet}
    <DataTable columns={detailColumns} rows={detail} rowKey={(row) => row.date} sortKey="date" sortDesc={false} />
  </Panel>
</div>

<style>
  .workspace { display: grid; min-height: 0; flex: 1; grid-template-columns: minmax(520px, 1.05fr) minmax(480px, .95fr); gap: var(--sp-2); }
  @media (max-width: 1100px) { .workspace { grid-template-columns: 1fr; grid-template-rows: minmax(340px, 1fr) minmax(300px, 1fr); } }
</style>
