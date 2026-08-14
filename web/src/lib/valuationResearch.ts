import type {
  RecordColumnDefinition,
  ResearchChartDefinition,
  ResearchRecord
} from './records';

export type ValuationDomain = 'relative' | 'equity' | 'volatility' | 'total-return';
export type EquityValuationView =
  | 'pe-industries' | 'pe-security-history' | 'pe-industry-members'
  | 'pb-roe-industries' | 'pb-roe-members';
export type VolatilityView = 'catalog' | 'security';

export interface ValuationRecord extends ResearchRecord {
  security?: Record<string, unknown>;
  industry?: Record<string, unknown>;
  index?: Record<string, unknown>;
}

export interface RelativeValuationDocument {
  schema: string;
  availability: string;
  mode: string;
  indices: ValuationRecord[];
  selected: ValuationRecord | null;
  history: ValuationRecord[];
  counts: Record<string, number | boolean>;
  parameters: Record<string, unknown>;
}

export interface ValuationRecordsDocument {
  schema: string;
  availability: string;
  view?: string;
  records: ValuationRecord[];
  counts: Record<string, number | boolean>;
  parameters: Record<string, unknown>;
}

export interface ValuationTableDefinition {
  title: string;
  description: string;
  columns: RecordColumnDefinition[];
  chart?: ResearchChartDefinition;
}

const JUDGMENT_LABELS = {
  overvalued: '高估',
  undervalued: '低估',
  reasonable: '合理'
};

export const VALUATION_DOMAINS = [
  { id: 'relative', label: '指数相对估值' },
  { id: 'equity', label: '个股/行业估值' },
  { id: 'volatility', label: '指数波动率' },
  { id: 'total-return', label: '全收益差' }
];

export const EQUITY_VIEWS = [
  { id: 'pe-industries', label: '行业 PE' },
  { id: 'pe-security-history', label: '个股 PE 历史' },
  { id: 'pe-industry-members', label: '行业成分 PE' },
  { id: 'pb-roe-industries', label: '行业 PB-ROE' },
  { id: 'pb-roe-members', label: '行业成分 PB-ROE' }
];

export const RELATIVE_INDEX_TYPES = [
  { id: 'broad', label: '宽基' },
  { id: 'industry', label: '行业' },
  { id: 'composite', label: '综合' },
  { id: 'theme', label: '主题' },
  { id: 'scale', label: '规模' }
];

export const RELATIVE_BENCHMARKS = [
  { id: '000001', label: '上证指数' },
  { id: '000300', label: '沪深300' },
  { id: '000016', label: '上证50' },
  { id: '000905', label: '中证500' },
  { id: '399006', label: '创业板指' }
];

export const RELATIVE_METHODS = [
  { id: 'pe-ttm', label: 'PE-TTM' },
  { id: 'pb-mrq', label: 'PB-MRQ' },
  { id: 'ps-ttm', label: 'PS-TTM' }
];

export const PE_TYPE_OPTIONS = [
  { id: 'ttm', label: '滚动 PE' },
  { id: 'annual', label: '年报 PE' }
];

export const VOLATILITY_WINDOWS = [5, 10, 20, 30, 60, 120, 250]
  .map((value) => ({ id: String(value), label: `${value}日` }));

export const MONTH_OPTIONS = [
  { id: '0', label: '全年/年至今' },
  ...Array.from({ length: 12 }, (_, index) => ({
    id: String(index + 1),
    label: `${index + 1}月`
  }))
];

