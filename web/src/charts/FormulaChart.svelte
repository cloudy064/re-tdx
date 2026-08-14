<script lang="ts">
  /** Native C++ indicator output rendered with TradingView Lightweight Charts. */
  import { onMount } from 'svelte';
  import {
    CandlestickSeries,
    HistogramSeries,
    LineStyle,
    LineSeries,
    createChart,
    type CandlestickData,
    type HistogramData,
    type IChartApi,
    type ISeriesApi,
    type LineData,
    type UTCTimestamp,
    type WhitespaceData
  } from 'lightweight-charts';
  import type {
    FormulaCalculationDocument,
    FormulaRenderEvent,
    FormulaRenderPrimitive,
    FormulaRenderStyle
  } from '../types';
  import { barTime, baseOptions, palette, watchTheme } from './theme';

  interface Props {
    document: FormulaCalculationDocument;
  }

  const { document }: Props = $props();
  type FormulaSeries = ISeriesApi<'Line'> | ISeriesApi<'Histogram'> | ISeriesApi<'Candlestick'>;
  interface OrderedSeries {
    item: FormulaSeries;
    renderOrder: number;
    componentOrder: number;
    insertionOrder: number;
  }
  interface FixedLabel {
    key: string;
    text: string;
    x: number;
    y: number;
    color: string;
    align: 'left' | 'right';
    framed: boolean;
    renderOrder: number;
  }
  interface BandLayer {
    primitive: FormulaRenderPrimitive;
    scale: ISeriesApi<'Line'>;
    fallback: string;
  }
  interface BandPolygon { key: string; points: string; fill: string; }
  interface OrderedGroup<T> {
    key: string;
    renderOrder: number;
    shapes: T[];
  }
  interface StickLayer {
    primitive: FormulaRenderPrimitive;
    scale: ISeriesApi<'Line'>;
    fallback: string;
  }
  interface StickShape {
    key: string;
    x: number;
    y: number;
    width: number;
    height: number;
    fill: string;
    stroke: string;
    dashed: boolean;
  }
  interface AnnotationLayer {
    primitive: FormulaRenderPrimitive;
    scale: ISeriesApi<'Line'>;
    fallback: string;
  }
  interface SegmentLayer {
    primitive: FormulaRenderPrimitive;
    scale: ISeriesApi<'Line'>;
    fallback: string;
  }
  interface FormulaSegment {
    key: string;
    x1: number;
    y1: number;
    x2: number;
    y2: number;
    color: string;
    width: number;
    dashed: boolean;
  }
  interface BackgroundLayer {
    primitive: FormulaRenderPrimitive;
    scale: ISeriesApi<'Line'>;
    fallback: string;
  }
  interface BackgroundShape {
    key: string;
    x: number;
    y: number;
    width: number;
    height: number;
    background: string;
    border: string;
  }
  interface BitmapLayer {
    primitive: FormulaRenderPrimitive;
    scale: ISeriesApi<'Line'>;
  }
  interface BitmapShape {
    key: string;
    x: number;
    y: number;
    url: string;
    renderOrder: number;
  }
  interface PaneLayer {
    primitive: FormulaRenderPrimitive;
    fallback: string;
  }
  interface IconLayer {
    primitive: FormulaRenderPrimitive;
    scale: ISeriesApi<'Line'>;
  }
  interface SequenceLayer {
    primitive: FormulaRenderPrimitive;
    scale: ISeriesApi<'Line'>;
    fallback: string;
  }
  interface IconShape {
    key: string;
    x: number;
    y: number;
    width: number;
    height: number;
    spriteX: number;
    spriteUrl: string;
    transform: string;
    type: number;
    renderOrder: number;
  }
  interface PriceAnnotation {
    key: string;
    x: number;
    y: number;
    text: string;
    color: string;
    transform: string;
    framed: boolean;
    nativeUnframed: boolean;
    frameSide: 'above' | 'below' | null;
    frameFill: string;
    renderOrder: number;
  }
  interface SequenceShape {
    key: string;
    x: number;
    y: number;
    width: number;
    height: number;
    text: string;
    color: string;
    fill: string;
    align: 'right' | 'center';
    fontTableIndex: 1 | 13;
    leader: boolean;
    boxed: boolean;
    leaderFrom: 'top' | 'bottom';
    renderOrder: number;
  }
  type PromotedSeriesMode = 'line' | 'histogram' | 'part-line' |
    'candlestick' | 'band-upper' | 'band-lower' | 'point-markers';
  interface PromotedSeriesLayer {
    primitive: FormulaRenderPrimitive;
    scale: FormulaSeries;
    fallback: string;
    mode: PromotedSeriesMode;
    componentOrder: number;
    colorStick?: boolean;
    volumeStick?: boolean;
    baseline?: number;
  }
  interface OverlayPathShape {
    key: string;
    points: string;
    color: string;
    width: number;
    dashed: boolean;
  }
  interface OverlayRectShape {
    key: string;
    x: number;
    y: number;
    width: number;
    height: number;
    fill: string;
    stroke: string;
  }
  interface OverlayLineShape {
    key: string;
    x1: number;
    y1: number;
    x2: number;
    y2: number;
    color: string;
    width: number;
  }
  interface OverlayCircleShape {
    key: string;
    cx: number;
    cy: number;
    r: number;
    fill: string;
    stroke: string;
    width: number;
  }
  interface PromotedSeriesGroup {
    key: string;
    renderOrder: number;
    componentOrder: number;
    paths: OverlayPathShape[];
    rects: OverlayRectShape[];
    lines: OverlayLineShape[];
    circles: OverlayCircleShape[];
  }

  let host = $state<HTMLDivElement | null>(null);
  let chart: IChartApi | null = null;
  let series: FormulaSeries[] = [];
  let orderedSeries: OrderedSeries[] = [];
  let bandLayers: BandLayer[] = [];
  let stickLayers: StickLayer[] = [];
  let annotationLayers: AnnotationLayer[] = [];
  let segmentLayers: SegmentLayer[] = [];
  let backgroundLayers: BackgroundLayer[] = [];
  let bitmapLayers: BitmapLayer[] = [];
  let paneBackgroundLayers: PaneLayer[] = [];
  let paneRectangleLayers: PaneLayer[] = [];
  let iconLayers: IconLayer[] = [];
  let sequenceLayers: SequenceLayer[] = [];
  let overlayFrame = 0;
  let bandGroups = $state<OrderedGroup<BandPolygon>[]>([]);
  let stickGroups = $state<OrderedGroup<StickShape>[]>([]);
  let priceAnnotations = $state<PriceAnnotation[]>([]);
  let segmentGroups = $state<OrderedGroup<FormulaSegment>[]>([]);
  let backgroundGroups = $state<OrderedGroup<BackgroundShape>[]>([]);
  let paneBackgroundGroups = $state<OrderedGroup<BackgroundShape>[]>([]);
  let paneRectangleGroups = $state<OrderedGroup<StickShape>[]>([]);
  let bitmapShapes = $state<BitmapShape[]>([]);
  let iconShapes = $state<IconShape[]>([]);
  let sequenceShapes = $state<SequenceShape[]>([]);
  let fixedLabels = $state<FixedLabel[]>([]);
  let promotedSeriesLayers: PromotedSeriesLayer[] = [];
  let promotedSeriesGroups = $state<PromotedSeriesGroup[]>([]);
  let orderedOverlayMode = false;

  const namedColors: Record<string, string> = {
    COLORBLACK: '#000000', COLORBLUE: '#0000ff', COLORGREEN: '#00ff00',
    COLORCYAN: '#00ffff', COLORRED: '#ff0000', COLORMAGENTA: '#ff00ff',
    COLORBROWN: '#808000', COLORLIGRAY: '#c0c0c0', COLORGRAY: '#808080',
    COLORLIBLUE: '#00c0c0', COLORLIGREEN: '#40c040', COLORLICYAN: '#008080',
    COLORLIRED: '#ff8080', COLORLIMAGENTA: '#ff0080', COLORYELLOW: '#ffff00',
    COLORWHITE: '#ffffff'
  };

  function colorFromToken(token: string | undefined, fallback: string): string {
    if (!token) return fallback;
    const upper = token.toUpperCase();
    if (namedColors[upper]) return namedColors[upper];
    const direct = upper.match(/^COLOR([0-9A-F]{6})$/);
    if (direct) return colorFromRef(Number.parseInt(direct[1], 16), fallback);
    const rgbx = upper.match(/^RGBX([0-9A-F]{6})$/);
    return rgbx ? `#${rgbx[1].toLowerCase()}` : fallback;
  }

  /** TCalc RGB() uses Windows COLORREF: red | green << 8 | blue << 16. */
  function colorFromRef(value: number | null | undefined, fallback: string): string {
    if (value === null || value === undefined || !Number.isFinite(value)) return fallback;
    const packed = Math.max(0, Math.trunc(value));
    const red = packed & 0xff;
    const green = (packed >>> 8) & 0xff;
    const blue = (packed >>> 16) & 0xff;
    return `#${[red, green, blue].map((item) => item.toString(16).padStart(2, '0')).join('')}`;
  }

  function colorWithAlphaByte(color: string, alphaByte: number): string {
    const alpha = Math.max(0, Math.min(255, Math.trunc(alphaByte)));
    if (alpha === 255) return color;
    const match = color.match(/^#([0-9a-f]{6})$/i);
    return match ? `${color}${alpha.toString(16).padStart(2, '0')}` : color;
  }

  function annotationFontStyle(fontTableIndex: 1 | 13 = 1): string {
    const annotation = document.render_environment?.annotation;
    const font = (fontTableIndex === 13
      ? annotation?.conditional_fonts?.['13']
      : undefined) ?? annotation?.font;
    const family = (font?.face ?? 'Arial')
      .replace(/["'\\;\r\n]/g, '')
      .trim() || 'Arial';
    const size = Math.max(8, Math.min(48, Math.abs(font?.css_pixel_size ?? 15)));
    const weight = Math.max(100, Math.min(1000, Math.trunc(font?.weight ?? 400)));
    return `font-family:"${family}",Arial,sans-serif;font-size:${size}px;` +
      `font-weight:${weight};font-style:normal;line-height:1;text-decoration:none`;
  }

  function annotationFontMetrics(fontTableIndex: 1 | 13 = 1) {
    const annotation = document.render_environment?.annotation;
    const font = (fontTableIndex === 13
      ? annotation?.conditional_fonts?.['13']
      : undefined) ?? annotation?.font;
    return {
      measuredHeight: Math.max(8, Math.min(48,
        Math.abs(font?.css_pixel_size ?? 15))),
      textoutYAdjustment: font?.native_textout_y_adjustment_pixels ?? 0
    };
  }

  function annotationChartRowHeight(): number {
    return Math.max(1, Math.trunc(
      document.render_environment?.annotation.native_chart_row_height_pixels ?? 16));
  }

  function colorFromStyle(style: FormulaRenderStyle | undefined, fallback: string): string {
    if (style?.color_ref_available !== false &&
        style?.color_ref !== null && style?.color_ref !== undefined &&
        Number.isFinite(style.color_ref)) {
      return colorFromRef(style.color_ref, fallback);
    }
    return colorFromToken(style?.color_token, fallback);
  }

  function lineWidth(style?: FormulaRenderStyle): 1 | 2 | 3 | 4 {
    return Math.max(1, Math.min(4, Math.trunc(style?.line_thickness || 1))) as 1 | 2 | 3 | 4;
  }

  function nativeSeriesLineWidth(primitive: FormulaRenderPrimitive): 1 | 2 | 3 | 4 {
    const width = lineWidth(primitive.style);
    return primitive.series_native_renderer === 'tdxw-sub_957620' && width === 1 &&
      document.render_environment?.series_lines.bold_zb_line ? 2 : width;
  }

  function eventTime(event: FormulaRenderEvent): UTCTimestamp | null {
    const point = document.points[event.index];
    return point ? barTime(point.date, point.time) as UTCTimestamp : null;
  }

  function finite(value: number | null | undefined): value is number {
    return typeof value === 'number' && Number.isFinite(value);
  }

  function primitiveOrder(primitive: FormulaRenderPrimitive): number {
    return primitive.render_order ?? primitive.statement_index ?? Number.MAX_SAFE_INTEGER;
  }

  function overlayZIndex(primitive: FormulaRenderPrimitive): number {
    return 10 + Math.max(0, primitiveOrder(primitive));
  }

  function registerSeries(
    item: FormulaSeries,
    primitive: FormulaRenderPrimitive | null,
    componentOrder = 0
  ) {
    series.push(item);
    orderedSeries.push({
      item,
      renderOrder: primitive ? primitiveOrder(primitive) : Number.MAX_SAFE_INTEGER,
      componentOrder,
      insertionOrder: orderedSeries.length
    });
  }

  function applySeriesOrder() {
    orderedSeries
      .sort((left, right) => left.renderOrder - right.renderOrder ||
        left.componentOrder - right.componentOrder ||
        left.insertionOrder - right.insertionOrder)
      .forEach((entry, index) => entry.item.setSeriesOrder(index));
  }

  function addLinePrimitive(
    primitive: FormulaRenderPrimitive,
    fallback: string,
    componentOrder = 0
  ) {
    if (!chart || !primitive.style.visible) return;
    const color = colorFromStyle(primitive.style, fallback);
    const promoted = orderedOverlayMode || primitive.series_native_mode === 'line' ||
      primitive.series_native_mode === 'line-stick' ||
      primitive.series_native_mode === 'dot-line';
    const item = chart.addSeries(LineSeries, {
      title: primitive.statement,
      color: promoted ? 'rgba(0,0,0,0)' : color,
      lineWidth: nativeSeriesLineWidth(primitive),
      lineStyle: primitive.style.dot_line ? LineStyle.Dotted : LineStyle.Solid,
      lineVisible: !promoted,
      priceLineVisible: !promoted,
      lastValueVisible: !promoted,
      priceScaleId: 'right',
      priceFormat: { type: 'price', precision: 4, minMove: 0.0001 }
    });
    const data: Array<LineData<UTCTimestamp> | WhitespaceData<UTCTimestamp>> = [];
    for (const point of document.points) {
      const value = point.values[primitive.statement];
      const time = barTime(point.date, point.time) as UTCTimestamp;
      if (value === null || !Number.isFinite(value)) data.push({ time });
      else data.push({ time, value });
    }
    item.setData(data);
    registerSeries(item, primitive, componentOrder);
    if (promoted) promotedSeriesLayers.push({
      primitive, scale: item, fallback: color, mode: 'line', componentOrder
    });
  }

  function addLineStickPrimitive(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const color = colorFromStyle(primitive.style, fallback);
    const sticks = chart.addSeries(HistogramSeries, {
      title: '', color: 'rgba(0,0,0,0)',
      base: primitive.line_stick_baseline ?? 0,
      priceScaleId: 'right',
      priceLineVisible: false,
      lastValueVisible: false,
      priceFormat: { type: 'price', precision: 4, minMove: 0.0001 }
    });
    const data: HistogramData<UTCTimestamp>[] = [];
    for (const point of document.points) {
      const value = point.values[primitive.statement];
      if (value === null || !Number.isFinite(value)) continue;
      data.push({
        time: barTime(point.date, point.time) as UTCTimestamp,
        value,
        color: 'rgba(0,0,0,0)'
      });
    }
    sticks.setData(data);
    registerSeries(sticks, primitive, 0);
    promotedSeriesLayers.push({
      primitive, scale: sticks, fallback: color, mode: 'histogram',
      componentOrder: 0, baseline: primitive.line_stick_baseline ?? 0
    });
    addLinePrimitive(primitive, fallback, 1);
  }

  function addNativeStemPrimitive(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const color = colorFromStyle(primitive.style, fallback);
    const item = chart.addSeries(HistogramSeries, {
      title: primitive.statement,
      color: 'rgba(0,0,0,0)',
      base: primitive.series_native_stem_baseline ?? 0,
      priceScaleId: 'right',
      priceLineVisible: false,
      lastValueVisible: false,
      priceFormat: { type: 'price', precision: 4, minMove: 0.0001 }
    });
    const data: HistogramData<UTCTimestamp>[] = [];
    for (const point of document.points) {
      const value = point.values[primitive.statement];
      if (!finite(value)) continue;
      data.push({
        time: barTime(point.date, point.time) as UTCTimestamp,
        value,
        color: 'rgba(0,0,0,0)'
      });
    }
    item.setData(data);
    registerSeries(item, primitive);
    promotedSeriesLayers.push({
      primitive,
      scale: item,
      fallback: color,
      mode: 'histogram',
      componentOrder: 0,
      baseline: primitive.series_native_stem_baseline ?? 0
    });
  }

  function addPointMarkerPrimitive(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const color = colorFromStyle(primitive.style, fallback);
    const item = chart.addSeries(LineSeries, {
      title: primitive.statement,
      color: 'rgba(0,0,0,0)',
      lineVisible: false,
      pointMarkersVisible: false,
      crosshairMarkerVisible: false,
      priceScaleId: 'right',
      priceLineVisible: false,
      lastValueVisible: false,
      priceFormat: { type: 'price', precision: 4, minMove: 0.0001 }
    });
    const data: LineData<UTCTimestamp>[] = [];
    for (const point of document.points) {
      const value = point.values[primitive.statement];
      if (!finite(value)) continue;
      data.push({ time: barTime(point.date, point.time) as UTCTimestamp, value });
    }
    item.setData(data);
    registerSeries(item, primitive);
    promotedSeriesLayers.push({
      primitive,
      scale: item,
      fallback: color,
      mode: 'point-markers',
      componentOrder: 0
    });
  }

  function addPartLine(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const color = colorFromStyle(primitive.style, fallback);
    const item = chart.addSeries(LineSeries, {
      title: primitive.statement,
      color: orderedOverlayMode ? 'rgba(0,0,0,0)' : color,
      lineWidth: lineWidth(primitive.style),
      lineStyle: primitive.style.dot_line ? LineStyle.Dotted : LineStyle.Solid,
      lineVisible: !orderedOverlayMode,
      priceLineVisible: !orderedOverlayMode,
      lastValueVisible: !orderedOverlayMode,
      priceScaleId: 'right',
      priceFormat: { type: 'price', precision: 4, minMove: 0.0001 }
    });
    const data: LineData<UTCTimestamp>[] = [];
    const events = primitive.events ?? [];
    const segmentColors = new Map<number, string>();
    for (const event of events) {
      const direction = Math.trunc(event.arguments[2] ?? 0);
      const from = event.segment_from_index !== undefined
        ? event.segment_from_index
        : direction === 1 ? event.index - 1 : event.index;
      if (from === null || from < 0) continue;
      const available = event.segment_color_available ?? finite(event.arguments[1]);
      segmentColors.set(from, available
        ? colorFromRef(event.arguments[1], fallback)
        : 'rgba(0,0,0,0)');
    }
    for (const event of events) {
      const time = eventTime(event);
      const value = event.arguments[0];
      if (time === null || !finite(value)) continue;
      data.push({ time, value, color: segmentColors.get(event.index) ?? fallback });
    }
    item.setData(data);
    registerSeries(item, primitive);
    if (orderedOverlayMode) promotedSeriesLayers.push({
      primitive, scale: item, fallback: color, mode: 'part-line', componentOrder: 0
    });
  }

  function addCandlesticks(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const transparent = 'rgba(0,0,0,0)';
    const item = chart.addSeries(CandlestickSeries, {
      title: primitive.statement,
      priceScaleId: 'right',
      priceFormat: { type: 'price', precision: 4, minMove: 0.0001 },
      upColor: orderedOverlayMode ? transparent : fallback,
      downColor: orderedOverlayMode ? transparent : fallback,
      borderUpColor: orderedOverlayMode ? transparent : fallback,
      borderDownColor: orderedOverlayMode ? transparent : fallback,
      wickUpColor: orderedOverlayMode ? transparent : fallback,
      wickDownColor: orderedOverlayMode ? transparent : fallback,
      priceLineVisible: !orderedOverlayMode,
      lastValueVisible: !orderedOverlayMode
    });
    const data: CandlestickData<UTCTimestamp>[] = [];
    for (const event of primitive.events ?? []) {
      const time = eventTime(event);
      if (time === null) continue;
      let open: number | null | undefined;
      let high: number | null | undefined;
      let low: number | null | undefined;
      let close: number | null | undefined;
      [high, open, low, close] = event.arguments;
      if (!finite(open) || !finite(high) || !finite(low) || !finite(close)) continue;
      const directionColor = orderedOverlayMode
        ? transparent : close >= open ? palette().up : palette().down;
      data.push({
        time, open, high, low, close,
        color: directionColor,
        borderColor: directionColor,
        wickColor: directionColor
      });
    }
    item.setData(data);
    registerSeries(item, primitive);
    if (orderedOverlayMode) promotedSeriesLayers.push({
      primitive, scale: item, fallback, mode: 'candlestick', componentOrder: 0
    });
  }

  function addStick(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const first = chart.addSeries(LineSeries, {
      title: '', color: 'rgba(0,0,0,0)', lineVisible: false,
      pointMarkersVisible: false, priceLineVisible: false, lastValueVisible: false,
      crosshairMarkerVisible: false, priceScaleId: 'right'
    });
    const second = chart.addSeries(LineSeries, {
      title: primitive.statement, color: 'rgba(0,0,0,0)', lineVisible: false,
      pointMarkersVisible: false, priceLineVisible: false, lastValueVisible: false,
      crosshairMarkerVisible: false, priceScaleId: 'right'
    });
    const firstData: LineData<UTCTimestamp>[] = [];
    const secondData: LineData<UTCTimestamp>[] = [];
    for (const event of primitive.events ?? []) {
      const time = eventTime(event);
      if (time === null) continue;
      const price1 = event.arguments[1];
      const price2 = event.arguments[2];
      if ((event.stick_price1_used ?? true) && finite(price1))
        firstData.push({ time, value: price1 });
      if (finite(price2)) secondData.push({ time, value: price2 });
    }
    first.setData(firstData);
    second.setData(secondData);
    registerSeries(first, primitive, 0);
    registerSeries(second, primitive, 1);
    stickLayers.push({ primitive, scale: second, fallback });
  }

  function addBand(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const upper = chart.addSeries(LineSeries, {
      title: `${primitive.statement} 上界`,
      color: orderedOverlayMode ? 'rgba(0,0,0,0)' : fallback, lineWidth: 1,
      lineVisible: !orderedOverlayMode,
      priceScaleId: 'right', priceLineVisible: false, lastValueVisible: false
    });
    const lower = chart.addSeries(LineSeries, {
      title: `${primitive.statement} 下界`,
      color: orderedOverlayMode ? 'rgba(0,0,0,0)' : fallback, lineWidth: 1,
      lineVisible: !orderedOverlayMode,
      priceScaleId: 'right', priceLineVisible: false, lastValueVisible: false
    });
    const upperData: LineData<UTCTimestamp>[] = [];
    const lowerData: LineData<UTCTimestamp>[] = [];
    for (const event of primitive.events ?? []) {
      const time = eventTime(event);
      if (time === null) continue;
      if (finite(event.arguments[0])) upperData.push({
        time, value: event.arguments[0], color: orderedOverlayMode
          ? 'rgba(0,0,0,0)' : colorFromRef(event.arguments[1], fallback)
      });
      if (finite(event.arguments[2])) lowerData.push({
        time, value: event.arguments[2], color: orderedOverlayMode
          ? 'rgba(0,0,0,0)' : colorFromRef(event.arguments[3], fallback)
      });
    }
    upper.setData(upperData);
    lower.setData(lowerData);
    registerSeries(upper, primitive, 0);
    registerSeries(lower, primitive, 1);
    if (orderedOverlayMode) {
      promotedSeriesLayers.push({
        primitive, scale: upper, fallback, mode: 'band-upper', componentOrder: 0
      });
      promotedSeriesLayers.push({
        primitive, scale: lower, fallback, mode: 'band-lower', componentOrder: 1
      });
    }
    bandLayers.push({ primitive, scale: upper, fallback });
  }

  function addPriceAnnotations(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const anchor = chart.addSeries(LineSeries, {
      title: '', color: 'rgba(0,0,0,0)', lineVisible: false,
      pointMarkersVisible: false, priceLineVisible: false, lastValueVisible: false,
      crosshairMarkerVisible: false, priceScaleId: 'right'
    });
    const data: LineData<UTCTimestamp>[] = [];
    const extentData: LineData<UTCTimestamp>[] = [];
    for (const event of primitive.events ?? []) {
      const time = eventTime(event);
      if (time === null) continue;
      const framed = (event.annotation_frame ?? false) &&
        primitive.annotation_frame_supported === true;
      const point = document.points[event.index];
      if (framed && point && finite(point.high) && finite(point.low)) {
        data.push({ time, value: point.high });
        extentData.push({ time, value: point.low });
        continue;
      }
      const price = event.annotation_price ?? event.arguments[1];
      if (finite(price)) data.push({ time, value: price });
    }
    anchor.setData(data);
    registerSeries(anchor, primitive);
    if (extentData.length) {
      const extent = chart.addSeries(LineSeries, {
        title: '', color: 'rgba(0,0,0,0)', lineVisible: false,
        pointMarkersVisible: false, priceLineVisible: false, lastValueVisible: false,
        crosshairMarkerVisible: false, priceScaleId: 'right'
      });
      extent.setData(extentData);
      registerSeries(extent, primitive, 1);
    }
    annotationLayers.push({ primitive, scale: anchor, fallback });
  }

  function addSegmentPrimitive(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const anchor = chart.addSeries(LineSeries, {
      title: '', color: 'rgba(0,0,0,0)', lineVisible: false,
      pointMarkersVisible: false, priceLineVisible: false, lastValueVisible: false,
      crosshairMarkerVisible: false, priceScaleId: 'right'
    });
    const values = new Map<number, number>();
    if (primitive.output_statement) {
      for (const [index, point] of document.points.entries()) {
        const value = point.values[primitive.statement];
        if (finite(value)) values.set(index, value);
      }
    }
    for (const event of primitive.events ?? []) {
      if (event.segment_from_index !== null && event.segment_from_index !== undefined &&
          finite(event.segment_from_price))
        values.set(event.segment_from_index, event.segment_from_price);
      if (event.segment_to_index !== null && event.segment_to_index !== undefined &&
          finite(event.segment_to_price))
        values.set(event.segment_to_index, event.segment_to_price);
    }
    const data: LineData<UTCTimestamp>[] = [];
    for (const [index, value] of [...values.entries()].sort((left, right) => left[0] - right[0])) {
      const point = document.points[index];
      if (point) data.push({ time: barTime(point.date, point.time) as UTCTimestamp, value });
    }
    anchor.setData(data);
    registerSeries(anchor, primitive);
    segmentLayers.push({ primitive, scale: anchor, fallback });
  }

  function addBackground(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const makeAnchor = () => chart!.addSeries(LineSeries, {
      title: '', color: 'rgba(0,0,0,0)', lineVisible: false,
      pointMarkersVisible: false, priceLineVisible: false, lastValueVisible: false,
      crosshairMarkerVisible: false, priceScaleId: 'right'
    });
    const top = makeAnchor();
    const bottom = makeAnchor();
    const topData: LineData<UTCTimestamp>[] = [];
    const bottomData: LineData<UTCTimestamp>[] = [];
    for (const event of primitive.events ?? []) {
      const time = eventTime(event);
      if (time === null) continue;
      if (finite(event.background_price_top))
        topData.push({ time, value: event.background_price_top });
      if (finite(event.background_price_bottom))
        bottomData.push({ time, value: event.background_price_bottom });
    }
    top.setData(topData);
    bottom.setData(bottomData);
    registerSeries(top, primitive, 0);
    registerSeries(bottom, primitive, 1);
    backgroundLayers.push({ primitive, scale: top, fallback });
  }

  function addBitmap(primitive: FormulaRenderPrimitive) {
    if (!chart || !primitive.style.visible) return;
    const anchor = chart.addSeries(LineSeries, {
      title: '', color: 'rgba(0,0,0,0)', lineVisible: false,
      pointMarkersVisible: false, priceLineVisible: false, lastValueVisible: false,
      crosshairMarkerVisible: false, priceScaleId: 'right'
    });
    const data: LineData<UTCTimestamp>[] = [];
    for (const event of primitive.events ?? []) {
      const time = eventTime(event);
      const price = event.bitmap_price ?? event.arguments[1];
      if (time !== null && finite(price)) data.push({ time, value: price });
    }
    anchor.setData(data);
    registerSeries(anchor, primitive);
    bitmapLayers.push({ primitive, scale: anchor });
  }

  function addPaneBackground(primitive: FormulaRenderPrimitive, fallback: string) {
    if (primitive.style.visible) paneBackgroundLayers.push({ primitive, fallback });
  }

  function addPaneRectangle(primitive: FormulaRenderPrimitive, fallback: string) {
    if (primitive.style.visible) paneRectangleLayers.push({ primitive, fallback });
  }

  function scheduleOverlays() {
    if (overlayFrame) cancelAnimationFrame(overlayFrame);
    overlayFrame = requestAnimationFrame(() => {
      overlayFrame = 0;
      updatePromotedSeriesGroups();
      updateBandPolygons();
      updateStickShapes();
      updatePriceAnnotations();
      updateFormulaSegments();
      updateBackgroundShapes();
      updatePaneBackgroundShapes();
      updatePaneRectangleShapes();
      updateBitmapShapes();
      updateIconShapes();
      updateSequenceShapes();
    });
  }

  function updatePromotedSeriesGroups() {
    if (!chart || promotedSeriesLayers.length === 0) {
      promotedSeriesGroups = [];
      return;
    }
    const groups: PromotedSeriesGroup[] = [];
    const timeX = (index: number) => {
      const point = document.points[index];
      return point ? chart!.timeScale().timeToCoordinate(
        barTime(point.date, point.time) as UTCTimestamp
      ) : null;
    };
    for (const [layerIndex, layer] of promotedSeriesLayers.entries()) {
      const paths: OverlayPathShape[] = [];
      const rects: OverlayRectShape[] = [];
      const lines: OverlayLineShape[] = [];
      const circles: OverlayCircleShape[] = [];
      const appendSegment = (
        key: string, leftIndex: number, leftValue: number,
        rightIndex: number, rightValue: number, color: string
      ) => {
        const x1 = timeX(leftIndex);
        const x2 = timeX(rightIndex);
        const y1 = layer.scale.priceToCoordinate(leftValue);
        const y2 = layer.scale.priceToCoordinate(rightValue);
        if (x1 === null || x2 === null || y1 === null || y2 === null) return;
        paths.push({
          key,
          points: `${x1},${y1} ${x2},${y2}`,
          color,
          width: nativeSeriesLineWidth(layer.primitive),
          dashed: layer.primitive.style.dot_line
        });
      };

      if (layer.mode === 'line') {
        let previousIndex: number | null = null;
        let previousValue: number | null = null;
        let runStart: number | null = null;
        let runLength = 0;
        const flushSingleton = () => {
          if (runStart !== null && runLength === 1 &&
              layer.primitive.series_native_single_point_rule ===
                'x-minus-3-to-x-horizontal') {
            const value = document.points[runStart]?.values[layer.primitive.statement];
            const x = timeX(runStart);
            const y = finite(value) ? layer.scale.priceToCoordinate(value) : null;
            if (x !== null && y !== null) lines.push({
              key: `${layerIndex}-${runStart}-singleton`,
              x1: x - 3, y1: y, x2: x, y2: y,
              color: layer.fallback,
              width: nativeSeriesLineWidth(layer.primitive)
            });
          }
          runStart = null;
          runLength = 0;
        };
        for (const [index, point] of document.points.entries()) {
          const value = point.values[layer.primitive.statement];
          if (!finite(value)) {
            flushSingleton();
            previousIndex = null;
            previousValue = null;
            continue;
          }
          if (runStart === null) runStart = index;
          ++runLength;
          if (previousIndex !== null && previousValue !== null &&
              index === previousIndex + 1)
            appendSegment(`${layerIndex}-${previousIndex}-${index}`,
              previousIndex, previousValue, index, value, layer.fallback);
          previousIndex = index;
          previousValue = value;
        }
        flushSingleton();
      } else if (layer.mode === 'part-line') {
        const values = new Map<number, number>();
        const colors = new Map<number, string>();
        for (const event of layer.primitive.events ?? []) {
          const value = event.arguments[0];
          if (finite(value)) values.set(event.index, value);
          const direction = Math.trunc(event.arguments[2] ?? 0);
          const from = event.segment_from_index ??
            (direction === 1 ? event.index - 1 : event.index);
          if (from !== null && from >= 0) colors.set(from,
            (event.segment_color_available ?? finite(event.arguments[1]))
              ? colorFromRef(event.arguments[1], layer.fallback)
              : 'rgba(0,0,0,0)');
        }
        for (const [index, value] of values) {
          const right = values.get(index + 1);
          if (!finite(right)) continue;
          appendSegment(`${layerIndex}-${index}-${index + 1}`,
            index, value, index + 1, right,
            colors.get(index) ?? layer.fallback);
        }
      } else if (layer.mode === 'band-upper' || layer.mode === 'band-lower') {
        const argument = layer.mode === 'band-upper' ? 0 : 2;
        const colorArgument = layer.mode === 'band-upper' ? 1 : 3;
        const events = layer.primitive.events ?? [];
        for (let index = 0; index + 1 < events.length; ++index) {
          const left = events[index];
          const right = events[index + 1];
          const leftValue = left.arguments[argument];
          const rightValue = right.arguments[argument];
          if (right.index !== left.index + 1 || !finite(leftValue) || !finite(rightValue))
            continue;
          appendSegment(`${layerIndex}-${left.index}-${right.index}`,
            left.index, leftValue, right.index, rightValue,
            colorFromRef(left.arguments[colorArgument], layer.fallback));
        }
      } else if (layer.mode === 'histogram') {
        const baseline = layer.baseline ?? 0;
        const baseY = layer.scale.priceToCoordinate(baseline);
        if (baseY !== null) {
          const events = new Map(
            (layer.primitive.events ?? []).map((event) => [event.index, event])
          );
          const nativeStick = layer.primitive.series_stick_mode;
          const stickEnvironment = document.render_environment?.series_sticks;
          for (const [index, point] of document.points.entries()) {
            const value = point.values[layer.primitive.statement];
            if (!finite(value)) continue;
            const x = timeX(index);
            const y = layer.scale.priceToCoordinate(value);
            if (x === null || y === null) continue;
            const spacing = eventBarSpacing({ index, arguments: [] }, x);
            const event = events.get(index);
            if (layer.primitive.series_native_mode === 'stick' ||
                layer.primitive.series_native_mode === 'line-stick') {
              lines.push({
                key: `${layerIndex}-${index}`,
                x1: x, y1: y, x2: x, y2: baseY,
                color: layer.fallback,
                width: 1
              });
              continue;
            }
            if (nativeStick === 'color-stick') {
              const role = event?.series_stick_color_role;
              if (!role || role === 'none') continue;
              lines.push({
                key: `${layerIndex}-${index}`,
                x1: x, y1: y, x2: x, y2: baseY,
                color: role === 'up' ? palette().up : palette().down,
                width: 1
              });
              continue;
            }
            if (nativeStick === 'volume-stick') {
              const role = stickEnvironment?.vol_k_use_zt
                ? event?.series_stick_previous_close_color_role
                : event?.series_stick_open_close_color_role;
              if (!role) continue;
              const body = spacing >= 3
                ? spacing - Math.max(spacing * 0.25, 2)
                : Math.min(spacing, 1);
              const halfWidth = Math.max(0, Math.floor(body * 0.5));
              const color = role === 'up' ? palette().up : palette().down;
              if (halfWidth === 0) {
                lines.push({
                  key: `${layerIndex}-${index}`,
                  x1: x, y1: y, x2: x, y2: baseY, color, width: 1
                });
              } else {
                rects.push({
                  key: `${layerIndex}-${index}`,
                  x: x - halfWidth,
                  y: Math.min(y, baseY),
                  width: 2 * halfWidth + 1,
                  height: Math.max(1, Math.abs(y - baseY)),
                  fill: role === 'up' && !stickEnvironment?.real_up_k
                    ? 'none' : color,
                  stroke: color
                });
              }
              continue;
            }
            const width = Math.max(1, spacing * 0.72);
            const color = layer.volumeStick
              ? (point.close >= point.open ? palette().up : palette().down)
              : layer.colorStick ? (value >= baseline ? palette().up : palette().down)
                : layer.fallback;
            rects.push({
              key: `${layerIndex}-${index}`,
              x: x - width / 2,
              y: Math.min(y, baseY),
              width,
              height: Math.max(1, Math.abs(y - baseY)),
              fill: color,
              stroke: color
            });
          }
        }
      } else if (layer.mode === 'point-markers') {
        const mode = layer.primitive.series_native_mode;
        const width = lineWidth(layer.primitive.style);
        for (const [index, point] of document.points.entries()) {
          const value = point.values[layer.primitive.statement];
          if (!finite(value)) continue;
          const x = timeX(index);
          const y = layer.scale.priceToCoordinate(value);
          if (x === null || y === null) continue;
          const spacing = eventBarSpacing({ index, arguments: [] }, x);
          if (mode === 'circle-dot') {
            if (spacing < 6) {
              for (const [offsetIndex, [dx, dy]] of
                [[-1, 0], [0, -1], [0, 1], [1, 0]].entries()) rects.push({
                  key: `${layerIndex}-${index}-${offsetIndex}`,
                  x: x + dx, y: y + dy, width: 1, height: 1,
                  fill: layer.fallback, stroke: 'none'
                });
            } else circles.push({
              key: `${layerIndex}-${index}`,
              cx: x, cy: y, r: 3,
              fill: 'none', stroke: layer.fallback, width: 1
            });
          } else if (mode === 'cross-dot') {
            const radius = spacing < 5 ? 1 : spacing < 10 ? 2 : 3;
            lines.push({
              key: `${layerIndex}-${index}-a`,
              x1: x - radius, y1: y - radius,
              x2: x + radius, y2: y + radius,
              color: layer.fallback, width: 1
            });
            lines.push({
              key: `${layerIndex}-${index}-b`,
              x1: x - radius, y1: y + radius,
              x2: x + radius, y2: y - radius,
              color: layer.fallback, width: 1
            });
          } else if (mode === 'point-dot') {
            if (width < 2) rects.push({
              key: `${layerIndex}-${index}`,
              x, y, width: 1, height: 1,
              fill: layer.fallback, stroke: 'none'
            });
            else circles.push({
              key: `${layerIndex}-${index}`,
              cx: x, cy: y, r: width / 2,
              fill: layer.fallback, stroke: layer.fallback, width: 1
            });
          }
        }
      } else if (layer.mode === 'candlestick') {
        for (const [eventIndex, event] of (layer.primitive.events ?? []).entries()) {
          const [high, open, low, close] = event.arguments;
          if (!finite(high) || !finite(open) || !finite(low) || !finite(close)) continue;
          const x = timeX(event.index);
          const highY = layer.scale.priceToCoordinate(high);
          const openY = layer.scale.priceToCoordinate(open);
          const lowY = layer.scale.priceToCoordinate(low);
          const closeY = layer.scale.priceToCoordinate(close);
          if (x === null || highY === null || openY === null ||
              lowY === null || closeY === null) continue;
          const spacing = eventBarSpacing(event, x);
          const width = Math.max(1, spacing * 0.72);
          const color = close >= open ? palette().up : palette().down;
          lines.push({
            key: `${layerIndex}-${eventIndex}-wick`,
            x1: x, y1: highY, x2: x, y2: lowY, color, width: 1
          });
          rects.push({
            key: `${layerIndex}-${eventIndex}-body`,
            x: x - width / 2,
            y: Math.min(openY, closeY),
            width,
            height: Math.max(1, Math.abs(closeY - openY)),
            fill: color,
            stroke: color
          });
        }
      }

      if (paths.length || rects.length || lines.length || circles.length) groups.push({
        key: `series-${layerIndex}`,
        renderOrder: overlayZIndex(layer.primitive),
        componentOrder: layer.componentOrder,
        paths,
        rects,
        lines,
        circles
      });
    }
    promotedSeriesGroups = groups.sort((left, right) =>
      left.renderOrder - right.renderOrder || left.componentOrder - right.componentOrder
    );
  }

  function updateIconShapes() {
    if (!chart) {
      iconShapes = [];
      return;
    }
    const next: IconShape[] = [];
    for (const [layerIndex, layer] of iconLayers.entries()) {
      const width = layer.primitive.icon_sprite_cell_width ?? 18;
      const height = layer.primitive.icon_sprite_cell_height ?? 18;
      const spriteUrl = layer.primitive.icon_sprite_endpoint ??
        '/api/v1/formulas/drawicon-strip.png';
      for (const [eventIndex, event] of (layer.primitive.events ?? []).entries()) {
        if (event.icon_sprite_cell_available === false ||
            !finite(event.icon_sprite_x) || !finite(event.icon_type)) continue;
        const time = eventTime(event);
        const price = event.icon_price ?? event.arguments[1];
        if (time === null || !finite(price)) continue;
        const x = chart.timeScale().timeToCoordinate(time);
        const y = layer.scale.priceToCoordinate(price);
        if (x === null || y === null) continue;
        const alignment = event.icon_vertical_align ??
          (layer.primitive.style.draw_above ? 'above-price' : 'below-price');
        next.push({
          key: `${layerIndex}-${event.index}-${eventIndex}`,
          x, y, width, height,
          spriteX: event.icon_sprite_x,
          spriteUrl,
          transform: alignment === 'above-price'
            ? 'translate(-50%, calc(-100% - 2px))'
            : 'translate(-50%, 2px)',
          type: Math.trunc(event.icon_type),
          renderOrder: overlayZIndex(layer.primitive)
        });
      }
    }
    iconShapes = next;
  }

  function updateSequenceShapes() {
    if (!chart || !host) {
      sequenceShapes = [];
      return;
    }
    const next: SequenceShape[] = [];
    const paneHeight = Math.max(1, host.clientHeight - chart.timeScale().height());
    for (const [layerIndex, layer] of sequenceLayers.entries()) {
      let layerFontTableIndex: 1 | 13 =
        layer.primitive.annotation_effective_font_table_index ?? 1;
      const styleSeries = layer.primitive.sequence_style_series;
      if (styleSeries?.length) {
        for (let index = 0; index < document.points.length && index < styleSeries.length;
             ++index) {
          const point = document.points[index];
          const pointX = chart.timeScale().timeToCoordinate(
            barTime(point.date, point.time) as UTCTimestamp
          );
          if (pointX === null || pointX < 0) continue;
          layerFontTableIndex = styleSeries[index] === 2 ? 13 : 1;
          break;
        }
      }
      for (const [eventIndex, event] of (layer.primitive.events ?? []).entries()) {
        const point = document.points[event.index];
        const time = eventTime(event);
        if (!point || time === null || !finite(point.high) || !finite(point.low)) continue;
        const x = chart.timeScale().timeToCoordinate(time);
        const highY = layer.scale.priceToCoordinate(point.high);
        const lowY = layer.scale.priceToCoordinate(point.low);
        if (x === null || highY === null || lowY === null) continue;
        const style = event.sequence_style ?? 0;
        const label = markerText(layer.primitive, event);
        const alpha = event.sequence_label_class === 'alpha' ||
          (event.sequence_label_class === undefined && /^[A-Z]$/.test(label));
        const width = event.sequence_box_width_pixels ??
          (alpha ? (layer.primitive.sequence_box_width_alpha_pixels ?? 14) :
            (layer.primitive.sequence_box_width_numeric_pixels ?? 8));
        const height = event.sequence_box_height_pixels ??
          layer.primitive.sequence_box_height_pixels ?? 14;
        const gap = layer.primitive.sequence_anchor_gap_pixels ?? 2;
        const offset = event.sequence_offset_pixels ??
          (style === 0 ? 0 : (layer.primitive.sequence_nonzero_offset_pixels ?? 10));
        const belowTop = lowY + gap + offset;
        const aboveTop = highY - gap - offset - height;
        let side: 'above' | 'below';
        let y: number;
        if (layer.primitive.style.draw_above) {
          side = 'above';
          y = aboveTop;
          if (y <= 25) {
            side = 'below';
            y = belowTop;
          }
        } else {
          side = 'below';
          y = belowTop;
          if (y + height >= paneHeight - 40) {
            side = 'above';
            y = aboveTop;
          }
        }
        const color = colorFromStyle(layer.primitive.style, layer.fallback);
        const boxed = event.sequence_boxed ?? style === 2;
        next.push({
          key: `${layerIndex}-${event.index}-${eventIndex}`,
          x: x - width / 2,
          y,
          width,
          height,
          text: label,
          color,
          fill: boxed
            ? colorWithAlphaByte(color,
                layer.primitive.sequence_box_fill_alpha_byte ?? 0x50)
            : 'transparent',
          align: event.sequence_text_alignment ?? (alpha ? 'center' : 'right'),
          fontTableIndex: layerFontTableIndex,
          leader: event.sequence_leader ?? (style === 1 || style === 2),
          boxed,
          leaderFrom: side === 'below' ? 'top' : 'bottom',
          renderOrder: overlayZIndex(layer.primitive)
        });
      }
    }
    sequenceShapes = next;
  }

  function signalImageUrl(endpoint: string | undefined, name: string, format: 'bmp' | 'auto') {
    const base = endpoint ?? '/api/v1/formulas/signal-image';
    return `${base}?name=${encodeURIComponent(name)}&format=${format}`;
  }

  function updatePaneBackgroundShapes() {
    if (!chart || !host) {
      paneBackgroundGroups = [];
      return;
    }
    const next: OrderedGroup<BackgroundShape>[] = [];
    const paneWidth = Math.max(1, chart.timeScale().width());
    const paneHeight = Math.max(1, host.clientHeight - chart.timeScale().height());
    for (const [layerIndex, layer] of paneBackgroundLayers.entries()) {
      const event = layer.primitive.events?.[0];
      if (!event) continue;
      let background = 'transparent';
      if (event.pane_background_mode === 'gradient') {
        const color1 = colorFromRef(event.pane_background_color1_ref, layer.fallback);
        const color2 = colorFromRef(event.pane_background_color2_ref, color1);
        background = event.pane_background_horizontal
          ? `linear-gradient(to right, ${color1}, ${color2})`
          : `linear-gradient(to bottom, ${color1}, ${color2})`;
      } else if (event.pane_background_name_available && event.pane_background_name) {
        const url = signalImageUrl(layer.primitive.pane_background_resource_endpoint,
          event.pane_background_name, 'auto');
        background = `url("${url}") no-repeat left top / ${event.pane_background_stretch ? '100% 100%' : 'auto'}`;
      }
      if (background === 'transparent') continue;
      next.push({
        key: `pane-background-${layerIndex}`,
        renderOrder: overlayZIndex(layer.primitive),
        shapes: [{
          key: `${layerIndex}-${event.index}`,
          x: 0, y: 0, width: paneWidth, height: paneHeight,
          background, border: 'transparent'
        }]
      });
    }
    paneBackgroundGroups = next;
  }

  function updatePaneRectangleShapes() {
    if (!chart || !host) {
      paneRectangleGroups = [];
      return;
    }
    const next: OrderedGroup<StickShape>[] = [];
    const paneWidth = Math.max(1, chart.timeScale().width());
    const paneHeight = Math.max(1, host.clientHeight - chart.timeScale().height());
    for (const [layerIndex, layer] of paneRectangleLayers.entries()) {
      const shapes: StickShape[] = [];
      for (const [eventIndex, event] of (layer.primitive.events ?? []).entries()) {
        if (!finite(event.rectangle_left) || !finite(event.rectangle_top) ||
            !finite(event.rectangle_right) || !finite(event.rectangle_bottom)) continue;
        const left = event.rectangle_left * paneWidth / 1000;
        const top = event.rectangle_top * paneHeight / 1000;
        const right = event.rectangle_right * paneWidth / 1000;
        const bottom = event.rectangle_bottom * paneHeight / 1000;
        const color = colorFromRef(event.rectangle_color_ref, layer.fallback);
        shapes.push({
          key: `${layerIndex}-${eventIndex}`,
          x: Math.min(left, right), y: Math.min(top, bottom),
          width: Math.max(1, Math.abs(right - left)),
          height: Math.max(1, Math.abs(bottom - top)),
          fill: event.rectangle_fill ? color : 'none',
          stroke: event.rectangle_frame ? layer.fallback : 'none',
          dashed: false
        });
      }
      if (shapes.length) next.push({
        key: `pane-rectangle-${layerIndex}`,
        renderOrder: overlayZIndex(layer.primitive), shapes
      });
    }
    paneRectangleGroups = next;
  }

  function updateBitmapShapes() {
    if (!chart) {
      bitmapShapes = [];
      return;
    }
    const next: BitmapShape[] = [];
    for (const [layerIndex, layer] of bitmapLayers.entries()) {
      for (const [eventIndex, event] of (layer.primitive.events ?? []).entries()) {
        const time = eventTime(event);
        const price = event.bitmap_price ?? event.arguments[1];
        if (time === null || !finite(price) || !event.bitmap_name) continue;
        const x = chart.timeScale().timeToCoordinate(time);
        const y = layer.scale.priceToCoordinate(price);
        if (x === null || y === null) continue;
        next.push({
          key: `${layerIndex}-${event.index}-${eventIndex}`,
          x, y,
          url: signalImageUrl(layer.primitive.bitmap_resource_endpoint,
            event.bitmap_name, 'bmp'),
          renderOrder: overlayZIndex(layer.primitive)
        });
      }
    }
    bitmapShapes = next;
  }

  function updateBackgroundShapes() {
    if (!chart || !host) {
      backgroundGroups = [];
      return;
    }
    const next: OrderedGroup<BackgroundShape>[] = [];
    const paneHeight = Math.max(1, host.clientHeight - chart.timeScale().height());
    for (const [layerIndex, layer] of backgroundLayers.entries()) {
      const shapes: BackgroundShape[] = [];
      for (const [eventIndex, event] of (layer.primitive.events ?? []).entries()) {
        if (event.background_region_leader === false) continue;
        const startIndex = event.background_region_start_index ?? event.index;
        const endIndex = event.background_region_end_index ?? event.index;
        const startPoint = document.points[startIndex];
        const endPoint = document.points[endIndex];
        if (!startPoint || !endPoint) continue;
        const startCenter = chart.timeScale().timeToCoordinate(
          barTime(startPoint.date, startPoint.time) as UTCTimestamp
        );
        const endCenter = chart.timeScale().timeToCoordinate(
          barTime(endPoint.date, endPoint.time) as UTCTimestamp
        );
        if (startCenter === null || endCenter === null) continue;
        const startSpacing = eventBarSpacing({ ...event, index: startIndex }, startCenter);
        const endSpacing = eventBarSpacing({ ...event, index: endIndex }, endCenter);
        const color1 = colorFromRef(event.background_color1_ref ?? event.arguments[1],
          layer.fallback);
        const color2 = colorFromRef(event.background_color2_ref ?? event.arguments[2],
          color1);
        const mode = event.background_fill_mode ?? Math.trunc(event.arguments[3] ?? 0);
        const range = event.background_range ?? Math.trunc(event.arguments[4] ?? 0);
        const alphaByte = event.background_fill_alpha_byte ??
          (mode >= 10 && mode <= 20 ? Math.trunc(255 * (mode - 10) / 10) : 255);
        let y = 0;
        let height = paneHeight;
        if (range === 1 || range === 2) {
          const topPrice = event.background_price_top;
          const bottomPrice = event.background_price_bottom;
          if (!finite(topPrice) || !finite(bottomPrice)) continue;
          const topY = layer.scale.priceToCoordinate(topPrice);
          const bottomY = layer.scale.priceToCoordinate(bottomPrice);
          if (topY === null || bottomY === null) continue;
          y = Math.min(topY, bottomY);
          height = Math.max(1, Math.abs(bottomY - topY));
        }
        const background = mode === 0
          ? `linear-gradient(to bottom, ${color1}, ${color2})`
          : mode === 1
            ? `linear-gradient(to right, ${color1}, ${color2})`
            : mode === 2 ? 'transparent'
              : mode === 3 ? color2
                : mode >= 10 && mode <= 20
                  ? colorWithAlphaByte(color1, alphaByte)
                  : 'transparent';
        const x = startCenter - startSpacing / 2;
        shapes.push({
          key: `${layerIndex}-${eventIndex}-${event.index}`,
          x,
          y,
          width: Math.max(1, endCenter + endSpacing / 2 + 1 - x),
          height,
          background,
          border: mode === 2 || mode === 3 ? color1 : 'transparent'
        });
      }
      if (shapes.length) next.push({
        key: `background-${layerIndex}`,
        renderOrder: overlayZIndex(layer.primitive),
        shapes
      });
    }
    backgroundGroups = next;
  }

  function updateFormulaSegments() {
    if (!chart) {
      segmentGroups = [];
      return;
    }
    const next: OrderedGroup<FormulaSegment>[] = [];
    for (const [layerIndex, layer] of segmentLayers.entries()) {
      const shapes: FormulaSegment[] = [];
      for (const [eventIndex, event] of (layer.primitive.events ?? []).entries()) {
        const fromIndex = event.segment_from_index;
        const fromPrice = event.segment_from_price;
        if (fromIndex === null || fromIndex === undefined || !finite(fromPrice)) continue;
        const fromPoint = document.points[fromIndex];
        if (!fromPoint) continue;
        const x1 = chart.timeScale().timeToCoordinate(
          barTime(fromPoint.date, fromPoint.time) as UTCTimestamp
        );
        const y1 = layer.scale.priceToCoordinate(fromPrice);
        if (x1 === null || y1 === null) continue;
        let x2: number | null = null;
        let y2: number | null = null;
        if (event.slope_vertical && finite(event.slope_vertical_pixel_delta)) {
          x2 = x1;
          y2 = y1 + event.slope_vertical_pixel_delta;
        } else {
          const toIndex = event.segment_to_index;
          const toPrice = event.segment_to_price;
          if (toIndex === null || toIndex === undefined || !finite(toPrice)) continue;
          const toPoint = document.points[toIndex];
          if (!toPoint) continue;
          x2 = chart.timeScale().timeToCoordinate(
            barTime(toPoint.date, toPoint.time) as UTCTimestamp
          );
          y2 = layer.scale.priceToCoordinate(toPrice);
        }
        if (x2 === null || y2 === null) continue;
        shapes.push({
          key: `${layerIndex}-${eventIndex}-${fromIndex}-${event.segment_to_index ?? fromIndex}`,
          x1, y1, x2, y2,
          color: colorFromStyle(layer.primitive.style, layer.fallback),
          width: lineWidth(layer.primitive.style),
          dashed: layer.primitive.style.dot_line
        });
      }
      if (shapes.length) next.push({
        key: `segment-${layerIndex}`,
        renderOrder: overlayZIndex(layer.primitive),
        shapes
      });
    }
    segmentGroups = next;
  }

  function eventBarSpacing(event: FormulaRenderEvent, x: number): number {
    if (!chart) return 6;
    for (const adjacentIndex of [event.index + 1, event.index - 1]) {
      const point = document.points[adjacentIndex];
      if (!point) continue;
      const adjacent = chart.timeScale().timeToCoordinate(
        barTime(point.date, point.time) as UTCTimestamp
      );
      if (adjacent !== null && Math.abs(adjacent - x) > 0.01)
        return Math.abs(adjacent - x);
    }
    const configured = chart.timeScale().options().barSpacing;
    return finite(configured) && configured > 0 ? configured : 6;
  }

  function updateStickShapes() {
    if (!chart || !host) {
      stickGroups = [];
      return;
    }
    const next: OrderedGroup<StickShape>[] = [];
    const paneHeight = Math.max(1, host.clientHeight - chart.timeScale().height());
    for (const [layerIndex, layer] of stickLayers.entries()) {
      const shapes: StickShape[] = [];
      for (const [eventIndex, event] of (layer.primitive.events ?? []).entries()) {
        const time = eventTime(event);
        const price2 = event.arguments[2];
        if (time === null || !finite(price2)) continue;
        const x = chart.timeScale().timeToCoordinate(time);
        const price2Y = layer.scale.priceToCoordinate(price2);
        if (x === null || price2Y === null) continue;
        const centered = event.stick_anchor === 'pane-middle' ||
          event.stick_mode === 'center-full' || event.stick_mode === 'center-half';
        const price1 = event.arguments[1];
        const price1Y = centered
          ? paneHeight / 2
          : finite(price1) ? layer.scale.priceToCoordinate(price1) : null;
        if (price1Y === null) continue;
        const spacing = eventBarSpacing(event, x);
        const rawWidth = finite(event.stick_width)
          ? event.stick_width
          : finite(event.arguments[3]) ? event.arguments[3] : 0;
        const widthRatio = finite(event.stick_width_ratio)
          ? event.stick_width_ratio
          : rawWidth > 0 ? rawWidth / (layer.primitive.stick_width_standard ?? 4) : 0;
        const occupancy = event.stick_occupancy ?? 'specified';
        const occupancyRatio = occupancy === 'half' ? 0.5 : 1;
        const width = rawWidth <= 0
          ? 1
          : Math.max(1, spacing * Math.abs(widthRatio) * occupancyRatio);
        const top = Math.min(price1Y, price2Y);
        const height = Math.max(1, Math.abs(price2Y - price1Y));
        const color = colorFromStyle(layer.primitive.style, layer.fallback);
        const mode = event.stick_mode ??
          (event.arguments[4] === 0 ? 'solid' : event.arguments[4] === -1
            ? 'dashed-hollow' : 'solid-hollow');
        const hollow = event.stick_hollow ??
          (mode === 'solid-hollow' || mode === 'dashed-hollow');
        shapes.push({
          key: `${layerIndex}-${event.index}-${eventIndex}`,
          x: x - width / 2,
          y: top,
          width,
          height,
          fill: hollow ? 'none' : color,
          stroke: color,
          dashed: event.stick_border_dashed ?? mode === 'dashed-hollow'
        });
      }
      if (shapes.length) next.push({
        key: `stick-${layerIndex}`,
        renderOrder: overlayZIndex(layer.primitive),
        shapes
      });
    }
    stickGroups = next;
  }

  function materializedAnnotationText(
    primitive: FormulaRenderPrimitive,
    event: FormulaRenderEvent,
    fixed: boolean
  ): string | null {
    if (event.annotation_text_available === false) return null;
    if (event.annotation_lines?.length) return event.annotation_lines.join('\n');
    if (event.annotation_text !== undefined && event.annotation_text !== null)
      return event.annotation_text.replace(/&/g, '\n');
    const argument = fixed ? 4 : 2;
    const stringValue = event.string_arguments?.[String(argument)];
    if (stringValue !== undefined) return stringValue.replace(/&/g, '\n');
    const numericValue = event.arguments[argument];
    return finite(numericValue) ? Number(numericValue.toPrecision(12)).toString() : null;
  }

  function updatePriceAnnotations() {
    if (!chart) {
      priceAnnotations = [];
      return;
    }
    const next: PriceAnnotation[] = [];
    const paneHeight = host
      ? Math.max(1, host.clientHeight - chart.timeScale().height()) : 1;
    for (const [layerIndex, layer] of annotationLayers.entries()) {
      for (const [eventIndex, event] of (layer.primitive.events ?? []).entries()) {
        const time = eventTime(event);
        const text = materializedAnnotationText(layer.primitive, event, false);
        if (time === null || text === null || text.length === 0) continue;
        const x = chart.timeScale().timeToCoordinate(time);
        if (x === null) continue;
        const color = colorFromStyle(layer.primitive.style, layer.fallback);
        const framed = (event.annotation_frame ?? false) &&
          layer.primitive.annotation_frame_supported === true;
        if (framed) {
          const point = document.points[event.index];
          const highY = point ? layer.scale.priceToCoordinate(point.high) : null;
          const lowY = point ? layer.scale.priceToCoordinate(point.low) : null;
          if (highY === null || lowY === null) continue;
          const side: 'above' | 'below' = paneHeight - lowY <= highY
            ? 'above' : 'below';
          const leader = layer.primitive.annotation_frame_leader_length_pixels ?? 20;
          const horizontalShift =
            (layer.primitive.annotation_frame_width_padding_pixels ?? 5) / 2;
          const transform = side === 'above'
            ? `translate(calc(-50% + ${horizontalShift}px), calc(-100% - ${leader}px))`
            : `translate(calc(-50% + ${horizontalShift}px), ${leader}px)`;
          const lines = event.annotation_lines?.length
            ? event.annotation_lines : text.split('\n');
          for (const [lineIndex, line] of lines.entries()) {
            if (!line.length) continue;
            next.push({
              key: `${layerIndex}-${event.index}-${eventIndex}-${lineIndex}`,
              x,
              y: side === 'above' ? highY : lowY,
              text: line,
              color,
              transform,
              framed: true,
              nativeUnframed: false,
              frameSide: side,
              frameFill: colorWithAlphaByte(
                color, layer.primitive.annotation_frame_fill_alpha_byte ?? 0x50),
              renderOrder: overlayZIndex(layer.primitive)
            });
          }
          continue;
        }
        const price = event.annotation_price ?? event.arguments[1];
        if (!finite(price)) continue;
        const y = layer.scale.priceToCoordinate(price);
        if (y === null) continue;
        const metrics = annotationFontMetrics();
        const rowHeight = annotationChartRowHeight();
        const drawAbove = event.annotation_drawabove ??
          layer.primitive.style.draw_above;
        if (layer.primitive.function === 'DRAWTEXT') {
          const lines = event.annotation_lines?.length
            ? event.annotation_lines : text.split('\n').slice(0, 10);
          let lineY = y - (drawAbove ? lines.length * (rowHeight - 2) : 0);
          for (const [lineIndex, line] of lines.entries()) {
            if (line.length) next.push({
              key: `${layerIndex}-${event.index}-${eventIndex}-${lineIndex}`,
              x: x + (event.annotation_x_offset_pixels ?? 0),
              y: lineY + (event.annotation_y_offset_pixels ?? -8) +
                metrics.textoutYAdjustment,
              text: line,
              color,
              transform: 'none',
              framed: false,
              nativeUnframed: true,
              frameSide: null,
              frameFill: 'transparent',
              renderOrder: overlayZIndex(layer.primitive)
            });
            // Native GetCmdLine advances an empty segment by the measured
            // height of "A" and a non-empty segment by its measured height.
            lineY += metrics.measuredHeight;
          }
          continue;
        }
        next.push({
          key: `${layerIndex}-${event.index}-${eventIndex}`,
          x: x + (event.annotation_x_offset_pixels ?? -3),
          y: y + (drawAbove ? 2 - rowHeight : 0) +
            metrics.textoutYAdjustment,
          text,
          color,
          transform: 'none',
          framed: false,
          nativeUnframed: true,
          frameSide: null,
          frameFill: 'transparent',
          renderOrder: overlayZIndex(layer.primitive)
        });
      }
    }
    priceAnnotations = next;
  }

  function updateBandPolygons() {
    if (!chart) {
      bandGroups = [];
      return;
    }
    const next: OrderedGroup<BandPolygon>[] = [];
    for (const [layerIndex, layer] of bandLayers.entries()) {
      const shapes: BandPolygon[] = [];
      const events = layer.primitive.events ?? [];
      const fill = (event: FormulaRenderEvent) => {
        if (event.fill_color_available === false) return 'rgba(0,0,0,0)';
        const firstAbove = finite(event.arguments[0]) && finite(event.arguments[2]) &&
          event.arguments[0] > event.arguments[2];
        const argument = event.fill_color_argument ?? (firstAbove ? 1 : 3);
        return colorFromRef(event.fill_color_ref ?? event.arguments[argument], layer.fallback);
      };
      const append = (
        key: string, x1: number, upper1: number, lower1: number,
        x2: number, upper2: number, lower2: number, color: string
      ) => {
        if (color === 'rgba(0,0,0,0)') return;
        shapes.push({
          key,
          points: `${x1},${upper1} ${x2},${upper2} ${x2},${lower2} ${x1},${lower1}`,
          fill: color
        });
      };
      for (let index = 0; index + 1 < events.length; ++index) {
        const left = events[index];
        const right = events[index + 1];
        if (right.index !== left.index + 1) continue;
        const leftTime = eventTime(left);
        const rightTime = eventTime(right);
        if (leftTime === null || rightTime === null) continue;
        const x1 = chart.timeScale().timeToCoordinate(leftTime);
        const x2 = chart.timeScale().timeToCoordinate(rightTime);
        const upperValue1 = left.arguments[0];
        const lowerValue1 = left.arguments[2];
        const upperValue2 = right.arguments[0];
        const lowerValue2 = right.arguments[2];
        if (x1 === null || x2 === null || !finite(upperValue1) || !finite(lowerValue1) ||
            !finite(upperValue2) || !finite(lowerValue2)) continue;
        const upper1 = layer.scale.priceToCoordinate(upperValue1);
        const lower1 = layer.scale.priceToCoordinate(lowerValue1);
        const upper2 = layer.scale.priceToCoordinate(upperValue2);
        const lower2 = layer.scale.priceToCoordinate(lowerValue2);
        if (upper1 === null || lower1 === null || upper2 === null || lower2 === null) continue;
        const difference1 = upperValue1 - lowerValue1;
        const difference2 = upperValue2 - lowerValue2;
        if (difference1 * difference2 < 0) {
          const ratio = Math.abs(difference1) /
            (Math.abs(difference1) + Math.abs(difference2));
          const crossX = x1 + (x2 - x1) * ratio;
          const crossValue = upperValue1 + (upperValue2 - upperValue1) * ratio;
          const crossY = layer.scale.priceToCoordinate(crossValue);
          if (crossY === null) continue;
          append(`${layerIndex}-${index}-a`, x1, upper1, lower1,
            crossX, crossY, crossY, fill(left));
          append(`${layerIndex}-${index}-b`, crossX, crossY, crossY,
            x2, upper2, lower2, fill(right));
        } else {
          append(`${layerIndex}-${index}`, x1, upper1, lower1,
            x2, upper2, lower2, fill(left));
        }
      }
      if (shapes.length) next.push({
        key: `band-${layerIndex}`,
        renderOrder: overlayZIndex(layer.primitive),
        shapes
      });
    }
    bandGroups = next;
  }

  function markerText(primitive: FormulaRenderPrimitive, event: FormulaRenderEvent): string {
    if (primitive.function === 'DRAWNUMBER_DIF' && event.sequence_label !== undefined) {
      return event.sequence_label;
    }
    const materialized = event.string_arguments?.['2'];
    if (materialized !== undefined) return materialized;
    const value = event.arguments[2];
    return finite(value) ? Number(value.toFixed(4)).toString() : '';
  }

  function addMarkers(primitive: FormulaRenderPrimitive, fallback: string) {
    if (!chart || !primitive.style.visible) return;
    const anchor = chart.addSeries(LineSeries, {
      title: '', color: 'rgba(0,0,0,0)', lineVisible: false,
      pointMarkersVisible: false, priceLineVisible: false, lastValueVisible: false,
      crosshairMarkerVisible: false, priceScaleId: 'right'
    });
    const data: LineData<UTCTimestamp>[] = [];
    const extentData: LineData<UTCTimestamp>[] = [];
    for (const event of primitive.events ?? []) {
      const time = eventTime(event);
      const point = document.points[event.index];
      const price = primitive.style.draw_above ? point?.high : point?.low;
      const extentPrice = primitive.style.draw_above ? point?.low : point?.high;
      if (time === null || !finite(price) || !finite(extentPrice)) continue;
      data.push({ time, value: price });
      extentData.push({ time, value: extentPrice });
    }
    anchor.setData(data);
    const extent = chart.addSeries(LineSeries, {
      title: '', color: 'rgba(0,0,0,0)', lineVisible: false,
      pointMarkersVisible: false, priceLineVisible: false, lastValueVisible: false,
      crosshairMarkerVisible: false, priceScaleId: 'right'
    });
    extent.setData(extentData);
    sequenceLayers.push({ primitive, scale: anchor, fallback });
    registerSeries(anchor, primitive);
    registerSeries(extent, primitive, 1);
  }

  function addIcons(primitive: FormulaRenderPrimitive) {
    if (!chart || !primitive.style.visible) return;
    const anchor = chart.addSeries(LineSeries, {
      title: '', color: 'rgba(0,0,0,0)', lineVisible: false,
      pointMarkersVisible: false, priceLineVisible: false, lastValueVisible: false,
      crosshairMarkerVisible: false, priceScaleId: 'right'
    });
    const data: LineData<UTCTimestamp>[] = [];
    for (const event of primitive.events ?? []) {
      if (event.icon_sprite_cell_available === false) continue;
      const time = eventTime(event);
      const price = event.icon_price ?? event.arguments[1];
      if (time === null || !finite(price)) continue;
      data.push({ time, value: price });
    }
    anchor.setData(data);
    registerSeries(anchor, primitive);
    iconLayers.push({ primitive, scale: anchor });
  }

  function collectFixedText(primitive: FormulaRenderPrimitive, fallback: string): FixedLabel[] {
    const fixed = primitive.function === 'DRAWTEXT_FIX' ||
      primitive.function === 'DRAWNUMBER_FIX';
    if (!primitive.style.visible || !fixed) return [];
    return (primitive.events ?? []).flatMap((event, index) => {
      const x = event.annotation_x ?? event.arguments[1];
      const y = event.annotation_y ?? event.arguments[2];
      const label = materializedAnnotationText(primitive, event, true);
      if (!finite(x) || !finite(y) || label === null || label.length === 0) return [];
      return [{
        key: `${primitive.statement}-${event.index}-${index}`,
        text: label,
        x: Math.max(0, Math.min(1, x)),
        y: Math.max(0, Math.min(1, y)),
        color: colorFromStyle(primitive.style, fallback),
        align: event.annotation_horizontal_align ??
          (Math.trunc(event.arguments[3] ?? 0) === 1 ? 'right' : 'left'),
        framed: (event.annotation_frame ?? false) &&
          primitive.annotation_frame_supported === true,
        renderOrder: overlayZIndex(primitive)
      }];
    });
  }

  function draw() {
    if (!chart) return;
    for (const item of series) chart.removeSeries(item);
    series = [];
    orderedSeries = [];
    bandLayers = [];
    stickLayers = [];
    annotationLayers = [];
    segmentLayers = [];
    backgroundLayers = [];
    bitmapLayers = [];
    paneBackgroundLayers = [];
    paneRectangleLayers = [];
    iconLayers = [];
    sequenceLayers = [];
    promotedSeriesLayers = [];
    promotedSeriesGroups = [];
    orderedOverlayMode = false;
    bandGroups = [];
    stickGroups = [];
    priceAnnotations = [];
    segmentGroups = [];
    backgroundGroups = [];
    paneBackgroundGroups = [];
    paneRectangleGroups = [];
    bitmapShapes = [];
    iconShapes = [];
    sequenceShapes = [];
    const p = palette();
    const colors = [p.focus, p.warn, p.up, p.down, p.neutral];
    const nextFixedLabels: FixedLabel[] = [];
    const primitives = [...(document.render_ir?.primitives ?? [])]
      .map((primitive, index) => ({ primitive, index }))
      .sort((left, right) => primitiveOrder(left.primitive) - primitiveOrder(right.primitive) ||
        left.index - right.index)
      .map(({ primitive }) => primitive);
    const hasCoordinateOverlay = primitives.some((primitive) =>
      primitive.style.visible && !['line', 'part-line', 'candlestick'].includes(primitive.kind)
    );
    const hasVisibleSeries = primitives.some((primitive) =>
      primitive.style.visible && ['line', 'part-line', 'candlestick', 'band'].includes(primitive.kind)
    );
    orderedOverlayMode = hasCoordinateOverlay && hasVisibleSeries;
    const primitiveByStatement = new Map(
      primitives.filter((item) => item.kind === 'line').map((item) => [item.statement, item])
    );
    const graphicalStatements = new Set(
      primitives.filter((item) => item.kind !== 'line').map((item) => item.statement)
    );
    for (const [index, output] of document.outputs.entries()) {
      if (graphicalStatements.has(output)) continue;
      const primitive = primitiveByStatement.get(output);
      if (primitive && !primitive.style.visible) continue;
      const colorStick = primitive?.style.directives.includes('COLORSTICK') ?? false;
      const volumeStick = primitive?.style.directives.includes('VOLSTICK') ?? false;
      const nativeSeriesStick = primitive?.series_stick_mode;
      const histogram = colorStick || volumeStick ||
        (document.formula === 'MACD' && output === 'MACD');
      if (histogram) {
        const seriesColor = orderedOverlayMode ? 'rgba(0,0,0,0)' : undefined;
        const overlayHistogram = orderedOverlayMode || nativeSeriesStick !== undefined;
        const item = chart.addSeries(HistogramSeries, {
          title: output,
          color: seriesColor,
          priceScaleId: 'right',
          priceLineVisible: !orderedOverlayMode,
          lastValueVisible: !orderedOverlayMode,
          priceFormat: { type: 'price', precision: 4, minMove: 0.0001 }
        });
        const data: HistogramData<UTCTimestamp>[] = [];
        for (const point of document.points) {
          const value = point.values[output];
          if (value === null || !Number.isFinite(value)) continue;
          data.push({
            time: barTime(point.date, point.time) as UTCTimestamp,
            value,
            color: overlayHistogram ? 'rgba(0,0,0,0)' : volumeStick
              ? (point.close >= point.open ? p.up : p.down)
              : colorStick || (document.formula === 'MACD' && output === 'MACD')
                ? (value >= 0 ? p.up : p.down)
                : colorFromStyle(primitive?.style, colors[index % colors.length])
          });
        }
        item.setData(data);
        registerSeries(item, primitive ?? null);
        if (overlayHistogram && primitive) promotedSeriesLayers.push({
          primitive,
          scale: item,
          fallback: colorFromStyle(primitive.style, colors[index % colors.length]),
          mode: 'histogram',
          componentOrder: 0,
          colorStick,
          volumeStick,
          baseline: 0
        });
      } else if (primitive) {
        const fallback = colors[index % colors.length];
        if (primitive.series_native_mode === 'stick')
          addNativeStemPrimitive(primitive, fallback);
        else if (primitive.series_native_mode === 'line-stick' || primitive.line_stick)
          addLineStickPrimitive(primitive, fallback);
        else if (primitive.series_native_mode === 'circle-dot' ||
                 primitive.series_native_mode === 'cross-dot' ||
                 primitive.series_native_mode === 'point-dot')
          addPointMarkerPrimitive(primitive, fallback);
        else addLinePrimitive(primitive, fallback);
      } else {
        const item = chart.addSeries(LineSeries, {
          title: output,
          color: colors[index % colors.length],
          lineWidth: 1,
          priceScaleId: 'right',
          priceFormat: { type: 'price', precision: 4, minMove: 0.0001 }
        });
        const data: LineData<UTCTimestamp>[] = [];
        for (const point of document.points) {
          const value = point.values[output];
          if (value === null || !Number.isFinite(value)) continue;
          data.push({ time: barTime(point.date, point.time) as UTCTimestamp, value });
        }
        item.setData(data);
        registerSeries(item, null);
      }
    }
    for (const [index, primitive] of primitives.entries()) {
      const fallback = colorFromStyle(primitive.style, colors[index % colors.length]);
      if (primitive.kind === 'part-line') addPartLine(primitive, fallback);
      else if (primitive.kind === 'draw-line' || primitive.kind === 'polyline' ||
               primitive.kind === 'slope-line')
        addSegmentPrimitive(primitive, fallback);
      else if (primitive.kind === 'stick') addStick(primitive, fallback);
      else if (primitive.kind === 'candlestick') addCandlesticks(primitive, fallback);
      else if (primitive.kind === 'band') addBand(primitive, fallback);
      else if (primitive.kind === 'background') addBackground(primitive, fallback);
      else if (primitive.kind === 'pane-background') addPaneBackground(primitive, fallback);
      else if (primitive.kind === 'pane-rectangle') addPaneRectangle(primitive, fallback);
      else if (primitive.kind === 'bitmap') addBitmap(primitive);
      else if (primitive.function === 'DRAWTEXT' || primitive.function === 'DRAWNUMBER')
        addPriceAnnotations(primitive, fallback);
      else if (primitive.kind === 'icon') addIcons(primitive);
      else if (primitive.kind === 'sequence-number')
        addMarkers(primitive, fallback);
      nextFixedLabels.push(...collectFixedText(primitive, fallback));
    }
    applySeriesOrder();
    fixedLabels = nextFixedLabels;
    if (document.points.length) {
      chart.timeScale().setVisibleLogicalRange({
        from: Math.max(0, document.points.length - 180),
        to: document.points.length + 5
      });
    }
    scheduleOverlays();
  }

  onMount(() => {
    if (!host) return;
    chart = createChart(host, baseOptions(palette()));
    const repaintOverlays = () => scheduleOverlays();
    chart.timeScale().subscribeVisibleLogicalRangeChange(repaintOverlays);
    const resizeObserver = new ResizeObserver(repaintOverlays);
    resizeObserver.observe(host);
    draw();
    const stopTheme = watchTheme(() => {
      chart?.applyOptions(baseOptions(palette()));
      draw();
    });
    return () => {
      stopTheme();
      resizeObserver.disconnect();
      chart?.timeScale().unsubscribeVisibleLogicalRangeChange(repaintOverlays);
      if (overlayFrame) cancelAnimationFrame(overlayFrame);
      chart?.remove();
      chart = null;
    };
  });

  $effect(() => {
    void document;
    if (chart) draw();
  });
</script>

<div class="chart-shell">
  <div class="chart" bind:this={host}></div>
  {#each promotedSeriesGroups as group (group.key)}
    <svg
      class="ordered-series-layer"
      style={`z-index:${group.renderOrder}`}
      width="100%"
      height="100%"
      aria-hidden="true"
    >
      {#each group.lines as line (line.key)}
        <line
          x1={line.x1}
          y1={line.y1}
          x2={line.x2}
          y2={line.y2}
          stroke={line.color}
          stroke-width={line.width}
          vector-effect="non-scaling-stroke"
        ></line>
      {/each}
      {#each group.rects as rect (rect.key)}
        <rect
          x={rect.x}
          y={rect.y}
          width={rect.width}
          height={rect.height}
          fill={rect.fill}
          stroke={rect.stroke}
          stroke-width="1"
          vector-effect="non-scaling-stroke"
        ></rect>
      {/each}
      {#each group.circles as circle (circle.key)}
        <circle
          cx={circle.cx}
          cy={circle.cy}
          r={circle.r}
          fill={circle.fill}
          stroke={circle.stroke}
          stroke-width={circle.width}
          vector-effect="non-scaling-stroke"
        ></circle>
      {/each}
      {#each group.paths as path (path.key)}
        <polyline
          points={path.points}
          fill="none"
          stroke={path.color}
          stroke-width={path.width}
          stroke-dasharray={path.dashed ? '1 2' : undefined}
          stroke-linecap={path.dashed ? 'butt' : 'round'}
          stroke-linejoin="round"
          vector-effect="non-scaling-stroke"
        ></polyline>
      {/each}
    </svg>
  {/each}
  {#each backgroundGroups as group (group.key)}
    <div class="background-layer" style={`z-index:${group.renderOrder}`} aria-hidden="true">
      {#each group.shapes as shape (shape.key)}
        <span
          class="background-shape"
          style={`left:${shape.x}px;top:${shape.y}px;width:${shape.width}px;height:${shape.height}px;background:${shape.background};border-color:${shape.border}`}
        ></span>
      {/each}
    </div>
  {/each}
  {#each paneBackgroundGroups as group (group.key)}
    <div class="background-layer" style={`z-index:${group.renderOrder}`} aria-hidden="true">
      {#each group.shapes as shape (shape.key)}
        <span
          class="background-shape"
          style={`left:${shape.x}px;top:${shape.y}px;width:${shape.width}px;height:${shape.height}px;background:${shape.background};border-color:${shape.border}`}
        ></span>
      {/each}
    </div>
  {/each}
  {#each paneRectangleGroups as group (group.key)}
    <svg class="stick-layer" style={`z-index:${group.renderOrder}`} width="100%" height="100%" aria-hidden="true">
      {#each group.shapes as shape (shape.key)}
        <rect
          x={shape.x}
          y={shape.y}
          width={shape.width}
          height={shape.height}
          fill={shape.fill}
          stroke={shape.stroke}
          stroke-width="1"
          vector-effect="non-scaling-stroke"
        ></rect>
      {/each}
    </svg>
  {/each}
  {#each bandGroups as group (group.key)}
    <svg class="band-layer" style={`z-index:${group.renderOrder}`} width="100%" height="100%" aria-hidden="true">
      {#each group.shapes as polygon (polygon.key)}
        <polygon points={polygon.points} fill={polygon.fill}></polygon>
      {/each}
    </svg>
  {/each}
  {#each stickGroups as group (group.key)}
    <svg class="stick-layer" style={`z-index:${group.renderOrder}`} width="100%" height="100%" aria-hidden="true">
      {#each group.shapes as shape (shape.key)}
        <rect
          x={shape.x}
          y={shape.y}
          width={shape.width}
          height={shape.height}
          fill={shape.fill}
          stroke={shape.stroke}
          stroke-width="1"
          stroke-dasharray={shape.dashed ? '3 2' : undefined}
          vector-effect="non-scaling-stroke"
        ></rect>
      {/each}
    </svg>
  {/each}
  {#each segmentGroups as group (group.key)}
    <svg class="formula-segment-layer" style={`z-index:${group.renderOrder}`} width="100%" height="100%" aria-hidden="true">
      {#each group.shapes as segment (segment.key)}
        <line
          x1={segment.x1}
          y1={segment.y1}
          x2={segment.x2}
          y2={segment.y2}
          stroke={segment.color}
          stroke-width={segment.width}
          stroke-dasharray={segment.dashed ? '4 3' : undefined}
          stroke-linecap="round"
          vector-effect="non-scaling-stroke"
        ></line>
      {/each}
    </svg>
  {/each}
  {#each priceAnnotations as annotation (annotation.key)}
    <span
      class={`price-annotation${annotation.framed ? ` native-frame frame-${annotation.frameSide}` : ''}${annotation.nativeUnframed ? ' native-unframed' : ''}`}
      style={`left:${annotation.x}px;top:${annotation.y}px;color:${annotation.color};transform:${annotation.transform};z-index:${annotation.renderOrder};--annotation-frame-fill:${annotation.frameFill};${annotationFontStyle()}`}
    >{annotation.text}</span>
  {/each}
  {#each iconShapes as shape (shape.key)}
    <span
      class="formula-icon"
      title={`DRAWICON ${shape.type}`}
      style={`left:${shape.x}px;top:${shape.y}px;width:${shape.width}px;height:${shape.height}px;background-image:url(${shape.spriteUrl});background-position:-${shape.spriteX}px 0;transform:${shape.transform};z-index:${shape.renderOrder}`}
    ></span>
  {/each}
  {#each bitmapShapes as shape (shape.key)}
    <img
      class="formula-bitmap"
      src={shape.url}
      alt=""
      style={`left:${shape.x}px;top:${shape.y}px;z-index:${shape.renderOrder}`}
    />
  {/each}
  {#each sequenceShapes as shape (shape.key)}
    <span
      class="sequence-label"
      class:leader={shape.leader}
      class:boxed={shape.boxed}
      class:align-right={shape.align === 'right'}
      class:align-center={shape.align === 'center'}
      class:from-top={shape.leaderFrom === 'top'}
      class:from-bottom={shape.leaderFrom === 'bottom'}
      style={`left:${shape.x}px;top:${shape.y}px;width:${shape.width}px;height:${shape.height}px;color:${shape.color};z-index:${shape.renderOrder};--sequence-box-fill:${shape.fill};${annotationFontStyle(shape.fontTableIndex)}`}
    ><span class="sequence-text">{shape.text}</span></span>
  {/each}
  {#if document.render_ir && document.render_ir.primitive_count > 0}
    <span class="ir-badge" title="使用 C++ 解释器的有序绘图事件；并非通达信像素级渲染器">TDX IR 预览</span>
  {/if}
  {#each fixedLabels as label (label.key)}
    <span
      class="fixed-label"
      class:framed={label.framed}
      style={`left:${label.x * 100}%;top:${label.y * 100}%;color:${label.color};transform:${label.align === 'right' ? 'translateX(-100%)' : 'none'};text-align:${label.align};z-index:${label.renderOrder};${annotationFontStyle()}`}
    >{label.text}</span>
  {/each}
</div>

<style>
  .chart-shell {
    position: relative;
    width: 100%;
    height: 280px;
    overflow: hidden;
  }

  .chart {
    width: 100%;
    height: 100%;
  }

  .band-layer {
    position: absolute;
    inset: 0;
    z-index: 1;
    pointer-events: none;
  }

  .ordered-series-layer {
    position: absolute;
    inset: 0;
    pointer-events: none;
    overflow: hidden;
    shape-rendering: crispEdges;
  }

  .background-layer {
    position: absolute;
    inset: 0;
    z-index: 1;
    pointer-events: none;
    overflow: hidden;
  }

  .background-shape {
    position: absolute;
    box-sizing: border-box;
    border: 1px solid transparent;
  }

  .stick-layer {
    position: absolute;
    inset: 0;
    z-index: 2;
    pointer-events: none;
    shape-rendering: crispEdges;
  }

  .formula-segment-layer {
    position: absolute;
    inset: 0;
    z-index: 2;
    pointer-events: none;
    overflow: hidden;
  }

  .ir-badge {
    position: absolute;
    top: 8px;
    right: 68px;
    z-index: 10000;
    padding: 2px 6px;
    border: 1px solid var(--line);
    border-radius: 3px;
    background: color-mix(in srgb, var(--bg-panel) 86%, transparent);
    color: var(--fg-mute);
    font: 10px/1.4 ui-monospace, "Cascadia Mono", Consolas, monospace;
    pointer-events: none;
  }

  .fixed-label,
  .price-annotation {
    position: absolute;
    z-index: 3;
    max-width: 88%;
    white-space: pre-wrap;
    font: 400 15px/1 Arial, sans-serif;
    pointer-events: none;
  }

  .price-annotation.native-unframed {
    max-width: none;
    white-space: pre;
  }

  .formula-icon {
    position: absolute;
    z-index: 3;
    display: block;
    background-repeat: no-repeat;
    image-rendering: pixelated;
    pointer-events: none;
  }

  .formula-bitmap {
    position: absolute;
    display: block;
    max-width: none;
    image-rendering: auto;
    pointer-events: none;
  }

  .sequence-label {
    position: absolute;
    box-sizing: border-box;
    padding: 0;
    overflow: visible;
    color: inherit;
    font: 400 15px/1 Arial, sans-serif;
    white-space: nowrap;
    pointer-events: none;
  }

  .sequence-text {
    display: flex;
    align-items: center;
    width: 100%;
    height: 100%;
    overflow: hidden;
    white-space: nowrap;
  }

  .sequence-label.align-right .sequence-text {
    justify-content: flex-end;
    text-align: right;
  }

  .sequence-label.align-center .sequence-text {
    justify-content: center;
    text-align: center;
  }

  .sequence-label.leader::before {
    position: absolute;
    left: calc(50% - 0.5px);
    width: 1px;
    height: 10px;
    background: repeating-linear-gradient(
      to bottom,
      currentColor 0 1px,
      transparent 1px 4px
    );
    content: '';
  }

  .sequence-label.leader.from-top::before {
    bottom: 100%;
  }

  .sequence-label.leader.from-bottom::before {
    top: 100%;
  }

  .sequence-label.boxed {
    background: var(--sequence-box-fill);
    box-shadow: inset 0 0 0 1px currentColor;
    text-shadow: none;
  }

  .price-annotation {
    max-width: 180px;
  }

  .price-annotation.native-frame {
    box-sizing: content-box;
    max-width: none;
    padding: 2px 1px 0 2px;
    border: 1px solid currentColor;
    border-radius: 4px;
    background: var(--annotation-frame-fill);
    white-space: pre;
  }

  .price-annotation.native-frame::before {
    position: absolute;
    left: calc(50% - 2.5px);
    width: 1px;
    height: 20px;
    background: repeating-linear-gradient(
      to bottom,
      currentColor 0 1px,
      transparent 1px 4px
    );
    content: '';
  }

  .price-annotation.native-frame.frame-above::before {
    top: 100%;
  }

  .price-annotation.native-frame.frame-below::before {
    bottom: 100%;
  }

  .framed {
    padding: 1px 3px;
    border: 1px solid currentColor;
    border-radius: 2px;
    background: color-mix(in srgb, var(--bg-panel) 82%, transparent);
    text-shadow: none;
  }
</style>
