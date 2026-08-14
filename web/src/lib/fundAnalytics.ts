import type {
  RecordColumnDefinition,
  ResearchChartDefinition,
  ResearchRecord
} from './records';

export type FundAnalyticsView =
  | 'risk' | 'risk-history' | 'monthly-risk' | 'monthly-history'
  | 'selection-skill' | 'reported-holdings'
  | 'reported-holding-industries' | 'reported-holding-securities'
  | 'holdings-stability' | 'holding-industries' | 'holding-history'
  | 'position-estimates' | 'market-position-history';

export interface FundAnalyticsRecord extends ResearchRecord {
  fund?: {
    fund_id?: string;
    code?: string;
    name?: string;
  };
  [key: string]: unknown;
}

export interface FundAnalyticsDocument {
  schema: string;
  availability: string;
  view: FundAnalyticsView;
  view_title: string;
  counts: Record<string, number | boolean>;
  parameters: Record<string, unknown>;
  records: FundAnalyticsRecord[];
}

export interface FundViewDefinition {
  id: FundAnalyticsView;
  label: string;
  group: string;
  description: string;
  requiresFund: boolean;
  usesRange?: boolean;
  usesReportDate?: boolean;
  usesEstimateDate?: boolean;
  columns: RecordColumnDefinition[];
  chart?: ResearchChartDefinition;
}

const FUND = (extra: RecordColumnDefinition[] = []): RecordColumnDefinition[] => [
  { key: 'fund', label: '基金', path: 'fund.name', subPath: 'fund.code', width: '210px' },
  ...extra
];

