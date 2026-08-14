<script lang="ts">
  /**
   * 集合竞价逐点图：虚拟匹配价格走势 + 匹配量 + 带方向的未匹配量。
   * 未匹配量为正（买方剩余）用涨色，为负（卖方剩余）用跌色。
   */
  import { onMount } from 'svelte';
  import {
    HistogramSeries,
    LineSeries,
    createChart,
    type HistogramData,
    type IChartApi,
    type ISeriesApi,
    type LineData,
    type UTCTimestamp
  } from 'lightweight-charts';
  import type { AuctionPoint } from '../types';
  import { alpha, baseOptions, dayTime, palette, watchTheme } from './theme';

  interface Props {
    points: AuctionPoint[];
    tradeDate?: string;
  }

  const { points, tradeDate = '' }: Props = $props();

  let host = $state<HTMLDivElement | null>(null);
  let chart: IChartApi | null = null;
  let priceSeries: ISeriesApi<'Line'> | null = null;
  let matchedSeries: ISeriesApi<'Histogram'> | null = null;
  let unmatchedSeries: ISeriesApi<'Histogram'> | null = null;

  function draw() {
    if (!chart || !priceSeries || !matchedSeries || !unmatchedSeries) return;
    const p = palette();
    const prices: LineData<UTCTimestamp>[] = [];
    const matched: HistogramData<UTCTimestamp>[] = [];
    const unmatched: HistogramData<UTCTimestamp>[] = [];

    for (const point of points) {
      const time = dayTime(tradeDate, point.time_seconds) as UTCTimestamp;
      prices.push({ time, value: point.price });
      matched.push({ time, value: point.matched_volume_hand, color: alpha(p.neutral, 0.3) });
      unmatched.push({
        time,
        value: point.unmatched_signed_hand,
        color: alpha(point.unmatched_signed_hand >= 0 ? p.up : p.down, 0.55)
      });
    }

    priceSeries.applyOptions({ color: p.focus });
    priceSeries.setData(prices);
    matchedSeries.setData(matched);
    unmatchedSeries.setData(unmatched);
    if (points.length) chart.timeScale().fitContent();
  }

  onMount(() => {
    if (!host) return;
    const p = palette();
    chart = createChart(host, {
      ...baseOptions(p),
      timeScale: { borderColor: p.border, timeVisible: true, secondsVisible: true }
    });
    priceSeries = chart.addSeries(LineSeries, {
      color: p.focus,
      lineWidth: 1,
      priceLineVisible: true
    });
    priceSeries.priceScale().applyOptions({ scaleMargins: { top: 0.06, bottom: 0.4 } });
    matchedSeries = chart.addSeries(HistogramSeries, {
      priceScaleId: 'auction-volume',
      priceLineVisible: false,
      lastValueVisible: false
    });
    unmatchedSeries = chart.addSeries(HistogramSeries, {
      priceScaleId: 'auction-volume',
      priceLineVisible: false,
      lastValueVisible: false
    });
    chart.priceScale('auction-volume').applyOptions({ scaleMargins: { top: 0.66, bottom: 0 } });
    draw();
    const stopTheme = watchTheme(() => {
      chart?.applyOptions(baseOptions(palette()));
      draw();
    });
    return () => {
      stopTheme();
      chart?.remove();
      chart = null;
    };
  });

  $effect(() => {
    void points;
    void tradeDate;
    if (chart) draw();
  });
</script>

<div class="chart">
  <div class="legend">
    <span><i style="background:var(--focus)"></i>虚拟匹配价</span>
    <span><i style="background:var(--fg-mute)"></i>匹配量</span>
    <span><i style="background:var(--up)"></i>买方未匹配</span>
    <span><i style="background:var(--down)"></i>卖方未匹配</span>
  </div>
  <div class="canvas" bind:this={host}></div>
</div>

<style>
  .chart {
    position: relative;
    flex: 1;
    min-height: 220px;
  }

  .canvas {
    width: 100%;
    height: 100%;
  }

  .legend {
    position: absolute;
    top: var(--sp-2);
    left: var(--sp-3);
    z-index: 2;
    display: flex;
    gap: var(--sp-4);
    font-size: 9px;
    color: var(--fg-mute);
    pointer-events: none;
  }

  .legend span {
    display: flex;
    align-items: center;
    gap: 4px;
  }

  .legend i {
    width: 10px;
    height: 2px;
  }
</style>
