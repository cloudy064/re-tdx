import { compact, date, delta, fixed, money, num, percent, text, tone } from './fmt';
import type { Column } from '../ui/DataTable.svelte';

export interface ResearchRecord {
  [key: string]: unknown;
}

export type RecordValueFormat =
  | 'text' | 'number' | 'integer' | 'percent' | 'delta' | 'money' | 'date';

export interface RecordColumnDefinition {
  key: string;
  label: string;
  path: string;
  subPath?: string;
  format?: RecordValueFormat;
  width?: string;
  signedTone?: boolean;
  labels?: Record<string, string>;
}

export interface ResearchChartSeriesDefinition {
  path: string;
  title: string;
  color: 'focus' | 'up' | 'down' | 'warn';
}

export interface ResearchChartDefinition {
  timePath: string;
  series: ResearchChartSeriesDefinition[];
}

export function valueAt(row: ResearchRecord, path: string): unknown {
  let value: unknown = row;
  for (const key of path.split('.')) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
    value = (value as Record<string, unknown>)[key];
  }
  return value;
}

export function formatRecordValue(value: unknown, kind: RecordValueFormat = 'text'): string {
  if (kind === 'number') return fixed(value, 2);
  if (kind === 'integer') return compact(value);
  if (kind === 'percent') return percent(value);
  if (kind === 'delta') return delta(value);
  if (kind === 'money') return money(value);
  if (kind === 'date') return date(value);
  return text(value);
}

export function recordColumns<T extends ResearchRecord>(
  definitions: RecordColumnDefinition[]
): Column<T>[] {
  return definitions.map((column) => ({
    key: column.key,
    label: column.label,
    width: column.width,
    align: column.format && column.format !== 'text' && column.format !== 'date'
      ? 'right' : 'left',
    num: Boolean(column.format && column.format !== 'text' && column.format !== 'date'),
    value: (row) => {
      const value = valueAt(row, column.path);
      const mapped = column.labels?.[String(value ?? '')];
      return mapped ?? formatRecordValue(value, column.format);
    },
    sub: column.subPath ? (row) => text(valueAt(row, column.subPath!)) : undefined,
    tone: column.signedTone ? (row) => tone(valueAt(row, column.path)) : undefined,
    sortValue: (row) => {
      const value = valueAt(row, column.path);
      return column.format && column.format !== 'text' && column.format !== 'date'
        ? num(value) ?? Number.NEGATIVE_INFINITY
        : text(value);
    }
  }));
}
