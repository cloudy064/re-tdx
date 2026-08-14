<script lang="ts">
  /** 单票强势股生命周期反查，并按区间展开逐日原因。 */
  import { queryString } from '../../../api';
  import { compact, count, date, percent, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid from '../../../ui/StatGrid.svelte';
  import type {
    MarketStrongStocksDocument,
    StrongStockDetailRecord,
    StrongStockIntervalRecord
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const intervalsResource = new Resource<MarketStrongStocksDocument>();
  const detailResource = new Resource<MarketStrongStocksDocument>();
  const intervals = $derived((intervalsResource.data?.records ?? []) as StrongStockIntervalRecord[]);
  const detail = $derived((detailResource.data?.records ?? []) as StrongStockDetailRecord[]);
  let selected = $state<StrongStockIntervalRecord | null>(null);

  async function load(refresh = false) {
    detailResource.reset();
    selected = null;
    const result = await intervalsResource.load(`/api/v1/market/strong-stocks?${queryString({
      view: 'security', market, code, sort: 'end-date', order: 'desc', limit: 100,
      refresh: refresh ? 1 : 0
    })}`);
    const rows = (result?.records ?? []) as StrongStockIntervalRecord[];
    if (rows[0]) loadDetail(rows[0], refresh);
  }

  function loadDetail(row: StrongStockIntervalRecord, refresh = false) {
    selected = row;
    void detailResource.load(`/api/v1/market/strong-stocks?${queryString({
      view: 'detail', market, code, interval_id: row.interval_id,
      sort: 'date', order: 'asc', limit: 100, refresh: refresh ? 1 : 0
    })}`);
  }

  const intervalColumns: Column<StrongStockIntervalRecord>[] = [
    { key: 'range', label: '强势区间', width: '170px', num: true, value: (row) => `${date(row.start_date)} → ${date(row.end_date)}`, sortValue: (row) => row.end_date },
    { key: 'boards', label: '区间统计', width: '88px', value: (row) => row.interval_statistics, sortValue: (row) => row.limit_up_days ?? 0 },
    { key: 'return', label: '个股涨幅', width: '88px', align: 'right', num: true, value: (row) => percent(row.stock_return_pct), tone: (row) => tone(row.stock_return_pct), sortValue: (row) => row.stock_return_pct ?? -9999 },
    { key: 'index', label: '上证涨幅', width: '88px', align: 'right', num: true, value: (row) => percent(row.index_return_pct), tone: (row) => tone(row.index_return_pct) },
    { key: 'excess', label: '超额收益', width: '88px', align: 'right', num: true, value: (row) => percent(row.excess_return_pct), tone: (row) => tone(row.excess_return_pct), sortValue: (row) => row.excess_return_pct ?? -9999 },
    { key: 'state', label: '状态', width: '70px', value: (row) => row.return_finalized ? '已结算' : '进行中' }
  ];

  const detailColumns: Column<StrongStockDetailRecord>[] = [
    { key: 'date', label: '日期', width: '90px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'return', label: '个股涨幅', width: '84px', align: 'right', num: true, value: (row) => percent(row.stock_return_pct), tone: (row) => tone(row.stock_return_pct) },
    { key: 'amount', label: '成交额', width: '112px', align: 'right', num: true, value: (row) => compact(row.turnover_amount_yuan, '元') },
    { key: 'reason', label: '涨停原因', width: '420px', wrap: true, value: (row) => row.limit_up_reason || '—' },
    { key: 'breadth', label: '涨停 / 炸板 / 跌停', width: '130px', align: 'right', num: true, value: (row) => `${count(row.market_limit_up_count)} / ${count(row.market_broken_limit_count)} / ${count(row.market_limit_down_count)}` },
    { key: 'seal', label: '封板率', width: '78px', align: 'right', num: true, value: (row) => percent(row.market_seal_success_pct) },
    { key: 'index', label: '上证涨幅', width: '84px', align: 'right', num: true, value: (row) => percent(row.index_return_pct), tone: (row) => tone(row.index_return_pct) }
  ];

  $effect(() => { market; code; void load(); });
</script>

<div class="stack">
  <Panel title={`${name} · 强势股生命周期`} subtitle="历史连板区间；未命中是正常关系" busy={intervalsResource.busy} error={intervalsResource.error} onRetry={() => void load()} empty={intervalsResource.loaded && intervals.length === 0} emptyText="当前证券没有进入通达信强势股历史区间。">
    {#snippet toolbar()}<Button icon="refresh" busy={intervalsResource.busy} onclick={() => void load(true)}>刷新主表</Button>{/snippet}
    {#if intervals.length}
      <StatGrid inline stats={[
        { label: '历史区间', value: count(intervals.length), note: '滚动主表命中' },
        { label: '最高连板', value: count(Math.max(...intervals.map((row) => row.limit_up_days ?? 0))) },
        { label: '最高区间涨幅', value: percent(Math.max(...intervals.map((row) => row.stock_return_pct ?? Number.NEGATIVE_INFINITY))) },
        { label: '最近结束日', value: date(intervals[0]?.end_date), note: intervals[0]?.return_finalized ? '收益已结算' : '收益仍待结算' }
      ]} />
    {/if}
  </Panel>

  {#if intervals.length}
    <Panel title="强势区间" subtitle="点击区间查看逐日涨停原因" flush scroll>
      <DataTable columns={intervalColumns} rows={intervals} rowKey={(row) => row.interval_id} onRowClick={(row) => loadDetail(row)} isActive={(row) => row.interval_id === selected?.interval_id} sortKey="range" />
    </Panel>
  {/if}

  <Panel title={selected ? `${selected.interval_statistics} · 逐日原因` : '逐日原因'} subtitle={selected ? `${date(selected.start_date)} 至 ${date(selected.end_date)}` : ''} busy={detailResource.busy} error={detailResource.error} onRetry={selected ? () => loadDetail(selected!, true) : undefined} empty={!selected || (detailResource.loaded && detail.length === 0)} emptyText="选择一个强势区间后查看明细。" flush scroll fill>
    {#snippet actions()}<Button icon="refresh" busy={detailResource.busy} disabled={!selected} onclick={() => selected && loadDetail(selected, true)} title="刷新区间明细" />{/snippet}
    <DataTable columns={detailColumns} rows={detail} rowKey={(row) => row.date} sortKey="date" sortDesc={false} />
  </Panel>
</div>

<style>
  .stack { display: flex; min-height: 0; flex: 1; flex-direction: column; gap: var(--sp-2); overflow: hidden; }
</style>