export const FUND_ANALYTICS_VIEWS: FundViewDefinition[] = [
  {
    id: 'risk', label: '收益风险', group: '风险收益', requiresFund: false, usesRange: true,
    description: '累计/年化收益、波动率、夏普、Beta 与最大回撤。',
    columns: FUND([
      { key: 'nav', label: '净值', path: 'latest_nav', format: 'number' },
      { key: 'change', label: '日涨跌', path: 'latest_change_pct', format: 'delta', signedTone: true },
      { key: 'annual', label: '年化收益', path: 'returns.annualized_pct', format: 'percent', signedTone: true },
      { key: 'volatility', label: '波动率', path: 'risk.volatility_pct', format: 'percent' },
      { key: 'sharpe', label: '夏普', path: 'risk_adjusted.sharpe_ratio', format: 'number' },
      { key: 'drawdown', label: '最大回撤', path: 'maximum_drawdown.maximum_pct', format: 'percent', signedTone: true }
    ])
  },
  {
    id: 'risk-history', label: '日收益历史', group: '风险收益', requiresFund: true, usesRange: true,
    description: '基金、基准与超额的逐日累计收益。',
    columns: [
      { key: 'date', label: '日期', path: 'date', format: 'date', width: '110px' },
      { key: 'nav', label: '基金净值', path: 'fund_nav', format: 'number' },
      { key: 'fund', label: '基金累计', path: 'fund_cumulative_return_pct', format: 'percent', signedTone: true },
      { key: 'benchmark', label: '基准累计', path: 'benchmark_cumulative_return_pct', format: 'percent', signedTone: true },
      { key: 'excess', label: '累计超额', path: 'excess_cumulative_return_pct', format: 'percent', signedTone: true },
      { key: 'daily', label: '当日收益', path: 'fund_daily_return_pct', format: 'delta', signedTone: true }
    ],
    chart: {
      timePath: 'date',
      series: [
        { path: 'fund_cumulative_return_pct', title: '基金累计', color: 'focus' },
        { path: 'benchmark_cumulative_return_pct', title: '基准累计', color: 'warn' },
        { path: 'excess_cumulative_return_pct', title: '累计超额', color: 'up' }
      ]
    }
  },
  {
    id: 'monthly-risk', label: '月度风险', group: '月度能力', requiresFund: false, usesRange: true,
    description: '月收益、胜率、牛熊表现和月度波动率。',
    columns: FUND([
      { key: 'average', label: '月均收益', path: 'average_monthly_return_pct', format: 'percent', signedTone: true },
      { key: 'win', label: '月胜率', path: 'monthly_win_rate_pct', format: 'percent' },
      { key: 'volatility', label: '月波动', path: 'monthly_volatility_pct', format: 'percent' },
      { key: 'maximum', label: '最佳月', path: 'maximum_monthly_return_pct', format: 'percent', signedTone: true },
      { key: 'minimum', label: '最差月', path: 'minimum_monthly_return_pct', format: 'percent', signedTone: true }
    ])
  },
  {
    id: 'monthly-history', label: '月收益历史', group: '月度能力', requiresFund: true, usesRange: true,
    description: '基金、基准和超额的逐月收益。',
    columns: [
      { key: 'month', label: '月份', path: 'month', width: '100px' },
      { key: 'nav', label: '基金净值', path: 'fund_nav', format: 'number' },
      { key: 'fund', label: '基金月收益', path: 'fund_monthly_return_pct', format: 'delta', signedTone: true },
      { key: 'benchmark', label: '基准月收益', path: 'benchmark_monthly_return_pct', format: 'delta', signedTone: true },
      { key: 'excess', label: '月超额', path: 'excess_monthly_return_pct', format: 'delta', signedTone: true },
      { key: 'cumulative', label: '基金累计', path: 'fund_cumulative_return_pct', format: 'percent', signedTone: true }
    ],
    chart: {
      timePath: 'month',
      series: [
        { path: 'fund_monthly_return_pct', title: '基金月收益', color: 'focus' },
        { path: 'benchmark_monthly_return_pct', title: '基准月收益', color: 'warn' },
        { path: 'excess_monthly_return_pct', title: '月超额', color: 'up' }
      ]
    }
  },
  {
    id: 'selection-skill', label: '择时选股', group: '月度能力', requiresFund: false, usesRange: true,
    description: 'CL、TM、HM 三类模型的择时与选股能力。',
    columns: FUND([
      { key: 'tm_timing', label: 'TM择时', path: 'models.treynor-mazuy.timing_ability', format: 'number', signedTone: true },
      { key: 'tm_selection', label: 'TM选股', path: 'models.treynor-mazuy.selection_ability_pct', format: 'percent', signedTone: true },
      { key: 'hm_timing', label: 'HM择时', path: 'models.henriksson-merton.timing_ability', format: 'number', signedTone: true },
      { key: 'hm_selection', label: 'HM选股', path: 'models.henriksson-merton.selection_ability_pct', format: 'percent', signedTone: true }
    ])
  },
  {
    id: 'reported-holdings', label: '报告期持仓', group: '公开持仓', requiresFund: false, usesReportDate: true,
    description: '完整报告期公开持仓概览，不代表实时完整仓位。',
    columns: FUND([
      { key: 'report', label: '报告期', path: 'report_date', format: 'date' },
      { key: 'position', label: '股票市值', path: 'stock_market_value_yuan', format: 'money' },
      { key: 'securities', label: '股票数', path: 'reported_security_count', format: 'integer' },
      { key: 'stock', label: '股票集中度', path: 'stock_concentration_pct', format: 'percent' },
      { key: 'industry', label: '行业集中度', path: 'industry_concentration_pct', format: 'percent' },
      { key: 'turnover', label: '换手率', path: 'turnover_pct', format: 'percent' }
    ])
  },
  {
    id: 'reported-holding-industries', label: '报告期行业', group: '公开持仓', requiresFund: true, usesReportDate: true,
    description: '所选基金在公开报告期的行业配置。',
    columns: [
      { key: 'industry', label: '行业', path: 'industry', width: '230px' },
      { key: 'value', label: '持仓市值', path: 'holding_market_value_yuan', format: 'money' },
      { key: 'share', label: '股票投资占比', path: 'share_of_stock_investment_pct', format: 'percent' }
    ]
  },
  {
    id: 'reported-holding-securities', label: '报告期股票', group: '公开持仓', requiresFund: true, usesReportDate: true,
    description: '所选基金在公开报告期披露的股票持仓。',
    columns: [
      { key: 'security', label: '股票', path: 'security_name', subPath: 'security_code', width: '180px' },
      { key: 'price', label: '现价', path: 'current_price', format: 'number' },
      { key: 'change', label: '涨跌幅', path: 'current_change_pct', format: 'delta', signedTone: true },
      { key: 'shares', label: '持股数', path: 'holding_shares', format: 'integer' },
      { key: 'value', label: '持仓市值', path: 'holding_market_value_yuan', format: 'money' },
      { key: 'share', label: '净值占比', path: 'share_of_fund_nav_pct', format: 'percent' }
    ]
  },
  {
    id: 'holdings-stability', label: '持仓稳定性', group: '持仓稳定', requiresFund: false, usesRange: true,
    description: '换手、股票/行业集中度和仓位的区间稳定性。',
    columns: FUND([
      { key: 'turnover', label: '平均换手', path: 'average_turnover_ratio', format: 'number' },
      { key: 'stock', label: '股票集中度', path: 'average_stock_concentration_pct', format: 'percent' },
      { key: 'industry', label: '行业集中度', path: 'average_industry_concentration_pct', format: 'percent' },
      { key: 'position_stability', label: '仓位稳定', path: 'stability.stock_position', format: 'number' },
      { key: 'turnover_stability', label: '换手稳定', path: 'stability.turnover', format: 'number' }
    ])
  },
  {
    id: 'holding-industries', label: '区间行业持仓', group: '持仓稳定', requiresFund: true, usesRange: true,
    description: '所选基金在区间内的平均行业配置。',
    columns: [
      { key: 'industry', label: '行业', path: 'industry', width: '230px' },
      { key: 'value', label: '平均市值', path: 'average_market_value', format: 'money' },
      { key: 'share', label: '平均占比', path: 'average_market_value_ratio_pct', format: 'percent' }
    ]
  },
  {
    id: 'holding-history', label: '持仓历史', group: '持仓稳定', requiresFund: true, usesRange: true,
    description: '所选基金逐报告期的股票仓位与集中度。',
    columns: [
      { key: 'date', label: '报告期', path: 'report_date', format: 'date', width: '110px' },
      { key: 'position', label: '股票仓位', path: 'stock_position_pct', format: 'percent' },
      { key: 'stock', label: '股票集中度', path: 'stock_concentration_pct', format: 'percent' },
      { key: 'industry', label: '行业集中度', path: 'industry_concentration_pct', format: 'percent' },
      { key: 'value', label: '股票市值', path: 'stock_market_value', format: 'money' },
      { key: 'turnover', label: '换手率', path: 'turnover_ratio', format: 'number' }
    ],
    chart: {
      timePath: 'report_date',
      series: [
        { path: 'stock_position_pct', title: '股票仓位', color: 'focus' },
        { path: 'stock_concentration_pct', title: '股票集中度', color: 'warn' },
        { path: 'industry_concentration_pct', title: '行业集中度', color: 'up' }
      ]
    }
  },
  {
    id: 'position-estimates', label: '基金仓位估算', group: '仓位估算', requiresFund: false, usesEstimateDate: true,
    description: '报告期仓位与最新估算仓位对比。',
    columns: FUND([
      { key: 'report', label: '报告期', path: 'report_date', format: 'date' },
      { key: 'reported', label: '报告仓位', path: 'reported_position_pct', format: 'percent' },
      { key: 'estimate_date', label: '估算日期', path: 'estimate_date', format: 'date' },
      { key: 'estimated', label: '估算仓位', path: 'estimated_position_pct', format: 'percent' },
      { key: 'change', label: '仓位变化', path: 'estimated_change_from_report_pct_points', format: 'delta', signedTone: true }
    ])
  },
  {
    id: 'market-position-history', label: '市场仓位历史', group: '仓位估算', requiresFund: false, usesRange: true,
    description: '股票型、偏股混合型及合并口径的市场仓位。',
    columns: [
      { key: 'date', label: '日期', path: 'date', format: 'date', width: '110px' },
      { key: 'stock', label: '股票型基金', path: 'stock_fund_position_pct', format: 'percent' },
      { key: 'mixed', label: '偏股混合型', path: 'equity_mixed_fund_position_pct', format: 'percent' },
      { key: 'combined', label: '合并市场仓位', path: 'combined_market_position_pct', format: 'percent' }
    ],
    chart: {
      timePath: 'date',
      series: [
        { path: 'combined_market_position_pct', title: '合并市场仓位', color: 'focus' },
        { path: 'stock_fund_position_pct', title: '股票型基金', color: 'up' },
        { path: 'equity_mixed_fund_position_pct', title: '偏股混合型', color: 'warn' }
      ]
    }
  }
];

export const FUND_STYLE_OPTIONS = [
  { id: '005001', label: '普通股票型' },
  { id: '005002', label: '复制指数型' },
  { id: '005003', label: '增强指数型' },
  { id: '005004', label: '平衡混合型' },
  { id: '005005', label: '偏股混合型' },
  { id: '005006', label: '偏债混合型' },
  { id: '005008', label: '中短期纯债型' },
  { id: '005009', label: '长期纯债型' },
  { id: '005010', label: '一级债基' },
  { id: '005011', label: '二级债基' },
  { id: '005014', label: '货币型' }
];

export const FUND_BENCHMARK_OPTIONS = [
  { id: '0', label: '上证指数' },
  { id: '1', label: '沪深300' },
  { id: '2', label: '中证500' }
];

export function fundViewDefinition(view: FundAnalyticsView): FundViewDefinition {
  return FUND_ANALYTICS_VIEWS.find((item) => item.id === view) ?? FUND_ANALYTICS_VIEWS[0];
}
