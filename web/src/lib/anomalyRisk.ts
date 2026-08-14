import type { RecordColumnDefinition, ResearchRecord } from './records';

export type AnomalyMode = 'moves' | 'statistics' | 'suspension-risk' | 'profit-gaps';

export interface AnomalyRecord extends ResearchRecord {
  security?: Record<string, unknown>;
}

export interface AnomalyDocument {
  schema: string;
  availability: string;
  view?: string;
  records: AnomalyRecord[];
  counts: Record<string, number | boolean>;
  parameters: Record<string, unknown>;
}

export interface AbnormalDetailDocument {
  schema: string;
  availability: string;
  view: 'summary' | 'explanation';
  data: ResearchRecord;
}

export interface AnomalyModeDefinition {
  id: AnomalyMode;
  label: string;
  title: string;
  description: string;
  columns: RecordColumnDefinition[];
}

export const ANOMALY_MODES: AnomalyModeDefinition[] = [
  {
    id: 'moves', label: '实时交易异动', title: '十九类交易异动',
    description: '点击证券查看上榜原因、累计偏离和异常区间。',
    columns: [
      { key: 'security', label: '股票', path: 'security.name', subPath: 'security.security_id', width: '170px' },
      { key: 'anomaly', label: '异动类型', path: 'anomaly.name', subPath: 'anomaly.code', width: '210px' },
      { key: 'time', label: '时间', path: 'time', width: '90px' },
      { key: 'price', label: '现价', path: 'price', format: 'number' },
      { key: 'change', label: '涨跌幅', path: 'change_pct', format: 'delta', signedTone: true },
      { key: 'turnover', label: '换手率', path: 'turnover_pct', format: 'percent' },
      { key: 'amplitude', label: '振幅', path: 'amplitude_pct', format: 'percent' },
      { key: 'amount', label: '成交额', path: 'amount', format: 'money' },
      { key: 'flow', label: '主力净流入', path: 'main_net_inflow', format: 'money', signedTone: true }
    ]
  },
  {
    id: 'statistics', label: '异常波动统计', title: '3/10/30日异常统计',
    description: '比较证券与所属分类指数收益，保留上游预警边界。',
    columns: [
      { key: 'security', label: '股票', path: 'security.name', subPath: 'security.security_id', width: '170px' },
      { key: 'board', label: '上市板块', path: 'listing_board', width: '105px' },
      { key: 'warning', label: '预警', path: 'warning.status_text', subPath: 'warning.kind', width: '145px' },
      { key: 'live', label: '实时偏离', path: 'live_snapshot.reported_deviation_pct', format: 'delta', signedTone: true },
      { key: 'days3', label: '3日偏离', path: 'windows.days_3.deviation_pct', format: 'delta', signedTone: true },
      { key: 'days10', label: '10日偏离', path: 'windows.days_10.deviation_pct', format: 'delta', signedTone: true },
      { key: 'count10', label: '10日正向异动', path: 'windows.days_10.positive_anomaly_count', format: 'integer' },
      { key: 'days30', label: '30日偏离', path: 'windows.days_30.deviation_pct', format: 'delta', signedTone: true }
    ]
  },
  {
    id: 'suspension-risk', label: '停牌风险', title: '异动停牌与复牌风险',
    description: '可能停牌、重复停牌和已触发异常的证券清单。',
    columns: [
      { key: 'security', label: '股票', path: 'security.name', subPath: 'security.security_id', width: '170px' },
      { key: 'board', label: '上市板块', path: 'listing_board', width: '105px' },
      { key: 'warning', label: '风险状态', path: 'warning.label', subPath: 'warning.status', width: '150px' },
      { key: 'count', label: '近10日异动', path: 'anomaly.recent_10_day_anomaly_count', format: 'integer' },
      { key: 'deviation', label: '累计偏离', path: 'anomaly.deviation_pct', format: 'delta', signedTone: true },
      { key: 'standard', label: '触发标准', path: 'anomaly.trigger_standard_pct', format: 'percent' },
      { key: 'trigger', label: '触发价', path: 'anomaly.trigger_price', format: 'number' },
      { key: 'suspension', label: '停牌日', path: 'anomaly.suspension_date', format: 'date' },
      { key: 'resumption', label: '复牌日', path: 'anomaly.resumption_date', format: 'date' }
    ]
  },
  {
    id: 'profit-gaps', label: '利润断层', title: '业绩披露后利润断层',
    description: '业绩预告、快报或定期报告后的价格断层与客户端安全分。',
    columns: [
      { key: 'security', label: '股票', path: 'security.name', subPath: 'security.security_id', width: '170px' },
      { key: 'date', label: '事件日期', path: 'event_date', format: 'date', width: '110px' },
      { key: 'disclosure', label: '披露类型', path: 'disclosure.name', subPath: 'disclosure.kind', width: '160px' },
      { key: 'period', label: '报告期', path: 'disclosure.report_period', width: '100px' },
      { key: 'gap', label: '断层涨跌', path: 'gap_change_pct', format: 'delta', signedTone: true },
      { key: 'safety', label: '安全分', path: 'safety_score', format: 'integer' }
    ]
  }
];

export const BOARD_OPTIONS = [
  { id: 'all', label: '全部板块' },
  { id: 'sz-main', label: '深市主板' },
  { id: 'gem', label: '创业板' },
  { id: 'sh-main', label: '沪市主板' },
  { id: 'star', label: '科创板' },
  { id: 'bj', label: '北交所' }
];

export const ABNORMAL_TYPE_OPTIONS = [
  { id: 'all', label: '全部19类' },
  { id: '101', label: '涨幅偏离较高' },
  { id: '102', label: '跌幅偏离较高' },
  { id: '103', label: '日振幅较高' },
  { id: '104', label: '日换手率较高' },
  { id: '105', label: '无涨跌幅限制' },
  { id: '121', label: '3日涨幅偏离' },
  { id: '122', label: '3日跌幅偏离' },
  { id: '123', label: '3日换手率累计' },
  { id: '131', label: '10日3次正向异动' },
  { id: '132', label: '10日3次负向异动' },
  { id: '133', label: '10日涨幅偏离100%' },
  { id: '134', label: '10日跌幅偏离50%' },
  { id: '135', label: '30日涨幅偏离200%' },
  { id: '136', label: '30日跌幅偏离70%' },
  { id: '141', label: '异常波动停牌1' },
  { id: '142', label: '异常波动停牌2' },
  { id: '143', label: '异常波动停牌3' },
  { id: '181', label: '退市整理证券' },
  { id: '191', label: '实施特别停牌' }
];

export const WARNING_OPTIONS = [
  { id: 'all', label: '全部状态' },
  { id: 'none', label: '无预警' },
  { id: 'repeat-suspension', label: '重复停牌' },
  { id: 'possible-suspension', label: '可能停牌' },
  { id: 'triggered', label: '已触发' }
];

export const WARNING_SCOPE_OPTIONS = [
  { id: 'all', label: '全部证券' },
  { id: 'active', label: '仅活跃预警' }
];

export function anomalyModeDefinition(mode: AnomalyMode): AnomalyModeDefinition {
  return ANOMALY_MODES.find((item) => item.id === mode) ?? ANOMALY_MODES[0];
}
