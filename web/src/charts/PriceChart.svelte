<script lang="ts">
  /**
   * 价格图：分时用面积线，K 线用蜡烛，下方叠成交量。
   * 涨跌配色统一红涨绿跌，与全局 token 同源。
   */
  import { onMount } from 'svelte';
  import {
    AreaSeries,
    CandlestickSeries,
    HistogramSeries,
    createChart,
    type AreaData,
    type CandlestickData,
    type HistogramData,
    type IChartApi,
    type ISeriesApi,
    type LogicalRange,
    type UTCTimestamp
  } from 'lightweight-charts';
  import type { MinuteBar } from '../types';
  import { alpha, barTime, baseOptions, palette, watchTheme } from './theme';

  interface Props {
    bars: MinuteBar[];
    display?: 'line' | 'candles';
    period?: string;
    /** 标的+周期的组合键。变化时重置视口，否则保持用户当前缩放。 */
    seriesKey?: string;
    canLoadEarlier?: boolean;
    loadingEarlier?: boolean;
    onLoadEarlier?: () => void;
  }

  const {
    bars,
    display = 'line',
    period = 'time',
    seriesKey = '',
    canLoadEarlier = false,
    loadingEarlier = false,
    onLoadEarlier
  }: Props = $props();

  let host = $state<HTMLDivElement | null>(null);
  let chart: IChartApi | null = null;
  let priceSeries: ISeriesApi<'Area'> | ISeriesApi<'Candlestick'> | null = null;
  let volumeSeries: ISeriesApi<'Histogram'> | null = null;
  let activeDisplay: 'line' | 'candles' | null = null;
  let activeKey = '';
  let activeCount = 0;
  let activeFirstBarKey = '';
  let rendering = false;
  /** 当前视图是否处于「自动贴合全部数据」状态。用户一旦手动缩放就置否。 */
  let autoFit = true;

  function applyTheme() {
    if (!chart) return;
    chart.applyOptions(baseOptions(palette()));
    if (priceSeries && activeDisplay) buildPriceSeries(activeDisplay);
    draw();
  }

  function buildPriceSeries(next: 'line' | 'candles') {
    if (!chart) return;
    const p = palette();
    if (priceSeries) chart.removeSeries(priceSeries);
    priceSeries =
      next === 'line'
        ? chart.addSeries(AreaSeries, {
            lineColor: p.focus,
            lineWidth: 1,
            topColor: alpha(p.focus, 0.22),
            bottomColor: alpha(p.focus, 0),
            priceLineVisible: true,
            priceLineColor: p.neutral,
            priceLineStyle: 3
          })
        : chart.addSeries(CandlestickSeries, {
            upColor: p.up,
            downColor: p.down,
            wickUpColor: p.up,
            wickDownColor: p.down,
            borderVisible: false
          });
    priceSeries.priceScale().applyOptions({ scaleMargins: { top: 0.06, bottom: 0.26 } });
    activeDisplay = next;
  }

  function draw() {
    if (!chart || !volumeSeries) return;
    const p = palette();
    const reset = activeKey !== seriesKey || activeDisplay !== display || activeCount === 0;
    const logical = reset ? null : chart.timeScale().getVisibleLogicalRange();
    const following = Boolean(logical) && activeCount > 0 && logical!.to >= activeCount - 2;
    const span = logical ? logical.to - logical.from : 0;
    const rightGap = logical ? logical.to - (activeCount - 1) : 0;
    const prepended =
      !reset && activeFirstBarKey
        ? Math.max(0, bars.findIndex((bar) => `${bar.date} ${bar.time}` === activeFirstBarKey))
        : 0;

    rendering = true;
    if (!priceSeries || activeDisplay !== display) buildPriceSeries(display);

    const area: AreaData<UTCTimestamp>[] = [];
    const candles: CandlestickData<UTCTimestamp>[] = [];
    const volumes: HistogramData<UTCTimestamp>[] = [];
    for (const bar of bars) {
      const time = barTime(bar.date, bar.time) as UTCTimestamp;
      area.push({ time, value: bar.close });
      candles.push({ time, open: bar.open, high: bar.high, low: bar.low, close: bar.close });
      volumes.push({
        time,
        value: bar.volume,
        color: alpha(bar.close >= bar.open ? p.up : p.down, 0.42)
      });
    }

    if (display === 'line') (priceSeries as ISeriesApi<'Area'>).setData(area);
    else (priceSeries as ISeriesApi<'Candlestick'>).setData(candles);
    volumeSeries.setData(volumes);

    chart.applyOptions({
      timeScale: {
        timeVisible: !['day', 'week', 'month'].includes(period),
        secondsVisible: false,
        barSpacing: display === 'line' ? 3 : 7
      }
    });

    activeKey = seriesKey;
    activeCount = bars.length;
    activeFirstBarKey = bars.length ? `${bars[0].date} ${bars[0].time}` : '';

    if (bars.length) {
      if (reset) {
        autoFit = true;
        if (display === 'line') fitAll();
        else {
          chart
            .timeScale()
            .setVisibleLogicalRange({ from: Math.max(0, bars.length - 120), to: bars.length + 5 });
          autoFit = false;
        }
      } else if (prepended > 0 && logical) {
        // 历史页插到数组头部后，旧 K 线的逻辑索引整体右移。同步平移视口，
        // 避免用户拖到左端触发加载时画面突然跳到更早一整页。
        chart.timeScale().setVisibleLogicalRange({
          from: logical.from + prepended,
          to: logical.to + prepended
        });
      } else if (following && logical) {
        const to = bars.length - 1 + rightGap;
        chart.timeScale().setVisibleLogicalRange({ from: to - span, to });
      }
    }
    rendering = false;
  }

  /**
   * 贴合全部数据。
   *
   * 直接调 fitContent 有竞态：首帧时容器尚未完成布局，barSpacing 按当时的窄宽度算好后，
   * 容器再变宽，lightweight-charts 会保持 barSpacing 并右对齐——结果是数据挤在右侧一小条，
   * 左边一大片空白。因此在下一帧再补一次。
   */
  function fitAll() {
    chart?.timeScale().fitContent();
    requestAnimationFrame(() => {
      if (autoFit) chart?.timeScale().fitContent();
    });
  }

  function onRangeChange(range: LogicalRange | null) {
    // 拖到左端外侧时向上游要更早的数据
    if (!rendering && range && range.from < -2 && canLoadEarlier && !loadingEarlier) {
      onLoadEarlier?.();
    }
  }

  onMount(() => {
    if (!host) return;
    const p = palette();
    chart = createChart(host, baseOptions(p));
    volumeSeries = chart.addSeries(HistogramSeries, {
      priceFormat: { type: 'volume' },
      priceScaleId: 'volume',
      priceLineVisible: false,
      lastValueVisible: false
    });
    chart.priceScale('volume').applyOptions({ scaleMargins: { top: 0.82, bottom: 0 } });
    chart.timeScale().subscribeVisibleLogicalRangeChange(onRangeChange);
    draw();

    // 容器宽度变化（折叠导航、切页签、缩窗）后 barSpacing 不会自动重算，
    // 仍处于贴合状态时补一次，避免又退化成「数据挤在右侧」。
    let lastWidth = host.clientWidth;
    const resize = new ResizeObserver(() => {
      if (!host) return;
      const width = host.clientWidth;
      if (width === lastWidth) return;
      lastWidth = width;
      if (autoFit && activeCount > 0) chart?.timeScale().fitContent();
    });
    resize.observe(host);

    const stopTheme = watchTheme(applyTheme);
    return () => {
      resize.disconnect();
      stopTheme();
      chart?.remove();
      chart = null;
    };
  });

  $effect(() => {
    // 依赖收集：bars / display / period / seriesKey 任一变化都重绘
    void bars;
    void display;
    void period;
    void seriesKey;
    if (chart) draw();
  });