export const VALUATION_TABLES: Record<string, ValuationTableDefinition> = {
  'relative-master': {
    title: '指数相对估值目录',
    description: '点击指数展开相对基准的历史估值比。',
    columns: [
      { key: 'index', label: '指数', path: 'security.name', subPath: 'security.security_id', width: '180px' },
      { key: 'ratio', label: '当前比值', path: 'current_ratio', format: 'number' },
      { key: 'quantile', label: '历史分位', path: 'historical_quantile_pct', format: 'percent' },
      { key: 'position', label: '相对位置', path: 'relative_position.label', width: '150px' },
      { key: 'minimum', label: '历史最低', path: 'historical_minimum_ratio', format: 'number' },
      { key: 'maximum', label: '历史最高', path: 'historical_maximum_ratio', format: 'number' }
    ]
  },
  'relative-history': {
    title: '指数相对估值历史',
    description: '目标指数估值指标除以所选基准指数估值指标。',
    columns: [
      { key: 'date', label: '日期', path: 'date', format: 'date', width: '110px' },
      { key: 'ratio', label: '相对估值比', path: 'ratio', format: 'number' }
    ],
    chart: { timePath: 'date', series: [{ path: 'ratio', title: '相对估值比', color: 'focus' }] }
  },
  'equity-pe-industries': {
    title: '一级行业 PE', description: '一级研究行业当前 PE 概览。',
    columns: [
      { key: 'industry', label: '行业', path: 'industry.name', subPath: 'industry.code', width: '210px' },
      { key: 'pe', label: 'PE', path: 'pe', format: 'number' }
    ]
  },
  'equity-pe-security-history': {
    title: '个股 PE 历史', description: '个股价格、PE 与历史分位带。',
    columns: [
      { key: 'date', label: '日期', path: 'date', format: 'date', width: '110px' },
      { key: 'close', label: '收盘价', path: 'close', format: 'number' },
      { key: 'pe', label: 'PE', path: 'pe', format: 'number' },
      { key: 'p25', label: '25%分位', path: 'pe_percentiles.p25', format: 'number' },
      { key: 'p50', label: '中位数', path: 'pe_percentiles.p50', format: 'number' },
      { key: 'p75', label: '75%分位', path: 'pe_percentiles.p75', format: 'number' }
    ],
    chart: {
      timePath: 'date',
      series: [
        { path: 'pe', title: 'PE', color: 'focus' },
        { path: 'pe_percentiles.p25', title: '25%分位', color: 'down' },
        { path: 'pe_percentiles.p50', title: '中位数', color: 'warn' },
        { path: 'pe_percentiles.p75', title: '75%分位', color: 'up' }
      ]
    }
  },
  'equity-pe-industry-members': {
    title: '行业成分 PE', description: '行业成分股当前 PE 与一致预期估值判断。',
    columns: [
      { key: 'security', label: '股票', path: 'security.name', subPath: 'security.security_id', width: '180px' },
      { key: 'pe', label: '当前 PE', path: 'pe', format: 'number' },
      { key: 'eps', label: '本年 EPS', path: 'current_year.eps_consensus', format: 'number' },
      { key: 'implied', label: '本年隐含价', path: 'current_year.implied_price', format: 'number' },
      { key: 'current', label: '本年估值', path: 'current_year.valuation_judgment.label', labels: JUDGMENT_LABELS },
      { key: 'next', label: '下年估值', path: 'next_year.valuation_judgment.label', labels: JUDGMENT_LABELS }
    ]
  },
  'equity-pb-roe-industries': {
    title: '行业 PB-ROE', description: '行业 PB、ROE 与成分股 ROE 中位数。',
    columns: [
      { key: 'industry', label: '行业', path: 'industry.name', subPath: 'industry.code', width: '210px' },
      { key: 'pb', label: 'PB', path: 'pb', format: 'number' },
      { key: 'roe', label: 'ROE', path: 'roe_pct', format: 'percent' },
      { key: 'median', label: '成分 ROE 中位', path: 'member_median_roe_pct', format: 'percent' }
    ]
  },
  'equity-pb-roe-members': {
    title: '行业成分 PB-ROE', description: '行业成分股 PB、ROE、预测 PB 与估值判断。',
    columns: [
      { key: 'security', label: '股票', path: 'security.name', subPath: 'security.security_id', width: '180px' },
      { key: 'close', label: '收盘价', path: 'close', format: 'number' },
      { key: 'pb', label: 'PB', path: 'pb', format: 'number' },
      { key: 'roe', label: 'ROE', path: 'roe_pct', format: 'percent' },
      { key: 'forecast', label: '预测 PB', path: 'forecast_pb', format: 'number' },
      { key: 'judgment', label: '估值判断', path: 'valuation_judgment.label', labels: JUDGMENT_LABELS }
    ]
  },
  'volatility-catalog': {
    title: '指数已实现波动率', description: '滚动波动率均值及当前季度均值的历史分位。',
    columns: [
      { key: 'index', label: '指数', path: 'index.name', subPath: 'index.index_id', width: '180px' },
      { key: 'quarter', label: '近季均值', path: 'mean_realized_volatility_pct.quarter', format: 'percent' },
      { key: 'half', label: '半年均值', path: 'mean_realized_volatility_pct.half_year', format: 'percent' },
      { key: 'year', label: '一年均值', path: 'mean_realized_volatility_pct.one_year', format: 'percent' },
      { key: 'quantile', label: '一年内分位', path: 'current_quarter_mean_percentile_pct.within_one_year', format: 'percent' }
    ]
  },
  'volatility-history': {
    title: '指数波动率历史', description: '所选指数滚动已实现波动率时间序列。',
    columns: [
      { key: 'date', label: '日期', path: 'date', format: 'date', width: '110px' },
      { key: 'volatility', label: '已实现波动率', path: 'realized_volatility_pct', format: 'percent' }
    ],
    chart: {
      timePath: 'date',
      series: [{ path: 'realized_volatility_pct', title: '已实现波动率', color: 'focus' }]
    }
  },
  'total-return': {
    title: '价格指数与全收益指数', description: '量化股息再投资对区间收益的贡献。',
    columns: [
      { key: 'pair', label: '指数对', path: 'price_index.name', subPath: 'pair_id', width: '210px' },
      { key: 'start', label: '开始', path: 'start_date', format: 'date' },
      { key: 'end', label: '结束', path: 'end_date', format: 'date' },
      { key: 'price', label: '价格指数收益', path: 'price_index_performance.return_pct', format: 'delta', signedTone: true },
      { key: 'total', label: '全收益指数收益', path: 'total_return_index_performance.return_pct', format: 'delta', signedTone: true },
      { key: 'advantage', label: '全收益优势', path: 'total_return_advantage_pct', format: 'delta', signedTone: true }
    ]
  }
};
