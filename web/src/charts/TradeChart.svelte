<script lang="ts">
  /**
   * 逐笔成交的分钟聚合图：价格走势 + 按买卖方向着色的分钟成交量。
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
  import type { TradeMinute } from '../types';
  import { alpha, baseOptions, dayTime, palette, watchTheme } from './theme';

  interface Props {
    minutes: TradeMinute[];
    tradeDate?: string;
  }

  const { minutes, tradeDate = '' }: Props = $props();

  let host = $state<HTMLDivElement | null>(null);
  let chart: IChartApi | null = null;
  let priceSeries: ISeriesApi<'Line'> | null = null;
  let volumeSeries: ISeriesApi<'Histogram'> | null = null;

  function draw() {
    if (!chart || !priceSeries || !volumeSeries) return;
    const p = palette();
    const prices: LineData<UTCTimestamp>[] = [];
    const volumes: HistogramData<UTCTimestamp>[] = [];

    for (const minute of minutes) {
      if (minute.last_price === null) continue;
      const time = dayTime(tradeDate, minute.time_minutes * 60) as UTCTimestamp;
      prices.push({ time, value: minute.last_price });
      volumes.push({
        time,
        value: minute.volume_hand,
        color: alpha(
          minute.buy_volume_hand >= minute.sell_volume_hand ? p.up : p.down,
          0.45
        )
      });
    }

    priceSeries.applyOptions({ color: p.focus });
    priceSeries.setData(prices);
    volumeSeries.setData(volumes);
    if (prices.length) chart.timeScale().fitContent();
  }

  onMount(() => {
    if (!host) return;
    const p = palette();
    chart = createChart(host, {
      ...baseOptions(p),
      timeScale: { borderColor: p.border, timeVisible: true, secondsVisible: false }
    });
    priceSeries = chart.addSeries(LineSeries, { color: p.focus, lineWidth: 1 });
    priceSeries.priceScale().applyOptions({ scaleMargins: { top: 0.06, bottom: 0.34 } });
    volumeSeries = chart.addSeries(HistogramSeries, {
      priceScaleId: 'trade-volume',
      priceLineVisible: false,
      lastValueVisible: false
    });
    chart.priceScale('trade-volume').applyOptions({ scaleMargins: { top: 0.7, bottom: 0 } });
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
    void minutes;
    void tradeDate;
    if (chart) draw();
  });
</script>

<div class="chart">
  <div class="legend">
    <span><i style="background:var(--up)"></i>主动买多</span>
    <span><i style="background:var(--down)"></i>主动卖多</span>
  </div>
  <div class="canvas" bind:this={host}></div>
</div>

<style>
  .chart {
    position: relative;
    flex: 1;
    min-height: 200px;
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
