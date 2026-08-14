<script lang="ts">
  /** 行情图表：分时与 1/5/15/30/60 分、日周月 K 线。 */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import Segmented from '../../../ui/Segmented.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import PriceChart from '../../../charts/PriceChart.svelte';
  import type { DepthRecord, MinuteBar, MinuteDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, live }: PanelProps & { live?: DepthRecord | null } = $props();

  const PERIODS = [
    { id: 'time', label: '分时' },
    { id: '1m', label: '1分' },
    { id: '5m', label: '5分' },
    { id: '15m', label: '15分' },
    { id: '30m', label: '30分' },
    { id: '60m', label: '60分' },
    { id: 'day', label: '日线' },
    { id: 'week', label: '周线' },
    { id: 'month', label: '月线' }
  ];
  const ADJUSTMENTS = [
    { id: 'none', label: '不复权' },
    { id: 'qfq', label: '前复权' },
    { id: 'hfq', label: '后复权' }
  ];

  let period = $state('time');
  let adjustment = $state('none');

  const kline = new Resource<MinuteDocument>();
  let bars = $state<MinuteBar[]>([]);
  let nextStart = $state(0);
  let hasMore = $state(false);
  let loadingEarlier = $state(false);
  let activeRequestKey = '';
  let lastLiveMinute = '';
  const display = $derived<'line' | 'candles'>(period === 'time' ? 'line' : 'candles');

  function requestPath(start = 0): string {
    return `/api/v1/kline?${queryString({
      market,
      code,
      period,
      adjust: adjustment,
      // 分时只看当天；其他周期保留服务器已经下载到的全部历史记录。
      date: period === 'time' ? 'latest' : 'all',
      start,
      page_size: 800
    })}`;
  }

  function barKey(bar: MinuteBar): string {
    return `${bar.date} ${bar.time}`;
  }

  function mergeBars(current: MinuteBar[], incoming: MinuteBar[]): MinuteBar[] {
    const rows = new Map<string, MinuteBar>();
    for (const bar of current) rows.set(barKey(bar), bar);
    for (const bar of incoming) rows.set(barKey(bar), bar);
    return [...rows.values()].sort((left, right) => barKey(left).localeCompare(barKey(right)));
  }

  async function load() {
    const requestKey = `${market}:${code}:${period}:${adjustment}`;
    activeRequestKey = requestKey;
    kline.reset();
    bars = [];
    nextStart = 0;
    hasMore = false;
    loadingEarlier = false;

    const document = await kline.load(requestPath());
    if (!document || activeRequestKey !== requestKey) return;
    bars = document.bars;
    nextStart = document.next_start ?? document.downloaded ?? document.count;
    hasMore = period !== 'time' && Boolean(document.has_more);
  }

  async function loadEarlier() {
    if (period === 'time' || loadingEarlier || !hasMore) return;
    const requestKey = activeRequestKey;
    const start = nextStart;
    loadingEarlier = true;
    try {
      const document = await kline.load(requestPath(start), { silent: true });
      if (!document || activeRequestKey !== requestKey) return;
      bars = mergeBars(bars, document.bars);
      nextStart = document.next_start ?? start + (document.downloaded ?? document.count);
      hasMore = Boolean(document.has_more) && nextStart > start;
    } finally {
      if (activeRequestKey === requestKey) loadingEarlier = false;
    }
  }

  async function refreshLatest() {
    const requestKey = activeRequestKey;
    const document = await kline.load(requestPath(), { silent: true });
    if (!document || activeRequestKey !== requestKey) return;
    bars = mergeBars(bars, document.bars);
  }

  $effect(() => {
    void market;
    void code;
    void period;
    void adjustment;
    load();
  });

  // The depth stream changes tick by tick.  K-line 0x052D only needs one
  // refresh when the exchange minute changes, preserving loaded history and
  // the chart viewport while avoiding a full K-line request per quote tick.
  $effect(() => {
    const raw = live?.update_time_raw ?? 0;
    if (!raw || !['time', '1m', '5m', '15m', '30m', '60m'].includes(period)) return;
    const hourMinute = Math.floor(raw / 100);
    const hour = Math.floor(hourMinute / 100);
    const minute = hourMinute % 100;
    const inSession = (hour === 9 && minute >= 30) || hour === 10 ||
      (hour === 11 && minute <= 30) || hour === 13 || hour === 14 ||
      (hour === 15 && minute === 0);
    if (!inSession) return;
    const token = `${market}:${code}:${period}:${adjustment}:${hourMinute}`;
    if (token === lastLiveMinute || !activeRequestKey) return;
    lastLiveMinute = token;
    void refreshLatest();
  });
</script>

<Panel
  title="价格与成交量"
  eyebrow="0x052D · TCP 7709"
  subtitle={kline.data
    ? `${bars.length} 根 · ${kline.data.adjustment_mode === 'none' ? '不复权' : kline.data.adjustment_mode?.toUpperCase()} · 源 ${kline.data.source}`
    : ''}
  busy={kline.busy}
  error={kline.error}
  onRetry={load}
  empty={kline.loaded && !kline.busy && bars.length === 0}
  emptyText="该周期没有返回 K 线数据"
  flush
  fill
>
  {#snippet toolbar()}
    <div class="chart-toolbar">
      <Segmented options={PERIODS} value={period} onChange={(next) => (period = next)} ariaLabel="周期" />
      <Segmented
        options={ADJUSTMENTS}
        value={adjustment}
        onChange={(next) => (adjustment = next)}
        ariaLabel="复权方式"
      />
    </div>
  {/snippet}

  {#if bars.length}
    <PriceChart
      {bars}
      {display}
      {period}
      seriesKey={`${market}${code}${period}${adjustment}`}
      canLoadEarlier={period !== 'time' && hasMore}
      {loadingEarlier}
      onLoadEarlier={loadEarlier}
    />
  {/if}
</Panel>

<style>
  .chart-toolbar {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: var(--sp-2);
  }
</style>
