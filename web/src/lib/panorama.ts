/**
 * 通达信 GX/AQFPH 个股全景的固定十类目录。
 *
 * 服务端 security 响应只携带命中 section，不重复 catalog 字段说明；前端保留
 * 这份固定元数据即可用一次请求生成类型化表格，也避免展示难以理解的 raw JSON。
 */
export type PanoramaFieldKind = 'date' | 'number' | 'percent' | 'price' | 'text';

export interface PanoramaFieldMeta {
  key: string;
  label: string;
  kind?: PanoramaFieldKind;
}

export interface PanoramaSectionMeta {
  id: string;
  label: string;
  resource: string;
  description: string;
  fields: PanoramaFieldMeta[];
}

export const PANORAMA_SECTIONS: PanoramaSectionMeta[] = [
  {
    id: 'quality-rating',
    label: '量化吸引力评级',
    resource: 'AQFPH101',
    description: '最近两期吸引力评分、评级变化、风险与领涨属性。',
    fields: [
      { key: 'latest_date', label: '最新日期', kind: 'date' },
      { key: 'latest_score', label: '最新评分', kind: 'number' },
      { key: 'previous_date', label: '上期日期', kind: 'date' },
      { key: 'previous_score', label: '上期评分', kind: 'number' },
      { key: 'rating_change', label: '评级变化', kind: 'text' },
      { key: 'risk_type', label: '风险类型', kind: 'text' },
      { key: 'leader_count', label: '领涨次数', kind: 'number' },
      { key: 'leader_type', label: '领涨类型', kind: 'text' },
      { key: 'pe_ttm', label: 'PE-TTM', kind: 'number' },
      { key: 'roe_pct', label: 'ROE', kind: 'percent' }
    ]
  },
  {
    id: 'capital-flow',
    label: '多周期资金流向',
    resource: 'GX_ZJLX101',
    description: '主力与整体净流入的一、五、十、二十及三十日窗口。',
    fields: [
      { key: 'statistics_date', label: '统计日', kind: 'date' },
      { key: 'main_net_inflow_1d', label: '主力净流入 1 日', kind: 'number' },
      { key: 'main_net_inflow_5d', label: '主力净流入 5 日', kind: 'number' },
      { key: 'main_net_inflow_10d', label: '主力净流入 10 日', kind: 'number' },
      { key: 'main_net_inflow_20d', label: '主力净流入 20 日', kind: 'number' },
      { key: 'main_net_inflow_30d', label: '主力净流入 30 日', kind: 'number' },
      { key: 'net_inflow_5d', label: '净流入 5 日', kind: 'number' },
      { key: 'net_inflow_10d', label: '净流入 10 日', kind: 'number' },
      { key: 'net_inflow_20d', label: '净流入 20 日', kind: 'number' },
      { key: 'net_inflow_30d', label: '净流入 30 日', kind: 'number' }
    ]
  },
  {
    id: 'risk-watch',
    label: '质押商誉解禁风险',
    resource: 'GX_FXGZ101',
    description: '股权质押预警、商誉变化及下一批限售解禁信息。',
    fields: [
      { key: 'pledged_shares_10k', label: '累计质押（万股）', kind: 'number' },
      { key: 'pledged_total_share_pct', label: '质押占总股本', kind: 'percent' },
      { key: 'pledge_warning_price', label: '预警价', kind: 'price' },
      { key: 'pledge_warning_shares_10k', label: '预警股数（万股）', kind: 'number' },
      { key: 'pledge_closeout_price', label: '平仓价', kind: 'price' },
      { key: 'pledge_closeout_shares_10k', label: '平仓股数（万股）', kind: 'number' },
      { key: 'goodwill_current', label: '本期商誉', kind: 'number' },
      { key: 'goodwill_previous', label: '上期商誉', kind: 'number' },
      { key: 'goodwill_change', label: '商誉变化', kind: 'number' },
      { key: 'goodwill_net_asset_pct', label: '商誉 / 净资产', kind: 'percent' },
      { key: 'unlock_date', label: '解禁日', kind: 'date' },
      { key: 'unlock_shares', label: '解禁股数', kind: 'number' },
      { key: 'unlock_total_share_pct', label: '解禁占总股本', kind: 'percent' },
      { key: 'unlock_reason', label: '解禁原因', kind: 'text' }
    ]
  },
  {
    id: 'financials',
    label: '财报与估值摘要',
    resource: 'GX_CBSJ101',
    description: '报告期估值、成长、盈利质量、费用率及机构持仓摘要。',
    fields: [
      { key: 'report_period', label: '报告期', kind: 'date' },
      { key: 'market_value_100m', label: '市值（亿元）', kind: 'number' },
      { key: 'pe_ttm', label: 'PE-TTM', kind: 'number' },
      { key: 'pb_mrq', label: 'PB-MRQ', kind: 'number' },
      { key: 'peg', label: 'PEG', kind: 'number' },
      { key: 'debt_ratio_pct', label: '资产负债率', kind: 'percent' },
      { key: 'profit_yoy_pct', label: '利润同比', kind: 'percent' },
      { key: 'revenue_yoy_pct', label: '营收同比', kind: 'percent' },
      { key: 'roe_pct', label: 'ROE', kind: 'percent' },
      { key: 'gross_margin_pct', label: '毛利率', kind: 'percent' },
      { key: 'rd_expense_ratio_pct', label: '研发费用率', kind: 'percent' },
      { key: 'sales_expense_ratio_pct', label: '销售费用率', kind: 'percent' },
      { key: 'management_expense_ratio_pct', label: '管理费用率', kind: 'percent' },
      { key: 'finance_expense_ratio_pct', label: '财务费用率', kind: 'percent' },
      { key: 'revenue_turnover_pct', label: '营收周转率', kind: 'percent' },
      { key: 'asset_turnover_pct', label: '资产周转率', kind: 'percent' },
      { key: 'institution_holding_float_pct', label: '机构持流通股', kind: 'percent' },
      { key: 'dividend_year', label: '分红年度', kind: 'text' },
      { key: 'dividend_yield_pct', label: '股息率', kind: 'percent' }
    ]
  },
  {
    id: 'earnings-forecast',
    label: '业绩预告与预测估值',
    resource: 'GX_CBYG101',
    description: '业绩预告区间、同比变化及预测利润边界。',
    fields: [
      { key: 'forecast_date', label: '预告日', kind: 'date' },
      { key: 'report_period', label: '报告期', kind: 'date' },
      { key: 'forecast_type', label: '预告类型', kind: 'text' },
      { key: 'net_profit_lower', label: '净利润下限', kind: 'number' },
      { key: 'net_profit_upper', label: '净利润上限', kind: 'number' },
      { key: 'net_profit_previous', label: '上年净利润', kind: 'number' },
      { key: 'profit_yoy_lower_pct', label: '同比下限', kind: 'percent' },
      { key: 'profit_yoy_upper_pct', label: '同比上限', kind: 'percent' },
      { key: 'eps', label: '每股收益', kind: 'number' },
      { key: 'forecast_profit_lower', label: '预测利润下限', kind: 'number' },
      { key: 'forecast_profit_upper', label: '预测利润上限', kind: 'number' },
      { key: 'total_shares_10k', label: '总股本（万股）', kind: 'number' }
    ]
  },
  {
    id: 'analyst-estimate',
    label: '机构评级与盈利预测',
    resource: 'GX_FXSPJ101',
    description: '近半年机构覆盖、综合评级、目标价和三年 EPS 预测。',
    fields: [
      { key: 'latest_date', label: '最新日期', kind: 'date' },
      { key: 'research_count_6m', label: '近半年研报数', kind: 'number' },
      { key: 'composite_rating', label: '综合评级', kind: 'text' },
      { key: 'pe', label: 'PE', kind: 'number' },
      { key: 'forecast_eps_growth_pct', label: '预测 EPS 增长', kind: 'percent' },
      { key: 'target_price', label: '目标价', kind: 'price' },
      { key: 'report_period', label: '报告期', kind: 'date' },
      { key: 'current_eps', label: '当前 EPS', kind: 'number' },
      { key: 'industry', label: '行业', kind: 'text' },
      { key: 'forecast_base_year', label: '预测基年', kind: 'text' },
      { key: 'forecast_eps_t', label: 'EPS 当年', kind: 'number' },
      { key: 'forecast_eps_t1', label: 'EPS 次年', kind: 'number' },
      { key: 'forecast_eps_t2', label: 'EPS 后年', kind: 'number' }
    ]
  },
  {
    id: 'distribution',
    label: '高送转与分红送配',
    resource: 'GX_ZFSP101',
    description: '每股资本、分红送转实施进度及配股时间窗口。',
    fields: [
      { key: 'capital_reserve_per_share', label: '每股资本公积', kind: 'number' },
      { key: 'undistributed_profit_per_share', label: '每股未分配利润', kind: 'number' },
      { key: 'parent_profit', label: '归母净利润', kind: 'number' },
      { key: 'parent_profit_yoy_pct', label: '归母利润同比', kind: 'percent' },
      { key: 'announcement_date', label: '公告日', kind: 'date' },
      { key: 'bonus_transfer_per_10', label: '每 10 股送转', kind: 'number' },
      { key: 'cash_dividend_per_10', label: '每 10 股派现', kind: 'number' },
      { key: 'record_date', label: '股权登记日', kind: 'date' },
      { key: 'ex_dividend_date', label: '除权除息日', kind: 'date' },
      { key: 'plan_stage', label: '方案进度', kind: 'text' },
      { key: 'rights_announcement_date', label: '配股公告日', kind: 'date' },
      { key: 'rights_code', label: '配股代码', kind: 'text' },
      { key: 'rights_ratio', label: '配股比例', kind: 'number' },
      { key: 'rights_start_date', label: '配股起始日', kind: 'date' },
      { key: 'rights_end_date', label: '配股截止日', kind: 'date' }
    ]
  },
  {
    id: 'lhb-overview',
    label: '龙虎榜成交与席位概览',
    resource: 'GX_LHBD101',
    description: '单次龙虎榜事件的成交占比、净买额和机构席位数量。',
    fields: [
      { key: 'event_id', label: '事件 ID', kind: 'text' },
      { key: 'event_date', label: '上榜日', kind: 'date' },
      { key: 'buy_turnover_pct', label: '买入占成交', kind: 'percent' },
      { key: 'sell_turnover_pct', label: '卖出占成交', kind: 'percent' },
      { key: 'net_buy', label: '净买额', kind: 'number' },
      { key: 'buy_total', label: '买入总额', kind: 'number' },
      { key: 'sell_total', label: '卖出总额', kind: 'number' },
      { key: 'stock_connect_seats', label: '陆股通席位', kind: 'number' },
      { key: 'institution_seats', label: '机构席位', kind: 'number' },
      { key: 'abnormal_reason', label: '异动原因', kind: 'text' }
    ]
  },
  {
    id: 'margin-overview',
    label: '融资融券概览',
    resource: 'GX_RZRQ101',
    description: '融资余额、净买入、融券余量及两融差额。',
    fields: [
      { key: 'statistics_date', label: '统计日', kind: 'date' },
      { key: 'float_market_value', label: '流通市值', kind: 'number' },
      { key: 'float_shares', label: '流通股本', kind: 'number' },
      { key: 'financing_balance', label: '融资余额', kind: 'number' },
      { key: 'financing_net_buy', label: '融资净买入', kind: 'number' },
      { key: 'financing_market_value_pct', label: '融资 / 流通市值', kind: 'percent' },
      { key: 'short_balance_shares', label: '融券余量', kind: 'number' },
      { key: 'short_net_sell_shares', label: '融券净卖出', kind: 'number' },
      { key: 'short_float_share_pct', label: '融券 / 流通股', kind: 'percent' },
      { key: 'margin_difference', label: '两融差额', kind: 'number' }
    ]
  },
  {
    id: 'ownership-change',
    label: '股东增减持区间',
    resource: 'GX_ZCJC101',
    description: '统计区间内股东增持、减持规模和成交价格边界。',
    fields: [
      { key: 'increase_value_10k', label: '增持金额（万元）', kind: 'number' },
      { key: 'decrease_value_10k', label: '减持金额（万元）', kind: 'number' },
      { key: 'net_change_value_10k', label: '净增减持（万元）', kind: 'number' },
      { key: 'increase_price_min', label: '增持最低价', kind: 'price' },
      { key: 'increase_price_max', label: '增持最高价', kind: 'price' },
      { key: 'decrease_price_min', label: '减持最低价', kind: 'price' },
      { key: 'decrease_price_max', label: '减持最高价', kind: 'price' },
      { key: 'start_date', label: '区间开始', kind: 'date' },
      { key: 'end_date', label: '区间结束', kind: 'date' }
    ]
  }
];
