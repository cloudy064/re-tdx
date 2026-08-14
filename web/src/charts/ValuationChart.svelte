<script lang="ts">
  /** 指数估值历史：PE/PB 走势（右轴）与历史百分位（左轴）。 */
  import { onMount } from 'svelte';
  import {
    LineSeries,
    createChart,
    type IChartApi,
    type ISeriesApi,
    type LineData,
    type UTCTimestamp
  } from 'lightweight-charts';
  import type { ValuationHistoryPoint } from '../types';
  import { baseOptions, dayTime, palette, watchTheme } from './theme';
  import { num } from '../lib/fmt';

  interface Props {
    history: ValuationHistoryPoint[];
    metric?: 'pe' | 'pb';
  }

  const { history, metric = 'pe' }: Props = $props();

  let host = $state<HTMLDivElement | null>(null);
  let chart: IChartApi | null = null;
  let valueSeries: ISeriesApi<'Line'> | null = null;
  let percentileSeries: ISeriesApi<'Line'> | null = null;
  let activeMetric = '';

  function draw() {
    if (!chart || !valueSeries || !percentileSeries) return;
    const p = palette();
    const values: LineData<UTCTimestamp>[] = [];
    const percentiles: LineData<UTCTimestamp>[] = [];
    const percentileField = `${metric}_percentile` as 'pe_percentile' | 'pb_percentile';

    for (const point of history) {
      const time = dayTime(point.date) as UTCTimestamp;
      const value = num(point[metric]);
      const percentile = num(point[percentileField]);
      if (value !== null) values.push({ time, value });
      if (percentile !== null) percentiles.push({ time, value: percentile });
    }

    valueSeries.applyOptions({ color: p.focus, title: metric.toUpperCase() });
    percentileSeries.applyOptions({ color: p.warn });
    valueSeries.setData(values);
    percentileSeries.setData(percentiles);

    if (activeMetric !== metric && history.length) {
      chart.timeScale().setVisibleLogicalRange({
        from: Math.max(0, history.length - 650),
        to: history.length + 8
      });
    }
    activeMetric = metric;
  }

  onMount(() => {
    if (!host) return;
    const p = palette();
    chart = createChart(host, {
      ...baseOptions(p),
      leftPriceScale: { visible: true, borderColor: p.border }
    });
    valueSeries = chart.addSeries(LineSeries, {
      color: p.focus,
      lineWidth: 1,
      priceScaleId: 'right',
      title: metric.toUpperCase()
    });
    percentileSeries = chart.addSeries(LineSeries, {
      color: p.warn,
      lineWidth: 1,
      priceScaleId: 'left',
      title: '历史百分位',
      priceFormat: { type: 'percent' }
    });
    chart.priceScale('left').applyOptions({ scaleMargins: { top: 0.06, bottom: 0.06 } });
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
    void history;
    void metric;
    if (chart) draw();
  });
</script>

<div class="chart">
  <div class="legend">
    <span><i style="background:var(--focus)"></i>{metric.toUpperCase()}（右轴）</span>
    <span><i style="background:var(--warn)"></i>历史百分位（左轴）</span>
  </div>
  <div class="canvas" bind:this={host}></div>
</div>

<style>
  .chart {
    position: relative;
    flex: 1;
    min-height: 300px;
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