</script>

<div class="chart">
  <div class="canvas" bind:this={host}></div>
  {#if loadingEarlier}
    <span class="hint">正在加载更早数据</span>
  {/if}
  <button class="fit" type="button" onclick={() => chart?.timeScale().fitContent()}>适应全部</button>
  <a class="credit" href="https://www.tradingview.com/" target="_blank" rel="noreferrer">
    TradingView Lightweight Charts™
  </a>
</div>

<style>
  .chart {
    position: relative;
    flex: 1;
    /* 与竞价/逐笔/估值三图一致地设下限：万一父级高度链再次断开，
       结果是「图变矮」而不是「图消失」。 */
    min-height: 240px;
  }

  .canvas {
    width: 100%;
    height: 100%;
  }

  .fit,
  .hint {
    position: absolute;
    top: var(--sp-2);
    z-index: 2;
    height: 20px;
    padding: 0 var(--sp-2);
    font-size: 10px;
    line-height: 18px;
    color: var(--fg-dim);
    background: var(--bg-raised);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  .fit {
    right: var(--sp-2);
  }

  .fit:hover {
    color: var(--fg);
    background: var(--bg-hover);
  }

  .hint {
    left: var(--sp-2);
    color: var(--focus);
  }

  .credit {
    position: absolute;
    bottom: 2px;
    left: var(--sp-2);
    z-index: 2;
    font-size: 8px;
    color: var(--fg-mute);
  }
</style>
