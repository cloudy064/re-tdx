<script lang="ts">
  /**
   * 逐笔成交明细：分钟聚合走势 + 最近成交列表。
   * 09:25 与 15:00 的集中撮合、深市盘后定价成交单独标出——它们不是连续竞价，
   * 混在普通分钟里会让人误判量能分布。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, fixed, price, text, tone } from '../../../lib/fmt';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import TextInput from '../../../ui/TextInput.svelte';
  import TradeChart from '../../../charts/TradeChart.svelte';
  import type { TradeTick, TradesDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const trades = new Resource<TradesDocument>();
  const record = $derived(trades.data?.records[0] ?? null);
  const summary = $derived(record?.summary ?? null);
  /** 只渲染最近 300 笔：全量可达数万行，浏览器渲染成本远大于信息价值。 */
  const ticks = $derived([...(record?.ticks ?? [])].slice(-300).reverse());

  let tradeDate = $state('');

  function load() {
    void trades.load(
      `/api/v1/market/trades?${queryString({ market, code, date: tradeDate.trim() })}`
    );
  }

  const stats = $derived.by<Stat[]>(() => {
    if (!summary) return [];
    return [
      { label: '成交笔数', value: compact(summary.tick_count) },
      { label: '成交量', value: compact(summary.volume_hand, '手') },
      { label: '成交额', value: compact(summary.amount_yuan, '元') },
      { label: '均价 VWAP', value: fixed(summary.vwap) },
      { label: '价格区间', value: `${fixed(summary.low_price)} — ${fixed(summary.high_price)}` },
      {
        label: '主买 / 主卖',
        value: `${compact(summary.buy_volume_hand)} / ${compact(summary.sell_volume_hand)}`,
        tone: summary.buy_volume_hand >= summary.sell_volume_hand ? 'up' : 'down'
      },
      {
        label: '09:25 开盘撮合',
        value: compact(summary.auction_0925.volume_hand, '手'),
        note: summary.auction_0925.execution_status
      },
      {
        label: '15:00 收盘撮合',
        value: compact(summary.closing_1500.volume_hand, '手'),
        note: summary.closing_1500.execution_status
      },
      {
        label: '盘后定价成交',
        value: compact(summary.post_close_status_5.volume_hand, '手'),
        note: `${text(summary.post_close_status_5.first_time)} — ${text(summary.post_close_status_5.last_time)}`
      }
    ];
  });

  /**
   * 后端 side 是 buy / sell / neutral / status_N 的英文键，不能直接上屏。
   * status_5 是深市盘后定价成交（与 summary 的 post_close_status_5 同源）。
   */
  function sideLabel(side: string): string {
    if (side === 'buy') return '主动买';
    if (side === 'sell') return '主动卖';
    if (side === 'neutral') return '中性';
    if (side === 'status_5') return '盘后定价';
    const raw = side.startsWith('status_') ? side.slice(7) : side;
    return `状态 ${raw}`;
  }

  const columns: Column<TradeTick>[] = [
    { key: 'time', label: '时间', width: '64px', num: true, value: (row) => row.time_label },
    {
      key: 'price',
      label: '价格',
      align: 'right',
      width: '62px',
      num: true,
      value: (row) => price(row.price)
    },
    {
      key: 'volume',
      label: '成交量',
      align: 'right',
      num: true,
      value: (row) => compact(row.volume_hand, '手')
    },
    {
      key: 'amount',
      label: '金额',
      align: 'right',
      num: true,
      value: (row) => compact(row.amount_yuan, '元')
    },
    { key: 'orders', label: '笔数', align: 'right', num: true, value: (row) => String(row.order_count) },
    {
      key: 'side',
      label: '方向',
      width: '62px',
      value: (row) => sideLabel(row.side),
      tone: (row) => (row.side === 'buy' ? 'up' : row.side === 'sell' ? 'down' : 'flat')
    }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="分钟成交走势"
  eyebrow="0x0FC5 / 0x0FC6 · TCP 7709"
  subtitle={record ? `${record.trading_date} · ${record.source_mode === 'today' ? '当日' : '历史'} · ${record.pages} 页` : ''}
  busy={trades.busy}
  error={trades.error}
  onRetry={load}
  empty={trades.loaded && !trades.busy && !record}
  emptyText="该交易日没有返回成交明细"
  fill
>
  {#snippet toolbar()}
    <TextInput
      bind:value={tradeDate}
      icon="clock"
      width="140px"
      label="交易日"
      placeholder="留空取当日 / 20260804"
      onEnter={load}
    />
    <Button icon="refresh" busy={trades.busy} onclick={load}>查询</Button>
  {/snippet}

  {#if summary?.minutes.length}
    <TradeChart minutes={summary.minutes} tradeDate={record?.trading_date ?? ''} />
  {/if}
</Panel>

{#if summary}
  <div class="lower">
    <Panel title="当日聚合" eyebrow="SUMMARY" scroll>
      <StatGrid stats={stats} columns={3} />
    </Panel>
    <Panel
      title="最近成交"
      eyebrow="TICKS"
      subtitle={`最近 ${ticks.length} 笔，共 ${compact(record?.ticks.length ?? 0)} 笔`}
      flush
      scroll
      empty={ticks.length === 0}
    >
      <DataTable {columns} rows={ticks} rowKey={(row) => row.absolute_index} />
    </Panel>
  </div>
{/if}

<style>
  .lower {
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(0, 1.1fr);
    gap: var(--sp-2);
    flex: 1;
    min-height: 0;
  }

  @media (max-width: 1000px) {
    .lower {
      grid-template-columns: minmax(0, 1fr);
    }
  }
</style>
