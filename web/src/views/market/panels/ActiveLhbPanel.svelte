<script lang="ts">
  /** HYLHB：当前证券在三个滚动周期内的龙虎榜活跃度与逐次异动。 */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, percent, text, tone } from '../../../lib/fmt';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import Segmented from '../../../ui/Segmented.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { ActiveLhbEvent, MarketActiveLhbDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  type Period = '5d' | 'month' | 'half-year';

  const { market, code }: PanelProps = $props();
  const PERIODS = [
    { id: '5d', label: '近5日' },
    { id: 'month', label: '近一月' },
    { id: 'half-year', label: '近半年' }
  ];

  let period = $state<Period>('5d');
  const active = new Resource<MarketActiveLhbDocument>();
  const doc = $derived(active.data);
  const ranking = $derived(doc?.selected_ranking ?? null);
  const events = $derived(doc?.events ?? []);

  function load(refresh = false) {
    void active.load(`/api/v1/market/active-lhb?${queryString({
      period,
      market,
      code,
      include_details: 1,
      detail_limit: 500,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function switchPeriod(value: string) {
    period = value as Period;
  }

  const stats = $derived.by<Stat[]>(() => ranking ? [
    { label: '周期排名', value: `#${count(ranking.source_rank)}`, note: ranking.period_label },
    { label: '上榜次数', value: count(ranking.event_count), note: date(ranking.statistics_date) },
    { label: '累计买入', value: compact(ranking.buy_amount_yuan, '元'), tone: 'up' },
    { label: '累计卖出', value: compact(ranking.sell_amount_yuan, '元'), tone: 'down' },
    { label: '累计净额', value: compact(ranking.net_buy_amount_yuan, '元'), tone: tone(ranking.net_buy_amount_yuan) },
    { label: '区间涨幅', value: percent(ranking.period_return_pct), tone: tone(ranking.period_return_pct) }
  ] : []);

  const columns: Column<ActiveLhbEvent>[] = [
    { key: 'date', label: '异动日', width: '90px', num: true, value: (row) => date(row.event_date) },
    { key: 'type', label: '上榜原因', wrap: true, value: (row) => text(row.event_type) },
    { key: 'change', label: '当日涨幅', width: '84px', align: 'right', num: true, value: (row) => percent(row.change_pct), tone: (row) => tone(row.change_pct) },
    { key: 'turnover', label: '换手率', width: '76px', align: 'right', num: true, value: (row) => percent(row.turnover_rate_pct) },
    { key: 'buy', label: '买入额', width: '105px', align: 'right', num: true, value: (row) => compact(row.buy_amount_yuan, '元'), tone: () => 'up' },
    { key: 'sell', label: '卖出额', width: '105px', align: 'right', num: true, value: (row) => compact(row.sell_amount_yuan, '元'), tone: () => 'down' },
    { key: 'net', label: '净买入', width: '105px', align: 'right', num: true, value: (row) => compact(row.net_buy_amount_yuan, '元'), tone: (row) => tone(row.net_buy_amount_yuan) }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="滚动周期活跃度"
  eyebrow="HYLHB · 13801 / 13901 / 14001"
  subtitle="按周期聚合上榜次数与买卖额；与下方单日事件和营业部席位口径分开"
  busy={active.busy}
  error={active.error}
  onRetry={load}
  empty={active.loaded && !active.busy && !ranking}
  emptyText={`这只股票未进入${PERIODS.find((item) => item.id === period)?.label ?? '当前周期'}活跃榜前 50 名`}
  flush
  scroll
>
  {#snippet toolbar()}
    <Segmented options={PERIODS} value={period} onChange={switchPeriod} ariaLabel="个股活跃龙虎周期" />
  {/snippet}
  {#snippet actions()}
    <Button icon="refresh" busy={active.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  {#if ranking}
    <div class="pad"><StatGrid stats={stats} columns={6} /></div>
    <DataTable
      columns={columns}
      rows={events}
      rowKey={(row, index) => `${row.event_date}-${row.event_type}-${index}`}
      maxHeight="300px"
      minWidth="780px"
    />
  {/if}
</Panel>

<style>
  .pad { padding: var(--sp-3); border-bottom: 1px solid var(--line); }
</style>
