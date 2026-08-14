/**
 * 全局格式化工具。
 *
 * 重构前 money() / compact() / numeric() / percent() / date() 在十余个组件里
 * 各写了一份，口径互不一致（有的 2 位小数有的 1 位，有的返回 '—' 有的返回 ''）。
 * 这里是唯一实现，所有视图必须走这里。
 */

/** 缺值统一显示为破折号，不用空串——空单元格会让人误以为是 0。 */
export const DASH = '—';

/** 宽松解析：后端 JSON 里数值字段可能是 number、数字字符串或空串。 */
export function num(value: unknown): number | null {
  if (value === null || value === undefined || value === '') return null;
  const parsed = typeof value === 'number' ? value : Number(value);
  return Number.isFinite(parsed) ? parsed : null;
}

export function text(value: unknown): string {
  if (value === null || value === undefined) return DASH;
  const s = String(value).trim();
  return s === '' ? DASH : s;
}

/** 固定小数位。价格用 2 位，指数/净值按需给 digits。 */
export function fixed(value: unknown, digits = 2): string {
  const parsed = num(value);
  return parsed === null ? DASH : parsed.toFixed(digits);
}

/** 价格：始终 2 位小数。 */
export function price(value: unknown): string {
  return fixed(value, 2);
}

/**
 * 中文数量级压缩：亿 / 万 / 原值。
 * @param unit 追加单位，如 '元' → "1.23 亿元"
 */
export function compact(value: unknown, unit = ''): string {
  const parsed = num(value);
  if (parsed === null) return DASH;
  const abs = Math.abs(parsed);
  if (abs >= 1e12) return `${(parsed / 1e12).toFixed(2)} 万亿${unit}`;
  if (abs >= 1e8) return `${(parsed / 1e8).toFixed(2)} 亿${unit}`;
  if (abs >= 1e4) return `${(parsed / 1e4).toFixed(2)} 万${unit}`;
  return `${round(parsed)}${unit}`;
}

/** 金额，等价于 compact(value, '元') 的常用简写。 */
export function money(value: unknown): string {
  return compact(value, '元');
}

/** 成交量（股 → 手/万手）。 */
export function volume(hands: unknown): string {
  const parsed = num(hands);
  if (parsed === null) return DASH;
  const abs = Math.abs(parsed);
  if (abs >= 1e8) return `${(parsed / 1e8).toFixed(2)} 亿手`;
  if (abs >= 1e4) return `${(parsed / 1e4).toFixed(2)} 万手`;
  return `${round(parsed)} 手`;
}

/** 千分位整数。 */
export function count(value: unknown): string {
  const parsed = num(value);
  return parsed === null ? DASH : parsed.toLocaleString('zh-CN', { maximumFractionDigits: 2 });
}

/**
 * 百分比。
 * @param signed 是否给正数补 '+'（涨跌幅需要，占比不需要）
 */
export function percent(value: unknown, digits = 2, signed = false): string {
  const parsed = num(value);
  if (parsed === null) return DASH;
  const sign = signed && parsed > 0 ? '+' : '';
  return `${sign}${parsed.toFixed(digits)}%`;
}

/** 涨跌幅：带符号百分比。 */
export function delta(value: unknown, digits = 2): string {
  return percent(value, digits, true);
}

/** 带符号数值，用于净流入/净买入这类有方向的金额。 */
export function signedCompact(value: unknown, unit = ''): string {
  const parsed = num(value);
  if (parsed === null) return DASH;
  const body = compact(Math.abs(parsed), unit);
  if (parsed > 0) return `+${body}`;
  if (parsed < 0) return `-${body}`;
  return body;
}

/**
 * 涨跌语义类名。全局约定红涨绿跌（A 股惯例）。
 * 返回 '' 而不是 'flat'，让调用方决定平盘是否需要额外着色。
 */
export function tone(value: unknown): 'up' | 'down' | 'flat' {
  const parsed = num(value);
  if (parsed === null || parsed === 0) return 'flat';
  return parsed > 0 ? 'up' : 'down';
}

/**
 * 日期规整。后端混用 `20260804`、`2026-08-04`、`2026-08-04T09:30:00` 三种形式。
 */
export function date(value: unknown): string {
  const raw = value === null || value === undefined ? '' : String(value).trim();
  if (raw === '') return DASH;
  const digits = raw.replace(/\D/g, '');
  if (digits.length >= 8) {
    return `${digits.slice(0, 4)}-${digits.slice(4, 6)}-${digits.slice(6, 8)}`;
  }
  return raw;
}

/** 从 ISO 或 `HH:MM:SS` 中取出时间部分。 */
export function time(value: unknown): string {
  const raw = value === null || value === undefined ? '' : String(value).trim();
  if (raw === '') return DASH;
  return raw.includes('T') ? raw.split('T')[1] : raw;
}

/** 市场号 → 展示前缀。1=沪 2=京 其余=深。 */
export function marketLabel(marketId: unknown): string {
  const parsed = num(marketId);
  if (parsed === 1) return 'SH';
  if (parsed === 2) return 'BJ';
  return 'SZ';
}

/** 市场号 → 接口用的市场键。 */
export function marketKey(marketId: unknown): string {
  const parsed = num(marketId);
  if (parsed === 1) return 'sh';
  if (parsed === 2) return 'bj';
  return 'sz';
}

/** `sz` + `000001` → `SZ000001`，用于标题和搜索结果。 */
export function ticker(market: string, code: string): string {
  return `${(market || '').toUpperCase()}${code || ''}`;
}

function round(value: number): string {
  return Number.isInteger(value)
    ? value.toLocaleString('zh-CN')
    : value.toLocaleString('zh-CN', { maximumFractionDigits: 2 });
}
