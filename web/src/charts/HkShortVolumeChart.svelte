<script lang="ts">
  /** 7727 港股日 K 辅助字段：历史沽空股数、MA5 与 MA20。 */
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
  import type { HkShortHistoryPoint } from '../types';
  import { alpha, barTime, baseOptions, palette, watchTheme } from './theme';

  interface Props {
    points: HkShortHistoryPoint[];
  }

  const { points }: Props = $props();
  let host = $state<HTMLDivElement | null>(null);
  let chart: IChartApi | null = null;
  let bars: ISeriesApi<'Histogram'> | null = null;
  let ma5: ISeriesApi<'Line'> | null = null;
  let ma20: ISeriesApi<'Line'> | null = null;

  function draw(fit = false) {
    if (!chart || !bars || !ma5 || !ma20) return;
    const p = palette();
    const histogram: HistogramData<UTCTimestamp>[] = points.map((point) => ({
      time: barTime(point.date) as UTCTimestamp,
      value: point.short_shares_10k,
      color: alpha(p.focus, 0.58)
    }));
    const line5: LineData<UTCTimestamp>[] = points
      .filter((point) => Number.isFinite(point.short_shares_ma5))
      .map((point) => ({
        time: barTime(point.date) as UTCTimestamp,
        value: (point.short_shares_ma5 as number) / 10000
      }));
    const line20: LineData<UTCTimestamp>[] = points
      .filter((point) => Number.isFinite(point.short_shares_ma20))
      .map((point) => ({
        time: barTime(point.date) as UTCTimestamp,
        value: (point.short_shares_ma20 as number) / 10000
      }));
    bars.applyOptions({ color: alpha(p.focus, 0.58) });
    ma5.applyOptions({ color: p.warn });
    ma20.applyOptions({ color: p.neutral });
    bars.setData(histogram);
    ma5.setData(line5);
    ma20.setData(line20);
    if (fit && histogram.length) chart.timeScale().fitContent();
  }

  onMount(() => {
    if (!host) return;
    const p = palette();
    chart = createChart(host, baseOptions(p));
    bars = chart.addSeries(HistogramSeries, {
      color: alpha(p.focus, 0.58),
      title: '沽空量',
      priceFormat: { type: 'price', precision: 2, minMove: 0.01 }
    });
    ma5 = chart.addSeries(LineSeries, { color: p.warn, lineWidth: 2, title: 'MA5' });
    ma20 = chart.addSeries(LineSeries, { color: p.neutral, lineWidth: 2, title: 'MA20' });
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
    void points;
    if (chart) draw(true);
  });
</script>

<div class="chart">
  <div class="legend"><i class="bars"></i>沽空量（万股） <i class="ma5"></i>MA5 <i class="ma20"></i>MA20</div>
  <div class="canvas" bind:this={host}></div>
</div>

<style>
  .chart { position: relative; min-height: 270px; margin-top: var(--sp-3); }
  .canvas { width: 100%; height: 270px; }
  .legend { position: absolute; top: var(--sp-2); left: var(--sp-3); z-index: 2; display: flex; align-items: center; gap: 5px; font-size: 9px; color: var(--fg-mute); pointer-events: none; }
  .legend i { width: 10px; height: 2px; display: inline-block; }
  .legend .bars { background: var(--focus); }
  .legend .ma5 { background: var(--warn); margin-left: 5px; }
  .legend .ma20 { background: var(--fg-dim); margin-left: 5px; }
</style>
