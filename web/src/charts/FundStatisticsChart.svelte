<script lang="ts">
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
  import type { FundStatisticsRecord } from '../types';
  import { barTime, baseOptions, dayTime, palette, watchTheme } from './theme';

  interface Props {
    rows: FundStatisticsRecord[];
    view: string;
  }

  const { rows, view }: Props = $props();
  let host = $state<HTMLDivElement | null>(null);
  let chart: IChartApi | null = null;
  let series: ISeriesApi<'Line'> | ISeriesApi<'Histogram'> | null = null;

  function point(row: FundStatisticsRecord): number | null {
    if (view === 'fund-market-size') return row.all_fund_nav_yuan ?? null;
    if (view === 'etf-market-size') return row.total_market_size_yuan ?? null;
    if (view === 'etf-weekly') return row.turnover_yuan ?? null;
    if (view === 'fund-market-size-chart') return row.all_fund_units ?? null;
    return row.total_net_subscription_units ?? null;
  }

  function chartTime(value: string): UTCTimestamp {
    return (/^\d{8}$/.test(value) ? dayTime(value) : barTime(value, '00:00')) as UTCTimestamp;
  }

  function draw() {
    if (!chart) return;
    if (series) chart.removeSeries(series);
    const p = palette();
    const histogram = view === 'etf-subscription-chart';
    if (histogram) {
      series = chart.addSeries(HistogramSeries, {
        title: 'ETF申购净量', priceScaleId: 'right',
        priceFormat: { type: 'volume' }
      });
      const data: HistogramData<UTCTimestamp>[] = [];
      for (const row of [...rows].sort((a, b) => a.date.localeCompare(b.date))) {
        const value = point(row);
        if (value === null || !Number.isFinite(value)) continue;
        data.push({ time: chartTime(row.date), value, color: value >= 0 ? p.up : p.down });
      }
      series.setData(data);
    } else {
      const title = view === 'fund-market-size' ? '基金资产净值'
        : view === 'etf-market-size' ? 'ETF市场规模'
          : view === 'etf-weekly' ? 'ETF周成交额' : '基金份额';
      series = chart.addSeries(LineSeries, {
        title, color: p.focus, lineWidth: 2, priceScaleId: 'right',
        priceFormat: { type: 'volume' }
      });
      const data: LineData<UTCTimestamp>[] = [];
      for (const row of [...rows].sort((a, b) => a.date.localeCompare(b.date))) {
        const value = point(row);
        if (value === null || !Number.isFinite(value)) continue;
        data.push({ time: chartTime(row.date), value });
      }
      series.setData(data);
    }
    chart.timeScale().fitContent();
  }

  onMount(() => {
    if (!host) return;
    chart = createChart(host, baseOptions(palette()));
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
    void rows;
    void view;
    if (chart) draw();
  });
</script>

<div class="chart" bind:this={host}></div>

<style>
  .chart { width: 100%; height: 300px; }
</style>
