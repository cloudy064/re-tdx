<script lang="ts">
  /** 配置驱动的研究序列图，供基金、估值、波动率等工作台复用。 */
  import { onMount } from 'svelte';
  import {
    LineSeries,
    createChart,
    type IChartApi,
    type ISeriesApi,
    type LineData,
    type UTCTimestamp
  } from 'lightweight-charts';
  import {
    valueAt,
    type ResearchRecord,
    type ResearchChartDefinition
  } from '../lib/records';
  import { barTime, baseOptions, palette, watchTheme } from './theme';

  interface Props {
    rows: ResearchRecord[];
    definition: ResearchChartDefinition;
    title: string;
  }

  const { rows, definition, title }: Props = $props();
  let host = $state<HTMLDivElement | null>(null);
  let chart: IChartApi | null = null;
  let series: ISeriesApi<'Line'>[] = [];

  function normalizedDate(raw: unknown): string | null {
    const value = String(raw ?? '').trim();
    if (/^\d{4}-\d{2}-\d{2}$/.test(value)) return value;
    if (/^\d{4}-\d{2}$/.test(value)) return `${value}-01`;
    if (/^\d{8}$/.test(value))
      return `${value.slice(0, 4)}-${value.slice(4, 6)}-${value.slice(6, 8)}`;
    return null;
  }

  function clearSeries() {
    if (!chart) return;
    for (const item of series) chart.removeSeries(item);
    series = [];
  }

  function draw(fit = false) {
    if (!chart) return;
    clearSeries();
    const colors = palette();
    for (const item of definition.series) {
      const api = chart.addSeries(LineSeries, {
        color: colors[item.color],
        lineWidth: 2,
        title: item.title,
        priceFormat: { type: 'price', precision: 2, minMove: 0.01 }
      });
      const values: LineData<UTCTimestamp>[] = [];
      for (const row of rows) {
        const date = normalizedDate(valueAt(row, definition.timePath));
        const value = Number(valueAt(row, item.path));
        if (!date || !Number.isFinite(value)) continue;
        values.push({ time: barTime(date) as UTCTimestamp, value });
      }
      values.sort((left, right) => Number(left.time) - Number(right.time));
      api.setData(values);
      series.push(api);
    }
    if (fit && rows.length) chart.timeScale().fitContent();
  }

  onMount(() => {
    if (!host) return;
    chart = createChart(host, baseOptions(palette()));
    draw(true);
    const stopTheme = watchTheme(() => {
      chart?.applyOptions(baseOptions(palette()));
      draw();
    });
    return () => {
      stopTheme();
      chart?.remove();
      chart = null;
      series = [];
    };
  });

  $effect(() => {
    void rows;
    void definition;
    if (chart) draw(true);
  });
</script>

<div class="chart">
  <div class="legend">
    <strong>{title}</strong>
    {#each definition.series as item (item.path)}
      <span><i class={item.color}></i>{item.title}</span>
    {/each}
  </div>
  <div class="canvas" bind:this={host}></div>
</div>

<style>
  .chart { position: relative; flex: 1; min-height: 260px; }
  .canvas { width: 100%; height: 100%; }
  .legend { position: absolute; top: var(--sp-2); left: var(--sp-3); z-index: 2; display: flex; align-items: center; gap: var(--sp-3); font-size: 9px; color: var(--fg-mute); pointer-events: none; }
  .legend strong { color: var(--fg); font-weight: 500; }
  .legend span { display: inline-flex; align-items: center; gap: 4px; }
  .legend i { width: 10px; height: 2px; }
  .legend i.focus { background: var(--focus); }
  .legend i.up { background: var(--up); }
  .legend i.down { background: var(--down); }
  .legend i.warn { background: var(--warn); }
</style>
