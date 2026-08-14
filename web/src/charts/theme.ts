/**
 * 图表主题桥接层。
 *
 * 重构前每个图表组件把配色硬编码一遍，而且互相矛盾：TradingChart 的 K 线是
 * 红涨绿跌，分时线却用绿色，全局 token 又是绿涨红跌。这里让图表统一从
 * CSS 变量取色，主题切换只有一条代码路径。
 */

import { ColorType, CrosshairMode, type DeepPartial, type ChartOptions } from 'lightweight-charts';

export interface Palette {
  bg: string;
  grid: string;
  border: string;
  text: string;
  up: string;
  down: string;
  focus: string;
  warn: string;
  neutral: string;
}

export function palette(): Palette {
  const style = getComputedStyle(document.documentElement);
  const read = (name: string, fallback: string) =>
    style.getPropertyValue(name).trim() || fallback;
  return {
    bg: read('--bg-panel', '#14171c'),
    grid: read('--line', '#262b33'),
    border: read('--line-strong', '#333a44'),
    text: read('--fg-mute', '#6b7280'),
    up: read('--up', '#f5484a'),
    down: read('--down', '#22b07d'),
    focus: read('--focus', '#3d7eff'),
    warn: read('--warn', '#d99e2b'),
    neutral: read('--fg-dim', '#9aa3af')
  };
}

/** 所有图表共用的基础选项，保证网格、边框、十字光标观感一致。 */
export function baseOptions(p: Palette): DeepPartial<ChartOptions> {
  return {
    autoSize: true,
    layout: {
      background: { type: ColorType.Solid, color: p.bg },
      textColor: p.text,
      fontSize: 10,
      fontFamily: 'ui-monospace, "Cascadia Mono", Consolas, monospace',
      attributionLogo: false
    },
    grid: {
      vertLines: { color: p.grid },
      horzLines: { color: p.grid }
    },
    crosshair: {
      mode: CrosshairMode.Normal,
      vertLine: { color: p.neutral, width: 1, style: 3, labelBackgroundColor: p.border },
      horzLine: { color: p.neutral, width: 1, style: 3, labelBackgroundColor: p.border }
    },
    rightPriceScale: { borderColor: p.border },
    timeScale: { borderColor: p.border, rightOffset: 4 }
  };
}

/** 把 6 位十六进制色加上透明度，用于成交量柱这类需要压暗的图元。 */
export function alpha(color: string, value: number): string {
  const hex = color.replace('#', '');
  if (hex.length !== 6) return color;
  const r = parseInt(hex.slice(0, 2), 16);
  const g = parseInt(hex.slice(2, 4), 16);
  const b = parseInt(hex.slice(4, 6), 16);
  return `rgba(${r}, ${g}, ${b}, ${value})`;
}

/** 订阅主题切换。返回取消订阅函数。 */
export function watchTheme(handler: () => void): () => void {
  const observer = new MutationObserver(handler);
  observer.observe(document.documentElement, { attributes: true, attributeFilter: ['data-theme'] });
  return () => observer.disconnect();
}

/** `YYYY-MM-DD` + `HH:MM` → UTC 秒（图表内部按 UTC 排布，不做时区换算）。 */
export function barTime(date: string, time = '00:00'): number {
  const [year, month, day] = date.split('-').map(Number);
  const [hour, minute] = time.split(':').map(Number);
  return Math.floor(Date.UTC(year, (month || 1) - 1, day || 1, hour || 0, minute || 0) / 1000);
}

/** `YYYYMMDD` + 当日偏移秒 → UTC 秒。 */
export function dayTime(compactDate: string, offsetSeconds = 0): number {
  const text = /^\d{8}$/.test(compactDate) ? compactDate : '20000101';
  const year = Number(text.slice(0, 4));
  const month = Number(text.slice(4, 6));
  const day = Number(text.slice(6, 8));
  return Math.floor(Date.UTC(year, month - 1, day, 0, 0, offsetSeconds) / 1000);
}
