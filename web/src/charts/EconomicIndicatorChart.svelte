<script lang="ts">
  /** 经济/商品指标历史，支持 TradingView 滚轮缩放与拖拽。 */
  import { onMount } from 'svelte';
  import {
    LineSeries,
    createChart,
    type IChartApi,
    type ISeriesApi,
    type LineData,
    type UTCTimestamp
  } from 'lightweight-charts';
  import type { EconomicIndicatorHistoryPoint } from '../types';
  import { barTime, baseOptions, palette, watchTheme } from './theme';

  interface Props {
    history: EconomicIndicatorHistoryPoint[];
    title?: string;
    unit?: string;
  }

  const { history, title = '指标值', unit = '' }: Props = $props();
  let host = $state<HTMLDivElement | null>(null);
  let chart: IChartApi | null = null;
  let series: ISeriesApi<'Line'> | null = null;

  function draw(fit = false) {
    if (!chart || !series) return;
    const values: LineData<UTCTimestamp>[] = history
      .filter((point) => Number.isFinite(point.value))
      .map((point) => ({ time: barTime(point.date) as UTCTimestamp, value: point.value }));
    series.applyOptions({ color: palette().focus, title });
    series.setData(values);
    if (fit && values.length) chart.timeScale().fitContent();
  }

  onMount(() => {
    if (!host) return;
    const p = palette();
    chart = createChart(host, baseOptions(p));
    series = chart.addSeries(LineSeries, {
      color: p.focus,
      lineWidth: 2,
      title,
      priceFormat: { type: 'price', precision: 2, minMove: 0.01 }
    });
    draw(true);
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
    void title;
    if (chart) draw(true);
  });
</script>

<div class="chart">
  <div class="legend"><i></i>{title}{unit ? ` · ${unit}` : ''}</div>
  <div class="canvas" bind:this={host}></div>
</div>

<style>
  .chart { position: relative; flex: 1; min-height: 250px; }
  .canvas { width: 100%; height: 100%; }
  .legend { position: absolute; top: var(--sp-2); left: var(--sp-3); z-index: 2; display: flex; align-items: center; gap: 5px; font-size: 9px; color: var(--fg-mute); pointer-events: none; }
  .legend i { width: 10px; height: 2px; background: var(--focus); }
</style>
