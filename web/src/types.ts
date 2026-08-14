export interface Health {
  ok: boolean;
  service: string;
  version: string;
  api_version: string;
  native_cpp: boolean;
  python_runtime: boolean;
  tdx_root: string;
  securities: number;
  blocks: number;
  memberships: number;
  formulas: number;
  formula_library_origin: 'bundled-snapshot';
  formula_runtime_dll_accessed: boolean;
  formula_icon_cells: number;
  formula_icon_runtime_dll_accessed: boolean;
  jsn_available: boolean;
  jsn_root: string;
}

export interface QuoteRecord {
  security_id: string;
  market_id: number;
  code: string;
  name: string;
  last_price: number;
  pre_close_price: number;
  open_price: number;
  high_price: number;
  low_price: number;
  change_pct: number | null;
  total_hand: number;
  current_hand: number;
  amount: number;
  inside_dish: number;
  outer_disc: number;
  open_amount_yuan: number;
}

export interface QuoteDocument {
  generated_at: string;
  endpoint: string;
  server_name: string;
  received: number;
  records: QuoteRecord[];
}

export interface DepthLevel {
  price: number;
  volume_hand: number;
  amount_yuan: number;
}

export interface DepthRecord extends QuoteRecord {
  update_time_raw: number;
  buy_levels: DepthLevel[];
  sell_levels: DepthLevel[];
  bid1_amount_yuan: number;
  ask1_amount_yuan: number;
}

export interface DepthDocument extends Omit<QuoteDocument, 'records'> {
  records: DepthRecord[];
}

export interface SpeedRecord extends QuoteRecord {
  active: number;
  auction_imbalance_hand_raw: number;
  auxiliary_price_delta_raw: number;
  buy_levels: DepthLevel[];
  extension_marker_raw: number;
  extension_values_raw: number[];
  fund_iopv: number | null;
  rise_speed_pct: number;
  rise_speed_raw: number;
  sell_levels: DepthLevel[];
  status_raw: number;
  tail_raw: number;
  time_raw: number;
}

export interface SpeedDocument extends Omit<QuoteDocument, 'records'> {
  schema: 'tdx-market-speed-native-v1';
  command: '0x053E';
  requested: number;
  transport: {
    available_endpoint_count: number;
    connection_attempts: number;
    endpoint_failover: boolean;
    endpoint_source: string;
    endpoints_attempted: number;
    max_attempts_per_endpoint: number;
    primary_configured: boolean;
    recovered_after_retry: boolean;
    transient_retries: number;
  };
  records: SpeedRecord[];
}

export interface MarketStreamStatusDocument {
  schema: 'tdx-market-l1-stream-status-v1';
  generated_at: string;
  upstream_mode: 'public-7709-L1-persistent-polling';
  downstream_mode: 'local-sse';
  command: '0x0547';
  subscriber_count: number;
  active_security_count: number;
  active_securities: string[];
  poll_count: number;
  successful_polls: number;
  failed_polls: number;
  consecutive_failures: number;
  last_error: string | null;
  last_success_at: string | null;
  last_source: unknown | null;
  interval_ms: number;
  heartbeat_ms: number;
  max_backoff_ms: number;
  off_session_interval_ms: number;
  market_hours_throttle: boolean;
  exchange_session_active: boolean;
  effective_interval_ms: number;
  subscriber_queue_limit: number;
  fast_hq_boundary: {
    fast_hq_subscribe_used: false;
    upstream_mode: 'public 7709 bounded polling';
    downstream_mode: 'local SSE push';
    reason: string;
  };
}

export interface RankingRecord {
  security_id: string;
  market_id: number;
  code: string;
  name: string;
  last_price: number;
  pre_close_price: number;
  change_pct: number | null;
  amount: number;
  total_hand: number;
  current_hand: number;
  open_amount_yuan: number;
  bid1_price: number;
  ask1_price: number;
  bid1_volume_hand: number;
  ask1_volume_hand: number;
  seal_amount_yuan: number;
  is_sealed: boolean;
  rise_speed: number;
  short_turnover: number;
  two_minute_amount: number;
  opening_rush: number;
  volume_rise_speed: number;
  depth: number;
}

export interface LimitQualityAuction {
  last_virtual_price: number | null;
  formal_open_price: number | null;
  opening_rush_pct: number | null;
  recomputed_opening_rush_pct: number | null;
  opening_rush_error_pct_point: number | null;
  last_sample_matched_volume_hand: number | null;
  last_sample_matched_amount_yuan: number | null;
  last_sample_unmatched_signed_hand: number | null;
  last_sample_unmatched_volume_hand: number | null;
  last_sample_unmatched_direction_raw: number | null;
  unmatched_direction_flips: number | null;
}

export interface LimitQualityMetrics {
  depth_status: 'sealed' | 'ranking-stale-or-unsealed' | 'depth-missing';
  current_seal_amount_yuan: number | null;
  free_float_market_value_yuan: number | null;
  seal_free_float_pct: number | null;
  stats_date: string | null;
  stats_alignment: 'same-day' | 'previous-resource-day' | 'stats-date-unaligned' | 'stats-unavailable';
  previous_positive_seal_amount_yuan: number | null;
  seal_to_previous: number | null;
  seal_change_pct: number | null;
  seal_decay_pct: number | null;
  statistics_current_seal_amount_yuan: number | null;
  statistics_consistency_error_yuan: number | null;
  statistics_matches_depth: boolean | null;
  auction: LimitQualityAuction | null;
}

export interface LimitQualityRecord extends RankingRecord {
  quality: LimitQualityMetrics;
}

export interface LimitQualityDocument {
  schema: string;
  generated_at: string;
  trade_date: string | null;
  requested_limit: number;
  auction_limit: number;
  cache_refreshed: boolean;
  cache_age_seconds: number;
  cache_ttl_seconds: number;
  statistics_cache_age_seconds: number;
  source_errors: Array<{ source: string; message: string }>;
  summary: {
    records: number;
    depth_sealed: number;
    ranking_stale_or_unsealed: number;
    total_current_seal_amount_yuan: number;
    stats_linked: number;
    history_comparable: number;
    statistics_comparable: number;
    statistics_matched: number;
    auction_linked: number;
  };
  records: LimitQualityRecord[];
}

export interface RankingDocument {
  schema: string;
  generated_at: string;
  command: string;
  endpoint: string;
  server_name: string;
  category: number;
  sort: string;
  sort_type: string;
  received: number;
  records: RankingRecord[];
}

export interface AuctionPoint {
  index: number;
  time_label: string;
  time_seconds: number;
  price: number;
  matched_volume_hand: number;
  matched_amount_yuan: number;
  unmatched_signed_hand: number;
  unmatched_volume_hand: number;
  unmatched_direction_raw: number;
  unmatched_direction: 'buy' | 'sell' | 'balanced';
}

export interface AuctionSegmentSummary {
  point_count: number;
  start_time: string | null;
  end_time: string | null;
  first_price: number | null;
  last_sample_price: number | null;
  min_price: number | null;
  max_price: number | null;
  last_sample_matched_volume_hand: number | null;
  last_sample_matched_amount_yuan: number | null;
  last_sample_unmatched_signed_hand: number | null;
  last_sample_unmatched_volume_hand: number | null;
  last_sample_unmatched_direction_raw: number | null;
  unmatched_direction_flips: number;
  max_unmatched_volume_hand: number | null;
  max_unmatched_time: string | null;
  matched_volume_monotonic_violations: number;
}

export interface AuctionRecord {
  market_id: number;
  code: string;
  security_id: string;
  name: string;
  selector: number;
  summary: {
    point_count: number;
    opening: AuctionSegmentSummary;
    closing: AuctionSegmentSummary;
    largest_gap_seconds: number;
  };
  points: AuctionPoint[];
}

export interface AuctionDocument {
  schema: string;
  generated_at: string;
  endpoint: string;
  server_name: string;
  server_trade_date: string;
  records: AuctionRecord[];
}

export interface TradeTick {
  index: number;
  absolute_index: number;
  time_minutes: number;
  time_label: string;
  price: number;
  volume_hand: number;
  amount_yuan: number;
  order_count: number;
  status_raw: number;
  side: string;
  price_delta_raw: number;
  price_acc_raw: number;
  tail_raw: number;
}

export interface TradeAggregate {
  tick_count: number;
  volume_hand: number;
  amount_yuan: number;
  order_count: number;
  vwap: number | null;
  first_price: number | null;
  last_price: number | null;
  high_price: number | null;
  low_price: number | null;
  buy_volume_hand: number;
  sell_volume_hand: number;
  neutral_volume_hand: number;
  buy_amount_yuan: number;
  sell_amount_yuan: number;
  neutral_amount_yuan: number;
  status_counts: Record<string, number>;
}

export interface TradeMinute extends TradeAggregate {
  time: string;
  time_minutes: number;
}

export interface TradeSummary extends TradeAggregate {
  first_time: string | null;
  last_time: string | null;
  minute_count: number;
  auction_0925: TradeAggregate & { execution_status: string };
  closing_1500: TradeAggregate & { execution_status: string };
  post_close_status_5: TradeAggregate & { first_time: string | null; last_time: string | null };
  minutes: TradeMinute[];
}

export interface TradeRecord {
  market_id: number;
  code: string;
  security_id: string;
  name: string;
  trading_date: string;
  source_mode: 'today' | 'history';
  pages: number;
  page_size: number;
  price_divisor: number;
  price_base_raw: number | null;
  summary: TradeSummary;
  ticks: TradeTick[];
}

export interface TradesDocument {
  schema: string;
  generated_at: string;
  command: '0x0FC5' | '0x0FC6';
  endpoint: string;
  server_name: string;
  server_trade_date: string;
  tick_count: number;
  records: TradeRecord[];
}

export interface TdxStatFields {
  stats_date: string | null;
  beta_60d: number | null;
  pe_ttm: number | null;
  pe_static: number | null;
  free_float_shares_10k: number | null;
  free_float_shares: number | null;
  year_limit_up_days: number | null;
  limit_stat_days: number | null;
  limit_up_count_in_stat_days: number | null;
  limit_up_streak_days: number | null;
}

export interface TdxStat2Fields {
  stats_date: string | null;
  amount_10k: number | null;
  amount_yuan: number | null;
  seal_amount_10k: number | null;
  seal_amount_yuan: number | null;
  prev_amount_10k: number | null;
  prev_amount_yuan: number | null;
  prev_seal_amount_10k: number | null;
  prev_seal_amount_yuan: number | null;
  prev2_amount_10k: number | null;
  prev2_amount_yuan: number | null;
  prev2_seal_amount_10k: number | null;
  prev2_seal_amount_yuan: number | null;
  open_volume_hand: number | null;
  prev_open_volume_hand: number | null;
  open_amount_10k: number | null;
  open_amount_yuan: number | null;
  prev_open_amount_10k: number | null;
  prev_open_amount_yuan: number | null;
}

export interface StatsRecord {
  market_id: number;
  code: string;
  security_id: string;
  name: string;
  stat: TdxStatFields | null;
  stat2: TdxStat2Fields | null;
}

export interface StatsDocument {
  schema: string;
  generated_at: string;
  command: string | null;
  source_path: string;
  endpoint: string;
  server_name: string;
  archive_size: number;
  stats_date: string | null;
  stats_date_coverage: number;
  stat_count: number;
  stat2_count: number;
  returned: number;
  cache_refreshed: boolean;
  cache_ttl_seconds: number;
  cache_age_seconds: number;
  valuation?: SecurityValuationDocument;
  records: StatsRecord[];
}

export interface SecurityValuationMetric {
  code: '$PE' | '$PES' | '$PETTM' | '$PBMRQ';
  label: string;
  value: number | null;
  source: string;
  as_of: string | null;
}

export interface SecurityValuationDocument {
  schema: 'tdx-security-valuation-native-v1';
  generated_at: string;
  market_id: number;
  code: string;
  security_id: string;
  name: string;
  availability: 'complete' | 'partial' | 'unavailable';
  available_metric_count: number;
  metrics: {
    pe_dynamic: SecurityValuationMetric;
    pe_static: SecurityValuationMetric;
    pe_ttm: SecurityValuationMetric;
    pb_mrq: SecurityValuationMetric;
  };
  inputs: {
    price: number | null;
    price_field: 'last_price' | 'pre_close_price' | null;
    annualized_eps: number | null;
    net_profit_yuan: number | null;
    report_months: number | null;
    total_shares: number | null;
    net_assets_per_share: number | null;
    net_assets_per_share_source: string;
    finance_updated_date: string | null;
    stats_date: string | null;
  };
  errors: string[];
}

export interface MinuteBar {
  date: string;
  time: string;
  open: number;
  high: number;
  low: number;
  close: number;
  amount: number;
  volume: number;
  extra_1: number;
  extra_2: number;
  adjustment_factor?: number;
}

export interface MinuteDocument {
  market: string;
  code: string;
  source: string;
  count: number;
  downloaded?: number;
  start?: number;
  page_size?: number;
  next_start?: number;
  has_more?: boolean;
  endpoint?: string;
  server_name?: string;
  index_mode?: boolean;
  period?: string;
  period_id?: number;
  adjustment_mode?: string;
  adjustment?: {
    mode: string;
    source_command: string;
    method: string;
    anchor_date: string | null;
    normalization: number;
    minimum_factor: number;
    maximum_factor: number;
    applied_event_count: number;
  };
  bars: MinuteBar[];
}

export interface FinanceRecord {
  security_id: string;
  market: string;
  market_id: number;
  code: string;
  province_id: number;
  industry_id: number;
  updated_date_raw: number;
  updated_date: string | null;
  listing_date_raw: number;
  listing_date: string | null;
  shares: {
    circulating: number;
    total: number;
    national: number;
    promoter_legal_person: number;
    legal_person: number;
    b_share: number;
    h_share: number;
  };
  per_share: { eps: number; net_assets: number };
  shareholder_count: number;
  balance_sheet: {
    total_assets_yuan: number;
    current_assets_yuan: number;
    fixed_assets_yuan: number;
    intangible_assets_yuan: number;
    current_liabilities_yuan: number;
    long_term_liabilities_yuan: number;
    capital_reserve_yuan: number;
    net_assets_yuan: number;
    accounts_receivable_yuan: number;
    inventory_yuan: number;
  };
  income_statement: {
    revenue_yuan: number;
    main_profit_yuan: number;
    operating_profit_yuan: number;
    investment_income_yuan: number;
    total_profit_yuan: number;
    after_tax_profit_yuan: number;
    net_profit_yuan: number;
    undistributed_profit_yuan: number;
  };
  cash_flow: { operating_yuan: number; total_yuan: number };
}

export interface FinanceDocument {
  schema: string;
  generated_at: string;
  command: string;
  endpoint: string;
  server_name: string;
  requested: number;
  received: number;
  records: FinanceRecord[];
}

export interface CapitalChangeRecord {
  security_id: string;
  market: string;
  market_id: number;
  code: string;
  date_raw: number;
  date: string | null;
  category: number;
  category_name: string;
  details: Record<string, number>;
}

export interface CapitalChangeBlock {
  security_id: string;
  market: string;
  market_id: number;
  code: string;
  block_count: number;
  count: number;
  records: CapitalChangeRecord[];
}

export interface CapitalDocument {
  schema: string;
  generated_at: string;
  command: string;
  endpoint: string;
  server_name: string;
  requested: number;
  received: number;
  event_count: number;
  blocks: CapitalChangeBlock[];
}

export interface SpecialLimitRecord {
  index: number;
  security_id: string;
  market: string;
  market_id: number;
  code: string;
  code_number: number;
  limit_up_price: number;
  limit_down_price: number;
}

export interface SpecialLimitsDocument {
  schema: string;
  generated_at: string;
  command: string;
  endpoint: string;
  server_name: string;
  count: number;
  total_count?: number;
  cache_source?: string;
  cache_age_seconds?: number;
  cache_ttl_seconds?: number;
  records: SpecialLimitRecord[];
}

export interface JsnMatch {
  resource: string;
  group: number;
  row_index: number;
  matched_by: string[][];
  record: Record<string, unknown>;
}

export interface JsnSecurityDocument {
  market: string;
  code: string;
  resource_count: number;
  match_count: number;
  truncated: boolean;
  matches: JsnMatch[];
}

export interface SecurityProfileSource {
  resource: string;
  size: number;
  row_count: number;
  endpoint: string;
}

export interface SecurityProfileError {
  resource: string;
  message: string;
}

export interface InstitutionHolder {
  rank: string | number;
  type: string;
  name: string;
  short_name: string;
  holding_shares: string | number;
  free_float_share: string | number;
  change_shares: string | number;
  change_label: string;
  holder_id: string;
  variant_id: string;
  reference_code: string;
  source_url: string;
  queryable: boolean;
  raw: Record<string, unknown>;
}

export interface SecurityProfileDocument {
  schema: string;
  market: string;
  code: string;
  institution_history: Array<Record<string, unknown>>;
  top_float_holders: Array<Record<string, unknown>>;
  holders?: InstitutionHolder[];
  sources: SecurityProfileSource[];
  errors: SecurityProfileError[];
}

export interface ProfessionalFinanceValue {
  id: number;
  value: number | null;
}

export interface ProfessionalFinancePeriod {
  report_date_raw: number;
  report_date: string;
  fields: ProfessionalFinanceValue[];
}

export interface ProfessionalPackageError {
  report_date_raw: number;
  report_date: string;
  package: string;
  error: string;
}

export interface ProfessionalFinanceSeriesDocument {
  schema: 'tdx-professional-finance-series-v1';
  security_id: string;
  mode: 'report-period-series-no-announcement-date';
  chronological: boolean;
  attempted_packages: number;
  period_count: number;
  periods: ProfessionalFinancePeriod[];
  package_errors: ProfessionalPackageError[];
}

export interface ProfessionalTradingField {
  id: number;
  name: string | null;
  documented: boolean;
  count: number;
  first_date: string | null;
  last_date: string | null;
  latest_value1: number | null;
  latest_value2: number | null;
}

export interface ProfessionalTradingHistoryPoint {
  id: number;
  date_raw: number;
  date: string;
  value1: number | null;
  value2: number | null;
}

export interface ProfessionalTradingDocument {
  schema: 'tdx-professional-trading-v1';
  kind: 'stock';
  security_id: string;
  record_count: number;
  observed_id_count: number;
  fields: ProfessionalTradingField[];
  history: ProfessionalTradingHistoryPoint[];
  history_truncated: boolean;
}

export type ProfessionalSecurityDocument =
  | ProfessionalFinanceSeriesDocument
  | ProfessionalTradingDocument;

export interface HolderHistoryRecord {
  row_id: string;
  currently_held: boolean;
  stock_record_count: string | number;
  report_date: string;
  market: string;
  market_id: number | null;
  code: string;
  security_id: string;
  name: string;
  holding_shares: string | number;
  share_percent: string | number;
  shareholder_list_type: string | number;
  share_nature: string;
  change_shares: string | number;
  change_type: string | number;
  change_label: string;
}

export interface HolderStockPeriod {
  report_date: string;
  holding_shares: string | number;
  share_percent: string | number;
  share_nature: string;
  change_type: string | number;
  change_label: string;
  change_shares: string | number;
  shareholder_list_type: string | number;
}

export interface HolderHistoryDocument {
  schema: string;
  holder: {
    holder_id: string;
    variant_id: string;
    name: string;
    reference_code: string;
  };
  counts: {
    records: number;
    unique_stocks: number;
    currently_held_records: number;
    stock_periods: number;
  };
  pagination: {
    offset: number;
    limit: number;
    returned: number;
    total: number;
    has_previous: boolean;
    has_next: boolean;
  };
  records: HolderHistoryRecord[];
  stock_detail: {
    code: string;
    records: HolderStockPeriod[];
  };
  cache: {
    history_ttl_seconds: number;
    history_refreshed: boolean;
    history_age_seconds: number;
    detail_ttl_seconds: number;
    detail_refreshed: boolean;
    detail_age_seconds: number;
  };
}

export interface IntradayFundPeriod {
  name: string;
  net_main_inflow: string | number;
  turnover: string | number;
  net_main_share_pct: string | number;
  change_pct?: string | number;
  volume?: string | number;
  relative_volume?: number | null;
}

export interface IntradayFundRecord {
  market: string;
  market_id: number;
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
  quote: {
    last: string | number;
    change_pct: string | number;
    change: string | number;
    previous_5day_minute_volume: string | number;
  };
  periods: Record<string, IntradayFundPeriod>;
}

export interface IntradayFundsDocument {
  schema: string;
  generated_at: string;
  mode: 'security';
  cache: {
    ttl_seconds: number;
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_refreshed: boolean;
    detail_age_seconds: number;
  };
  security: {
    market_id: number;
    market: string;
    code: string;
    security_id: string;
    name: string;
    name_resolved: boolean;
  };
  industry: {
    market: string;
    code: string;
    name: string;
    summary: IntradayFundRecord;
    component_count: number;
  };
  found: boolean;
  funds: IntradayFundRecord | null;
}

export interface LhbDetailRow {
  market: string;
  code: string;
  date: string;
  category: string | number;
  broker: string;
  side: 'B' | 'S' | string;
  rank: string | number;
  buy_amount: string | number;
  sell_amount: string | number;
  net_buy: string | number;
  share_percent: string | number;
  buy_success_rate_1d: string | number;
  buy_success_rate_3d: string | number;
  buy_success_rate_5d: string | number;
  estimated_cost: string | number;
  estimated_return: string | number;
  tag: string;
  operation_url: string;
  row_type: 'broker' | 'total';
}

export interface LhbEvent {
  event_id: string;
  source_row_index: number;
  date: string;
  dates: string[];
  views: string[];
  event_types: string[];
  key_conflict: boolean;
  security: {
    market: string;
    market_id: number;
    code: string;
    security_id: string;
    name: string;
  };
  metrics: {
    buy_share_pct: string | number;
    sell_share_pct: string | number;
    net_buy: string | number;
    buy_amount: string | number;
    sell_amount: string | number;
    institution_buy_count: string | number;
    institution_sell_count: string | number;
    category: string | number;
  };
  detail_available: boolean;
  detail_resource?: string;
  detail_row_count?: number;
  details: LhbDetailRow[];
}

export interface LhbDocument {
  schema: string;
  generated_at: string;
  mode: 'security' | 'event' | 'master';
  security?: {
    market: string;
    market_id: number;
    code: string;
    security_id: string;
    name: string;
  };
  master_counts: {
    master_views: number;
    master_rows: number;
    events: number;
    event_view_memberships: number;
    stocks: number;
    conflicting_master_keys: number;
  };
  cache: {
    master_ttl_seconds: number;
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_ttl_seconds: number;
    details_refreshed: number;
    maximum_detail_age_seconds: number;
  };
  summary: {
    event_count: number;
    unique_dates: number;
    view_memberships: number;
    detail_rows: number;
    event_reported_buy_amount_sum: number;
    event_reported_sell_amount_sum: number;
    event_reported_net_buy_sum: number;
    latest_date: string | null;
  };
  events: LhbEvent[];
}

export interface JsnResource {
  resource: string;
  size: number;
  groups: number;
  rows: number;
  unique_securities: number;
  unique_member_securities: number;
  columns: string[];
}

export interface JsnCatalogDocument {
  counts: { resources: number; bytes: number; rows: number };
  resources: JsnResource[];
}

export interface JsnColumnProfile {
  seen: number;
  nonempty: number;
  numeric: number;
  integer: number;
  date_like: number;
  url_like: number;
  html_like: number;
  samples: string[];
}

export interface JsnDiscoveryFile {
  resource: string;
  status: 'added' | 'changed' | 'removed' | 'unchanged' | 'unbaselined';
  family: string;
  template: string | null;
  matched_templates: string[];
  key_values: Record<string, string>;
  coverage: 'typed-command' | 'generic-only' | 'unrecognized';
  typed_commands: string[];
  api_endpoints: string[];
  bytes: number;
  sha256: string;
  groups: number | null;
  rows: number;
  columns: string[];
  column_profiles: Record<string, JsnColumnProfile>;
  new_columns: string[];
  removed_columns: string[];
  security_related: boolean;
  parse_error: string | null;
  priority_score: number;
}

export interface JsnDiscoveryDocument {
  schema: string;
  generated_at: string;
  scan_root: string;
  field_profile_sample_limit_per_column: number;
  baseline: {
    path: string | null;
    available: boolean;
    captured_at: string | null;
    file_count: number;
    captured_now: boolean;
  };
  summary: {
    baseline_available: boolean;
    current_file_count: number;
    baseline_file_count: number;
    added_file_count: number;
    changed_file_count: number;
    removed_file_count: number;
    unchanged_file_count: number;
    unbaselined_file_count: number;
    recognized_file_count: number;
    unrecognized_file_count: number;
    typed_file_count: number;
    generic_only_file_count: number;
    security_related_file_count: number;
    parse_error_file_count: number;
    new_column_count: number;
    removed_column_count: number;
  };
  changes: JsnDiscoveryFile[];
  priority_changes: JsnDiscoveryFile[];
  files: JsnDiscoveryFile[];
}

export interface JsnCandidate {
  template: string;
  resource: string;
  family: string;
  key_values: Record<string, string>;
  status: 'local-present' | 'local-missing';
  origin: 'refunit+observed' | 'refunit-row' | 'observed-only';
  probe_eligible: boolean;
  local_bytes: number | null;
  source_resources: string[];
  source_unit_ids: string[];
  source_configs: string[];
  evidence_rows: number;
  priority_score: number;
}

export interface JsnCandidateTemplate {
  template: string;
  family: string;
  relation_found: boolean;
  master_resources: string[];
  loaded_master_resources: number;
  candidate_count: number;
  observed_count: number;
  missing_count: number;
  issues: string[];
}

export interface JsnCandidateDocument {
  schema: string;
  generated_at: string;
  scan_root: string;
  family: string | null;
  missing_only: boolean;
  network_used: boolean;
  safety: string;
  summary: {
    dynamic_template_count: number;
    refunit_template_count: number;
    candidate_count: number;
    unique_resource_count: number;
    derived_candidate_count: number;
    observed_candidate_count: number;
    local_missing_candidate_count: number;
    unique_missing_resource_count: number;
    returned_candidate_count: number;
    probe_queue_count: number;
    probed_count: number;
  };
  probe_queue: string[];
  probe_results: JsnTransferResult[];
  templates: JsnCandidateTemplate[];
  candidates: JsnCandidate[];
}

export interface JsnTransferResult {
  resource: string;
  remote_path: string;
  endpoint: string;
  server: string;
  size: number;
  md5: string;
  has_md5: boolean;
  output?: string;
  status?: string;
}

export interface InstallArtifact {
  path: string;
  size: number;
  modified: string;
}

export interface InstallDocument {
  root: string;
  count: number;
  artifacts: InstallArtifact[];
}

export interface Feature {
  command: string;
  title: string;
  category: string;
  description: string;
  uses_network: boolean;
  api_endpoint: string;
  api_operations?: Array<{
    method: string;
    path: string;
  }>;
  surface?: 'http' | 'cli-only';
  implemented: boolean;
}

export interface FeatureDocument {
  count: number;
  features: Feature[];
}

export interface TqlexConfig {
  body: string;
  entry: string;
  placeholders: string[];
  request_format: string;
  request_id: string;
  source_file: string;
}

export interface TqlexConfigsDocument {
  schema: string;
  config_count: number;
  entry_count: number;
  request_id_count: number;
  records: TqlexConfig[];
}

export interface TqlexColumn {
  Name?: string;
  Caption?: string;
  [key: string]: unknown;
}

export interface TqlexResultSet {
  ColDes?: TqlexColumn[];
  ColNum?: number | string;
  Content?: unknown[][];
  RowNum?: number | string;
  [key: string]: unknown;
}

export interface TqlexResponse {
  ErrorCode?: number | string;
  ErrorInfo?: string;
  ResultSetNum?: number | string;
  ResultSets?: TqlexResultSet[];
  [key: string]: unknown;
}

export interface TqlexQueryDocument {
  schema: string;
  request_id: string;
  entry: string;
  source_file: string;
  all_pages: boolean;
  request: unknown;
  response: TqlexResponse;
}

export interface PbrpcConfig extends TqlexConfig {
  module: string;
}

export interface PbrpcConfigsDocument {
  schema: string;
  config_count: number;
  entry_count: number;
  module_count: number;
  request_id_count: number;
  records: PbrpcConfig[];
}

export interface PbrpcQueryDocument {
  schema: string;
  request_id: string;
  entry: string;
  module: string;
  source_file: string;
  rpc_id: number;
  rounds: number;
  raw_size: number;
  request: unknown;
  response: TqlexResponse;
}

export interface CloudWorkflowStepRef {
  transport: 'json' | 'pbrpc';
  request_id: string;
}

export interface CloudWorkflowCatalogItem {
  name: string;
  description: string;
  key_field: string;
  master: CloudWorkflowStepRef;
  details: CloudWorkflowStepRef[];
  defaults: Record<string, string>;
}

export interface CloudWorkflowsDocument {
  schema: string;
  count: number;
  records: CloudWorkflowCatalogItem[];
}

export interface CloudWorkflowStepDocument {
  transport: 'json' | 'pbrpc';
  req_id: string;
  request_id: string;
  entry: string;
  module?: string;
  source_file: string;
  response: TqlexResponse;
  [key: string]: unknown;
}

export interface CloudWorkflowDetail {
  key: string;
  master_row: Record<string, unknown>;
  children: CloudWorkflowStepDocument[];
}

export interface CloudWorkflowDocument {
  schema: string;
  workflow: string;
  description: string;
  parameters: Record<string, string>;
  selection: {
    key_field: string;
    master_rows: number;
    selected_rows: number;
  };
  master: CloudWorkflowStepDocument;
  details: CloudWorkflowDetail[];
}

export interface CloudVariantCoverageSummary {
  template_count: number;
  tqlex_template_count: number;
  pbrpc_template_count: number;
  semantic_variant_count: number;
  fixed_variant_count: number;
  generic_only_variant_count: number;
  duplicate_template_count: number;
  parse_error_count: number;
  tqlex_request_id_count: number;
  fixed_tqlex_request_id_count: number;
  pbrpc_request_id_count: number;
  fixed_pbrpc_request_id_count: number;
  fully_fixed: boolean;
}

export interface CloudVariantCoverageRecord {
  transport: 'tqlex' | 'pbrpc';
  request_id: string | null;
  entry: string;
  module: string | null;
  variant_id: string;
  body_size: number;
  template_occurrences: number;
  source_files: string[];
  placeholders: string[];
  request_fields: string[];
  selectors: Record<string, string>;
  parse_error: string | null;
  coverage: 'fixed-command' | 'generic-only';
  fixed_command: string | null;
  selection_key: 'request-id+selectors' | 'entry+body';
}

export interface CloudVariantCoverageDocument {
  schema: 'tdx-cloud-variant-coverage-native-v1';
  gaps_only: boolean;
  semantics: string;
  summary: CloudVariantCoverageSummary;
  commands: Array<{ command: string; semantic_variants: number }>;
  variants: CloudVariantCoverageRecord[];
}

export interface JsnVariantDownloadSummary {
  matched_files: number;
  bytes: number;
  groups: number;
  rows: number;
  columns: string[];
  security_related: boolean;
  parse_error_count: number;
}

export interface JsnVariantCoverageResource {
  resource: string;
  family: string;
  source_files: string[];
  source_occurrences: number;
  placeholders: string[];
  dynamic: boolean;
  selection_key: 'static-resource' | 'master-row placeholders';
  coverage: 'typed-command' | 'generic-only';
  typed_commands: string[];
  api_endpoints: string[];
  downloaded: JsnVariantDownloadSummary;
  gap_score: number | null;
}

export interface JsnVariantCoverageSummary {
  resource_template_count: number;
  xml_source_file_count: number;
  cfg_source_file_count: number;
  xml_backed_template_count: number;
  cfg_backed_template_count: number;
  static_template_count: number;
  dynamic_template_count: number;
  typed_template_count: number;
  generic_only_template_count: number;
  downloaded_file_count: number;
  matched_downloaded_file_count: number;
  downloaded_template_count: number;
  generic_downloaded_template_count: number;
  parse_error_template_count: number;
  fully_fixed: boolean;
}

export interface JsnVariantGapFamily {
  family: string;
  resource_templates: number;
  downloaded_templates: number;
  matched_files: number;
  rows: number;
  bytes: number;
  security_related_templates: number;
  samples: string[];
}

export interface JsnVariantCoverageDocument {
  schema: 'tdx-jsn-variant-coverage-native-v1';
  gaps_only: boolean;
  downloaded_root: string | null;
  semantics: string;
  summary: JsnVariantCoverageSummary;
  commands: Array<{ command: string; resource_templates: number }>;
  gap_families: JsnVariantGapFamily[];
  excluded: Array<{ resource: string; reason: string }>;
  high_value_gaps: JsnVariantCoverageResource[];
  resources: JsnVariantCoverageResource[];
}

export interface Block {
  block_id: string;
  family: string;
  family_name: string;
  block_code: string;
  name: string;
  source_key: string;
  parent_block_id: string;
  level: number;
  is_leaf: boolean;
  member_count: number;
}

export interface Member {
  block_id: string;
  block_code: string;
  block_name: string;
  family: string;
  security_id: string;
  market_id: number;
  market: string;
  code: string;
  security_name: string;
  membership: string;
}

export interface BlockResult {
  query: string;
  family: string;
  block_count: number;
  membership_count: number;
  returned_memberships: number;
  truncated: boolean;
  blocks: Block[];
  members: Member[];
}

export interface SecurityBlocksResult {
  security_id: string;
  market: string;
  code: string;
  name: string;
  block_count: number;
  blocks: Block[];
  memberships: Member[];
}

export interface SecuritySearchRecord {
  security_id: string;
  market: string;
  market_id: number;
  code: string;
  name: string;
  multiple?: number;
  decimal?: number;
  previous_close_price?: number;
  volume_ratio_base?: number;
  category?: string;
  category_reason?: string;
  board?: string;
  raw_tail_hex?: string;
}

export interface SecuritySearchDocument {
  schema?: string;
  generated_at?: string;
  market?: string;
  category?: string;
  query: string;
  match_count: number;
  returned: number;
  truncated: boolean;
  securities: SecuritySearchRecord[];
  command_count?: string;
  command_list?: string;
  summary?: { available: number; by_category: Record<string, number> };
  sources?: Array<{
    market: string;
    reported_count: number;
    received_count: number;
    endpoint: string;
    server_name: string;
    age_seconds: number;
  }>;
}

export interface Formula {
  code: string;
  name: string;
  kind_key: string;
  kind_name: string;
  category_name: string;
  source_text_available: boolean;
  source_rva: number | null;
  source_text: string | null;
  source?: string;
  source_text_origin?: string;
  analysis?: FormulaAnalysis;
  [key: string]: unknown;
}

export interface FormulaAnalysis {
  source_available: boolean;
  syntax_supported: boolean;
  executable: boolean;
  executable_with_context?: boolean;
  context_bindable?: boolean;
  explicit_context_bindable?: boolean;
  explicit_context_bindings_required?: string[];
  pure_ohlcv: boolean;
  has_future_function: boolean;
  read_only_future_executable?: boolean;
  has_external_dependency: boolean;
  has_adjustment_mode_dependency?: boolean;
  has_graphics: boolean;
  numeric_signal_safe?: boolean;
  presentation_semantics_faithful?: boolean;
  render_semantics_materialized?: boolean;
  render_ir_available?: boolean;
  pixel_renderer_equivalent?: false;
  has_semantic_surrogate?: boolean;
  has_presentation_return_surrogate?: boolean;
  semantic_surrogate_scope?: 'none' | 'presentation-return-only' |
    'numeric-output-degraded';
  semantic_fidelity?: 'numeric-safe' | 'numeric-degraded' |
    'numeric-safe-render-ir-materialized' |
    'numeric-safe-presentation-degraded';
  presentation_return_surrogates?: string[];
  unsupported_presentation_directives?: string[];
  unsupported_presentation_functions?: string[];
  functions: string[];
  unsupported: string[];
  future_functions: string[];
  external_dependencies: string[];
  market_dependencies: string[];
  directives: string[];
  outputs: string[];
  statement_count: number;
  reason?: string;
}

export interface FormulaCoverageKind {
  total: number;
  source_available: number;
  syntax_supported: number;
  executable: number;
  executable_with_context: number;
  explicit_context_bindable?: number;
  external_dependency?: number;
  future_function?: number;
  graphics?: number;
  numeric_signal_safe?: number;
  presentation_semantics_faithful?: number;
  render_ir_available?: number;
  render_semantics_materialized?: number;
  semantic_surrogate?: number;
  presentation_return_surrogate?: number;
  unsupported_presentation_directive?: number;
  degraded_numeric_output?: number;
}

export interface FormulaInterpreterCapabilities {
  schema: 'tdx-formula-interpreter-capabilities-v1';
  formula_engine: 'tdx-source-interpreter-v1';
  supported_function_count: number;
  supported_functions: string[];
  automatic_symbol_count: number;
  automatic_symbols: string[];
  explicit_context_symbols: string[];
  custom_formula_core_function_count: number;
  custom_formula_core_functions: string[];
  custom_formula_core_symbol_count: number;
  custom_formula_core_symbols: string[];
  custom_formula_sequence_statistics_function_count: number;
  custom_formula_sequence_statistics_functions: string[];
  custom_formula_rolling_variance_function_count: number;
  custom_formula_rolling_variance_functions: string[];
  custom_formula_benchmark_cumulative_function_count: number;
  custom_formula_benchmark_cumulative_functions: string[];
  custom_formula_calendar_filter_function_count: number;
  custom_formula_calendar_filter_functions: string[];
  custom_formula_calendar_filter_symbol_count: number;
  custom_formula_calendar_filter_symbols: string[];
  custom_formula_security_string_function_count: number;
  custom_formula_security_string_functions: string[];
  custom_formula_security_string_symbol_count: number;
  custom_formula_security_string_symbols: string[];
  custom_formula_adjustment_function_count?: number;
  custom_formula_adjustment_functions?: string[];
  custom_formula_adjustment_symbol_count?: number;
  custom_formula_adjustment_symbols?: string[];
  tcalc_registry_evidence: {
    profile: string;
    source_sha256: string;
    static_registry_entry_count: number;
    initializer: string;
    scope: string;
  };
}

export interface FormulaCoverage extends FormulaCoverageKind {
  analysis_schema_version?: 7;
  formula_engine?: 'tdx-source-interpreter-v1';
  capabilities?: FormulaInterpreterCapabilities;
  by_kind: Record<string, FormulaCoverageKind>;
}

export interface FormulaResult {
  kind: string;
  origin: 'all' | 'system' | 'user';
  query: string;
  match_count: number;
  returned: number;
  truncated: boolean;
  source_text_count: number;
  source_text_missing_count: number;
  library_schema?: string;
  user_library_enabled?: boolean;
  user_formula_count?: number;
  formulas: Formula[];
}

export interface FormulaCalculationPoint {
  date: string;
  time: string;
  open: number;
  high: number;
  low: number;
  close: number;
  amount: number;
  volume: number;
  values: Record<string, number | null>;
}

export interface FormulaFutureReplayStamp {
  date: string;
  time: string;
}

export interface FormulaFutureReplayObservation {
  observed_at: FormulaFutureReplayStamp;
  bar_count: number;
  changed_point_count: number;
  stored_event_count: number;
}

export interface FormulaFutureReplayEvent {
  observed_at: FormulaFutureReplayStamp;
  observed_bar_count: number;
  target: FormulaFutureReplayStamp;
  target_index: number;
  age_bars: number;
  output: string;
  previous_value: number | null;
  current_value: number | null;
}

export interface FormulaFutureReplayDocument {
  schema: 'tdx-formula-future-replay-v1';
  formula: string;
  future_functions: string[];
  source_bar_count: number;
  baseline_bar_count: number;
  observation_count: number;
  evaluation_count: number;
  changed_target_count: number;
  event_count: number;
  stored_event_count: number;
  events_truncated: boolean;
  max_observations: number;
  max_events: number;
  observations: FormulaFutureReplayObservation[];
  events: FormulaFutureReplayEvent[];
  mode: 'successive-prefix-read-only';
  newly_appended_points_counted_as_repaint: false;
  scan_allowed: false;
  backtest_allowed: false;
  account_accessed: false;
  orders_submitted: false;
  sdk_called: false;
  subscription_sent: false;
  network_requests: 0;
  entitlement_bypass: false;
}

export interface FormulaContextMetadata {
  finance_point_in_time_mode?: string;
  finance_point_in_time_archive_event_count?: number;
  finance_point_in_time_loaded_report_count?: number;
  finance_point_in_time_failed_report_count?: number;
  finance_point_in_time_first_available_from?: string | null;
  finance_point_in_time_last_available_from?: string | null;
  [key: string]: unknown;
}

export interface FormulaRenderStyle {
  visible: boolean;
  draw_above: boolean;
  draw_cframe?: boolean;
  dot_line: boolean;
  line_thickness: number;
  color_token: string;
  color_ref_available?: boolean;
  color_ref?: number | null;
  color_source?: 'none' | 'tcalc-named-colorref' | 'tcalc-literal-colorref' |
    'tcalc-rgbx-rgb';
  color_encoding?: 'Windows COLORREF: red | green<<8 | blue<<16';
  directives: string[];
  directive_order_preserved: boolean;
}

export interface FormulaRenderEvent {
  index: number;
  arguments: Array<number | null>;
  series_stick_value?: number;
  series_stick_color_role?: 'up' | 'down' | 'none';
  series_stick_open_close_color_role?: 'up' | 'down';
  series_stick_previous_close_color_role?: 'up' | 'down';
  string_arguments?: Record<string, string>;
  source_index?: number;
  sequence_offset?: number;
  sequence_source_start?: number;
  sequence_source_count?: number;
  sequence_style?: number;
  sequence_style_mode?: 'plain' | 'leader' | 'leader-box' |
    'offset-without-leader';
  sequence_value?: number;
  sequence_label?: string;
  sequence_label_class?: 'numeric' | 'alpha';
  sequence_leader?: boolean;
  sequence_boxed?: boolean;
  sequence_offset_pixels?: number;
  sequence_box_width_pixels?: number;
  sequence_box_height_pixels?: number;
  sequence_text_alignment?: 'right' | 'center';
  sequence_drawtext_flags?: 0x825 | 0x826;
  anchor?: 'bar-low' | 'bar-high';
  icon_price?: number | null;
  icon_type?: number | null;
  icon_type_available?: boolean;
  icon_sprite_cell_available?: boolean;
  icon_sprite_x?: number | null;
  icon_vertical_align?: 'above-price' | 'below-price';
  segment_color_available?: boolean;
  segment_direction?: 'current-to-next' | 'previous-to-current';
  segment_from_index?: number | null;
  segment_to_index?: number | null;
  segment_from_price?: number | null;
  segment_to_price?: number | null;
  segment_anchor_to_index?: number | null;
  segment_anchor_to_price?: number | null;
  segment_slope_per_bar?: number | null;
  segment_expansion?: 'none' | 'right';
  slope_per_bar?: number;
  slope_length?: number;
  slope_direction_value?: 0 | 1 | 2;
  slope_direction?: 'right' | 'left';
  slope_vertical?: boolean;
  slope_vertical_pixel_delta?: number;
  bitmap_price?: number | null;
  bitmap_name?: string;
  bitmap_format?: 'bmp';
  pane_background_mode?: 'gradient' | 'image';
  pane_background_color1_available?: boolean;
  pane_background_color2_available?: boolean;
  pane_background_color1_ref?: number | null;
  pane_background_color2_ref?: number | null;
  pane_background_horizontal?: boolean;
  pane_background_stretch?: boolean;
  pane_background_name_available?: boolean;
  pane_background_name?: string | null;
  pane_background_format?: 'auto' | null;
  rectangle_left?: number;
  rectangle_top?: number;
  rectangle_right?: number;
  rectangle_bottom?: number;
  rectangle_color_ref?: number;
  rectangle_fill?: boolean;
  rectangle_frame?: boolean;
  polyline_vertex_index?: number;
  polyline_vertex_price?: number | null;
  background_color1_available?: boolean;
  background_color2_available?: boolean;
  background_color1_ref?: number | null;
  background_color2_ref?: number | null;
  background_fill_mode?: number | null;
  background_fill_mode_name?: 'vertical-gradient' | 'horizontal-gradient' |
    'border' | 'border-and-fill' | 'alpha-solid' | 'vendor-defined';
  background_fill_compositing?: 'opaque-gdi-gradient-or-solid' | 'border-only' |
    'opaque-color2-fill-color1-border' | 'gdiplus-argb-solid-color1' |
    'unsupported';
  background_fill_alpha_byte?: number | null;
  background_fill_alpha_denominator?: 255;
  background_range?: number | null;
  background_range_name?: 'pane' | 'bar-high-low' | 'bar-open-close' |
    'vendor-defined';
  background_region_leader?: boolean;
  background_region_start_index?: number;
  background_region_end_index?: number;
  background_region_end_index_exclusive?: number;
  background_price_aggregation?: 'whole-pane' | 'region-high-low-extrema' |
    'region-first-open-last-close' | 'vendor-defined';
  background_price_top?: number | null;
  background_price_bottom?: number | null;
  band_side?: 'arg0-above' | 'arg2-above';
  fill_color_argument?: number;
  fill_color_available?: boolean;
  fill_color_ref?: number | null;
  stick_width_available?: boolean;
  stick_width?: number | null;
  stick_width_ratio?: number;
  stick_hairline?: boolean;
  stick_mode?: 'solid' | 'dashed-hollow' | 'solid-hollow' | 'center-full' |
    'center-half' | null;
  stick_hollow?: boolean;
  stick_border_dashed?: boolean;
  stick_price1_used?: boolean;
  stick_anchor?: 'price-pair' | 'pane-middle';
  stick_occupancy?: 'specified' | 'full' | 'half';
  annotation_text_available?: boolean;
  annotation_text?: string | null;
  annotation_lines?: string[];
  annotation_frame?: boolean;
  annotation_price?: number | null;
  annotation_x?: number | null;
  annotation_y?: number | null;
  annotation_horizontal_align?: 'left' | 'right';
  annotation_vertical_align?: 'above-price' | 'below-price' | 'price-origin' | 'top';
  annotation_drawabove?: boolean;
  annotation_x_offset_pixels?: number;
  annotation_y_offset_pixels?: number;
  annotation_line_count?: number;
  annotation_drawabove_row_adjustment_pixels?: -2;
}

export interface FormulaRenderPrimitive {
  statement: string;
  statement_index?: number;
  render_order?: number;
  render_order_semantics?: 'source-statement-order';
  function: string;
  kind: 'line' | 'stick' | 'icon' | 'candlestick' | 'text' | 'number' |
    'sequence-number' | 'band' | 'background' | 'part-line' | 'draw-line' |
    'polyline' | 'slope-line' | 'bitmap' | 'pane-background' | 'pane-rectangle';
  output_statement: boolean;
  style: FormulaRenderStyle;
  value_source?: string;
  finite_point_count?: number;
  line_stick?: boolean;
  line_stick_components?: ['zero-baseline-stick', 'indicator-line'];
  line_stick_baseline?: number;
  line_stick_draw_order?: 'sticks-then-line';
  series_native_mode?: 'line' | 'stick' | 'line-stick' | 'circle-dot' |
    'cross-dot' | 'point-dot' | 'dot-line';
  series_native_render_type?: 0 | 4 | 5 | 6 | 7 | 8 | 9;
  series_native_renderer?: 'tdxw-sub_957620' | 'tdxw-sub_957D70' |
    'tdxw-sub_957F70' | 'tdxw-sub_95ABA0' | 'tdxw-sub_95AE00' |
    'tdxw-sub_95B0A0';
  series_native_coordinate_space?: 'bar-price';
  series_native_pen_width_source?: 'LINETHICK-or-default-1';
  series_native_pen_style?: 'PS_SOLID' | 'PS_DOT';
  series_native_missing_value_rule?: 'break-contiguous-run';
  series_native_single_point_rule?: 'x-minus-3-to-x-horizontal';
  series_native_bold_width_config_key?: 'Other/BoldZBLine';
  series_native_pen_width_rule?: string;
  series_native_stem_baseline?: 0;
  series_native_stem_shape?: 'one-pixel-vertical-line';
  series_native_point_geometry_rule?:
    'spacing<6?four-cardinal-pixels:hollow-circle-radius-3' |
    'spacing<5?diagonal-radius-1:spacing<10?diagonal-radius-2:diagonal-radius-3' |
    'width<2?one-pixel:filled-ellipse-diameter-width';
  series_stick_mode?: 'color-stick' | 'volume-stick';
  series_stick_native_render_type?: 1 | 2;
  series_stick_native_renderer?: 'tdxw-sub_9555B0' | 'tdxw-sub_957030';
  series_stick_baseline?: 0;
  series_stick_coordinate_space?: 'bar-zero-baseline';
  series_stick_value_float_cast?: true;
  series_stick_epsilon?: number;
  series_stick_width_rule?: string;
  series_stick_shape?: 'one-pixel-vertical-line' | 'volume-body';
  series_stick_color_rule?: string;
  series_stick_up_fill_rule?: 'RealUPK?solid:hollow';
  series_stick_down_fill?: 'solid';
  series_stick_real_up_k_config_key?: 'Other/RealUPK';
  series_stick_vol_k_use_zt_config_key?: 'Other/VolKUseZT';
  argument_count?: number;
  condition_argument?: number | null;
  sequence_semantics?: 'increment-by-one-on-consecutive-bars';
  sequence_argument_order?: ['condition', 'style', 'start', 'count'];
  sequence_condition_true_rule?: 'abs(value-1)<0.0001';
  sequence_active_rule?: 'first-trigger-wins-through-source-plus-count-minus-one';
  sequence_overlap_rule?: 'ignore-trigger-while-active';
  sequence_style_evaluation?: 'per-rendered-bar-exact-float';
  sequence_anchor?: 'bar-low' | 'bar-high';
  sequence_limit?: number;
  sequence_styles?: Record<string, 'plain' | 'leader' | 'leader-box' |
    'offset-without-leader'>;
  sequence_anchor_gap_pixels?: 2;
  sequence_nonzero_offset_pixels?: 10;
  sequence_leader_length_pixels?: 10;
  sequence_leader_style?: 'dotted';
  sequence_leader_dot_step_pixels?: 4;
  sequence_leader_accelerated_step_pixels?: 5;
  sequence_box_width_numeric_pixels?: 8;
  sequence_box_width_alpha_pixels?: 14;
  sequence_box_height_pixels?: 14;
  sequence_box_fill_alpha_byte?: 0x50;
  sequence_box_fill_compositing?: 'gdiplus-argb-solid-fill';
  sequence_box_border_alpha_byte?: 0xff;
  sequence_box_border_width_pixels?: 1;
  sequence_box_border_path?: 'closed-gdi-polyline';
  sequence_text_numeric_alignment?: 'right';
  sequence_text_alpha_alignment?: 'center';
  sequence_text_vertical_alignment?: 'center';
  sequence_style_series_alignment?: 'document-points';
  sequence_style_series?: Array<number | null>;
  sequence_default_side?: 'below-low';
  sequence_default_flip_rule?:
    'box-bottom>=pane-bottom-40?above-high:below-low';
  sequence_drawabove_side?: 'above-high';
  sequence_drawabove_flip_rule?:
    'box-top<=pane-top+25?below-low:above-high';
  icon_coordinate_space?: 'bar-price';
  icon_price_argument?: number;
  icon_type_argument?: number;
  icon_official_type_min?: number;
  icon_official_type_max?: number;
  icon_sprite_cell_min?: number;
  icon_sprite_cell_max?: number;
  icon_sprite_cell_width?: number;
  icon_sprite_cell_height?: number;
  icon_sprite_indexing?: 'one-based-left-to-right';
  icon_sprite_endpoint?: string;
  icon_bitmap_endpoint?: string;
  icon_resource_type?: number;
  icon_resource_id?: number;
  icon_renderer?: 'tcalc-resource-bitmap';
  segment_color_argument?: number;
  segment_direction_argument?: number;
  segment_directions?: Record<string, 'current-to-next' | 'previous-to-current'>;
  segment_coordinate_space?: 'bar-price';
  segment_connection?: 'straight';
  polyline_condition_argument?: number;
  polyline_price_argument?: number;
  polyline_vertex_rule?: 'condition-true';
  polyline_connection_rule?: 'previous-vertex-to-current';
  line_start_condition_argument?: number;
  line_start_price_argument?: number;
  line_end_condition_argument?: number;
  line_end_price_argument?: number;
  line_expand_argument?: number;
  line_expansions?: Record<string, 'none' | 'right'>;
  slope_native_render_type?: 20;
  slope_native_renderer?: 'tdxw-sub_957A30';
  slope_coordinate_space?: 'bar-price-or-pixel-vertical';
  slope_condition_argument?: 0;
  slope_price_argument?: 1;
  slope_per_bar_argument?: 2;
  slope_length_argument?: 3;
  slope_direction_argument?: 4;
  slope_condition_true_rule?: 'finite-and-abs(value)>=1e-5';
  slope_direction_evaluation?: 'final-bar-float-to-int';
  slope_length_evaluation?: 'per-event-float-to-int-bound';
  slope_vertical_sentinel?: 10000;
  slope_vertical_length_unit?: 'pixels';
  slope_directions?: Record<string, 'right' | 'left' | 'both'>;
  bitmap_native_render_type?: 9;
  bitmap_native_renderer?: 'tdxw-sub_959F60';
  bitmap_coordinate_space?: 'bar-price';
  bitmap_price_argument?: 1;
  bitmap_name_argument?: 2;
  bitmap_condition_true_rule?: 'abs(value-1)<0.0001';
  bitmap_resource_directory?: 'T0002/signals';
  bitmap_resource_extension?: '.bmp';
  bitmap_resource_endpoint?: string;
  bitmap_position_rule?: 'left=bar-x;top=price-y;natural-size';
  pane_background_native_render_type?: 10;
  pane_background_native_renderer?: 'tdxw-sub_961A20';
  pane_background_condition_argument?: 0;
  pane_background_color_arguments?: [1, 2];
  pane_background_horizontal_argument?: 3;
  pane_background_name_argument?: 4;
  pane_background_stretch_argument?: 5;
  pane_background_condition_true_rule?: 'abs(value-1)<0.0001';
  pane_background_scalar_argument_scope?: 'evaluation-first-bar';
  pane_background_mode_rule?: 'color1!=0||color2!=0?gradient:image';
  pane_background_gradient_direction_rule?: 'horizontal==0?vertical:horizontal';
  pane_background_resource_directory?: 'T0002/signals';
  pane_background_resource_precedence?: 'bmp-then-png';
  pane_background_resource_endpoint?: string;
  pane_background_image_stretch_rule?:
    'stretch!=0?pane-size:natural-size-at-pane-origin';
  rectangle_native_render_type?: 11;
  rectangle_native_renderer?: 'tdxw-sub_95A970';
  rectangle_coordinate_space?: 'pane-thousandths';
  rectangle_argument_order?: ['left', 'top', 'right', 'bottom', 'color'];
  rectangle_coordinate_min?: 0;
  rectangle_coordinate_max?: 999;
  rectangle_fill_rule?: 'color!=0?solid:no-fill';
  rectangle_frame_rule?: 'NOFRAME?none:native-frame';
  rectangle_no_frame?: boolean;
  candle_argument_order?: ['high', 'open', 'low', 'close'];
  candle_color_rule?: 'close>=open?up:down';
  background_color_arguments?: [number, number];
  background_fill_mode_argument?: number;
  background_range_argument?: number;
  background_condition_scope?: 'per-bar-contiguous-regions';
  background_native_render_type?: 21;
  background_native_renderer?: 'tdxw-sub_95A6C0-sub_95A330';
  background_condition_true_rule?: 'abs(value-1)<0.0001';
  background_region_rule?: 'maximal-contiguous-native-true-bars';
  background_region_horizontal_bounds?:
    'first-bar-half-spacing-to-last-bar-half-spacing';
  background_fill_modes?: Record<string, 'vertical-gradient' | 'horizontal-gradient' |
    'border' | 'border-and-fill' | 'alpha-solid'>;
  background_alpha_mode_min?: 10;
  background_alpha_mode_max?: 20;
  background_alpha_rule?: '255*(mode-10)/10';
  background_alpha_byte_denominator?: 255;
  background_alpha_color_argument?: 1;
  background_alpha_compositing?: 'gdiplus-argb-solid-fill';
  background_ranges?: Record<string, 'pane' | 'bar-high-low' | 'bar-open-close'>;
  background_range_aggregations?: Record<string, 'whole-pane' |
    'region-high-low-extrema' | 'region-first-open-last-close'>;
  band_value_arguments?: [number, number];
  band_color_arguments?: [number, number];
  band_fill_rule?: 'arg0>arg2?arg1:arg3';
  band_fill_opacity?: 1;
  band_fill_compositing?: 'opaque-gdi-stroke-and-fill-path';
  stick_price_arguments?: [number, number];
  stick_width_argument?: number;
  stick_empty_argument?: number;
  stick_width_standard?: number;
  stick_width_unit?: 'bar-spacing-ratio';
  stick_center_modes_price1_ignored?: boolean;
  stick_modes?: Record<string, 'solid' | 'dashed-hollow' | 'solid-hollow' |
    'center-full' | 'center-half'>;
  annotation_coordinate_space?: 'bar-price' | 'pane-fraction';
  annotation_price_argument?: number;
  annotation_x_argument?: number;
  annotation_y_argument?: number;
  annotation_alignment_argument?: number;
  annotation_text_argument?: number;
  annotation_line_break?: '&';
  annotation_max_characters?: number;
  annotation_max_lines?: 10;
  annotation_alignments?: Record<string, 'left' | 'right'>;
  annotation_condition_true_rule?: 'abs(value-1)<0.0001';
  annotation_missing_price_rule?: 'skip-event';
  annotation_edge_behavior?: 'no-clamp-no-flip';
  annotation_collision_behavior?: 'none-source-order-overpaint';
  annotation_textout_y_adjustment_rule?:
    'font-aux-mode==0?0:font-aux-mode==1?-1:-3';
  annotation_unframed_horizontal_rule?: 'bar-x' | 'bar-x-minus-3';
  annotation_unframed_base_y_rule?: 'price-y-minus-8' | 'price-y';
  annotation_unframed_drawabove_rule?:
    'price-y-line-count*(native-chart-row-height-2)-8' |
    'price-y-(native-chart-row-height-2)';
  annotation_unframed_line_step_rule?:
    'measured-text-height;empty-line-measured-A-height';
  annotation_number_chart_precision?: number;
  annotation_number_chart_precision_source?:
    'native-constructor-default' | 'kline.price_precision';
  annotation_number_index_info_format_mode?: number;
  annotation_number_index_info_format_precision?: number;
  annotation_number_index_info_source?:
    'native-ordinary-security-default' |
    'kline.index_info_format_mode/precision';
  annotation_number_format_rule?:
    'sub_591950-integer-or-2/3-decimals' |
    'sub_59C390-fixed-0..5-decimals-plus-1e-6';
  annotation_native_render_type?: 4 | 6 | 7 | 8 | 23;
  annotation_native_renderer?: 'tdxw-sub_961290' | 'tdxw-sub_9593C0' |
    'tdxw-sub_958F90' | 'tdxw-sub_959620' | 'tdxw-sub_9626E0';
  annotation_font_selector?: 'tdxw-sub_68F020';
  annotation_font_table_index?: 1;
  annotation_font_user_ini_ordinal?: 2;
  annotation_font_config_section?: 'Other';
  annotation_font_face_key?: 'FONTNAME2';
  annotation_font_height_key?: 'FONTSIZE2';
  annotation_font_weight_key?: 'FontWeigth2';
  annotation_conditional_font_table_index?: 13;
  annotation_conditional_font_rule?: 'first-rendered-bar-arg1==2';
  annotation_conditional_font_scope?: 'renderer-wide-before-event-loop';
  annotation_effective_font_table_index?: 1 | 13;
  annotation_effective_font_basis?:
    'evaluation-window-first-bar-fallback;frontend-recomputes-visible-first-bar';
  annotation_background_mode?: 'transparent';
  annotation_background_mode_value?: 1;
  annotation_measurement_api?: 'GetTextExtentPoint32A';
  annotation_text_encoding?: 'Win32-ANSI';
  annotation_text_api?: 'TextOutA' | 'DrawTextA';
  annotation_drawtext_common_flags?: 0x824;
  annotation_drawtext_numeric_flags?: 0x826;
  annotation_drawtext_alpha_flags?: 0x825;
  annotation_frame_directive_present?: boolean;
  annotation_frame_supported?: boolean;
  annotation_frame_directive_effect?: 'native-price-text-frame' |
    'ignored-by-native-renderer' | 'not-forwarded-to-native-renderer';
  annotation_frame_native_renderer?: 'tdxw-sub_961290';
  annotation_frame_anchor?: 'bar-high-low';
  annotation_frame_price_argument_ignored?: true;
  annotation_frame_drawabove_ignored?: true;
  annotation_frame_side_rule?: 'available-below<=available-above?above:below';
  annotation_frame_horizontal_rule?: 'bar-x-minus-half-text-width';
  annotation_frame_leader_length_pixels?: 20;
  annotation_frame_leader_style?: 'dotted';
  annotation_frame_leader_dot_step_pixels?: 4;
  annotation_frame_leader_accelerated_step_pixels?: 5;
  annotation_frame_corner_radius_pixels?: 4;
  annotation_frame_width_padding_pixels?: 5;
  annotation_frame_height_padding_pixels?: 4;
  annotation_frame_text_inset_x_pixels?: 3;
  annotation_frame_text_inset_y_pixels?: 3;
  annotation_frame_fill_alpha_byte?: 0x50;
  annotation_frame_fill_compositing?: 'gdiplus-argb-solid-fill';
  annotation_frame_border_alpha_byte?: 0xff;
  annotation_frame_border_width_pixels?: 1;
  annotation_frame_smoothing_mode?: 4;
  annotation_frame_multiline_rule?: 'per-line-overlap-same-anchor';
  event_count?: number;
  events?: FormulaRenderEvent[];
}

export interface FormulaTradeEventCandidate {
  index: number;
  condition: number;
  price: number;
  projection: string;
  native_host_action: false;
  execution_side_effects?: false;
  autofilter_position_before?: 'flat' | 'long' | 'short';
  autofilter_position_after?: 'flat' | 'long' | 'short';
  autofilter_effective_action?: string;
  autofilter_atomic_action?: boolean;
}

export interface FormulaTradeEventAction {
  index: number;
  condition: number;
  price: number;
  wrapper_action: number;
  host_action_bits: number;
  projection?: string;
  execution_side_effects: false;
  order_submission: false;
  autofilter_position_before?: 'flat' | 'long' | 'short';
  autofilter_position_after?: 'flat' | 'long' | 'short';
  autofilter_effective_action?: string;
  autofilter_atomic_action?: boolean;
}

export interface FormulaAutofilterDecision {
  decision_ordinal: number;
  index: number;
  statement_index: number;
  function: FormulaTradeEventPrimitive['function'];
  condition: number;
  price: number;
  projection: 'offline-autofilter-decision';
  accepted: boolean;
  position_before: 'flat' | 'long' | 'short';
  position_after: 'flat' | 'long' | 'short';
  position_changed: boolean;
  effective_action: string;
  rejection_reason: 'already-long' | 'already-short' | 'requires-flat' |
    'requires-long' | 'requires-short' | null;
  composite_action: boolean;
  native_host_action: false;
  execution_side_effects: false;
}

export interface FormulaTradeEventPrimitive {
  statement: string;
  statement_index: number;
  function: 'BUY' | 'SELL' | 'SELLSHORT' | 'BUYSHORT' |
    'BUYSHORT_BUY' | 'SELL_SELLSHORT';
  wrapper_action: number;
  host_action_bits: number;
  historical_signal_candidates: FormulaTradeEventCandidate[];
  latest_host_action: FormulaTradeEventAction | null;
  autofilter_applied?: true;
  filtered_historical_signal_candidates?: FormulaTradeEventCandidate[];
  filtered_latest_host_action?: FormulaTradeEventAction | null;
  autofilter_filtered_out_candidate_count?: number;
}

export interface FormulaTradeEventIr {
  schema_version: 1;
  schema: 'tdx-formula-trade-event-ir-v1';
  scope: 'top-level-trading-signal-statements-only';
  primitive_count: number;
  execution_side_effects: false;
  order_submission: false;
  account_access: false;
  network_access: false;
  autofilter: {
    schema: 'tdx-formula-autofilter-projection-v1';
    enabled: boolean;
    accepted_candidate_count: number;
    filtered_out_candidate_count: number;
    decision_count?: number;
    position_change_count?: number;
    decision_trace?: FormulaAutofilterDecision[];
    rejection_reason_counts?: Partial<Record<Exclude<
      FormulaAutofilterDecision['rejection_reason'], null>, number>>;
    final_position: 'flat' | 'long' | 'short';
    complete_native_action_set?: false;
    execution_side_effects: false;
    order_submission: false;
  };
  primitives: FormulaTradeEventPrimitive[];
}

export interface FormulaCalculationDocument {
  schema_version: number;
  engine: string;
  execution_mode: 'native-cpp';
  dll_loaded?: false;
  formula: string;
  parameters: Record<string, number>;
  outputs: string[];
  count: number;
  points: FormulaCalculationPoint[];
  market: string;
  code: string;
  period: string;
  start: number;
  next_start: number;
  has_more: boolean;
  chronological: boolean;
  adjustment_mode?: 'none' | 'qfq' | 'hfq' | 'fixed_qfq' | 'fixed_hfq';
  adjustment?: MinuteDocument['adjustment'];
  formula_volume_unit: 'hand' | 'index-native' | 'contract';
  formula_volume_divisor: 1 | 100;
  context_bindings?: string[];
  context_metadata?: FormulaContextMetadata;
  analysis?: FormulaAnalysis;
  formula_source_mode?: 'inline-post' | 'library-post';
  library_formula_kind?: 'technical' | 'selection' | 'expert' | 'color-k';
  library_formula_code?: string;
  source_bytes?: number;
  request_body_retained?: false;
  future_execution_mode?: 'explicit-read-only-lookahead';
  future_functions?: string[];
  future_replay?: FormulaFutureReplayDocument;
  scan_allowed?: false;
  backtest_allowed?: false;
  render_environment?: {
    schema: 'tdx-formula-render-environment-v1';
    native_source: 'TdxW.exe';
    annotation: {
      background_mode: 'transparent';
      background_mode_value: 1;
      text_encoding: 'Win32-ANSI';
      font: {
        native_font_table_index: 1;
        user_ini_font_ordinal: 2;
        selected_profile: 'tdx-user-ini-font-ordinal-2' |
          'recovered-legacy-font-table' | 'recovered-new-font-style-table';
        source_available: boolean;
        source_path: string | null;
        config_keys: {
          section: 'Other'; style: 'NewFontStyle'; elder_style: 'ElderStyle';
          face: 'FONTNAME2'; height: 'FONTSIZE2'; weight: 'FontWeigth2';
        };
        new_font_style: number;
        elder_style: boolean;
        face: string;
        configured_height: number;
        logical_height: number;
        logical_height_semantics: 'gdi-cell-height' | 'gdi-character-height';
        weight: number;
        italic: false;
        underline: false;
        strikeout: false;
        charset: number;
        charset_name: 'ANSI_CHARSET' | 'DEFAULT_CHARSET';
        quality: number | null;
        quality_name: 'ANTIALIASED_QUALITY' | 'runtime-dependent';
        measurement_height_adjustment: number;
        native_textout_aux_mode: number;
        native_textout_y_adjustment_pixels: number;
        native_textout_y_adjustment_rule:
          'aux-mode==0?0:aux-mode==1?-1:-3';
        css_pixel_size: number;
        css_mapping: 'browser-approximation-from-gdi-logical-height';
      };
      conditional_fonts?: {
        '13'?: {
          native_font_table_index: 13;
          user_ini_font_ordinal: null;
          selected_profile: 'recovered-drawnumber-dif-style2-table';
          source_available: true;
          source_path: null;
          native_source: 'TdxW.exe!sub_690350';
          style_condition: 'DRAWNUMBER_DIF first-rendered-bar arg1==2';
          face: 'Arial';
          configured_height: 15;
          logical_height: 15;
          logical_height_semantics: 'gdi-cell-height';
          weight: 400;
          italic: false;
          underline: false;
          strikeout: false;
          charset: 1;
          charset_name: 'DEFAULT_CHARSET';
          quality: 4;
          quality_name: 'ANTIALIASED_QUALITY';
          measurement_height_adjustment: 0;
          native_textout_aux_mode: 0;
          native_textout_y_adjustment_pixels: 0;
          native_textout_y_adjustment_rule:
            'aux-mode==0?0:aux-mode==1?-1:-3';
          css_pixel_size: 15;
          css_mapping: 'browser-approximation-from-gdi-logical-height';
        };
      };
      native_chart_row_height_pixels: number;
      native_chart_row_height_rule: string;
      native_chart_row_height_source: 'TdxW.exe!sub_92A830+0x1EA';
    };
    series_sticks: {
      source_available: boolean;
      source_path: string | null;
      config_keys: {
        section: 'Other';
        real_up_k: 'RealUPK';
        vol_k_use_zt: 'VolKUseZT';
      };
      real_up_k: boolean;
      vol_k_use_zt: boolean;
      volume_color_rule: 'previous-close' |
        'open-close-with-flat-previous-close-fallback';
      volume_up_fill: 'solid' | 'hollow';
      volume_down_fill: 'solid';
      native_up_pen_index: 2;
      native_down_pen_index: 3;
      bar_body_width_rule: string;
      native_config_source: 'TdxW.exe!sub_987080/sub_956F40/sub_957030';
    };
    series_lines: {
      source_available: boolean;
      source_path: string | null;
      config_keys: {
        section: 'Other';
        bold_zb_line: 'BoldZBLine';
      };
      bold_zb_line: boolean;
      effective_default_width_rule: string;
      native_source: 'TdxW.exe!sub_957620';
    };
    pixel_font_equivalent: false;
    pixel_boundary: string;
  };
  render_ir?: {
    schema_version: number;
    schema: 'tdx-formula-render-ir-v1';
    primitive_count: number;
    event_count: number;
    directive_order_preserved: boolean;
    primitive_order?: 'source-statement-order';
    primitive_order_contiguous?: boolean;
    source_statement_count?: number;
    string_expressions_materialized: boolean;
    pixel_renderer_equivalent: boolean;
    primitives: FormulaRenderPrimitive[];
  };
  trade_event_ir?: FormulaTradeEventIr;
}

export interface FormulaAuditRow {
  code: string;
  name: string;
  kind_key: string;
  kind_name?: string;
  status: 'passed' | 'error' | 'market_inapplicable' | 'period_inapplicable' |
    'dependency_unavailable' | 'context_unavailable' |
    'explicit_context_unavailable' | 'future_read_only_disabled';
  error?: string;
  numeric_outputs?: string[];
  latest_numeric_outputs?: string[];
  required_market_context?: string[];
  external_dependencies?: string[];
  context_bindings_required?: string[];
  context_bindings_unavailable?: string[];
  explicit_context_bindings_required?: string[];
  unsupported?: string[];
  explicit_context?: boolean;
  future_read_only?: boolean;
  future_read_only_capable?: boolean;
  unsupported_expansion_dependencies?: string[];
}

export interface FormulaAuditDocument {
  schema_version: 2;
  engine: 'tdx-source-interpreter-v1';
  audit_mode: string;
  market_scope: 'a-share' | 'tdx-expansion';
  market: string;
  code: string;
  period: string;
  bar_count: number;
  audit: {
    library_total: number;
    eligible: number;
    passed: number;
    errors: number;
    market_inapplicable: number;
    period_inapplicable: number;
    dependency_unavailable: number;
    context_unavailable: number;
    future_read_only_disabled: number;
    with_numeric_output: number;
    with_latest_numeric_output: number;
    reported: number;
    unreported: number;
  };
  formulas: FormulaAuditRow[];
}

export interface CloudCalcAuditDocument {
  schema: 'tdx-tbigdata-cloud-calc-audit-v1';
  execution_mode: 'native-cpp-offline';
  dll_loaded: false;
  summary: {
    cfg_files: number;
    cfg_with_calc: number;
    calc_columns: number;
    expressions: number;
    builtin_formulas: number;
    valid: number;
    invalid: number;
    current_config_executable: number;
    current_config_unimplemented: number;
    registered_builtin_count: number;
    registered_builtin_implemented: number;
    parse_errors: number;
    dependency_cycles: number;
    effective_host_columns: number;
    auto_resolvable_host_columns: number;
    unresolved_host_columns: number;
  };
  files: Array<Record<string, unknown>>;
  host_system_columns: Array<Record<string, unknown>>;
  builtin_catalog: Array<Record<string, unknown>>;
}

export interface CloudCalcTemplateField {
  code: string;
  name?: string;
  datatype: string;
  units: string[];
  required_by: string[];
  reasons: string[];
}

export interface CloudCalcHostTemplateField extends CloudCalcTemplateField {
  system_column: string;
  resolver: string;
  security_suffix: string;
  identity_fields: string[];
}

export interface CloudCalcTemplateDocument {
  schema: 'tdx-tbigdata-cloud-calc-template-v1';
  execution_mode: 'native-cpp-offline';
  dll_loaded: false;
  cfg_name: string;
  row_template: Record<string, null>;
  input_fields: CloudCalcTemplateField[];
  host_fields: CloudCalcHostTemplateField[];
  derived_fields: CloudCalcTemplateField[];
  calculated_fields: string[];
  source_resources: string[];
  counts: {
    input_fields: number;
    host_fields: number;
    derived_fields: number;
    calculated_fields: number;
    units: number;
  };
}

export interface CloudCalcResultRow {
  code: string;
  name?: string;
  calc: string;
  status: 'evaluated' | 'unavailable' | 'error';
  value?: number | string | null;
  missing?: string[];
  error?: string;
  reason?: string;
}

export interface CloudCalcEvaluationDocument {
  schema: 'tdx-tbigdata-cloud-calc-evaluation-v1';
  execution_mode: 'native-cpp-offline' | 'native-cpp-public-l1-finance';
  dll_loaded: false;
  cfg?: string;
  cfg_name: string;
  as_of: number;
  row_source: 'inline-request';
  override_count: number;
  counts: {
    calculated: number;
    evaluated: number;
    unavailable: number;
    errors: number;
  };
  units: Array<{
    id: string;
    resource?: string;
    counts: Record<string, number>;
    results: CloudCalcResultRow[];
  }>;
  host_context: {
    snapshot_source: string;
    finance_source: string;
    industry_source: string;
    bindings: Array<{
      code: string;
      system_column?: string;
      security?: string;
      source: string;
      value: unknown;
    }>;
    unresolved: Array<{
      code: string;
      system_column?: string;
      reason?: string;
    }>;
  };
}

export interface CloudCalcBatchEntry {
  row_index: number;
  status: 'ok' | 'error';
  result?: CloudCalcEvaluationDocument;
  error?: string;
}

export interface CloudCalcBatchDocument {
  schema: 'tdx-tbigdata-cloud-calc-batch-v1';
  execution_mode: 'native-cpp-offline' | 'native-cpp-public-l1-finance';
  dll_loaded: false;
  cfg_name: string;
  as_of: number;
  row_source: 'inline-request-batch';
  request_body_retained: false;
  input_field_count: number;
  counts: {
    rows: number;
    succeeded: number;
    failed: number;
    calculated: number;
    evaluated: number;
    unavailable: number;
    errors: number;
  };
  fetch_plan: {
    quote_mode: string;
    unique_quote_securities: number;
    unique_finance_securities: number;
    unique_industry_securities: number;
    unique_seal_securities: number;
    quote_document_fetches: number;
    finance_document_fetches: number;
    special_limit_document_fetches: number;
  };
  rows: CloudCalcBatchEntry[];
}

export interface FormulaBacktestDocument {
  schema_version: number;
  engine: string;
  formula: string;
  kind: string;
  signal_timing: string;
  initial_capital: number;
  final_equity: number;
  total_return_pct: number;
  max_drawdown_pct: number;
  commission_bps: number;
  slippage_bps: number;
  trade_count: number;
  wins: number;
  losses: number;
  win_rate_pct: number;
  profit_factor: number | null;
  trades: Array<Record<string, string | number>>;
  equity_curve: Array<{ date: string; time: string; equity: number }>;
  market: string;
  code: string;
  name?: string;
  period: string;
  count: number;
  point_in_time_finance?: boolean;
  adjustment_mode?: 'none' | 'qfq' | 'hfq' | 'fixed_qfq' | 'fixed_hfq';
  adjustment?: Record<string, unknown>;
  context_bindings?: string[];
  context_metadata?: FormulaContextMetadata;
  formula_source_mode?: 'inline-post' | 'source-file';
  source_bytes?: number;
  request_body_retained?: false;
}

export interface FormulaScanSignal {
  output: string;
  value: number;
  date: string;
  time: string;
}

export interface FormulaScanMatch {
  security_id: string;
  market?: string;
  code?: string;
  name?: string;
  period?: string;
  adjustment_mode?: 'none' | 'qfq' | 'hfq' | 'fixed_qfq' | 'fixed_hfq';
  trigger_date: string;
  trigger_time: string;
  context_metadata?: FormulaContextMetadata;
  signals: FormulaScanSignal[];
}

export interface FormulaScanDocument {
  schema_version: number;
  engine: string;
  formula: string;
  kind: string;
  lookback: number;
  requested_count: number;
  input_count: number;
  evaluated: number;
  match_count: number;
  error_count: number;
  fetch_error_count: number;
  fetch_workers: number;
  point_in_time_finance?: boolean;
  adjustment_mode?: 'none' | 'qfq' | 'hfq' | 'fixed_qfq' | 'fixed_hfq';
  adjustment_summary?: Record<string, unknown>;
  matches: FormulaScanMatch[];
  errors: Array<Record<string, unknown>>;
  fetch_errors: Array<Record<string, unknown>>;
  formula_source_mode?: 'inline-post' | 'source-file';
  source_bytes?: number;
  request_body_retained?: false;
}

export interface FormulaStrategyRuleSummary {
  id: string;
  label: string;
  formula_code: string;
  source_mode: 'inline' | 'library';
  source_md5: string;
  parameters: Record<string, number>;
  analysis: FormulaAnalysis;
}

export interface FormulaStrategySummary {
  code: string;
  name: string;
  operator: 'all' | 'any' | 'at-least';
  minimum_matches: number;
  rule_count: number;
  signal_alignment: string;
  rules: FormulaStrategyRuleSummary[];
}

export interface FormulaStrategyTrigger {
  date: string;
  time: string;
  matched_rule_count: number;
  matched_rule_ids: string[];
  rules: Array<{
    id: string;
    matched: boolean;
    signals: Array<{ output: string; value: number }>;
  }>;
}

export interface FormulaStrategyScanMatch {
  security_id: string;
  market: string;
  code: string;
  name?: string;
  period: string;
  adjustment_mode?: 'none' | 'qfq' | 'hfq' | 'fixed_qfq' | 'fixed_hfq';
  trigger_date: string;
  trigger_time: string;
  triggers: FormulaStrategyTrigger[];
}

export interface FormulaStrategyScanDocument {
  schema: 'tdx-formula-strategy-scan-v1';
  engine: 'tdx-formula-strategy-v1';
  strategy: FormulaStrategySummary;
  lookback: number;
  requested_count: number;
  input_count: number;
  evaluated: number;
  match_count: number;
  error_count: number;
  fetch_error_count: number;
  fetch_workers: number;
  matches: FormulaStrategyScanMatch[];
  errors: Array<Record<string, unknown>>;
  fetch_errors: Array<Record<string, unknown>>;
  strategy_source_mode: 'manifest-post';
  request_body_retained: false;
  point_in_time_finance: boolean;
  adjustment_mode?: 'none' | 'qfq' | 'hfq' | 'fixed_qfq' | 'fixed_hfq';
  adjustment_summary?: Record<string, unknown>;
}

export interface FormulaStrategyAttribution {
  security_id: string;
  market: string;
  code: string;
  name: string;
  gross_contribution: number;
  allocated_cost: number;
  net_contribution: number;
  net_contribution_pct: number;
  turnover: number;
  active_intervals: number;
  entries: number;
  exits: number;
}

export interface FormulaStrategyBacktestDocument {
  schema: 'tdx-formula-strategy-portfolio-backtest-v1';
  engine: 'tdx-formula-strategy-portfolio-v1';
  execution_mode: 'native-cpp';
  strategy: FormulaStrategySummary;
  signal_timing: string;
  portfolio_method: string;
  alignment: string;
  period: string;
  security_count: number;
  aligned_bar_count: number;
  initial_capital: number;
  final_equity: number;
  total_return_pct: number;
  max_drawdown_pct: number;
  commission_bps: number;
  slippage_bps: number;
  total_cost: number;
  total_turnover: number;
  rebalance_count: number;
  average_holding_count: number;
  equity_curve: Array<{ date: string; time: string; equity: number; holding_count: number }>;
  rebalances: Array<Record<string, unknown>>;
  attribution: FormulaStrategyAttribution[];
  requested_count: number;
  fetch_error_count: number;
  fetch_workers: number;
  fetch_errors: Array<Record<string, unknown>>;
  strategy_source_mode: 'manifest-post';
  request_body_retained: false;
  point_in_time_finance: boolean;
  adjustment_mode?: 'none' | 'qfq' | 'hfq' | 'fixed_qfq' | 'fixed_hfq';
  adjustment_summary?: Record<string, unknown>;
}

export interface FuturesContract {
  name: string;
  contract_code: string;
  market_id: string;
  contract_key: string;
  date: string;
  return_5d_pct?: number | null;
  return_10d_pct?: number | null;
  price_change?: number | null;
  open_interest?: number | null;
  daily_position_change?: number | null;
  volume?: number | null;
  spot_price?: number | null;
  basis_state?: string;
  net_position?: number | null;
}

export interface FuturesRelatedStock {
  security: ValuationSecurity;
  return_5d_pct: number | null;
  return_10d_pct: number | null;
  date: string;
}

export interface FuturesPositionPoint {
  date: string;
  net_position: number | null;
}

export interface FuturesMonthlyRecord {
  year_month: string;
  product: string;
  monthly_volume: number | null;
  monthly_volume_yoy_pct: number | null;
  monthly_turnover_yuan: number | null;
  monthly_turnover_yoy_pct: number | null;
  year_to_date_volume: number | null;
  year_to_date_turnover_yuan: number | null;
}

export interface IpoIndustryRecord {
  industry_code: string;
  industry_name: string;
  year: string;
  listed_count: number | null;
  raised_10k_yuan: number | null;
  average_listing_gain_pct: number | null;
  largest_fundraiser: string;
  largest_raised_10k_yuan: number | null;
  industry_key: string;
}

export interface IpoSecurityRecord {
  security: ValuationSecurity;
  listing_date: string;
  raised_10k_yuan: number | null;
  issue_price: number | null;
}

export interface IpoMonthlyRecord {
  month: string;
  raised_100m_yuan: number | null;
  listed_count: number | null;
}

export interface BondIssuerRecord {
  issuer_name: string;
  issuer_id: string;
  detail_key?: string;
  security?: ValuationSecurity;
}

export interface PrivatePlacementDates {
  board_approved: string;
  shareholders_approved: string;
  regulator_approved: string;
  registered: string;
  implemented: string;
  listed: string;
  unlock: string;
}

export interface PrivatePlacementRecord {
  event_id: string;
  security: ValuationSecurity;
  source_resource: string;
  lifecycle: 'implemented-locked' | 'implemented-unlocked' | 'plan-active' | 'plan-stopped' | 'implemented' | 'registered';
  lifecycle_label: string;
  stage: string;
  industry: string;
  dates: PrivatePlacementDates;
  sort_date: string;
  issue_price: number | null;
  approved_close: number | null;
  implemented_close: number | null;
  listing_close: number | null;
  unlock_close: number | null;
  actual_gross_10k_yuan: number | null;
  actual_net_10k_yuan: number | null;
  expected_raise_10k_yuan: number | null;
  issue_shares_10k: number | null;
  post_issue_shares_10k: number | null;
  approval_to_implementation_return_pct: number | null;
  lock_period_return_pct: number | null;
  issue_to_post_shares_pct: number | null;
  issue_details: string;
  change_notes: string;
  registration_announcement: string;
  raw: Record<string, unknown>;
}

export interface PrivatePlacementSummary {
  records: number;
  unique_securities: number;
  actual_gross_10k_yuan: number;
  expected_raise_10k_yuan: number;
  lifecycle_counts: Array<{ label: string; count: number }>;
  stage_counts: Array<{ label: string; count: number }>;
}

export interface RightsOfferingRecord {
  kind: 'rights-offering';
  event_id: string;
  security: ValuationSecurity;
  source_resource: string;
  phase: 'implemented' | 'deliberating' | 'abnormal';
  phase_label: string;
  stage: string;
  announcement_date: string;
  progress_date: string;
  sort_date: string;
  rights_code: string;
  rights_name: string;
  rights_per_10_shares: number | null;
  rights_price_yuan: number | null;
  offered_shares: number | null;
  raised_yuan: number | null;
  amount_semantics: 'actual' | 'planned';
  payment_start_date: string;
  payment_end_date: string;
  registration_date: string;
  ex_rights_date: string;
  major_shareholder_subscription: string;
  issue_details: string;
  raw: Record<string, unknown>;
}

export interface RightsOfferingSummary {
  records: number;
  unique_securities: number;
  implemented_raised_yuan: number;
  planned_raised_yuan: number;
  implemented_shares: number;
  planned_shares: number;
  phase_counts: Record<string, number>;
  status_counts: Record<string, number>;
}

export interface PreferredShareRecord {
  kind: 'preferred-share';
  record_id: string;
  underlying_security: ValuationSecurity;
  preferred_code: string;
  preferred_name: string;
  listing_date: string;
  par_value_yuan: number | null;
  issue_price_yuan: number | null;
  issue_shares_10k: number | null;
  issue_shares: number | null;
  issue_size_100m_yuan: number | null;
  issue_size_yuan: number | null;
  issue_method: string;
  initial_dividend_yield_pct: number | null;
  cumulative_dividend: string;
  adjustable_dividend: string;
  annual_payment_count: number | null;
  payment_method: string;
  source_resource: string;
  raw: Record<string, unknown>;
}

export interface PreferredShareSummary {
  records: number;
  unique_underlying_securities: number;
  unique_preferred_codes: number;
  total_issue_shares: number;
  total_issue_size_yuan: number;
  cumulative_dividend_count: number;
  adjustable_dividend_count: number;
}

export interface FuturesIssuanceDocument {
  schema: 'tdx-futures-issuance-native-v1';
  generated_at: string;
  section: 'all' | 'futures' | 'ipo' | 'placements' | 'rights' | 'preferred-shares';
  commodity_futures: FuturesContract[];
  monthly_futures: FuturesMonthlyRecord[];
  index_futures: FuturesContract[];
  selected_contract: FuturesContract | null;
  related_stocks: FuturesRelatedStock[];
  position_history: FuturesPositionPoint[];
  ipo_annual: Array<Record<string, unknown>>;
  ipo_industries: IpoIndustryRecord[];
  ipo_monthly: IpoMonthlyRecord[];
  ipo_securities: IpoSecurityRecord[];
  listed_bond_issuers: BondIssuerRecord[];
  unlisted_bond_issuers: BondIssuerRecord[];
  private_placements: PrivatePlacementRecord[];
  placement_summary: PrivatePlacementSummary;
  rights_offerings: RightsOfferingRecord[];
  rights_summary: RightsOfferingSummary;
  preferred_shares: PreferredShareRecord[];
  preferred_share_summary: PreferredShareSummary;
  counts: Record<string, number>;
  sources: Array<Record<string, unknown>>;
}

export interface ValuationSecurity {
  market: string;
  market_id: number;
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
}

export interface ValuationIndex {
  date: string;
  detail_id: string;
  security: ValuationSecurity;
  metrics: {
    pe: string | number | null;
    pe_percentile: string | number | null;
    pb: string | number | null;
    pb_percentile: string | number | null;
    dividend_yield: string | number | null;
    roe: string | number | null;
    earnings_yield: string | number | null;
    valuation_label: string | null;
  };
  returns: {
    days_5: string | number | null;
    days_10: string | number | null;
    days_20: string | number | null;
    days_30: string | number | null;
  };
  history_start_date: string | null;
}

export interface ValuationFund extends ValuationSecurity {
  fund_type: string | null;
  net_asset_value: string | number | null;
  premium_pct: string | number | null;
  latest_shares: string | number | null;
  minimum_redemption_unit: string | number | null;
}

export interface ValuationHistoryPoint {
  date: string;
  pe: string | number | null;
  pe_percentile: string | number | null;
  pb: string | number | null;
  pb_percentile: string | number | null;
}

export interface MarketValuationDocument {
  schema: string;
  generated_at: string;
  mode: 'master' | 'index';
  indices: ValuationIndex[];
  selected: ValuationIndex | null;
  related_funds: ValuationFund[];
  history: ValuationHistoryPoint[];
  summary: {
    first_date?: string | null;
    last_date?: string | null;
    pe?: { minimum: number | null; maximum: number | null; latest: number | null };
    pb?: { minimum: number | null; maximum: number | null; latest: number | null };
  };
  counts: {
    indices: number;
    related_funds: number;
    history_points: number;
    full_history_points?: number;
  };
  cache: {
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_refreshed?: boolean;
    detail_age_seconds?: number;
  };
}

export interface ConsensusForecast {
  year: number | null;
  eps: string | number | null;
  pe: string | number | null;
  net_profit_yi: string | number | null;
  revenue_yi: string | number | null;
}

export interface ConsensusRecord {
  category: string;
  security: ValuationSecurity;
  latest_date: string | null;
  industry: string | null;
  institution_count: string | number | null;
  rating_score: string | number | null;
  pe: string | number | null;
  expected_eps_growth_pct: string | number | null;
  peg: string | number | null;
  target_price: string | number | null;
  latest_close: string | number | null;
  year_high_price: string | number | null;
  change_from_year_high_pct: string | number | null;
  consecutive_rise_days: string | number | null;
  year_low_price: string | number | null;
  change_from_year_low_pct: string | number | null;
  base_year: string | number | null;
  forecasts: ConsensusForecast[];
  growth: {
    revenue_cagr_pct: string | number | null;
    profit_cagr_pct: string | number | null;
    revenue_yearly_pct: Array<string | number | null>;
    profit_yearly_pct: Array<string | number | null>;
  };
}

export interface ConsensusCategory {
  id: string;
  label: string;
  resource: string;
  record_count: number;
}

export interface ConsensusReport {
  report_date: string;
  institution: string | null;
  institution_grade: string | null;
  analyst: string | null;
  rating: string | null;
  rating_change: string | null;
  target_price: string | number | null;
  base_year: string | number | null;
  report_text_length: number;
  report_text?: string;
  forecasts: ConsensusForecast[];
}

export interface MarketConsensusDocument {
  schema: string;
  generated_at: string;
  mode: 'master' | 'security';
  category: string;
  category_label: string;
  query: string;
  categories: ConsensusCategory[];
  records: ConsensusRecord[];
  selected_security: ValuationSecurity | null;
  selected_consensus: ConsensusRecord | null;
  category_memberships: Array<{
    category: string;
    category_label: string;
    record: ConsensusRecord;
  }>;
  reports: ConsensusReport[];
  report_summary: {
    report_count?: number;
    institution_count?: number;
    analyst_count?: number;
    latest_report_date?: string | null;
    rating_counts?: Record<string, number>;
    target_price?: {
      count: number;
      minimum: number | null;
      maximum: number | null;
      average: number | null;
    };
  };
  counts: {
    categories: number;
    total_rows: number;
    unique_securities: number;
    category_rows: number;
    returned_records: number;
    reports: number;
    full_reports: number;
  };
  cache: {
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_refreshed?: boolean;
    detail_age_seconds?: number;
  };
}

export interface ResearchEntity {
  type: 'security' | 'industry' | 'institution';
  id: string;
  code: string;
  name: string;
  name_resolved: boolean;
  market?: string;
  market_id?: number;
  security_id?: string;
}

export interface ResearchRecord {
  category: string;
  entity: ResearchEntity;
  detail_id: string;
  latest_date: string | null;
  data: Record<string, string | number | null>;
}

export interface ResearchCategory {
  id: string;
  label: string;
  entity_type: 'security' | 'industry' | 'institution';
  resource: string;
  record_count: number;
}

export interface ResearchActivity {
  date: string;
  title: string;
  text_length: number;
  text?: string;
  source_urls: string[];
}

export interface ResearchRegulatoryEvent {
  date: string;
  subject: string | null;
  event_type: string | null;
  progress: string | null;
  summary_length: number;
  summary?: string;
  source_urls: string[];
}

export interface ResearchSecurityRow {
  security: ResearchEntity;
  latest_date: string | null;
  data: Record<string, string | number | null>;
}

export interface MarketResearchDocument {
  schema: string;
  generated_at: string;
  mode: 'master' | 'security' | 'detail';
  category: string;
  category_label: string;
  entity_type: 'security' | 'industry' | 'institution';
  query: string;
  categories: ResearchCategory[];
  records: ResearchRecord[];
  selected: ResearchRecord | null;
  selected_entity: ResearchEntity | null;
  category_memberships: Array<{
    category: string;
    category_label: string;
    record: ResearchRecord;
  }>;
  activities: ResearchActivity[];
  regulatory_events: ResearchRegulatoryEvent[];
  related_securities: ResearchSecurityRow[];
  detail_errors: Array<{ resource: string | null; message: string }>;
  counts: {
    categories: number;
    total_rows: number;
    unique_entities: number;
    category_rows: number;
    returned_records: number;
    memberships: number;
    activities: number;
    regulatory_events: number;
    related_securities: number;
    detail_errors: number;
  };
  cache: {
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_refreshed?: boolean;
    detail_age_seconds?: number;
    detail_id?: string;
  };
}

export type ProfileValue = string | number | null;

export interface IndustryHoldingMetrics {
  market_value: ProfileValue;
  previous_market_value?: ProfileValue;
  market_value_change?: ProfileValue;
  market_value_change_pct: ProfileValue;
  institution_count: ProfileValue;
  institution_count_change: ProfileValue;
  shares_held: ProfileValue;
  previous_shares_held?: ProfileValue;
  shares_change: ProfileValue;
  shares_change_pct?: ProfileValue;
  float_shares: ProfileValue;
  total_shares: ProfileValue;
  float_share_pct: ProfileValue;
  float_share_change_pct: ProfileValue;
  total_share_pct: ProfileValue;
  total_share_change_pct: ProfileValue;
}

export interface IndustryHoldingPeriod {
  industry?: IndustryProfileNode;
  industry_code?: string;
  period_key?: string;
  period_label?: string;
  report_date: ProfileValue;
  metrics: IndustryHoldingMetrics;
}

export interface IndustryChangeMetric {
  current: ProfileValue;
  initial: ProfileValue;
  change: ProfileValue;
  change_pct: ProfileValue;
  share_pct?: ProfileValue;
}

export interface IndustryShareholderProfile {
  industry: IndustryProfileNode;
  detail_id: string;
  start_date: ProfileValue;
  end_date: ProfileValue;
  metrics: {
    per_capita_float_shares: IndustryChangeMetric;
    top10_float_holding: IndustryChangeMetric;
    top10_holding: IndustryChangeMetric;
    institution_holding: IndustryChangeMetric;
  };
}

export interface IndustryProfileNode {
  block_id: string;
  code: string;
  name: string;
  level: number;
  parent_code: string;
  member_count: number;
  is_leaf: boolean;
  has_holdings: boolean;
  holdings_periods: IndustryHoldingPeriod[];
  has_shareholder_profile: boolean;
  shareholder_profile: IndustryShareholderProfile | null;
  children?: IndustryProfileNode[];
}

export interface IndustrySecurityProfile {
  security: ValuationSecurity;
  shareholders: {
    start_date: ProfileValue;
    end_date: ProfileValue;
    days: ProfileValue;
    households: ProfileValue;
    households_change: ProfileValue;
    households_change_pct: ProfileValue;
    daily_change_pct: ProfileValue;
  };
  per_capita: { float_shares: ProfileValue };
  top10_float: { report_date: ProfileValue; shares: ProfileValue; share_pct: ProfileValue };
  top10: { report_date: ProfileValue; shares: ProfileValue; share_pct: ProfileValue };
  institution: { report_date: ProfileValue; shares: ProfileValue; float_share_pct: ProfileValue };
}

export interface IndustryProfileDocument {
  schema: string;
  generated_at: string;
  mode: 'master' | 'industry' | 'security';
  industries: IndustryProfileNode[];
  tree: IndustryProfileNode[];
  selected_industry: IndustryProfileNode | null;
  selected_security: ValuationSecurity | null;
  selected_security_profile: IndustrySecurityProfile | null;
  industry_path: IndustryProfileNode[];
  holdings_industry: IndustryProfileNode | null;
  holdings_history: IndustryHoldingPeriod[];
  shareholder_industries: IndustryProfileNode[];
  shareholder_securities: IndustrySecurityProfile[];
  detail_errors: Array<{ resource: string; message: string }>;
  counts: {
    industries: number;
    holding_industries: number;
    shareholder_industries: number;
    holding_master_rows: number;
    shareholder_master_rows: number;
    holding_history_points: number;
    selected_shareholder_industries?: number;
    shareholder_securities: number;
    detail_errors: number;
  };
  cache: {
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_refreshed?: boolean;
    detail_age_seconds?: number;
  };
}

export interface UnlockLot {
  unlock_shares: number | null;
  lock_months: number | null;
  issue_price: number | null;
  lock_return_pct: number | null;
  pre_month_return_pct: number | null;
  post_month_return_pct: number | null;
}

export interface UnlockEvent {
  detail_id: string;
  date: string;
  security: ValuationSecurity;
  progress: string;
  reason: string;
  unlock_shares: number;
  pre_unlock_close: number | null;
  unlock_market_value: number | null;
  lot_count: number;
  lots: UnlockLot[];
  unlock_to_total_ratio?: number | null;
  unlock_to_total_pct?: number | null;
  total_shares?: number | null;
  source_kind?: 'recent-large-window' | string;
  raw?: Record<string, unknown>;
}

export interface UnlockShareholder {
  security: ValuationSecurity;
  shareholder: string;
  progress: string;
  reason: string;
  unlock_shares: number | null;
  pre_unlock_close: number | null;
  unlock_market_value: number | null;
}

export interface UnlockDetail {
  detail_id: string;
  resource: string;
  shareholders: UnlockShareholder[];
  shareholder_count: number;
  returned_shareholders: number;
  detail_unlock_shares: number;
  master_unlock_shares: number;
  share_difference: number;
}

export interface UnlockSummary {
  events: number;
  raw_rows: number;
  unique_securities: number;
  implemented_events: number;
  pending_events: number;
  total_unlock_shares: number;
  total_unlock_market_value: number | null;
  first_date: string;
  last_date: string;
  reason_counts: Array<{ label: string; count: number; unlock_shares: number }>;
  date_counts: Array<{ date: string; count: number; unlock_shares: number }>;
}

export interface UnlockMonthlyPressure {
  month: string;
  unlock_shares_100m: number | null;
  unlock_shares: number | null;
  unlock_market_value_100m_yuan: number | null;
  unlock_market_value_yuan: number | null;
  total_market_cap_yuan: number | null;
  float_market_cap_yuan: number | null;
  unlock_to_total_market_cap_pct: number | null;
  unlock_to_float_market_cap_pct: number | null;
  security_count: number | null;
  lot_count: number | null;
  source_resource: string;
  raw?: Record<string, unknown>;
}

export interface UnlockMonthlySummary {
  months: number;
  first_month: string;
  last_month: string;
  total_unlock_shares: number;
  total_unlock_market_value_yuan: number;
  peak_month: string;
  peak_unlock_market_value_yuan: number | null;
  formula_checked: number;
  formula_mismatches: number;
}

export interface MarketUnlocksDocument {
  schema: string;
  generated_at: string;
  view: 'calendar' | 'recent-large' | 'monthly-pressure';
  mode: 'master' | 'security' | 'event' | 'detail' | 'recent-large' | 'monthly-pressure';
  months?: UnlockMonthlyPressure[];
  events: UnlockEvent[];
  selected_event: UnlockEvent | null;
  selected_security: ValuationSecurity | null;
  details: UnlockDetail[];
  detail_errors: Array<{ detail_id: string; resource: string; message: string }>;
  summary: UnlockSummary | UnlockMonthlySummary;
  catalog_summary: UnlockSummary | UnlockMonthlySummary;
  counts: {
    catalog_events: number;
    matched_events: number;
    returned_events: number;
    details: number;
    detail_errors: number;
    returned_shareholders: number;
    months?: number;
  };
  cache: {
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_refreshed: boolean;
    detail_age_seconds: number;
  };
  semantics?: string;
}

export interface BlockTradeFrequency {
  days_7: number | null;
  days_30: number | null;
  days_90: number | null;
}

export interface BlockTradeRow {
  security: ValuationSecurity;
  date: string;
  price: number | null;
  close: number | null;
  premium_pct: number | null;
  amount_yuan: number | null;
  volume_shares: number | null;
  frequency: BlockTradeFrequency;
  buyer: { name: string; label: string };
  seller: { name: string; label: string };
  security_type: string;
}

export interface BlockTradeHistoryRow {
  date: string;
  price: number | null;
  amount_yuan: number | null;
  buyer: string;
  seller: string;
}

export interface BlockTradeIntentionRow {
  security?: ValuationSecurity;
  date: string;
  declaration_price: number | null;
  close: number | null;
  premium_pct: number | null;
  quantity_shares: number | null;
  amount_yuan: number | null;
  direction: string;
  frequency?: BlockTradeFrequency;
}

export interface BlockTradeMonthRow {
  month: string;
  amount_yuan: number | null;
  volume_shares: number | null;
  premium_pct: number | null;
  trade_count: number | null;
}

export interface BlockTradeIndustryRow {
  industry_id: string;
  detail_id: string;
  name: string;
  amount_yuan: number | null;
  volume_shares: number | null;
  premium_pct: number | null;
  trade_count: number | null;
}

export interface BlockTradeIndustrySecurityRow {
  security: ValuationSecurity;
  month: string;
  amount_yuan: number | null;
  volume_shares: number | null;
  trade_count: number | null;
  trade_detail_text: string;
}

export interface BlockTradeBrokerPerformance {
  days: number;
  success_pct: number | null;
  average_return_pct: number | null;
}

export interface BlockTradeBrokerRow {
  broker_id: string;
  period: string;
  period_label: string;
  name: string;
  hot_money_label: string;
  latest_date: string;
  trade_count: number | null;
  buy_count: number | null;
  sell_count: number | null;
  buy_amount_yuan: number | null;
  sell_amount_yuan: number | null;
  net_buy_yuan: number | null;
  performance: BlockTradeBrokerPerformance[];
}

export interface BlockTradeBrokerDetailRow {
  date: string;
  security: ValuationSecurity;
  direction: string;
  price: number | null;
  premium_pct: number | null;
  volume_shares: number | null;
  amount_yuan: number | null;
}

export interface MarketBlockTradesDocument {
  schema: string;
  generated_at: string;
  view: 'trades' | 'intentions' | 'brokers' | 'industries';
  mode: 'catalog' | 'security' | 'broker' | 'industry';
  monthly: BlockTradeMonthRow[];
  trades: BlockTradeRow[];
  intentions: BlockTradeIntentionRow[];
  brokers: BlockTradeBrokerRow[];
  industries: BlockTradeIndustryRow[];
  industry_securities: BlockTradeIndustrySecurityRow[];
  security_history: BlockTradeHistoryRow[];
  security_intentions: BlockTradeIntentionRow[];
  broker_trades: BlockTradeBrokerDetailRow[];
  selected_security: ValuationSecurity | null;
  selected_broker: BlockTradeBrokerRow | null;
  selected_industry: BlockTradeIndustryRow | null;
  detail_errors: Array<{ resource: string; message: string }>;
  summary: {
    monthly_points: number;
    recent_trades: number;
    recent_securities: number;
    recent_amount_yuan: number;
    intention_rows: number;
    intention_securities: number;
    intention_amount_yuan: number;
  };
  filters: {
    query: string;
    period: string;
    month: string;
    industry: string;
    broker_id: string;
  };
  counts: {
    monthly: number;
    trades: number;
    intentions: number;
    brokers: number;
    industries: number;
    industry_securities: number;
    security_history: number;
    security_intentions: number;
    broker_trades: number;
    detail_errors: number;
  };
}

export interface RepurchasePlan {
  security: ValuationSecurity;
  board_approval_date: string;
  start_date: string;
  end_date: string;
  cutoff_date: string;
  planned_amount_upper_yuan: number | null;
  planned_price_upper: number | null;
  planned_shares: number | null;
  actual_price_high: number | null;
  actual_price_low: number | null;
  actual_shares: number | null;
  actual_capital_pct: number | null;
  actual_amount_yuan: number | null;
  amount_completion_pct: number | null;
  completed: boolean;
  status: string;
  purpose: string;
  region: string;
  city: string;
}

export interface RepurchaseMonth {
  month: string;
  planned_shares: number | null;
  planned_amount_yuan: number | null;
  planned_capital_pct: number | null;
  actual_shares: number | null;
  actual_amount_yuan: number | null;
  actual_capital_pct: number | null;
  completion_pct: number | null;
}

export interface RepurchaseAnnual {
  segment: 'all' | 'a' | 'hk';
  segment_label: string;
  year?: string;
  month?: string;
  shares: number | null;
  amount_yuan: number | null;
  company_count: number | null;
  financing_amount_yuan: number | null;
}

export interface HongKongRepurchase {
  security?: ValuationSecurity;
  date: string;
  average_price: number | null;
  currency: string;
  close: number | null;
  premium_pct: number | null;
  shares: number | null;
  amount: number | null;
  high: number | null;
  low: number | null;
  method: string;
}

export interface RepurchaseSummary {
  plans: number;
  unique_securities: number;
  completed_plans: number;
  active_plans: number;
  planned_amount_upper_yuan: number;
  actual_amount_yuan: number;
  actual_shares: number;
  amount_completion_pct: number | null;
  monthly_points?: number;
  hong_kong_securities?: number;
  annual_years?: number;
}

export interface MarketRepurchasesDocument {
  schema: string;
  generated_at: string;
  view: 'plans' | 'monthly' | 'annual' | 'hong-kong';
  mode: 'catalog' | 'security' | 'year';
  plans: RepurchasePlan[];
  monthly: RepurchaseMonth[];
  annual: RepurchaseAnnual[];
  annual_monthly: RepurchaseAnnual[];
  hong_kong: HongKongRepurchase[];
  hong_kong_history: HongKongRepurchase[];
  selected_security: ValuationSecurity | null;
  selected_year: string;
  summary: RepurchaseSummary;
  matched_plan_summary: RepurchaseSummary;
  sources: Array<{
    resource: string;
    size: number;
    row_count: number;
    endpoint: string;
  }>;
  detail_errors: Array<{ resource: string; message: string }>;
  filters: { query: string; segment: string; year: string };
  counts: {
    plans: number;
    monthly: number;
    annual: number;
    annual_monthly: number;
    hong_kong: number;
    hong_kong_history: number;
    detail_errors: number;
  };
}

export interface OwnershipChange {
  security?: ValuationSecurity;
  direction: 'increase' | 'decrease';
  direction_label: string;
  start_date: string;
  end_date: string;
  announcement_date: string;
  change_shares: number | null;
  average_price: number | null;
  holding_after_shares: number | null;
  holding_after_capital_pct: number | null;
  actor: string;
}

export interface OwnershipPlan {
  security: ValuationSecurity;
  direction: 'increase' | 'decrease';
  direction_label: string;
  announcement_date: string;
  announcement_close: number | null;
  start_date: string;
  end_date: string;
  range: string;
  scale: string;
  clearance_style: string;
  capital_pct: number | null;
  actor: string;
  method: string;
}

export interface InsiderChange {
  security?: ValuationSecurity;
  direction: 'increase' | 'decrease';
  direction_label: string;
  date: string;
  actor: string;
  average_price: number | null;
  close: number | null;
  change_shares: number | null;
  change_amount_yuan: number | null;
  reason: string;
  holding_after_shares: number | null;
  share_class: string;
  related_person: string;
  position: string;
  relationship: string;
}

export interface NoReductionCommitment {
  security: ValuationSecurity;
  announcement_date: string;
  announcement_close: number | null;
  start_date: string;
  end_date: string;
  actor: string;
  identity: string;
  method: string;
  purpose: string;
  transaction_method: string;
  period_note: string;
  detail: string;
  status: 'active' | 'upcoming' | 'expired' | 'unknown';
  status_label: string;
}

export interface PledgeRecord {
  security: ValuationSecurity;
  kind: 'latest' | 'warning' | 'liquidation' | 'release';
  kind_label: string;
  announcement_date?: string;
  relationship?: string;
  single_capital_pct?: number | null;
  cumulative_shares?: number | null;
  holder_shares?: number | null;
  single_holder_pct?: number | null;
  restricted_shares?: number | null;
  unrestricted_shares?: number | null;
  pledge_count?: number | null;
  cumulative_capital_pct?: number | null;
  risk_status?: string;
  pledged_shares?: number | null;
  price_range?: string;
  affected_shares?: number | null;
  affected_pct?: number | null;
  shareholder?: string;
  pledgee?: string;
  release_date?: string;
  released_shares?: number | null;
  released_capital_pct?: number | null;
  released_holder_pct?: number | null;
  cumulative_holder_pct?: number | null;
}

export interface PledgeHistory {
  pledge_date: string;
  shareholder: string;
  relationship: string;
  pledgee: string;
  shares: number | null;
  single_holder_pct: number | null;
  cumulative_holder_pct: number | null;
  single_capital_pct: number | null;
  cumulative_capital_pct: number | null;
  description: string;
  maturity_date: string;
  previous_close: number | null;
  liquidation_line: number | null;
  warning_line: number | null;
  decline_to_warning_pct: number | null;
  risk_status: string;
}

export interface OwnershipStatistic {
  period: string;
  period_type: 'month' | 'year';
  increase_amount_yuan: number | null;
  decrease_amount_yuan: number | null;
  net_amount_yuan: number | null;
  increase_companies: number | null;
  decrease_companies: number | null;
}

export interface OwnershipChangeCountTrend {
  period: string;
  increase_companies: number | null;
  decrease_companies: number | null;
  raw?: Record<string, unknown>;
}

export interface PledgeMonth {
  month: string;
  company_count: number | null;
  total_capital_shares: number | null;
  single_large_shares: number | null;
  large_cumulative_shares: number | null;
  large_holder_shares: number | null;
  restricted_shares: number | null;
  unrestricted_shares: number | null;
  pledge_count: number | null;
  cumulative_capital_pct: number | null;
}

export interface PledgeInstitution {
  institution_id: string;
  category: 'trust' | 'broker';
  category_label: string;
  name: string;
  pledge_count: number | null;
  company_count: number | null;
  market_value_yuan: number | null;
  warning_value_yuan: number | null;
  warning_pct: number | null;
  liquidation_value_yuan: number | null;
  liquidation_pct: number | null;
}

export interface PledgeInstitutionDetail {
  security: ValuationSecurity;
  announcement_date: string;
  pledge_date: string;
  maturity_date: string;
  shareholder: string;
  relationship: string;
  shares: number | null;
  holder_pct: number | null;
  capital_pct: number | null;
  liquidation_line: number | null;
  warning_line: number | null;
  risk_status: string;
}

export interface OwnershipSummary {
  actual_changes: number;
  increase_changes: number;
  decrease_changes: number;
  plans: number;
  insider_changes: number;
  insider_increases: number;
  insider_decreases: number;
  commitments: number;
  active_commitments: number;
  upcoming_commitments: number;
  expired_commitments: number;
  pledge_records: number;
  pledge_latest: number;
  pledge_warning: number;
  pledge_liquidation: number;
  pledge_release: number;
  institutions: number;
  unique_securities: number;
}

export interface MarketOwnershipDocument {
  schema: string;
  generated_at: string;
  view: 'changes' | 'plans' | 'insiders' | 'commitments' | 'pledges' | 'statistics' | 'institutions';
  mode: 'catalog' | 'security' | 'institution';
  changes: OwnershipChange[];
  plans: OwnershipPlan[];
  insiders: InsiderChange[];
  commitments: NoReductionCommitment[];
  pledges: PledgeRecord[];
  change_history: OwnershipChange[];
  insider_history: InsiderChange[];
  pledge_history: PledgeHistory[];
  change_monthly: OwnershipStatistic[];
  change_annual: OwnershipStatistic[];
  change_count_trend?: OwnershipChangeCountTrend[];
  change_count_reconciliation?: {
    master_rows: number;
    chart_rows: number;
    matched_rows: number;
    mismatch_rows: number;
    chart_only_rows: number;
    all_overlaps_match: boolean;
  } | null;
  pledge_monthly: PledgeMonth[];
  institutions: PledgeInstitution[];
  institution_details: PledgeInstitutionDetail[];
  selected_security: ValuationSecurity | null;
  selected_institution_id: string;
  summary: OwnershipSummary;
  detail_errors: Array<{ resource: string; message: string }>;
  filters: { query: string; category: string };
  counts: Record<string, number>;
}

export interface ForecastIndustry {
  detail_id: string;
  industry: { code: string; name: string };
  is_market_summary: boolean;
  report_period: string;
  previous_period_close: number | null;
  company_count: number;
  forecast_count: number;
  coverage_pct: number | null;
  favorable_count: number;
  adverse_count: number;
  favorable_pct: number | null;
  client_favorable_pct: number | null;
  category_counts: Record<string, number>;
}

export interface SecurityForecast {
  security: ValuationSecurity;
  forecast_date: string;
  report_period: string;
  forecast_type: string;
  sentiment: 'positive' | 'negative' | 'uncertain';
  profit_lower_yuan: number | null;
  profit_upper_yuan: number | null;
  growth_lower_pct: number | null;
  growth_upper_pct: number | null;
  prior_profit_yuan: number | null;
  annualized_profit_lower_yuan: number | null;
  annualized_profit_upper_yuan: number | null;
  eps: number | null;
  total_capital_shares: number | null;
  contents: string;
  reason: string;
  source_variant: 'industry-detail' | 'all-market-static';
  raw: Record<string, unknown>;
}

export interface HongKongForecast {
  security: ValuationSecurity;
  forecast_date: string;
  report_type: string;
  currency: string;
  start_date: string;
  end_date: string;
  forecast_type: string;
  sentiment: 'positive' | 'negative' | 'uncertain';
  profit_lower: number | null;
  profit_upper: number | null;
  growth_lower_pct: number | null;
  growth_upper_pct: number | null;
  prior_profit: number | null;
  annualized_profit_lower: number | null;
  annualized_profit_upper: number | null;
  total_capital_shares: number | null;
  eps: number | null;
  industry: string;
  contents: string;
  reason: string;
}

export interface ForecastSummary {
  industries: number;
  market_summary_rows: number;
  companies: number;
  forecast_companies: number;
  coverage_pct: number | null;
  favorable: number;
  adverse: number;
  hong_kong_forecasts: number;
  hong_kong_securities: number;
  hong_kong_positive: number;
  hong_kong_negative: number;
  hong_kong_uncertain: number;
  latest_forecasts: number;
  latest_securities: number;
  latest_positive: number;
  latest_negative: number;
  latest_uncertain: number;
  latest_min_date: string;
  latest_max_date: string;
  latest_report_periods: Array<{ report_period: string; count: number }>;
  current_report_period: string;
  current_report_expected: number;
  current_report_rows: number;
  current_report_gap: number;
  future_report_rows: number;
}

export interface MarketForecastDocument {
  schema: string;
  generated_at: string;
  view: 'industries' | 'securities' | 'latest' | 'hong-kong';
  mode: 'catalog' | 'industry' | 'security';
  industries: ForecastIndustry[];
  securities: SecurityForecast[];
  hong_kong: HongKongForecast[];
  selected_industry: ForecastIndustry | null;
  selected_security: ValuationSecurity | null;
  summary: ForecastSummary;
  detail_errors: Array<{ resource: string | null; message: string }>;
  filters: {
    category: string;
    query: string;
    industry: string;
    report_period: string;
  };
  counts: Record<string, number>;
}

export interface ActiveFundSecurityHolding {
  security: ValuationSecurity;
  start_date: string;
  end_date: string;
  period_end_float_market_cap_yuan: number | null;
  period_end_total_market_cap_yuan: number | null;
  holding_market_value_yuan: number | null;
  holding_market_value_change_yuan: number | null;
  holding_pct_float: number | null;
  holding_shares: number | null;
  holding_share_change: number | null;
  fund_count: number | null;
  direction: 'increased' | 'decreased' | 'new' | 'unchanged';
  industry: string;
}

export interface ActiveFundPosition {
  fund: {
    market_id: number;
    code: string;
    fund_id: string;
    name: string;
  };
  holding_market_value_yuan: number | null;
  holding_shares: number | null;
  nav_pct: number | null;
  holding_rank: number | null;
}

export interface ActiveFundSummary {
  securities: number;
  increased: number;
  decreased: number;
  newly_held: number;
  unchanged: number;
  fund_positions: number;
  total_holding_market_value_yuan: number;
  total_holding_market_value_change_yuan: number;
  total_holding_shares: number;
  period_count: number;
  periods: string[];
}

export interface MarketActiveFundsDocument {
  schema: string;
  generated_at: string;
  view: 'securities' | 'funds';
  mode: 'catalog' | 'security';
  securities: ActiveFundSecurityHolding[];
  funds: ActiveFundPosition[];
  selected_holding: ActiveFundSecurityHolding | null;
  summary: ActiveFundSummary;
  detail_errors: Array<{ resource: string | null; message: string }>;
  filters: { direction: string; query: string };
  counts: Record<string, number>;
  cache: {
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_refreshed: boolean;
    detail_age_seconds: number;
  };
}

export interface InstitutionLhbRanking {
  period: 'week' | 'month' | 'quarter' | 'year';
  period_label: string;
  unit_id: string;
  security: ValuationSecurity;
  institution_participations: number;
  institution_buy_amount_yuan: number;
  institution_sell_amount_yuan: number;
  net_institution_amount_yuan: number;
  buy_sell_ratio: number | null;
  direction: 'net-buy' | 'net-sell' | 'flat';
  latest_trade_date: string;
  earliest_trade_date: string;
}

export interface InstitutionLhbEvent {
  security: ValuationSecurity;
  event_date: string;
  event_type: string;
  event_change_pct: number;
  event_total_buy_amount_yuan: number;
  event_total_sell_amount_yuan: number;
  event_net_buy_amount_yuan: number;
  direction: 'net-buy' | 'net-sell' | 'flat';
  outside_master_date_range: boolean;
}

export interface InstitutionLhbSummary {
  period: 'week' | 'month' | 'quarter' | 'year';
  period_label: string;
  securities: number;
  net_buy_securities: number;
  net_sell_securities: number;
  flat_securities: number;
  institution_participations: number;
  institution_buy_amount_yuan: number;
  institution_sell_amount_yuan: number;
  net_institution_amount_yuan: number;
  earliest_trade_date: string;
  latest_trade_date: string;
}

export interface MarketInstitutionLhbDocument {
  schema: string;
  generated_at: string;
  period: 'week' | 'month' | 'quarter' | 'year';
  period_label: string;
  mode: 'catalog' | 'security';
  rankings: InstitutionLhbRanking[];
  events: InstitutionLhbEvent[];
  selected_ranking: InstitutionLhbRanking | null;
  summary: InstitutionLhbSummary;
  period_summaries: Record<'week' | 'month' | 'quarter' | 'year', InstitutionLhbSummary>;
  detail_errors: Array<{ resource: string | null; message: string }>;
  filters: { direction: string; query: string };
  counts: Record<string, number>;
  cache: {
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_refreshed: boolean;
    detail_age_seconds: number;
  };
}

export interface ActiveLhbRanking {
  ranking_id: string;
  source_rank: number;
  period: '5d' | 'month' | 'half-year';
  period_label: string;
  unit_id: string;
  security: ValuationSecurity;
  event_count: number;
  buy_amount_yuan: number;
  sell_amount_yuan: number;
  net_buy_amount_yuan: number;
  buy_sell_ratio: number | null;
  period_return_pct: number | null;
  direction: 'net-buy' | 'net-sell' | 'flat';
  statistics_date: string;
}

export interface ActiveLhbEvent {
  source_rank: number;
  event_date: string;
  event_type: string;
  change_pct: number | null;
  buy_amount_yuan: number;
  sell_amount_yuan: number;
  net_buy_amount_yuan: number;
  direction: 'net-buy' | 'net-sell' | 'flat';
  turnover_rate_pct: number | null;
}

export interface ActiveLhbSummary {
  period: '5d' | 'month' | 'half-year';
  period_label: string;
  rows: number;
  names_resolved: number;
  event_count_sum: number;
  buy_amount_yuan_sum: number;
  sell_amount_yuan_sum: number;
  net_buy_amount_yuan_sum: number;
  net_buy_rows: number;
  net_sell_rows: number;
  statistics_date: string | null;
}

export interface MarketActiveLhbDocument {
  schema: 'tdx-market-active-lhb-native-v1';
  generated_at: string;
  period: '5d' | 'month' | 'half-year';
  period_label: string;
  unit_id: string;
  mode: 'ranking' | 'security';
  availability: 'live' | 'empty' | 'partial' | 'stale-cache';
  summary: ActiveLhbSummary;
  period_summaries: Record<'5d' | 'month' | 'half-year', ActiveLhbSummary>;
  selected_ranking: ActiveLhbRanking | null;
  rankings: ActiveLhbRanking[];
  events: ActiveLhbEvent[];
  detail_errors: Array<{ resource: string; message: string }>;
  counts: { matched: number; returned: number; events: number };
  filters: { market: string | null; code: string | null; query: string; direction: string; sort: string; order: string };
}

export interface StateOwnedReformGroup {
  group_id: string;
  dimension: 'industry' | 'region' | 'integration' | 'company';
  dimension_label: string;
  unit_id: string;
  name: string;
  declared_member_count: number | null;
  member_count: number;
  count_matches?: boolean;
  detail_resource: string;
  members?: ValuationSecurity[];
}

export interface StateOwnedReformDetail {
  security: ValuationSecurity;
  actual_controller: string;
  controlling_stake_pct: number | null;
  leading_count: number | null;
  logic: string;
  reference_close_prices: Record<'3d' | '5d' | '20d' | '60d' | '3m' | 'ytd', number | null>;
  last_price: number | null;
  change_pct: number | null;
  amount_yuan: number | null;
  returns_pct: Record<'3d' | '5d' | '20d' | '60d' | '3m' | 'ytd', number | null>;
  quote_available: boolean;
  group?: StateOwnedReformGroup;
}

export interface StateOwnedRestructuring {
  security: ValuationSecurity;
  major_shareholder_holding_pct: number | null;
  net_profit_10k_yuan: number | null;
  capital_operation: string;
  explanation: string;
  actual_controller: string;
  controlling_stake_pct: number | null;
  as_of_date: string | null;
  last_price: number | null;
  change_pct: number | null;
  amount_yuan: number | null;
  quote_available: boolean;
}

export interface StateOwnedReformSummary {
  group_count: number;
  group_relationships: number;
  unique_grouped_securities: number;
  member_count_mismatches: number;
  restructuring_rows: number;
}

export interface MarketStateOwnedReformDocument {
  schema: 'tdx-market-state-owned-reform-native-v1';
  generated_at: string;
  view: 'groups' | 'group' | 'security' | 'restructuring' | 'catalog';
  dimension: string;
  availability: 'live' | 'empty' | 'partial' | 'stale-cache';
  summary: Partial<StateOwnedReformSummary>;
  groups: StateOwnedReformGroup[];
  details: StateOwnedReformDetail[];
  restructuring: StateOwnedRestructuring[];
  errors: Array<{ resource: string; message: string }>;
  counts: { matched: number; returned: number; details: number };
  quote_source: { received: number; requested: number } | null;
}

export interface EconomicIndicatorRecord {
  indicator_id: string;
  name: string;
  indicator_type: string;
  current_value: number | null;
  unit: string;
  month_on_month_pct: number | null;
  year_on_year_pct: number | null;
  frequency: string;
  update_date: string;
  report_period: string;
  history_resource: string;
  related_resource: string;
  raw: Record<string, unknown>;
}

export interface EconomicIndicatorHistoryPoint {
  date: string;
  value: number;
  raw: Record<string, unknown>;
}

export interface EconomicIndicatorRelatedSecurity {
  security: ValuationSecurity;
  industry: string;
  last_price: number | null;
  change_pct: number | null;
  amount_yuan: number | null;
  quote_available: boolean;
  raw: Record<string, unknown>;
}

export interface MarketEconomicIndicatorsDocument {
  schema: 'tdx-market-economic-indicators-native-v1';
  generated_at: string;
  view: 'catalog' | 'indicator';
  availability: 'live' | 'partial' | 'stale-cache' | 'empty';
  indicators: EconomicIndicatorRecord[];
  selected_indicator: EconomicIndicatorRecord | null;
  history: EconomicIndicatorHistoryPoint[];
  related_securities: EconomicIndicatorRelatedSecurity[];
  summary: {
    indicator_count: number;
    by_type: Array<{ name: string; count: number }>;
    by_frequency: Array<{ name: string; count: number }>;
    first_update_date: string;
    last_update_date: string;
    related_security_count?: number;
    history_points?: number;
  };
  errors: Array<{ resource: string; message: string }>;
  counts: {
    matched: number;
    returned_indicators: number;
    history_points: number;
    returned_related_securities: number;
  };
  quote_source: { requested: number; received: number } | null;
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string }>;
}

export type RatingStance = 'positive' | 'neutral' | 'negative' | 'unknown';

export interface HongKongRating {
  security: ValuationSecurity;
  latest_report_date: string;
  latest_institution: string;
  latest_rating: string;
  stance: RatingStance;
  latest_target_price_hkd: number | null;
  six_month_report_count: number;
  six_month_average_target_price_hkd: number | null;
}

export interface RatingIndustryEntity {
  code: string;
  industry_id: string;
  name: string;
  name_resolved: boolean;
}

export interface IndustryRating {
  industry: RatingIndustryEntity;
  latest_report_date: string;
  latest_institution: string;
  latest_rating: string;
  latest_rating_change: string;
  stance: RatingStance;
  six_month_report_count: number;
  six_month_bullish_count: number;
  six_month_bearish_count: number;
  six_month_unclassified_count: number;
  six_month_bullish_ratio: number | null;
}

export interface RatingReport {
  report_date: string;
  institution: string;
  rating: string;
  previous_rating: string;
  rating_change: string;
  rating_changed: boolean;
  stance: RatingStance;
  target_price_hkd: number | null;
  reason: string;
  related_code: string;
  security_code?: string;
}

export interface HongKongRatingSummary {
  securities: number;
  six_month_reports: number;
  positive_latest_ratings: number;
  neutral_latest_ratings: number;
  negative_latest_ratings: number;
  unknown_latest_ratings: number;
  latest_target_prices: number;
  average_target_prices: number;
  earliest_latest_report_date: string;
  latest_report_date: string;
}

export interface IndustryRatingSummary {
  industries: number;
  six_month_reports: number;
  six_month_bullish_reports: number;
  six_month_bearish_reports: number;
  six_month_unclassified_reports: number;
  earliest_latest_report_date: string;
  latest_report_date: string;
}

export interface MarketRatingsDocument {
  schema: string;
  generated_at: string;
  view: 'hong-kong' | 'industries';
  mode: 'catalog' | 'selection';
  hong_kong: HongKongRating[];
  industries: IndustryRating[];
  reports: RatingReport[];
  selected_hong_kong: HongKongRating | null;
  selected_industry: IndustryRating | null;
  selected_security: ValuationSecurity | null;
  hong_kong_summary: HongKongRatingSummary;
  industry_summary: IndustryRatingSummary;
  detail_errors: Array<{ resource: string | null; message: string }>;
  filters: { stance: string; query: string };
  counts: Record<string, number>;
  cache: {
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_refreshed: boolean;
    detail_age_seconds: number;
  };
}

export type ForeignAlertStatusKey =
  | 'forced-reduction'
  | 'suspended-buy'
  | 'warning'
  | 'approaching'
  | 'unknown';

export interface ForeignAlertRecord {
  security: ValuationSecurity;
  date: string;
  foreign_holding_shares: number | null;
  foreign_holding_ratio_pct: number | null;
  status: string;
  status_key: ForeignAlertStatusKey;
  daily_change_shares: number | null;
  daily_change_pct: number | null;
}

export interface ForeignAlertSummary {
  securities: number;
  forced_reduction: number;
  suspended_buy: number;
  warning: number;
  approaching: number;
  unknown: number;
  foreign_holding_shares: number;
  daily_change_shares: number;
  maximum_holding_ratio_pct: number;
  date: string;
}

export interface MarketForeignAlertsDocument {
  schema: string;
  generated_at: string;
  mode: 'catalog' | 'security';
  alerts: ForeignAlertRecord[];
  history: ForeignAlertRecord[];
  selected_alert: ForeignAlertRecord | null;
  summary: ForeignAlertSummary;
  detail_errors: Array<{ resource: string | null; message: string }>;
  filters: { status: string; query: string };
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string }>;
  cache: {
    master_refreshed: boolean;
    master_age_seconds: number;
    detail_refreshed: boolean;
    detail_age_seconds: number;
  };
  counts: { alerts: number; history: number; detail_errors: number };
}

export interface MarginRecord {
  security?: ValuationSecurity;
  date: string;
  classification?: 'industry' | 'concept' | 'style';
  classification_code?: string;
  classification_name?: string;
  change_pct?: number | null;
  float_market_cap_yuan?: number | null;
  float_shares?: number | null;
  financing_net_buy_yuan?: number | null;
  financing_balance_yuan: number | null;
  financing_balance_float_market_pct: number | null;
  short_net_sell_shares?: number | null;
  short_balance_shares?: number | null;
  short_balance_float_shares_pct?: number | null;
  financing_short_difference_yuan?: number | null;
  financing_net_change_yuan?: number | null;
  sh_financing_balance_yuan?: number | null;
  sz_financing_balance_yuan?: number | null;
  short_balance_yuan?: number | null;
  short_balance_float_market_pct?: number | null;
  financing_buy_yuan?: number | null;
  financing_sell_yuan?: number | null;
  short_buy_yuan?: number | null;
  short_sell_yuan?: number | null;
  short_net_sell_yuan?: number | null;
  transfer_financing_lent_yuan?: number | null;
  transfer_financing_repaid_yuan?: number | null;
  transfer_financing_net_change_yuan?: number | null;
  transfer_financing_balance_yuan?: number | null;
  securities_lending_lent_shares?: number | null;
  securities_lending_repaid_shares?: number | null;
  securities_lending_net_change_shares?: number | null;
  securities_lending_balance_shares?: number | null;
  securities_lending_balance_yuan?: number | null;
  raw?: Record<string, unknown>;
}

export interface MarketMarginDocument {
  schema: string;
  generated_at: string;
  view: 'market' | 'transfer' | 'ranking' | 'security' | 'classifications' | 'classification-history';
  category: string;
  date?: string;
  group_id?: string;
  availability?: 'live' | 'stale-cache' | 'partial';
  upstream_health?: Record<string, unknown>;
  records: MarginRecord[];
  trend: Array<{ date: string; financing_balance_float_market_pct: number | null; short_balance_float_shares_pct: number | null }>;
  latest: MarginRecord | null;
  count: number;
  detail_errors: Array<{ resource: string; message: string }>;
  cache: { refreshed: boolean; age_seconds: number };
}

export interface StockConnectFlowRecord {
  date: string;
  turnover_yuan: number | null;
  net_inflow_yuan: number | null;
  daily_balance_yuan: number | null;
  buy_turnover_yuan: number | null;
  sell_turnover_yuan: number | null;
  net_buy_turnover_yuan: number | null;
  reference_index: number | null;
  reference_index_change_pct: number | null;
}

export interface StockConnectHoldingRecord {
  security: ValuationSecurity;
  date: string;
  holding_shares: number | null;
  holding_ratio_pct: number | null;
  holding_market_value_yuan: number | null;
  previous_holding_shares: number | null;
  previous_holding_ratio_pct: number | null;
  holding_share_change: number | null;
  net_buy_amount_yuan: number | null;
  daily_market_value_change_yuan: number | null;
  five_day_market_value_change_yuan: number | null;
  first_inclusion_date: string;
  channel: string;
  snapshot_freshness: 'current' | 'historical-snapshot';
}

export interface StockConnectHistoryRecord {
  date: string;
  holding_shares: number | null;
  holding_share_change: number | null;
  holding_market_value_yuan: number | null;
  holding_ratio_pct: number | null;
  market_value_change_yuan: number | null;
  market_value_change_pct: number | null;
}

export interface StockConnectChartRecord {
  date: string;
  holding_ratio_pct: number | null;
  market_value_change_yuan: number | null;
  raw?: Record<string, unknown>;
}

export interface SouthboundIndustryMemberRecord {
  security: ValuationSecurity;
  daily_net_inflow_yuan: number | null;
  five_day_net_inflow_yuan: number | null;
  one_month_net_inflow_yuan: number | null;
  latest_close: number | null;
  five_day_base_close: number | null;
  one_month_base_close: number | null;
  five_day_change_pct: number | null;
  one_month_change_pct: number | null;
  raw?: Record<string, unknown>;
}

export interface SouthboundIndustryTrendRecord {
  date: string;
  daily_net_inflow_yuan: number | null;
  raw?: Record<string, unknown>;
}

export interface StockConnectActivityRecord {
  security: ValuationSecurity;
  category: string;
  date: string;
  holding_shares: number | null;
  previous_holding_shares: number | null;
  holding_share_change: number | null;
  holding_change_pct: number | null;
  net_buy_amount: number | null;
  consecutive_days: number | null;
  source_age_days: number | null;
  snapshot_freshness: 'current' | 'historical-snapshot';
  raw?: Record<string, unknown>;
}

export interface StockConnectIndustryRecord {
  category: string;
  classification_code: string;
  classification_market: string;
  date: string;
  daily_net_inflow: number | null;
  five_day_net_inflow: number | null;
  one_month_net_inflow: number | null;
  latest_close: number | null;
  five_day_change_pct: number | null;
  one_month_change_pct: number | null;
  float_market_value: number | null;
  source_age_days: number | null;
  snapshot_freshness: 'current' | 'historical-snapshot';
  raw?: Record<string, unknown>;
}

export interface StockConnectActiveRecord {
  security: ValuationSecurity;
  channel: string;
  date: string;
  close: number | null;
  change_pct: number | null;
  total_turnover_10k_yuan: number | null;
  net_buy_10k_yuan: number | null;
  buy_10k_yuan: number | null;
  sell_10k_yuan: number | null;
  connect_turnover_yuan: number | null;
  net_buy_total_turnover_pct: number | null;
  connect_total_turnover_pct: number | null;
  raw?: Record<string, unknown>;
}

export interface MarketStockConnectDocument {
  schema: string;
  generated_at: string;
  view: 'flows' | 'holdings' | 'security' | 'activity' | 'industry' | 'industry-detail' | 'active-stocks';
  category: string;
  group_id?: string;
  records: Array<StockConnectFlowRecord | StockConnectHoldingRecord | StockConnectHistoryRecord | SouthboundIndustryMemberRecord | StockConnectActivityRecord | StockConnectIndustryRecord | StockConnectActiveRecord>;
  trend?: Array<StockConnectChartRecord | SouthboundIndustryTrendRecord>;
  latest: StockConnectFlowRecord | StockConnectHoldingRecord | StockConnectHistoryRecord | null;
  count: number;
  reconciliation?: Record<string, unknown> | null;
  detail_errors: Array<{ resource: string; message: string }>;
  cache: { refreshed: boolean; age_seconds: number };
}

export interface AttentionRecord {
  security: ValuationSecurity;
  date: string;
  listing_status: string;
  rank: number | null;
  rank_change: number | null;
  attention_total: number | null;
  attention_intraday: number | null;
  attention_after_hours: number | null;
  professional_attention_total: number | null;
  professional_attention_intraday: number | null;
  professional_attention_after_hours: number | null;
  resonance_total: number | null;
  resonance_intraday: number | null;
  resonance_after_hours: number | null;
  resonance_change: number | null;
  sentiment_heat: number | null;
  sentiment_weekly_new: number | null;
  bullish_votes_month: number | null;
  bearish_votes_month: number | null;
  bullish_ratio_pct: number | null;
}

export interface IntelligenceRiskRecord {
  security: ValuationSecurity;
  category: 'observation' | 'potential' | 'discredited';
  category_label: string;
  risk_type: string;
  detail: string;
  safety_score: number | null;
  announcement_date: string;
  involved_subject: string;
  object_type: string;
  occurrences_past_year: number | null;
  record_id: string;
  source_resource: string;
  raw: Record<string, unknown>;
}

export interface IntelligenceHighlightRecord {
  security: ValuationSecurity;
  highlight_count: number | null;
  primary_highlight_type: string;
  highlight_detail: string;
  safety_score: number | null;
  highlight_score: number | null;
  raw: Record<string, unknown>;
}

export interface IntelligenceValueAttentionCategory {
  category_id: string;
  category_name: string;
  reported_member_count: number | null;
  inline_member_count: number;
  dynamic_member_count?: number;
  members?: ValuationSecurity[];
  member_source: 'inline-master' | 'inline-master+dynamic-detail';
  source_resource: string;
  raw: Record<string, unknown>;
  selected_security_related?: boolean;
}

export interface IntelligenceValueAttentionRecord {
  category_id: string;
  security: ValuationSecurity;
  anchor_price_yuan: number | null;
  adjusted_anchor_price_yuan: number | null;
  three_month_adjusted_close_yuan: number | null;
  /** The client formula needs the host's live $NOW value, so static JSN data leaves this null. */
  breach_depth_pct: number | null;
  source_resource: string;
  raw: Record<string, unknown>;
}

export interface IntelligenceEventRecord {
  event_id: string;
  raw_id: string;
  source: 'events' | 'ministries' | 'hotspots';
  source_label: string;
  date: string;
  title: string;
  type: string;
  organization: string;
  content: string;
  importance: number | null;
  stock_count_reported: number | null;
  days: number | null;
  member_count: number;
  member_source?: 'inline-master' | 'dynamic-detail';
  inline_member_count?: number;
  members?: ValuationSecurity[];
  selected_security_related?: boolean;
}

export interface IntelligenceTopicRecord {
  topic_id: string;
  created_date: string;
  updated_date: string;
  name: string;
  category: string;
  description: string;
  raw?: Record<string, unknown>;
}

export interface IntelligenceTimelineRecord {
  date: string;
  headline: string;
  content: string;
  source_url: string | null;
  has_embedded_content: boolean;
  kind: 'topic' | 'news';
  raw?: Record<string, unknown>;
}

export interface IntelligenceMarketAnomalyRecord {
  date: string;
  anomaly_type: string;
  reason: string;
  previous_close: number | null;
  close: number | null;
  week_later_close: number | null;
  change_points: number | null;
  previous_day_change_pct: number | null;
  same_day_change_pct: number | null;
  next_day_change_pct: number | null;
  next_week_change_pct: number | null;
  limit_up_count: number | null;
  limit_down_count: number | null;
  market_volume: number | null;
  market_turnover_yuan: number | null;
  raw?: Record<string, unknown>;
}

export interface IntelligenceGraphNode {
  id: string;
  kind: 'event' | 'security';
  label: string;
  source?: string;
  date?: string;
  type?: string;
  member_count?: number;
  security?: ValuationSecurity;
}

export interface IntelligenceGraphEdge {
  source: string;
  target: string;
  kind: 'affects';
}

export interface IntelligenceGraph {
  nodes: IntelligenceGraphNode[];
  edges: IntelligenceGraphEdge[];
  counts: {
    events: number;
    securities: number;
    relationships: number;
    returned_relationships: number;
    omitted_relationships: number;
  };
  truncated: boolean;
}

export interface MarketIntelligenceDocument {
  schema: string;
  generated_at: string;
  view: 'attention' | 'value-attention' | 'risks' | 'highlights' | 'events' | 'event' | 'graph' | 'security' | 'topics' | 'topic' | 'news' | 'market-anomalies';
  category: string;
  records: Array<AttentionRecord | IntelligenceValueAttentionCategory | IntelligenceValueAttentionRecord | IntelligenceRiskRecord | IntelligenceHighlightRecord | IntelligenceEventRecord | IntelligenceTopicRecord | IntelligenceTimelineRecord | IntelligenceMarketAnomalyRecord>;
  attention: AttentionRecord | null;
  value_attention: IntelligenceValueAttentionCategory[];
  selected_value_attention: IntelligenceValueAttentionCategory | null;
  value_attention_summary: {
    categories: number;
    relationships: number;
    unique_securities: number;
  } | null;
  value_attention_reconciliation: {
    inline_member_count: number;
    dynamic_member_count: number;
    counts_match: boolean;
    exact_match: boolean;
    inline_only: string[];
    dynamic_only: string[];
  } | null;
  risks: IntelligenceRiskRecord[];
  highlights: IntelligenceHighlightRecord[];
  highlight_summary: {
    securities: number;
    names_resolved: number;
    type_count: number;
    by_type: Array<{ type: string; count: number }>;
    by_market: Record<string, number>;
    minimum_safety_score: number | null;
    maximum_safety_score: number | null;
    average_safety_score: number | null;
  } | null;
  events: IntelligenceEventRecord[];
  graph: IntelligenceGraph | null;
  selected_topic?: IntelligenceTopicRecord | null;
  event_reconciliation?: {
    inline_member_count: number;
    dynamic_member_count: number;
    counts_match: boolean;
    inline_only: string[];
    dynamic_only: string[];
    exact_match: boolean;
  } | null;
  security: ValuationSecurity | null;
  counts: { records: number; risks: number; highlights: number; events: number; value_attention: number };
  cache: { refreshed: boolean; age_seconds: number; stale?: boolean };
}

export interface EtfFlowEntity {
  type: 'security' | 'industry';
  market_id: number;
  market: string;
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
}

export interface EtfFlowRow {
  entity: EtfFlowEntity;
  cutoff_date: string;
  etf_count: number | null;
  holding_shares: number | null;
  holding_market_value_yuan: number | null;
  broad_net_inflow_yuan: number | null;
  theme_net_inflow_yuan: number | null;
  total_net_inflow_yuan: number | null;
  turnover_yuan: number | null;
  net_inflow_share_turnover_pct: number | null;
  weekly_net_inflow_yuan: number | null;
}

export interface EtfFlowSummary {
  rows: number;
  etf_relationships: number;
  inflow: number;
  outflow: number;
  flat: number;
  holding_shares: number;
  holding_market_value_yuan: number;
  broad_net_inflow_yuan: number;
  theme_net_inflow_yuan: number;
  total_net_inflow_yuan: number;
  weekly_net_inflow_yuan: number;
  cutoff_dates: string[];
}

export interface MarketEtfFlowsDocument {
  schema: string;
  generated_at: string;
  view: 'stocks' | 'industries';
  mode: 'catalog' | 'security' | 'industry';
  found: boolean;
  rows: EtfFlowRow[];
  selected: EtfFlowRow | null;
  related_industry: EtfFlowRow | null;
  summaries: { stocks: EtfFlowSummary; industries: EtfFlowSummary };
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string }>;
  filters: { direction: string; sort: string; query: string };
  cache: {
    ttl_seconds: number;
    stocks_refreshed: boolean;
    stocks_age_seconds: number;
    industries_refreshed: boolean;
    industries_age_seconds: number;
  };
  counts: { available_stocks: number; available_industries: number; returned: number };
}

export interface ConvertibleBondSecurity {
  market_id: number;
  market: string;
  code: string;
  security_id: string;
  name: string;
}

export interface ConvertibleBondTrigger {
  condition: string;
  price_ratio_pct: number | null;
  start_date: string;
  trigger_price: number | null;
  conversion_price: number | null;
  current_days: string;
  current_ratio_pct: number | null;
  status: string;
  history_count: number | null;
  history_dates: string[];
  available_days: number | null;
}

export interface ConvertibleBondRecord {
  bond: ConvertibleBondSecurity;
  instrument_type: 'convertible-bond' | 'exchangeable-bond';
  exchangeable_supplemented: boolean;
  exchangeable_projection_verified: boolean;
  underlying: ConvertibleBondSecurity | null;
  overview: {
    issuer: string;
    risk_notice: string;
    face_value: number | null;
    listing_date: string;
    issue_date: string;
    issue_price: number | null;
    return_since_listing_pct: number | null;
    return_5d_pct: number | null;
    return_10d_pct: number | null;
    issue_size_100m_yuan: number | null;
    remaining_balance_100m_yuan: number | null;
    remaining_ratio_pct: number | null;
    conversion_price: number | null;
    conversion_start_date: string;
    conversion_end_date: string;
    maturity_date: string;
    remaining_years: number | null;
    maturity_redemption_price: number | null;
    unpaid_coupon_sum: number | null;
    sellback_trigger_ratio_pct: number | null;
    redemption_trigger_ratio_pct: number | null;
    bond_rating: string;
    issuer_rating: string;
    current_state: string;
    source_resource: string;
    projection_resource: string;
    core_terms_complete: boolean;
  };
  progress: {
    issue_size_100m_yuan: number | null;
    remaining_balance_100m_yuan: number | null;
    conversion_progress_pct: number | null;
    redeemed_amount_100m_yuan: number | null;
    sellback_amount_100m_yuan: number | null;
    maturity_progress_pct: number | null;
  };
  coupons: {
    term_years: number | null;
    rates_pct: Array<number | null>;
    compensation_rate_pct: number | null;
    payment_dates: string[];
    payment_rates: Array<number | string>;
  };
  sellback: ConvertibleBondTrigger;
  redemption: ConvertibleBondTrigger;
  revision: ConvertibleBondTrigger;
}

export interface ConvertibleBondProjectionReconciliation {
  primary_resource: string;
  projection_resource: string;
  primary_rows?: number;
  projection_rows?: number;
  primary_securities: number;
  projection_securities: number;
  common_securities: number;
  primary_only_security_ids: string[];
  projection_only_security_ids: string[];
  exact_security_set: boolean;
}

export interface NewConvertibleBondProjectionRecord {
  kind: 'new-convertible-bond-projection';
  event_id: string;
  underlying: ConvertibleBondSecurity;
  subscription_code: string;
  subscription_name: string;
  subscription_date: string;
  subscription_date_text: string;
  issue_date: string;
  issue_price_yuan: number | null;
  issue_type: string;
  issue_size_100m_yuan: number | null;
  stock_rights_yuan: number | null;
  conversion_price_yuan: number | null;
  conversion_available: string;
  conversion_start_date: string;
  conversion_end_date: string;
  shareholder_placement_ratio: number | null;
  lottery_date: string;
  lottery_rate_pct: number | null;
  region: string;
  plan_progress: string;
  progress_date: string;
  source_resource: string;
  subscription_match: {
    matched: boolean;
    match_method: 'subscription-code' | 'underlying-only' | 'none';
    event_id: string;
    bond: ConvertibleBondSecurity | null;
    primary_subscription_date: string;
    primary_issue_size_100m_yuan: number | null;
    subscription_date_mismatch: boolean;
    issue_size_delta_100m_yuan: number | null;
    issue_size_mismatch: boolean;
  };
  raw: Record<string, unknown>;
}

export interface NewConvertibleBondReconciliation {
  primary_resource: string;
  projection_resource: string;
  primary_rows: number;
  projection_rows: number;
  exact_subscription_code_matches: number;
  underlying_only_matches: number;
  unmatched_projection_rows: number;
  issue_size_mismatch_count: number;
  subscription_date_mismatch_count: number;
  hybrid_or_stale_count: number;
  unmatched_subscription_codes: string[];
  exact_projection: boolean;
}

export interface PendingConvertibleBondRecord {
  underlying: ConvertibleBondSecurity;
  issue_type: string;
  planned_issue_size_100m_yuan: number | null;
  plan_progress: string;
  progress_date: string;
  stock_rights_yuan: number | null;
  conversion_price_yuan: number | null;
  shareholder_placement_ratio: number | null;
  subscription_date: string;
  issue_date: string;
  lottery_rate: number | null;
  lottery_date: string;
  subscription_code: string;
  subscription_name: string;
  issue_price_yuan: number | null;
  raw: Record<string, unknown>;
}

export interface ConvertibleBondSubscriptionRecord {
  kind: 'convertible-bond-subscription';
  event_id: string;
  bond: ConvertibleBondSecurity;
  underlying: ConvertibleBondSecurity;
  subscription_date: string;
  subscription_code: string;
  subscription_limit_10k_yuan: number | null;
  conversion_start_date: string;
  underlying_close_yuan: number | null;
  conversion_price_yuan: number | null;
  conversion_value_yuan: number | null;
  bond_close_yuan: number | null;
  conversion_premium_pct: number | null;
  issue_size_100m_yuan: number | null;
  lottery_date: string;
  lottery_rate_pct: number | null;
  listing_date: string;
  listed: boolean;
  source_resource: string;
  raw: Record<string, unknown>;
}

export interface ConvertibleBondPricingRecord {
  bond: ConvertibleBondSecurity;
  underlying: ConvertibleBondSecurity | null;
  instrument_type: 'convertible-bond' | 'exchangeable-bond';
  active: boolean;
  terms: {
    face_value: number | null;
    return_since_listing_pct: number | null;
    return_5d_pct: number | null;
    return_10d_pct: number | null;
    conversion_price: number | null;
    conversion_start_date: string;
    conversion_end_date: string;
    listing_date: string;
    interest_start_date: string;
    maturity_date: string;
    remaining_years: number | null;
    bond_rating: string;
    issuer_rating: string;
    bond_type: string;
    rate_type: string;
    payment_dates: string[];
    payment_rates: Array<number | string>;
    remaining_payment_count: number | null;
    remaining_payment_dates: string[];
    remaining_payment_rates: Array<number | string>;
    previous_payment_date: string;
    next_payment_date: string;
    payment_frequency_months: number | null;
    remaining_balance_pct: number | null;
    revision_trigger_ratio_pct: number | null;
    sellback_trigger_ratio_pct: number | null;
    redemption_trigger_ratio_pct: number | null;
    revision_trigger_price: number | null;
    sellback_trigger_price: number | null;
    redemption_trigger_price: number | null;
    curve_short_years: number | null;
    curve_short_yield_pct: number | null;
    curve_long_years: number | null;
    curve_long_yield_pct: number | null;
    interpolated_curve_yield_pct: number | null;
  };
  quote: {
    bond_last_price: number | null;
    bond_price_source: 'last-price' | 'pre-close' | 'unavailable';
    bond_change_pct: number | null;
    bond_amount_yuan: number | null;
    underlying_last_price: number | null;
    underlying_price_source: 'last-price' | 'pre-close' | 'unavailable';
    underlying_change_pct: number | null;
    underlying_amount_yuan: number | null;
    bond_available: boolean;
    underlying_available: boolean;
  };
  valuation: {
    as_of_date: string;
    accrued_interest: number | null;
    accrued_interest_source: string;
    full_price: number | null;
    conversion_value: number | null;
    conversion_premium_pct: number | null;
    maturity_yield_pct: number | null;
    pure_bond_value: number | null;
    pure_bond_premium_pct: number | null;
    double_low_score: number | null;
    cash_flow_count: number;
    availability: 'complete' | 'bond-only' | 'terms-only';
    calculation_note: string;
  };
  raw: Record<string, unknown>;
}

export interface ConvertibleBondDetailEvent {
  kind: 'sellback' | 'redemption' | 'revision';
  start_date?: string;
  end_date?: string;
  payment_date?: string;
  date?: string;
  price?: number | null;
  conversion_price?: number | null;
  ratio_pct?: number | null;
  quantity?: number | null;
  amount?: number | null;
  remaining_quantity?: number | null;
  reason?: string;
}

export interface ConvertibleBondDetail {
  bond: ConvertibleBondSecurity;
  sellback: ConvertibleBondDetailEvent[];
  redemption: ConvertibleBondDetailEvent[];
  revision: ConvertibleBondDetailEvent[];
}

export interface MarketConvertibleBondsDocument {
  schema: string;
  generated_at: string;
  view: 'listed' | 'pending' | 'subscriptions' | 'pricing';
  mode: 'catalog' | 'security';
  availability: 'live' | 'partial' | 'stale-cache';
  query: string;
  found: boolean;
  match_count: number;
  returned: number;
  bonds: ConvertibleBondRecord[];
  pricing: ConvertibleBondPricingRecord[];
  pending_issues: PendingConvertibleBondRecord[];
  subscriptions: ConvertibleBondSubscriptionRecord[];
  new_bond_projection?: NewConvertibleBondProjectionRecord[];
  new_bond_projection_match_count?: number;
  new_bond_reconciliation?: NewConvertibleBondReconciliation;
  projection_reconciliation?: ConvertibleBondProjectionReconciliation[];
  exchangeable_projection_reconciliation?: ConvertibleBondProjectionReconciliation | null;
  details: ConvertibleBondDetail[];
  detail_errors: Array<{ bond: string; resource: string; message: string }>;
  master_errors: Array<{ resource: string; message: string }>;
  summary: {
    bonds?: number;
    remaining_balance_100m_yuan?: number;
    sellback_triggered_bonds?: number;
    redemption_triggered_bonds?: number;
    conversion_price_revisions?: number;
    exchangeable_bonds?: number;
    exchangeable_bonds_supplemented?: number;
    exchangeable_bonds_projection_verified?: number;
    core_terms_complete?: number;
    core_terms_missing?: number;
    pending_issues?: number;
    planned_issue_size_100m_yuan?: number;
    by_progress?: Array<{ progress: string; count: number; planned_issue_size_100m_yuan: number }>;
    subscriptions?: number;
    listed?: number;
    not_listed?: number;
    formula_complete?: number;
    issue_size_100m_yuan?: number;
    projection_count?: number;
    new_bond_projection_rows?: number;
    new_bond_hybrid_or_stale?: number | null;
    pricing_rows?: number;
    active_bonds?: number;
    bond_quotes?: number;
    underlying_quotes?: number;
    complete_valuations?: number;
  };
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string }>;
  quote_availability?: 'live' | 'unavailable' | 'disabled';
  quote_errors?: Array<{ source: string; message: string }>;
  cache: { refreshed: boolean; age_seconds: number; ttl_seconds: number; quotes_refreshed?: boolean; quote_age_seconds?: number; quote_ttl_seconds?: number };
}

export interface CalendarSecurity {
  market_id: number;
  market: string;
  code: string;
  security_id: string;
  name: string;
}

export interface CalendarRecord {
  kind: 'macro' | 'meeting' | 'company' | 'listing' | 'major-event' |
    'ipo-announcement' | 'recent-ipo' | 'ipo-guidance' | 'ipo-review' | 'ipo-subscription' |
    'ipo-subscription-detail' | 'ipo-companion-news' | 'us-ipo-application' |
    'us-ipo-calendar' | 'us-ipo-listed' | 'us-ipo-pending' | 'futures-calendar' |
    'star-news' | 'chinext-news' | 'neeq-news' |
    'ipo-listing-event' | 'ipo-issue-event' | 'suspension-resumption' |
    'special-treatment' | 'listing-status' | 'additional-issuance' | 'shareholder-meeting';
  date: string;
  title: string;
  importance?: string;
  region?: string;
  frequency?: string;
  previous?: number | null;
  consensus?: number | null;
  actual?: number | null;
  revised?: number | null;
  event_id: string;
  industry: string;
  event_type: string;
  content: string;
  content_excerpt?: string;
  source_url?: string;
  source_resource?: string;
  exchange_code?: string;
  security: CalendarSecurity | null;
  related_securities?: CalendarSecurity[];
  subscription_start?: string;
  subscription_end?: string;
  issue_price_low?: number | null;
  issue_price_high?: number | null;
  issue_shares?: number | null;
  raised_yuan?: number | null;
  issue_price?: number | null;
  subscription_multiple?: number | null;
  winning_rate_pct?: number | null;
  listing_return_pct?: number | null;
  snapshot_date?: string;
  winning_lot_shares?: number | null;
  overfunding_yuan?: number | null;
  change_5d_pct?: number | null;
  change_10d_pct?: number | null;
  pe?: number | null;
  industry_pe?: number | null;
  issue_pe?: number | null;
  sponsor?: string;
  listing_limit_up_count?: number | null;
  board_category?: string;
  company_name?: string;
  guidance_progress?: string;
  guidance_institution?: string;
  acceptance_date?: string;
  review_status?: string;
  prospectus_url?: string;
  effective_subscription_yuan?: number | null;
  financial_report_date?: string;
  pre_issue_total_shares?: number | null;
  eps?: number | null;
  net_assets_per_share?: number | null;
  cash_flow_per_share?: number | null;
  roe_pct?: number | null;
  planned_issue_shares?: number | null;
  post_issue_share_pct?: number | null;
  planned_financing_yuan?: number | null;
  source_record_id?: string;
  subscription_code?: string;
  subscription_date?: string;
  payment_date?: string;
  refund_date?: string;
  issue_price_yuan?: number | null;
  pre_issue_price_yuan?: number | null;
  subscription_min_shares?: number | null;
  subscription_max_shares?: number | null;
  pricing_method?: string;
  inquiry_start_date?: string;
  inquiry_end_date?: string;
  inquiry_price_lower_yuan?: number | null;
  inquiry_price_upper_yuan?: number | null;
  issue_total_shares?: number | null;
  online_issue_shares?: number | null;
  listing_date?: string;
  exchange?: string;
  bookrunner?: string;
  price_text?: string;
  issue_amount_source_value?: number | null;
  issue_amount_source_unit?: 'million-usd' | 'usd';
  issue_amount_usd?: number | null;
  issue_shares_source_value?: number | null;
  issue_shares_source_unit?: 'million-shares' | 'shares';
  application_date?: string;
  scheduled_listing_date?: string;
  offer_price_usd?: number | null;
  suspension_date?: string;
  expected_resumption_date?: string;
  resumption_date?: string;
  suspension_days?: number | null;
  pre_suspension_day_change_pct?: number | null;
  resumption_day_change_pct?: number | null;
  raw?: Record<string, unknown>;
}

export interface MarketCalendarDocument {
  schema: string;
  generated_at: string;
  view: 'all' | 'macro' | 'meetings' | 'company' | 'listings' |
    'major-events' | 'ipo-announcements' | 'recent-ipos' | 'board-news' |
    'ipo-guidance' | 'ipo-review' | 'ipo-subscriptions' | 'ipo-subscription-details' |
    'ipo-companion-news' | 'us-ipo' | 'us-ipo-applications' | 'us-ipo-calendar' |
    'us-ipo-listed' | 'us-ipo-pending' | 'star-news' | 'chinext-news' | 'neeq-news' | 'futures';
  mode?: 'calendar' | 'security';
  availability?: 'live' | 'empty';
  match_count?: number;
  returned?: number;
  rows: CalendarRecord[];
  meeting_members: Array<{ event_id: string; security: CalendarSecurity }>;
  related_meetings: CalendarRecord[];
  detail_errors: Array<{ message: string }>;
  summary: {
    macro: number;
    meeting: number;
    company: number;
    listing: number;
    'major-event'?: number;
    'ipo-announcement'?: number;
    'recent-ipo'?: number;
    'star-news'?: number;
    'chinext-news'?: number;
    'neeq-news'?: number;
    major_events?: number;
    ipo_announcements?: number;
    recent_ipos?: number;
    ipo_subscriptions?: number;
    ipo_subscription_details?: number;
    ipo_companion_news?: number;
    ipo_guidance?: number;
    ipo_guidance_linked_securities?: number;
    ipo_guidance_progress?: Record<string, number>;
    ipo_guidance_boards?: Record<string, number>;
    ipo_guidance_regions?: Record<string, number>;
    ipo_reviews?: number;
    ipo_review_linked_securities?: number;
    ipo_review_statuses?: Record<string, number>;
    ipo_review_boards?: Record<string, number>;
    board_news?: number;
    company_events?: number;
    rights_issue_events?: number;
    rights_issue_stages?: Record<string, number>;
    futures_calendar?: number;
    us_ipo_applications?: number;
    us_ipo_calendar?: number;
    us_ipo_listed?: number;
    us_ipo_pending?: number;
    us_ipo_calendar_listed_code_overlap?: number;
    us_ipo_calendar_listed_same_date?: number;
    us_ipo_calendar_listed_date_changed?: number;
    us_ipo_calendar_pending_code_overlap?: number;
    us_ipo_calendar_pending_same_date?: number;
    us_ipo_issue_amount_usd?: Record<string, number>;
  };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count?: number; endpoint: string }>;
  cache: { refreshed: boolean; age_seconds: number };
}

export interface EmployeeCompanyRecord {
  security: CalendarSecurity;
  net_profit_yuan: number | null;
  liabilities_yuan: number | null;
  employees: number | null;
  previous_employees: number | null;
  employee_change: number | null;
  employee_change_pct: number | null;
  employee_compensation_yuan: number | null;
  rd_expense_yuan: number | null;
  rd_revenue_pct: number | null;
  executive_compensation_yuan: number | null;
  executive_compensation_liability_pct: number | null;
  executive_compensation_employee_pct: number | null;
  compensation_per_employee_yuan: number | null;
  profit_per_employee_yuan: number | null;
  masters_or_above: number | null;
  bachelors_or_above: number | null;
}

export interface ExecutiveRecord {
  name: string;
  gender_age_education: string;
  position: string;
  annual_compensation_yuan: number | null;
  cutoff_date: string;
}

export interface EmployeeSharePlanDates {
  implementation_start: string;
  implementation_end: string;
  duration_start: string;
  duration_end: string;
  lock_start: string;
  lock_end: string;
}

export interface EmployeeSharePlanRecord {
  event_id: string;
  security: CalendarSecurity;
  status: string;
  active: boolean;
  industry: string;
  sort_date: string;
  dates: EmployeeSharePlanDates;
  purchase_average_price: number | null;
  purchase_shares_10k: number | null;
  purchase_shares: number | null;
  purchase_amount_yuan: number | null;
  share_capital_pct: number | null;
  planned_tranches: number | null;
  implemented_tranches: number | null;
  tranche_completion_pct: number | null;
  details: string;
  source_resource: string;
  raw: Record<string, unknown>;
}

export interface MarketEmployeesDocument {
  schema: string;
  generated_at: string;
  mode: 'catalog' | 'security';
  view: 'catalog' | 'share-plans' | 'all';
  sort: string;
  match_count: number;
  returned: number;
  companies: EmployeeCompanyRecord[];
  selected: EmployeeCompanyRecord | null;
  executives: ExecutiveRecord[];
  share_plans: EmployeeSharePlanRecord[];
  share_plan_match_count: number;
  share_plan_returned: number;
  share_plan_summary: {
    plans: number;
    unique_securities: number;
    active_plans: number;
    completed_plans: number;
    purchase_shares: number;
    purchase_amount_yuan: number;
  };
  detail_errors: Array<{ resource: string; message: string }>;
  summary: {
    companies: number;
    employees: number;
    employee_compensation_yuan: number;
    rd_expense_yuan: number;
    employee_growing_companies: number;
    employee_shrinking_companies: number;
  };
  source: { resource: string; size: number; row_count: number; endpoint: string };
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string }>;
  cache: { refreshed: boolean; age_seconds: number };
}

export type HkEventKind =
  | 'dividend'
  | 'holding-disclosure'
  | 'short-selling'
  | 'listing-application';

export interface HkEventDates {
  announcement: string;
  fiscal_year_end: string;
  payment: string;
  ex_dividend: string;
  register_start: string;
  register_end: string;
}

export interface HkEventRecord {
  kind: HkEventKind;
  kind_label: string;
  event_id: string;
  date: string;
  title: string;
  security: CalendarSecurity | null;
  source_resource: string;
  raw: Record<string, unknown>;
  plan?: string;
  dates?: HkEventDates;
  investor?: string;
  changed_shares_10k?: number | null;
  changed_shares?: number | null;
  holding_after_shares_10k?: number | null;
  holding_after_shares?: number | null;
  holding_after_pct?: number | null;
  disclosure_reason?: string;
  position?: string;
  short_shares_10k?: number | null;
  short_shares?: number | null;
  short_amount_10k_currency_units?: number | null;
  short_amount_currency_units?: number | null;
  turnover_10k_currency_units?: number | null;
  turnover_currency_units?: number | null;
  short_turnover_pct?: number | null;
  currency?: string;
  session?: string;
  company?: string;
  filing_date?: string;
  sequence?: string;
  board?: string;
  listing_type?: string;
  status?: string;
  status_date?: string;
  sponsors?: string;
  last_year_revenue_thousand_currency_units?: number | null;
  last_year_revenue_currency_units?: number | null;
  last_year_profit_thousand_currency_units?: number | null;
  last_year_profit_currency_units?: number | null;
  controlling_shareholders?: string;
  business?: string;
}

export interface MarketHkEventsDocument {
  schema: string;
  generated_at: string;
  view: 'all' | 'dividends' | 'holdings' | 'short-selling' | 'applications';
  match_count: number;
  returned: number;
  rows: HkEventRecord[];
  summary: {
    dividends: number;
    holding_disclosures: number;
    short_selling: number;
    listing_applications: number;
    unique_securities: number;
    short_amount_currency_units: number;
    turnover_amount_currency_units: number;
    earliest_date: string;
    latest_date: string;
  };
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string }>;
  cache: { refreshed: boolean; age_seconds: number };
}

export interface HkShortHistoryPoint {
  date: string;
  close_hkd: number | null;
  short_shares: number;
  short_shares_10k: number;
  volume_lots: number | null;
  volume_shares: number | null;
  short_share_volume_pct: number | null;
  short_volume_change_pct: number | null;
  short_shares_ma5: number | null;
  short_shares_ma20: number | null;
  event_short_shares: number | null;
  event_short_amount_hkd: number | null;
  event_turnover_hkd: number | null;
  event_short_turnover_pct: number | null;
  event_exact_match: boolean | null;
}

export interface MarketHkShortHistoryDocument {
  schema: 'tdx-market-hk-short-history-native-v1';
  generated_at: string;
  security: CalendarSecurity;
  summary: {
    history_count: number;
    earliest_date: string;
    latest_date: string;
    latest_close_hkd: number | null;
    latest_short_shares: number;
    latest_short_shares_10k: number;
    latest_volume_shares: number | null;
    latest_short_share_volume_pct: number | null;
    latest_short_shares_ma5: number | null;
    latest_short_shares_ma20: number | null;
  };
  reconciliation: {
    event_row_count: number;
    overlap_day_count: number;
    exact_match_count: number;
    mismatch_count: number;
    event_only_day_count: number;
    history_only_day_count: number;
    all_overlaps_exact: boolean;
  };
  history: HkShortHistoryPoint[];
  source: {
    endpoint?: string;
    server_name?: string;
    transport?: string;
    period?: string;
    period_id?: number;
    auxiliary_field?: string;
    volume_unit?: string;
    market: string;
    code: string;
    volume_lot_size_shares: number;
  };
  event_sources: Array<{ resource: string; size: number; row_count: number; endpoint: string }>;
  event_cache: { refreshed: boolean; age_seconds: number };
  paging: { start: number; page_size: number; pages: number; next_start?: number; has_more?: boolean };
  semantics: string;
}

export interface RoadshowSecurity {
  market: 'sz' | 'sh' | 'bj';
  market_id: 0 | 1 | 2;
  code: string;
  security_id: string;
  name: string | null;
}

export interface RoadshowRecord {
  event_id: string;
  record_id: string | null;
  security: RoadshowSecurity;
  title: string;
  roadshow_type: string | null;
  start_date: string;
  start_time: string | null;
  end_time: string | null;
  summary: string | null;
  url: string | null;
  title_image: string | null;
  plate_code: string | null;
  listing_status_code: string | null;
}

export interface RoadshowDocument {
  schema: 'tdx-roadshows-native-v1';
  availability: 'live' | 'stale-cache';
  generated_at: string;
  mode: 'market' | 'security';
  security: RoadshowSecurity | null;
  filters: {
    q: string | null;
    type: string | null;
    status: string | null;
    start_date: string | null;
    end_date: string | null;
  };
  counts: {
    upstream_rows: number;
    matched: number;
    returned: number;
    offset: number;
    limit: number;
    has_more: boolean;
  };
  types: Array<{ name: string | null; count: number }>;
  records: RoadshowRecord[];
  source: {
    transport: 'TQLEX reqformat=2 KV';
    entry: string;
    request_id: null;
    source_file: string;
    config_present: boolean;
    action: 'get';
    key: string;
    attempts: number;
    template_selection: string;
  };
  cache: {
    hit: boolean;
    stale: boolean;
    age_seconds: number;
    ttl_seconds: number;
    upstream_error: string | null;
  };
}

export type SpecialSituationKind =
  | 'merger'
  | 'b-to-h'
  | 'market-cap-risk'
  | 'major-restructuring-plan'
  | 'major-restructuring-review'
  | 'major-restructuring-completed'
  | 'ordinary-merger-plan'
  | 'neeq-transfer-plan'
  | 'neeq-regulation'
  | 'neeq-transfer-completed';

export interface SpecialSituationQuote {
  market_id: number;
  code: string;
  security_id: string;
  name: string;
  last_price: number | null;
  change_pct: number | null;
  pre_close_price: number | null;
  amount: number | null;
  fund_iopv?: number | null;
}

export interface SpecialSituationRecord {
  kind: SpecialSituationKind;
  kind_label: string;
  event_id: string;
  date: string;
  primary_security: CalendarSecurity;
  related_security: CalendarSecurity | null;
  status: string;
  active: boolean;
  currency: string;
  announcement_url: string;
  description: string;
  source_resource: string;
  raw: Record<string, unknown>;
  primary_quote: SpecialSituationQuote | null;
  related_quote: SpecialSituationQuote | null;
  absorber_exchange_price?: number | null;
  absorbed_cash_option_price?: number | null;
  absorbed_exchange_price?: number | null;
  cash_option_price?: number | null;
  absorber_exchange_premium_pct?: number | null;
  cash_option_premium_pct?: number | null;
  absorbed_exchange_premium_pct?: number | null;
  sample_index?: string;
  sample_indexes?: string[];
  trigger_type?: string;
  twenty_day_change_pct?: number | null;
  one_year_high_price?: number | null;
  high_to_current_change_pct?: number | null;
  live_high_to_current_change_pct?: number | null;
  twenty_day_triggered?: boolean;
  one_year_triggered?: boolean;
  breach_count?: number;
  industry?: string;
  acquiring_party?: string;
  disposing_party?: string;
  transaction_type?: string;
  transaction_amount_yuan?: number | null;
  transaction_amount_100m_yuan?: number | null;
  report_period?: string;
  target_board?: string;
  net_assets_yuan?: number | null;
  net_profit_yuan?: number | null;
  prior_net_profit_yuan?: number | null;
  revenue_yuan?: number | null;
  adviser?: string;
  regulation_reason?: string;
  regulation_measure?: string;
  acceptance_date?: string;
  registration_date?: string;
  listing_date?: string;
  listing_venue_before?: string;
  listing_venue_after?: string;
}

export interface MarketSpecialSituationsDocument {
  schema: string;
  generated_at: string;
  view: 'all' | 'legacy' | 'mergers' | 'b-to-h' | 'market-cap-risk' | 'corporate-actions' | 'neeq-transfers' | 'neeq-regulation';
  mode: 'catalog' | 'security';
  match_count: number;
  returned: number;
  records: SpecialSituationRecord[];
  summary: {
    mergers: number;
    b_to_h: number;
    active_merger_events: number;
    market_cap_warnings: number;
    market_cap_unique_securities: number;
    twenty_day_only: number;
    one_year_only: number;
    both_triggers: number;
    major_restructuring_plans: number;
    major_restructuring_reviews: number;
    major_restructuring_completed: number;
    ordinary_merger_plans: number;
    neeq_transfer_plans: number;
    neeq_regulation_events: number;
    neeq_transfer_completed: number;
    unique_related_securities: number;
  };
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string }>;
  quote_source: { endpoint: string; server_name: string; requested: number; received: number } | null;
  quote_errors: Array<{ resource: string; message: string }>;
  cache: {
    refreshed: boolean;
    age_seconds: number;
    quote_refreshed: boolean;
    quote_age_seconds: number;
  };
}

export type ExchangeFundKind =
  | 'etf-performance'
  | 'etf-share-ranking'
  | 'etf-scale-flow'
  | 'commodity-etf'
  | 'cash-arbitrage'
  | 'cash-yield'
  | 'lof'
  | 'closed-fund'
  | 'cash-management-calendar'
  | 'reit-issued'
  | 'reit-pipeline';

export interface ExchangeFundRecord {
  kind: ExchangeFundKind;
  kind_label: string;
  event_id: string;
  security: CalendarSecurity;
  source_market_id: number;
  source_resource: string;
  snapshot_date: string;
  raw: Record<string, unknown>;
  current_quote: SpecialSituationQuote | null;
  close_price?: number | null;
  turnover_yuan?: number | null;
  turnover_5d_yuan?: number | null;
  reference_close_5d?: number | null;
  reference_close_20d?: number | null;
  reference_close_60d?: number | null;
  reference_close_month?: number | null;
  reference_close_ytd?: number | null;
  change_5d_pct?: number | null;
  change_20d_pct?: number | null;
  change_60d_pct?: number | null;
  change_month_pct?: number | null;
  change_ytd_pct?: number | null;
  live_change_from_snapshot_close_pct?: number | null;
  trade_date?: string;
  next_trade_date?: string;
  settlement_date?: string;
  capital_tieup_days?: number | null;
  seven_day_annualized_pct?: number | null;
  monthly_average_seven_day_pct?: number | null;
  yearly_average_seven_day_pct?: number | null;
  share_change?: number | null;
  latest_shares?: number | null;
  net_inflow_yuan?: number | null;
  buy_redeem_interest_days?: number | null;
  subscribe_sell_interest_days?: number | null;
  theoretical_nav?: number | null;
  premium_pct?: number | null;
  buy_redeem_annualized_pct?: number | null;
  subscribe_sell_annualized_pct?: number | null;
  interest_calculation_type?: string;
  per_10k_yield?: number | null;
  latest_shares_100m?: number | null;
  nav_per_unit?: number | null;
  weekly_share_change?: number | null;
  monthly_share_change?: number | null;
  latest_scale_yuan?: number | null;
  prior_scale_yuan?: number | null;
  prior_week_scale_yuan?: number | null;
  prior_month_scale_yuan?: number | null;
  daily_scale_change_yuan?: number | null;
  weekly_scale_change_yuan?: number | null;
  monthly_scale_change_yuan?: number | null;
  subscription_unit_10k_shares?: number | null;
  subscription_unit_shares?: number | null;
  iopv?: number | null;
  max_subscription_fee_pct?: number | null;
  max_redemption_fee_pct?: number | null;
  reference_instrument?: CalendarSecurity | null;
  reference_change_pct?: number | null;
  dataset_variant?: string;
  current_shares?: number | null;
  new_shares?: number | null;
  equity_ratio_pct?: number | null;
  bond_ratio_pct?: number | null;
  subscription_status?: string;
  maturity_date?: string;
  remaining_years?: number | null;
  aggregate_discount_premium?: number | null;
  aggregate_estimated_value?: number | null;
  discount_pct?: number | null;
  holding_days?: number | null;
  fee_pct?: number | null;
  capital_available_date?: string;
  market_settlement_date?: string;
  capital_withdrawable_date?: string;
  calendar_days?: number | null;
  available_days?: number | null;
  withdrawable_days?: number | null;
  status?: string;
  dates?: {
    updated: string;
    inquiry: string;
    online_subscription_start: string;
    online_subscription_end: string;
    financial_report: string;
  };
  inquiry_price_range?: string;
  inquiry_price_low?: number | null;
  inquiry_price_high?: number | null;
  subscription_price?: number | null;
  term?: string;
  offering_total_units?: number | null;
  strategic_placement_units?: number | null;
  original_owner_subscription_units?: number | null;
  offline_offering_units?: number | null;
  online_offering_units?: number | null;
  project_net_profit_yuan?: number | null;
  project_net_assets_yuan?: number | null;
  project_net_cash_flow_yuan?: number | null;
  project_description?: string;
  live_to_subscription_price_pct?: number | null;
}

export interface MarketExchangeFundsDocument {
  schema: string;
  generated_at: string;
  view: 'all' | 'etf-performance' | 'etf-share-ranking' | 'etf-scale-flow' | 'commodity-etf' |
    'cash-arbitrage' | 'cash-yield' | 'lof' | 'closed-fund' |
    'cash-management-calendar' | 'reits-issued' | 'reits-pipeline';
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: ExchangeFundRecord[];
  summary: {
    etf_performance: number;
    etf_share_ranking: number;
    etf_scale_flow: number;
    commodity_etf: number;
    cash_arbitrage: number;
    cash_yield: number;
    lof: number;
    closed_fund: number;
    cash_management_calendar: number;
    reits_issued: number;
    reits_pipeline: number;
    unique_securities: number;
    daily_turnover_yuan: number;
    five_day_turnover_yuan: number;
    etf_share_ranking_latest_shares: number;
    etf_share_ranking_net_inflow_yuan: number;
    etf_share_ranking_nonzero_net_inflow: number;
    configured_empty_sources: number;
  };
  sources: Array<{
    resource: string;
    size: number;
    row_count: number;
    normalized_row_count: number;
    blank_row_count: number;
    endpoint: string;
  }>;
  quote_source: { endpoint: string; server_name: string; requested: number; received: number } | null;
  quote_errors: Array<{ resource: string; message: string }>;
  cache: {
    refreshed: boolean;
    age_seconds: number;
    quote_refreshed: boolean;
    quote_age_seconds: number;
  };
}

export type CuratedDataKind =
  | 'media-entertainment'
  | 'low-valuation-smallcap'
  | 'dividend-fundraising'
  | 'buyback-statistics'
  | 'high-dividend'
  | 'hk-performance'
  | 'high-refinancing-lending'
  | 'below-book-soe';

export interface CuratedDataRecord {
  kind: CuratedDataKind;
  kind_label: string;
  event_id: string;
  date: string;
  security: CalendarSecurity | null;
  source_resource: string;
  raw: Record<string, unknown>;
  title?: string;
  release_date?: string;
  movie_count?: number | null;
  drama_count?: number | null;
  variety_count?: number | null;
  background?: string;
  background_excerpt?: string;
  rating_date?: string;
  institution_count?: number | null;
  composite_rating?: number | null;
  pe?: number | null;
  forecast_eps_growth_pct?: number | null;
  estimated_peg?: number | null;
  market_cap_yuan?: number | null;
  circulating_market_cap_yuan?: number | null;
  pe_ttm?: number | null;
  target_price?: number | null;
  report_period?: string;
  forecast_profit_lower_yuan?: number | null;
  forecast_profit_upper_yuan?: number | null;
  forecast_profit_growth_pct_lower?: number | null;
  forecast_profit_growth_pct_upper?: number | null;
  cumulative_dividend_100m_yuan?: number | null;
  cumulative_dividend_yuan?: number | null;
  dividend_count?: number | null;
  cumulative_fundraising_100m_yuan?: number | null;
  cumulative_fundraising_yuan?: number | null;
  fundraising_count?: number | null;
  dividend_fundraising_ratio?: number | null;
  dividend_yield_pct?: number | null;
  month?: string;
  planned_buyback_10k_shares?: number | null;
  planned_buyback_shares?: number | null;
  planned_buyback_100m_yuan?: number | null;
  planned_buyback_yuan?: number | null;
  company_count?: number | null;
  annual_profit_yuan?: number | null;
  a_share_market_cap_yuan?: number | null;
  market_cap_ratio_pct?: number | null;
  circulating_market_cap_ratio_pct?: number | null;
  annual_profit_ratio_pct?: number | null;
  fiscal_year_end?: string;
  dividend_yuan?: number | null;
  latest_annual_profit_yuan?: number | null;
  payout_ratio_pct?: number | null;
  snapshot_date?: string;
  close_price_hkd?: number | null;
  turnover_yuan?: number | null;
  change_5d_pct?: number | null;
  turnover_5d_yuan?: number | null;
  change_20d_pct?: number | null;
  change_60d_pct?: number | null;
  change_month_pct?: number | null;
  change_ytd_pct?: number | null;
  trade_date?: string;
  refinancing_lending_balance_10k_yuan?: number | null;
  refinancing_lending_balance_yuan?: number | null;
  balance_ratio_pct?: number | null;
  refinancing_lending_10k_shares?: number | null;
  refinancing_lending_shares?: number | null;
  has_lending_data?: boolean;
  net_assets_yuan?: number | null;
  price_to_book_ratio?: number | null;
  controlling_shareholder?: string;
  controlling_shareholder_nature?: string;
  controlling_shareholder_pct?: number | null;
  actual_controller?: string;
  actual_controller_nature?: string;
  actual_controller_pct?: number | null;
}

export interface MarketCuratedDataDocument {
  schema: string;
  generated_at: string;
  view: 'all' | CuratedDataKind;
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: CuratedDataRecord[];
  summary: {
    media_entertainment: number;
    low_valuation_smallcap: number;
    dividend_fundraising: number;
    buyback_statistics: number;
    high_dividend: number;
    hk_performance: number;
    high_refinancing_lending: number;
    below_book_soe: number;
    unique_securities: number;
    valid_refinancing_lending_rows: number;
    refinancing_lending_latest_date: string;
    earliest_date: string;
    latest_date: string;
  };
  sources: Array<{
    resource: string;
    size: number;
    row_count: number;
    normalized_row_count: number;
    endpoint: string;
  }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type SpecialAttentionKind =
  | 'equity-dispersion'
  | 'st-risk'
  | 'star-cap-removal'
  | 'investigations'
  | 'goodwill-risk';

export interface SpecialAttentionRecord {
  kind: SpecialAttentionKind;
  kind_label: string;
  event_id: string;
  date: string;
  security: CalendarSecurity;
  source_resource: string;
  raw: Record<string, unknown>;
  industry?: string;
  region?: string;
  largest_shareholder?: string;
  largest_shareholder_pct?: number | null;
  cutoff_date?: string;
  risk_type?: string;
  risk_reason?: string;
  reference_close?: number | null;
  report_period?: string;
  shareholder_households?: number | null;
  shareholder_households_10k?: number | null;
  net_profit_yuan?: number | null;
  prior_annual_net_profit_yuan?: number | null;
  net_assets_yuan?: number | null;
  revenue_yuan?: number | null;
  deducted_net_profit_yuan?: number | null;
  risk_type_id?: string;
  current_net_profit_10k_yuan?: number | null;
  current_net_profit_yuan?: number | null;
  price_to_book_ratio?: number | null;
  prior_net_profit_10k_yuan?: number | null;
  prior_net_profit_yuan?: number | null;
  two_years_prior_net_profit_10k_yuan?: number | null;
  two_years_prior_net_profit_yuan?: number | null;
  status_or_forecast?: string;
  implementation_date?: string;
  explanation?: string;
  explanation_excerpt?: string;
  filing_date?: string;
  filing_close?: number | null;
  reason?: string;
  case_detail?: string;
  case_detail_excerpt?: string;
  progress?: string;
  penalty_date?: string;
  occurrence_count_10y?: number | null;
  source_event_key?: string;
  active?: boolean;
  goodwill_current_yuan?: number | null;
  goodwill_prior_yuan?: number | null;
  goodwill_change_yuan?: number | null;
  goodwill_change_pct?: number | null;
  goodwill_to_net_profit_pct?: number | null;
  goodwill_to_total_assets_pct?: number | null;
  net_profit_current_yuan?: number | null;
  net_profit_prior_yuan?: number | null;
  net_profit_growth_pct?: number | null;
  revenue_current_yuan?: number | null;
  revenue_prior_yuan?: number | null;
  revenue_growth_pct?: number | null;
}

export interface MarketSpecialAttentionDocument {
  schema: 'tdx-market-special-attention-native-v1';
  generated_at: string;
  view: 'all' | SpecialAttentionKind;
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: SpecialAttentionRecord[];
  summary: {
    equity_dispersion: number;
    st_risk: number;
    star_cap_removal: number;
    investigations: number;
    goodwill_risk: number;
    active_investigations: number;
    unique_securities: number;
    earliest_date: string;
    latest_date: string;
  };
  sources: Array<{
    resource: string;
    size: number;
    row_count: number;
    normalized_row_count: number;
    endpoint: string;
  }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type FundStatisticsKind =
  | 'new-funds'
  | 'fund-dividends'
  | 'equity-fund-performance'
  | 'fund-market-size'
  | 'fund-market-size-chart'
  | 'etf-market-size'
  | 'etf-subscription-chart'
  | 'etf-weekly'
  | 'listed-funds';

export interface FundStatisticsRecord {
  kind: FundStatisticsKind;
  kind_label: string;
  event_id: string;
  date: string;
  security: CalendarSecurity | null;
  source_resource: string;
  raw: Record<string, unknown>;
  offering_start?: string;
  offering_end?: string;
  subscription_fee_pct?: number | null;
  minimum_subscription_amount?: number | null;
  purchase_fee_pct?: number | null;
  minimum_purchase_units?: number | null;
  redemption_fee_pct?: number | null;
  minimum_redemption_units?: number | null;
  fund_class?: string;
  product_type?: string;
  tracking_index_code?: string;
  tracking_index_name?: string;
  manager?: string;
  fund_managers?: string;
  announcement_date?: string;
  record_date?: string;
  ex_dividend_date?: string;
  payment_date?: string;
  unit_nav?: number | null;
  cumulative_nav?: number | null;
  distribution_ratio_raw?: number | null;
  distribution_description?: string;
  snapshot_date?: string;
  return_1m_pct?: number | null;
  return_3m_pct?: number | null;
  return_6m_pct?: number | null;
  return_1y_pct?: number | null;
  return_ytd_pct?: number | null;
  all_fund_count?: number | null;
  fund_company_count?: number | null;
  all_fund_units_100m?: number | null;
  all_fund_units?: number | null;
  all_fund_nav_100m_yuan?: number | null;
  all_fund_nav_yuan?: number | null;
  equity_holdings_100m_yuan?: number | null;
  equity_holdings_yuan?: number | null;
  open_fund_count?: number | null;
  open_fund_units?: number | null;
  open_fund_nav_yuan?: number | null;
  closed_fund_count?: number | null;
  closed_fund_units?: number | null;
  closed_fund_nav_yuan?: number | null;
  sh_market_size_yuan?: number | null;
  sz_market_size_yuan?: number | null;
  total_market_size_yuan?: number | null;
  sh_net_subscription_units?: number | null;
  sz_net_subscription_units?: number | null;
  total_net_subscription_units?: number | null;
  sh_composite_close?: number | null;
  sh_composite_change_pct?: number | null;
  week_end_date?: string;
  turnover_yuan?: number | null;
  turnover_change_yuan?: number | null;
  total_units?: number | null;
  total_units_change?: number | null;
  financing_balance_yuan?: number | null;
  financing_balance_change_yuan?: number | null;
  securities_lending_units?: number | null;
  securities_lending_change_units?: number | null;
  sh_composite_weekly_change_pct?: number | null;
  listing_date?: string;
  listing_announcement_date?: string;
  raised_units?: number | null;
  listed_tradable_units?: number | null;
  listing_day_nav?: number | null;
  holder_households?: number | null;
  investment_style?: string;
}

export interface MarketFundStatisticsDocument {
  schema: 'tdx-market-fund-statistics-native-v1';
  generated_at: string;
  view: 'all' | FundStatisticsKind;
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: FundStatisticsRecord[];
  summary: {
    new_funds: number;
    fund_dividends: number;
    equity_fund_performance: number;
    fund_market_size: number;
    fund_market_size_chart: number;
    etf_market_size: number;
    etf_subscription_chart: number;
    etf_weekly: number;
    listed_funds: number;
    unique_funds: number;
    earliest_date: string;
    latest_date: string;
  };
  sources: Array<{
    resource: string;
    size: number;
    row_count: number;
    normalized_row_count: number;
    endpoint: string;
  }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type SpecializedMetricsKind = 'banks' | 'securities' | 'insurers';

export interface SpecializedMetricsRecord {
  kind: SpecializedMetricsKind;
  kind_label: string;
  event_id: string;
  date: string;
  security: CalendarSecurity;
  source_resource: string;
  raw: Record<string, unknown>;
  report_period?: string;
  capital_net_yuan?: number | null;
  capital_adequacy_ratio_pct?: number | null;
  core_tier1_capital_net_yuan?: number | null;
  core_tier1_adequacy_ratio_pct?: number | null;
  tier1_capital_net_yuan?: number | null;
  tier1_adequacy_ratio_pct?: number | null;
  deposits_yuan?: number | null;
  loans_yuan?: number | null;
  loan_to_deposit_ratio_pct?: number | null;
  nonperforming_loan_ratio_pct?: number | null;
  loan_loss_reserve_yuan?: number | null;
  provision_coverage_ratio_pct?: number | null;
  net_interest_margin_ratio_pct?: number | null;
  net_interest_spread_ratio_pct?: number | null;
  interest_income_yuan?: number | null;
  interest_expense_yuan?: number | null;
  monthly_revenue_yuan?: number | null;
  prior_monthly_revenue_yuan?: number | null;
  monthly_revenue_yoy_pct?: number | null;
  monthly_net_profit_yuan?: number | null;
  prior_monthly_net_profit_yuan?: number | null;
  monthly_net_profit_yoy_pct?: number | null;
  net_assets_yuan?: number | null;
  net_capital_yuan?: number | null;
  net_capital_to_liabilities_ratio_pct?: number | null;
  proprietary_equity_to_net_capital_ratio_pct?: number | null;
  proprietary_fixed_income_to_net_capital_ratio_pct?: number | null;
  commission_income_yuan?: number | null;
  brokerage_income_yuan?: number | null;
  underwriting_income_yuan?: number | null;
  asset_management_income_yuan?: number | null;
  entrusted_asset_scale_yuan?: number | null;
  embedded_value_yuan?: number | null;
  adjusted_net_assets_yuan?: number | null;
  new_business_value_after_cost_yuan?: number | null;
  core_solvency_adequacy_ratio_pct?: number | null;
  combined_solvency_adequacy_ratio_pct?: number | null;
  persistency_13m_ratio_pct?: number | null;
  persistency_25m_ratio_pct?: number | null;
  surrender_ratio_pct?: number | null;
  combined_cost_ratio_pct?: number | null;
  loss_ratio_pct?: number | null;
  net_investment_yield_ratio_pct?: number | null;
  total_investment_yield_ratio_pct?: number | null;
  total_investments_yuan?: number | null;
  fixed_income_investments_yuan?: number | null;
  equity_investments_yuan?: number | null;
  stock_investments_yuan?: number | null;
  fund_investments_yuan?: number | null;
}

export interface MarketSpecializedMetricsDocument {
  schema: 'tdx-market-specialized-metrics-native-v1';
  generated_at: string;
  view: 'all' | SpecializedMetricsKind;
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: SpecializedMetricsRecord[];
  summary: {
    banks: number;
    securities: number;
    insurers: number;
    unique_securities: number;
    earliest_date: string;
    latest_date: string;
  };
  sources: Array<{
    resource: string;
    size: number;
    row_count: number;
    normalized_row_count: number;
    endpoint: string;
  }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type CompanyChangeKind =
  | 'security-renames' | 'company-renames' | 'mainland-index'
  | 'major-equity' | 'hk-index' | 'controllers' | 'industries'
  | 'equity-transfers' | 'neeq-index';

export interface CompanyChangeRecord {
  kind: CompanyChangeKind;
  kind_label: string;
  event_id: string;
  event_date: string;
  security: CalendarSecurity;
  source_resource: string;
  source_rank: number;
  raw?: Record<string, unknown>;
  announcement_date?: string;
  effective_date?: string;
  change_date?: string;
  updated_date?: string;
  old_name?: string;
  new_name?: string;
  rename_count?: number;
  history?: string;
  direction?: string;
  index_name?: string;
  seller?: string;
  buyer?: string;
  shares_10k?: number | null;
  shares?: number | null;
  transfer_shares?: number | null;
  total_amount_yuan?: number | null;
  transfer_price_yuan?: number | null;
  total_share_pct?: number | null;
  seller_post_transfer_pct?: number | null;
  buyer_post_transfer_pct?: number | null;
  method?: string;
  details?: string;
  status?: string;
  before_controller?: string;
  after_controller?: string;
  ownership_chain?: string;
  industry?: string;
  before_industry?: string;
  after_industry?: string;
  cross_industry?: string;
  business_change?: string;
  buyer_controller?: string;
  buyer_type?: string;
  controller_changed?: string;
  announcement_close?: number | null;
  effective_close?: number | null;
  pre_change_close?: number | null;
  pre_announcement_close?: number | null;
}

export interface MarketCompanyChangesDocument {
  schema: 'tdx-market-company-changes-native-v1';
  generated_at: string;
  view: 'all' | CompanyChangeKind;
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: CompanyChangeRecord[];
  summary: Record<CompanyChangeKind, number> & {
    unique_securities: number;
    earliest_date: string;
    latest_date: string;
  };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type FinancialScreenBoard = 'sh-main' | 'sz-main' | 'chinext' | 'star' | 'beijing';
export interface FinancialScreenRecord {
  dataset: 'snapshot' | 'small-cap-growth';
  board: FinancialScreenBoard;
  board_label: string;
  record_id: string;
  security: CalendarSecurity;
  report_period: string;
  dividend_year: string;
  market_cap_source_10m_yuan: number | null;
  market_cap_yuan: number | null;
  pe_ttm: number | null;
  pb_mrq: number | null;
  ps_ttm: number | null;
  peg: number | null;
  debt_ratio_pct: number | null;
  net_profit_yoy_pct: number | null;
  adjusted_net_profit_yoy_pct: number | null;
  adjusted_net_profit_yoy_t_minus_1_pct: number | null;
  adjusted_net_profit_yoy_t_minus_2_pct: number | null;
  adjusted_net_profit_yoy_t_minus_3_pct: number | null;
  adjusted_net_profit_cagr_3y_pct: number | null;
  revenue_yoy_pct: number | null;
  revenue_yoy_t_minus_1_pct: number | null;
  revenue_yoy_t_minus_2_pct: number | null;
  revenue_yoy_t_minus_3_pct: number | null;
  revenue_cagr_3y_pct: number | null;
  contract_liability_yuan: number | null;
  prior_contract_liability_yuan: number | null;
  contract_liability_yoy_pct: number | null;
  roe_pct: number | null;
  gross_margin_pct: number | null;
  rd_to_revenue_pct: number | null;
  selling_to_revenue_pct: number | null;
  admin_to_revenue_pct: number | null;
  finance_to_revenue_pct: number | null;
  inventory_turnover_source: number | null;
  current_asset_turnover_source: number | null;
  institution_float_holding_pct: number | null;
  dividend_yield_pct: number | null;
  source_resource: string;
  raw?: Record<string, unknown>;
}
export interface MarketFinancialScreenDocument {
  schema: 'tdx-market-financial-screen-native-v1';
  generated_at: string;
  dataset: 'snapshot' | 'small-cap-growth';
  view: 'all' | FinancialScreenBoard;
  sort: 'market-cap' | 'pe' | 'pb' | 'roe' | 'revenue-growth' | 'profit-growth' | 'dividend-yield' | 'profit-cagr' | 'revenue-cagr' | 'code';
  order: 'asc' | 'desc';
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: FinancialScreenRecord[];
  summary: Record<FinancialScreenBoard, number> & { report_period_count: number; latest_report_period: string };
  projection_reconciliation: {
    master_unique_count: number;
    chinext: { master_count: number; projection_count: number; exact_match: boolean };
    star: { master_count: number; projection_count: number; exact_match: boolean };
  } | null;
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type EquityPerformanceSort =
  | 'return-5d' | 'return-20d' | 'return-60d' | 'month' | 'ytd'
  | 'turnover-day' | 'turnover-5d' | 'pe' | 'close' | 'code';
export interface EquityPerformanceRecord {
  record_id: string;
  security: CalendarSecurity;
  quote_date: string;
  close: number | null;
  prior_month_close: number | null;
  daily_turnover_yuan: number | null;
  five_day_turnover_yuan: number | null;
  return_5d_pct: number | null;
  return_20d_pct: number | null;
  return_60d_pct: number | null;
  month_to_date_pct: number | null;
  year_to_date_pct: number | null;
  pe: number | null;
  source_resource: string;
  raw?: Record<string, unknown>;
}
export interface MarketEquityPerformanceDocument {
  schema: 'tdx-market-equity-performance-native-v1';
  generated_at: string;
  sort: EquityPerformanceSort;
  order: 'asc' | 'desc';
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: EquityPerformanceRecord[];
  summary: { sh: number; sz: number; bj: number; quote_date_count: number; latest_quote_date: string };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type CorporateOrderKind = 'tender' | 'major-contract';
export interface CorporateOrderRecord {
  record_id: string;
  kind: CorporateOrderKind;
  kind_label: string;
  security: CalendarSecurity;
  announcement_date: string;
  stage: string;
  title: string;
  source_url: string;
  amount_yuan: number | null;
  revenue_yuan: number | null;
  source_revenue_share_pct: number | null;
  calculated_revenue_share_pct: number | null;
  revenue_share_formula_matches: boolean | null;
  non_recurring_profit_yuan: number | null;
  source_resource: string;
  raw?: Record<string, unknown>;
}
export interface MarketCorporateOrdersDocument {
  schema: 'tdx-market-corporate-orders-native-v1';
  generated_at: string;
  view: 'all' | 'tenders' | 'contracts';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: CorporateOrderRecord[];
  summary: { tenders: number; major_contracts: number; unique_securities: number; latest_date: string; formula_checked: number; formula_mismatches: number };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
}

export interface EventImpactRecord {
  record_id: string;
  benchmark: 'shanghai-composite' | 'hang-seng' | 'nasdaq-composite';
  benchmark_label: string;
  event_type: string;
  description: string;
  start_date: string;
  end_date: string;
  closes: { week_before_start: number | null; day_before_start: number | null; start_day: number | null; end_day: number | null; day_after_end: number | null; week_after_end: number | null };
  impacts: { start_day_pct: number | null; week_before_start_pct: number | null; event_interval_pct: number | null; day_after_end_pct: number | null; week_after_end_pct: number | null };
  source_resource: string;
  raw?: Record<string, unknown>;
}
export interface MarketEventImpactDocument {
  schema: 'tdx-market-event-impact-native-v1';
  generated_at: string;
  benchmark: string;
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: EventImpactRecord[];
  summary: { shanghai_composite: number; hang_seng: number; nasdaq_composite: number; event_types: number; earliest_date: string; latest_date: string };
  sources: Array<{ resource: string; row_count: number; normalized_row_count: number; endpoint: string }>;
}

export interface GlobalPerformanceRecord {
  record_id: string;
  kind: 'major-index' | 'overseas-china';
  kind_label: string;
  instrument: CalendarSecurity;
  quote_date: string;
  close: number | null;
  daily_turnover: number | null;
  five_day_turnover: number | null;
  pe: number | null;
  return_5d_pct: number | null;
  return_20d_pct: number | null;
  return_60d_pct: number | null;
  month_to_date_pct: number | null;
  year_to_date_pct: number | null;
  source_resource: string;
  raw?: Record<string, unknown>;
}
export interface MarketGlobalPerformanceDocument {
  schema: 'tdx-market-global-performance-native-v1';
  generated_at: string;
  view: 'all' | 'major-indices' | 'overseas-china';
  sort: EquityPerformanceSort;
  order: 'asc' | 'desc';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: GlobalPerformanceRecord[];
  summary: { major_indices: number; overseas_china: number; unresolved_names: number; latest_quote_date: string };
  sources: Array<{ resource: string; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
}

export type ShareholderSignalKind = 'notable-investors' | 'institution-accumulation' | 'research-growth' | 'small-cap-institution';
export type ShareholderSignalsSort =
  | 'signal' | 'holding-value' | 'institution-growth' | 'holder-change'
  | 'research-6m' | 'profit-growth' | 'return-6m' | 'code';
export interface ShareholderSignalRecord {
  kind: ShareholderSignalKind;
  kind_label: string;
  record_id: string;
  security: CalendarSecurity;
  report_period: string;
  signal_value: number | null;
  source_resource: string;
  raw?: Record<string, unknown>;
  notable_investor_count?: number | null;
  holding_shares?: number | null;
  total_shares?: number | null;
  report_close?: number | null;
  pe_ttm?: number | null;
  holding_pct?: number | null;
  holding_value_yuan?: number | null;
  institution_holding_shares?: number | null;
  prior_institution_holding_shares?: number | null;
  institution_holding_change_shares?: number | null;
  institution_holding_growth_ratio?: number | null;
  institution_holding_growth_pct?: number | null;
  report_float_market_cap_yuan?: number | null;
  institution_holding_value_yuan?: number | null;
  institution_float_holding_pct?: number | null;
  institution_holding_value_change_yuan?: number | null;
  institution_holding_value_change_pct?: number | null;
  institution_count_source?: number | null;
  institution_count_change_source?: number | null;
  institution_count_change_pct?: number | null;
  shareholder_count?: number | null;
  prior_shareholder_count?: number | null;
  shareholder_count_change?: number | null;
  shareholder_count_change_ratio?: number | null;
  shareholder_count_change_pct?: number | null;
  top10_float_holding_shares?: number | null;
  top10_float_holding_pct?: number | null;
  top10_holding_shares?: number | null;
  top10_holding_pct?: number | null;
  latest_research_date?: string;
  net_profit_yuan?: number | null;
  prior_net_profit_yuan?: number | null;
  net_profit_growth_pct?: number | null;
  research_count_1m?: number | null;
  research_count_3m?: number | null;
  research_count_6m?: number | null;
  institutions_last?: number | null;
  institutions_1m?: number | null;
  institutions_3m?: number | null;
  institutions_6m?: number | null;
  return_1m_pct?: number | null;
  return_3m_pct?: number | null;
  return_6m_pct?: number | null;
}
export interface NotableInvestorDirectoryRecord {
  kind: 'investor-directory';
  kind_label: string;
  record_id: string;
  investor_id: string;
  investor_name: string;
  detail_url?: string;
  holding_company_count: number | null;
  holding_value_yuan: number | null;
  holding_shares: number | null;
  as_of_date: string;
  source_resource: string;
  raw?: Record<string, unknown>;
}
export interface NotableInvestorHoldingRecord {
  kind: 'investor-holding';
  kind_label: string;
  record_id: string;
  investor_id: string;
  investor_name: string;
  security: CalendarSecurity;
  current_rank: number | null;
  prior_rank: number | null;
  holding_shares: number | null;
  prior_holding_shares: number | null;
  holding_change_shares: number | null;
  holding_value_yuan: number | null;
  prior_holding_value_yuan: number | null;
  holding_value_change_yuan: number | null;
  holding_pct: number | null;
  prior_holding_pct: number | null;
  holding_pct_change: number | null;
  latest_date: string;
  source_type: string;
  source_resource: string;
  raw?: Record<string, unknown>;
}
export interface MarketShareholderSignalsDocument {
  schema: 'tdx-market-shareholder-signals-native-v1';
  generated_at: string;
  view: 'all' | ShareholderSignalKind | 'investor-directory';
  sort: ShareholderSignalsSort;
  order: 'asc' | 'desc';
  mode: 'catalog' | 'security' | 'investor-directory' | 'investor';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: ShareholderSignalRecord[];
  summary: Record<ShareholderSignalKind, number> & {
    unique_securities: number;
    investor_directory?: number;
    investor_directory_matched?: number;
    investor_holdings?: number;
  };
  investor_directory?: NotableInvestorDirectoryRecord[];
  selected_investor?: NotableInvestorDirectoryRecord | null;
  investor_holdings?: NotableInvestorHoldingRecord[];
  investor_detail_source?: { resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string } | null;
  investor_reconciliation?: {
    expected_company_count: number | null;
    actual_company_count: number;
    company_count_matches: boolean;
    expected_holding_shares: number | null;
    actual_holding_shares: number;
    holding_shares_match: boolean;
    expected_holding_value_yuan: number | null;
    actual_holding_value_yuan: number;
    holding_value_matches: boolean;
  };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type FinancialInsightKind =
  | 'buffett-quality' | 'high-bonus-potential' | 'investment-property'
  | 'low-price-sales' | 'dividend-shortfall' | 'equity-investment'
  | 'cash-above-market-cap' | 'high-receivables' | 'profit-warning'
  | 'cash-flow-quality' | 'earnings-reversal' | 'steady-growth'
  | 'quality-growth' | 'profit-breakout' | 'dividend-plan';
export interface FinancialInsightRecord {
  kind: FinancialInsightKind;
  kind_label: string;
  record_id: string;
  security: CalendarSecurity;
  report_period?: string;
  source_resource: string;
  signal_value: number | null;
  amount_value_yuan: number | null;
  ratio_value_pct: number | null;
  raw?: Record<string, unknown>;
  net_profit_yuan?: number | null;
  pe?: number | null;
  roe_pct?: number | null;
  roe_qualified_years?: number | null;
  listing_date?: string;
  capital_reserve_per_share_yuan?: number | null;
  undistributed_profit_per_share_yuan?: number | null;
  parent_net_profit_yuan?: number | null;
  parent_net_profit_yoy_pct?: number | null;
  latest_share_capital_shares?: number | null;
  forecast_type?: string;
  forecast_detail?: string;
  investment_property_yuan?: number | null;
  investment_property_change_yuan?: number | null;
  investment_property_qoq_pct?: number | null;
  investment_property_yoy_pct?: number | null;
  expected_revenue_cagr_3y_pct?: number | null;
  expected_net_margin_pct?: number | null;
  price_to_sales?: number | null;
  price_to_rd?: number | null;
  market_cap_yuan?: number | null;
  pb?: number | null;
  price_to_cash_flow?: number | null;
  net_margin_pct?: number | null;
  latest_dividend_yuan?: number | null;
  latest_net_profit_yuan?: number | null;
  undistributed_profit_yuan?: number | null;
  dividend_3y_yuan?: number | null;
  average_profit_3y_yuan?: number | null;
  dividend_to_average_profit_3y_pct?: number | null;
  rd_investment_3y_yuan?: number | null;
  revenue_3y_yuan?: number | null;
  rd_to_revenue_3y_pct?: number | null;
  a_share_investment_count?: number | null;
  other_investment_count?: number | null;
  investment_total_yuan?: number | null;
  investment_return_yuan?: number | null;
  investment_return_pct?: number | null;
  cash_yuan?: number | null;
  cash_excess_yuan?: number | null;
  cash_to_market_cap_pct?: number | null;
  debt_ratio_pct?: number | null;
  dividend_yield_pct?: number | null;
  cumulative_dividend_yuan?: number | null;
  cumulative_raised_yuan?: number | null;
  dividend_to_fundraising_ratio?: number | null;
  institutions_6m?: number | null;
  consensus_rating?: number | null;
  receivables_yuan?: number | null;
  receivables_to_market_cap_pct?: number | null;
  report_type?: string;
  net_assets_yuan?: number | null;
  operating_revenue_yuan?: number | null;
  actual_profit_yuan?: number | null;
  actual_profit_positive?: boolean;
  forecast_period?: string;
  forecast_profit_lower_yuan?: number | null;
  forecast_profit_upper_yuan?: number | null;
  forecast_profit_midpoint_yuan?: number | null;
  forecast_disclosure_date?: string;
  has_forecast?: boolean;
  free_cash_flow_yuan?: number | null;
  operating_cash_flow_per_share_yuan?: number | null;
  operating_cash_flow_yuan?: number | null;
  operating_cash_to_short_debt_pct?: number | null;
  operating_cash_to_net_profit_pct?: number | null;
  current_ratio_source?: number | null;
  quick_ratio_source?: number | null;
  interest_coverage_ratio?: number | null;
  inventory_to_current_assets_pct?: number | null;
  screen_criteria_complete?: boolean;
  screen_criteria?: Record<string, boolean>;
  consensus_date?: string;
  consensus_institutions?: number | null;
  target_price_yuan?: number | null;
  disclosed_net_profit_yuan?: number | null;
  forecast_net_profit_yuan?: number | null;
  profit_growth_pct?: number | null;
  profit_growth_recalculated_pct?: number | null;
  disclosed_revenue_yuan?: number | null;
  forecast_revenue_yuan?: number | null;
  revenue_growth_pct?: number | null;
  profit_reversal?: boolean;
  three_year_reference_close_yuan?: number | null;
  listed_since_return_pct?: number | null;
  cumulative_dividend_100m_yuan?: number | null;
  dividend_count?: number | null;
  cumulative_net_profit_yuan?: number | null;
  dividend_payout_pct?: number | null;
  beta?: number | null;
  revenue_t_minus_2_yuan?: number | null;
  revenue_t_minus_1_yuan?: number | null;
  revenue_t_yuan?: number | null;
  revenue_growth_t_minus_1_pct?: number | null;
  revenue_growth_t_pct?: number | null;
  selling_expense_rate_t_minus_2_pct?: number | null;
  selling_expense_rate_t_minus_1_pct?: number | null;
  selling_expense_rate_t_pct?: number | null;
  gross_margin_t_minus_2_pct?: number | null;
  gross_margin_t_minus_1_pct?: number | null;
  gross_margin_t_pct?: number | null;
  rd_expense_t_minus_2_yuan?: number | null;
  rd_expense_t_minus_1_yuan?: number | null;
  rd_expense_t_yuan?: number | null;
  rd_growth_t_minus_1_pct?: number | null;
  rd_growth_t_pct?: number | null;
  prior_period?: string;
  comparison_period?: string;
  latest_or_forecast_profit_yuan?: number | null;
  prior_period_adjusted_profit_yuan?: number | null;
  comparison_adjusted_profit_yuan?: number | null;
  profit_yoy_change_yuan?: number | null;
  profit_yoy_growth_pct?: number | null;
  profit_breakout_change_yuan?: number | null;
  profit_breakout_growth_pct?: number | null;
  announcement_date?: string;
  record_date?: string;
  ex_dividend_date?: string;
  plan_stage?: string;
  industry?: string;
  stock_transfer_per_10_shares?: number | null;
  stock_transfer_per_share?: number | null;
  cash_dividend_per_10_shares_yuan?: number | null;
  cash_dividend_per_share_yuan?: number | null;
  return_1w_pct?: number | null;
  return_1m_pct?: number | null;
  return_3m_pct?: number | null;
}
export interface MarketFinancialInsightsDocument {
  schema: 'tdx-market-financial-insights-native-v1';
  generated_at: string;
  view: 'all' | FinancialInsightKind;
  sort: 'signal' | 'amount' | 'ratio' | 'pe' | 'roe' | 'code';
  order: 'asc' | 'desc';
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: FinancialInsightRecord[];
  summary: Record<FinancialInsightKind, number> & { unique_securities: number };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export interface GdrRecord {
  record_id: string;
  gdr_code: string;
  gdr_name: string;
  quote_date: string;
  gdr_price: number | null;
  currency: string;
  listing_location: string;
  issuance_units: number | null;
  conversion_ratio: number | null;
  underlying: CalendarSecurity;
  underlying_price: number | null;
  converted_gdr_price: number | null;
  premium_pct: number | null;
  source_resource: string;
  raw?: Record<string, unknown>;
}
export interface MarketGdrDocument {
  schema: 'tdx-market-gdr-native-v1';
  generated_at: string;
  sort: 'date' | 'premium' | 'issuance' | 'price' | 'underlying-price' | 'code';
  order: 'asc' | 'desc';
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: GdrRecord[];
  summary: { unique_underlyings: number; currency_count: number; listing_location_count: number; latest_quote_date: string };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export interface FundCalendarRecord {
  event_id: string;
  security: CalendarSecurity;
  event_type: string;
  event_date: string;
  content: string;
  fund_category: string;
  source_resource: string;
  source_rank: number;
  raw?: Record<string, unknown>;
}
export interface MarketFundCalendarDocument {
  schema: 'tdx-market-fund-calendar-native-v1';
  generated_at: string;
  order: 'asc' | 'desc';
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: FundCalendarRecord[];
  summary: {
    unique_funds: number;
    event_type_count: number;
    category_count: number;
    earliest_date: string;
    latest_date: string;
    event_types: Record<string, number>;
    categories: Record<string, number>;
  };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type RecentWatchKind = 'earnings-divergence' | 'foreign-business' | 'st-turnaround';
export interface RecentWatchRecord {
  kind: RecentWatchKind;
  kind_label: string;
  event_id: string;
  event_date: string;
  security: CalendarSecurity;
  source_resource: string;
  source_rank: number;
  raw?: Record<string, unknown>;
  announcement_date?: string;
  report_period?: string;
  forecast_type?: string;
  return_since_announcement_pct?: number | null;
  forecast_profit_yoy_pct?: number | null;
  actual_profit_yoy_pct?: number | null;
  reason?: string;
  revenue_yuan?: number | null;
  cost_yuan?: number | null;
  profit_yuan?: number | null;
  foreign_revenue_yuan?: number | null;
  foreign_cost_yuan?: number | null;
  foreign_profit_yuan?: number | null;
  foreign_revenue_pct?: number | null;
  foreign_cost_pct?: number | null;
  foreign_profit_pct?: number | null;
  effect?: string;
  profit_lower_yuan?: number | null;
  profit_upper_yuan?: number | null;
  growth_lower_pct?: number | null;
  growth_upper_pct?: number | null;
}
export interface MarketRecentWatchDocument {
  schema: 'tdx-market-recent-watch-native-v1';
  generated_at: string;
  view: 'all' | RecentWatchKind;
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: RecentWatchRecord[];
  summary: Record<RecentWatchKind, number> & { unique_securities: number };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export interface PatentStatisticsRecord {
  security: CalendarSecurity;
  report_date: string;
  period_application_invention: number | null;
  period_application_utility_model: number | null;
  period_application_design: number | null;
  period_application_total: number | null;
  period_application_classified_total: number | null;
  period_application_total_delta: number | null;
  period_grant_invention: number | null;
  period_grant_utility_model: number | null;
  period_grant_design: number | null;
  period_grant_total: number | null;
  period_grant_classified_total: number | null;
  period_grant_total_delta: number | null;
  cumulative_grant_invention: number | null;
  cumulative_grant_utility_model: number | null;
  cumulative_grant_design: number | null;
  cumulative_grant_total: number | null;
  cumulative_grant_classified_total: number | null;
  cumulative_grant_total_delta: number | null;
  source_resource: string;
  source_rank: number;
  raw?: Record<string, unknown>;
}

export interface MarketPatentStatisticsDocument {
  schema: 'tdx-market-patent-statistics-native-v1';
  generated_at: string;
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  sort: 'report-date' | 'period-applications' | 'period-grants' | 'cumulative-total' | 'cumulative-invention' | 'code';
  order: 'asc' | 'desc';
  records: PatentStatisticsRecord[];
  summary: {
    unique_securities: number;
    sz: number;
    sh: number;
    bj: number;
    first_report_date: string;
    last_report_date: string;
    records_with_period_applications: number;
    records_with_period_grants: number;
    source_total_below_classified_count: number;
  };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type OverviewFactorSignal = 'positive' | 'neutral' | 'negative' | 'unrated';

export interface OverviewFactorRecord {
  factor_id: string;
  name: string;
  description: string;
  signal: OverviewFactorSignal;
  signal_label: string;
  chart_indicator: string;
  source_rank: number;
  source_resource: string;
  raw?: Record<string, unknown>;
}

export interface MarketOverviewFactorsDocument {
  schema: 'tdx-market-overview-factors-native-v1';
  generated_at: string;
  signal: 'all' | OverviewFactorSignal;
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: OverviewFactorRecord[];
  summary: Record<OverviewFactorSignal, number>;
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export type BenchmarkAnalysisKind = 'stocks' | 'industries' | 'suspensions' | 'new-stocks';
export interface BenchmarkAnalysisRecord {
  kind: BenchmarkAnalysisKind;
  kind_label: string;
  event_id: string;
  date: string;
  security: CalendarSecurity;
  source_resource: string;
  raw?: Record<string, unknown>;
  stage_id?: string;
  stage_start_date?: string;
  stage_end_date?: string;
  stage_open?: boolean;
  stage_start_anchor?: number;
  stage_end_anchor?: number | null;
  security_start_price?: number | null;
  security_end_price?: number | null;
  security_return_pct?: number | null;
  market_return_pct?: number | null;
  industry_return_pct?: number | null;
  excess_market_pct?: number | null;
  excess_industry_pct?: number | null;
  recent_1m_pct?: number | null;
  recent_3m_pct?: number | null;
  year_to_date_pct?: number | null;
  suspension_date?: string;
  resumption_date?: string;
  suspension_trading_days?: number | null;
  reason?: string;
  industry_return_during_suspension_pct?: number | null;
  market_return_during_suspension_pct?: number | null;
  issue_price?: number | null;
  return_3m_pct?: number | null;
  market_return_3m_pct?: number | null;
  industry_return_3m_pct?: number | null;
  excess_market_3m_pct?: number | null;
  excess_industry_3m_pct?: number | null;
  return_1m_pct?: number | null;
  excess_market_1m_pct?: number | null;
  excess_industry_1m_pct?: number | null;
  return_1w_pct?: number | null;
  excess_market_1w_pct?: number | null;
  excess_industry_1w_pct?: number | null;
}
export interface MarketBenchmarkAnalysisDocument {
  schema: 'tdx-market-benchmark-analysis-native-v1';
  generated_at: string;
  view: 'all' | BenchmarkAnalysisKind;
  stage: string;
  mode: 'catalog' | 'security';
  availability: 'live' | 'empty';
  match_count: number;
  returned: number;
  records: BenchmarkAnalysisRecord[];
  summary: { stocks: number; industries: number; suspensions: number; new_stocks: number; stages: number; unique_entities: number };
  sources: Array<{ resource: string; size: number; row_count: number; normalized_row_count: number; endpoint: string }>;
  semantics: string;
  cache: { refreshed: boolean; age_seconds: number };
}

export interface ThresholdHistoryRecord {
  universe: 'high-price' | 'mega-cap';
  date: string;
  total_count: number | null;
  index_change_pct: {
    shanghai_composite_change_pct: number | null;
    chinext_change_pct: number | null;
    star_50_change_pct: number | null;
  };
  hundred_to_thousand_yuan_count?: number | null;
  thousand_yuan_or_more_count?: number | null;
  trillion_yuan_market_cap_count?: number | null;
  hundred_billion_to_trillion_yuan_count?: number | null;
  aggregate_market_cap_yuan: number | null;
  aggregate_market_share_pct: number | null;
  entered_count: number;
  exited_count: number;
  detail_key: string;
}

export interface ThresholdMemberRecord {
  universe: 'high-price' | 'mega-cap';
  security: ValuationSecurity;
  day_change_pct: number | null;
  close_price_yuan?: number | null;
  price_net_change_yuan?: number | null;
  market_cap_100m_yuan?: number | null;
  market_cap_net_change_100m_yuan?: number | null;
  status: 'continuing' | 'entered' | 'exited' | 'unknown' | 'upstream-other';
  status_label: string;
  region: string;
  controlling_shareholder: string;
}

export interface ThresholdTrendPoint {
  date: string;
  count: number | null;
}

export interface MarketThresholdStocksDocument {
  schema: string;
  generated_at: string;
  view: 'history' | 'members' | 'security' | 'catalog';
  universe: 'high-price' | 'mega-cap';
  availability: 'live' | 'empty' | 'stale-cache';
  selected_period: ThresholdHistoryRecord | null;
  summary: {
    latest?: ThresholdHistoryRecord | null;
    earliest_date?: string | null;
    periods?: number;
    trend_check?: { selected_date_present: boolean; selected_count_matches: boolean | null };
    members?: {
      rows: number;
      active_count: number;
      continuing_count: number;
      entered_count: number;
      exited_count: number;
      names_resolved: number;
      active_count_matches: boolean | null;
      entered_count_matches: boolean | null;
      exited_count_matches: boolean | null;
    };
  };
  counts: { matched: number; returned: number; trend_points: number };
  records: Array<ThresholdHistoryRecord | ThresholdMemberRecord>;
  trend: ThresholdTrendPoint[];
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string; attempts?: number; stale?: boolean }>;
  cache: { ttl_seconds: number; refreshed: boolean; oldest_age_seconds: number };
}

export type CapitalStrengthPeriod = '5d' | '10d' | '20d' | '30d' | '3m';

export type BlockRotationCategory = 'all' | 'industry' | 'concept' | 'region' | 'style';
export type BlockRotationPeriod = '1w' | '1m' | '3m' | '1y';
export type BlockRotationSignal =
  | 'all'
  | 'up-dominant'
  | 'down-dominant'
  | 'balanced'
  | 'inactive';

export interface BlockRotationPeriodMetrics {
  anomaly_count: number | null;
  up_anomaly_count: number | null;
  down_anomaly_count: number | null;
  direction_imbalance: number | null;
  return_pct: number | null;
}

export interface BlockRotationBlock {
  block_id: string | null;
  category: Exclude<BlockRotationCategory, 'all'>;
  code: string;
  entity_type: 'block';
  local_family: string | null;
  local_family_name: string | null;
  member_count: number | null;
  members_api: string | null;
  members_available: boolean;
  name: string;
  name_resolved: boolean;
  security_id: string | null;
  upstream_market: string | null;
  upstream_market_id: number | null;
}

export interface BlockRotationRecord {
  block: BlockRotationBlock;
  last_anomaly_date: string;
  days_since_last_anomaly: number | null;
  average_cycle_days: number | null;
  cycle_gap_days: number | null;
  periods: Record<BlockRotationPeriod, BlockRotationPeriodMetrics>;
  selected_period: BlockRotationPeriod;
  selected_signal: Exclude<BlockRotationSignal, 'all'>;
  raw: Record<string, unknown>;
}

export interface BlockRotationCategorySummary {
  category: Exclude<BlockRotationCategory, 'all'>;
  blocks: number;
  names_resolved: number;
  anomaly_count: number;
  up_anomaly_count: number;
  down_anomaly_count: number;
  up_dominant_blocks: number;
  down_dominant_blocks: number;
  balanced_blocks: number;
  inactive_blocks: number;
}

export interface MarketBlockRotationDocument {
  schema: 'tdx-market-block-rotation-native-v1';
  generated_at: string;
  mode: 'market' | 'block';
  availability: 'live' | 'stale-cache' | 'empty';
  filters: {
    category: BlockRotationCategory;
    period: BlockRotationPeriod;
    sort: string;
    order: 'asc' | 'desc';
    signal: BlockRotationSignal;
    code: string | null;
    query: string;
  };
  summary: Omit<BlockRotationCategorySummary, 'category'> & {
    category: BlockRotationCategory;
    period: BlockRotationPeriod;
    latest_anomaly_date: string | null;
    positive_return_blocks: number;
    negative_return_blocks: number;
    flat_return_blocks: number;
    missing_return_blocks: number;
    by_category: BlockRotationCategorySummary[];
  };
  counts: {
    source_rows: number;
    normalized_rows: number;
    matched: number;
    returned: number;
  };
  records: BlockRotationRecord[];
  sources: Array<{
    resource: string;
    endpoint: string;
    attempts: number;
    row_count: number;
    size: number;
    age_seconds: number;
    stale: boolean;
    upstream_error: string | null;
  }>;
  upstream_health: {
    stale: boolean;
    live_sources: number;
    stale_sources: number;
    max_attempts: number;
    oldest_age_seconds: number;
    upstream_errors: Array<{ resource: string | null; message: string }>;
  };
  cache: { ttl_seconds: number; refreshed: boolean; age_seconds: number };
  categories: Array<Exclude<BlockRotationCategory, 'all'>>;
  periods: BlockRotationPeriod[];
  semantics: string;
}

export type LimitLadderCategory = 'all' | 'industry' | 'concept';
export type LimitLadderActivity =
  | 'all'
  | 'sealed'
  | 'broken'
  | 'consecutive'
  | 'advanced'
  | 'inactive';
export type LimitLadderSort =
  | 'date'
  | 'sealed'
  | 'broken'
  | 'prior-limit'
  | 'consecutive'
  | 'max-height'
  | 'total-height'
  | 'advancement-rate';

export interface LimitLadderBlock {
  entity_type: 'block';
  category: Exclude<LimitLadderCategory, 'all'>;
  code: string;
  name: string;
  name_resolved: boolean;
  upstream_name: string;
  upstream_market_id: number | null;
  security_id: string | null;
  block_id: string | null;
  local_family: string | null;
  local_family_name: string | null;
  parent_block_id: string | null;
  level: number | null;
  is_leaf: boolean | null;
  member_count: number | null;
  members_available: boolean;
  members_api: string | null;
}

export interface LimitLadderRecord {
  block: LimitLadderBlock;
  date: string;
  sealed_limit_up_count: number | null;
  broken_board_count: number | null;
  prior_limit_up_count: number | null;
  consecutive_limit_up_count: number | null;
  max_streak_height: number | null;
  sum_streak_heights: number | null;
  advancement_rate_pct: number | null;
  calculated_advancement_rate_pct: number | null;
  advancement_rate_formula_matches: boolean | null;
  raw: Record<string, unknown>;
}

export interface LimitLadderSummary {
  category: LimitLadderCategory;
  blocks: number;
  names_resolved: number;
  sealed_blocks: number;
  broken_blocks: number;
  consecutive_blocks: number;
  inactive_blocks: number;
  advancement_formula_checked_blocks: number;
  advancement_formula_mismatch_blocks: number;
  sealed_limit_up_count: number;
  broken_board_count: number;
  prior_limit_up_count: number;
  consecutive_limit_up_count: number;
  max_streak_height: number;
  sum_streak_heights: number;
  weighted_advancement_rate_pct: number | null;
}

export interface MarketLimitLadderDocument {
  schema: 'tdx-market-limit-ladder-native-v1';
  generated_at: string;
  mode: 'market' | 'block';
  availability: 'live' | 'stale-cache' | 'empty';
  filters: {
    category: LimitLadderCategory;
    sort: LimitLadderSort;
    order: 'asc' | 'desc';
    activity: LimitLadderActivity;
    code: string | null;
    query: string;
  };
  summary: LimitLadderSummary & {
    latest_date: string | null;
    by_category: Array<LimitLadderSummary & {
      category: Exclude<LimitLadderCategory, 'all'>;
    }>;
  };
  counts: {
    source_rows: number;
    category_rows: number;
    matched: number;
    returned: number;
  };
  records: LimitLadderRecord[];
  sources: Array<{
    resource: string;
    endpoint: string;
    attempts: number;
    row_count: number;
    size: number;
    age_seconds: number;
    stale: boolean;
    upstream_error: string | null;
  }>;
  upstream_health: {
    stale: boolean;
    live_sources: number;
    stale_sources: number;
    max_attempts: number;
    oldest_age_seconds: number;
    upstream_errors: Array<{ resource: string | null; message: string }>;
  };
  cache: { ttl_seconds: number; refreshed: boolean; age_seconds: number };
  categories: Array<Exclude<LimitLadderCategory, 'all'>>;
  semantics: string;
}

export type SessionTurnoverUniverse = 'a' | 'etf';
export type SessionTurnoverActivity = 'all' | 'after-hours' | 'opening' | 'both';
export type SessionTurnoverSort =
  | 'after-hours'
  | 'opening'
  | 'total'
  | 'after-hours-share'
  | 'opening-share'
  | 'close-change'
  | 'open-change';

export interface SessionTurnoverSecurity {
  market: 'sz' | 'sh' | 'bj';
  market_id: 0 | 1 | 2;
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
}

export interface SessionTurnoverRecord {
  security: SessionTurnoverSecurity;
  statistics_date: string;
  close_price: number | null;
  previous_close_price: number | null;
  open_price: number | null;
  close_change_pct: number | null;
  open_change_pct: number | null;
  total_turnover_yuan: number | null;
  opening_turnover_yuan: number | null;
  after_hours_turnover_yuan: number | null;
  opening_share_total_pct: number | null;
  after_hours_share_total_pct: number | null;
  raw: Record<string, unknown>;
}

export interface MarketSessionTurnoverDocument {
  schema: 'tdx-market-session-turnover-native-v1';
  generated_at: string;
  universe: SessionTurnoverUniverse;
  mode: 'market' | 'security';
  availability: 'live' | 'stale-cache' | 'empty';
  statistics_date: string | null;
  summary: {
    available: number;
    statistics_dates: string[];
    opening_active: number;
    after_hours_active: number;
    total_turnover_yuan: number;
    opening_turnover_yuan: number;
    after_hours_turnover_yuan: number;
    opening_share_total_pct: number | null;
    after_hours_share_total_pct: number | null;
  };
  filters: {
    sort: SessionTurnoverSort;
    order: 'asc' | 'desc';
    activity: SessionTurnoverActivity;
    market: 'sz' | 'sh' | 'bj' | null;
    code: string | null;
    query: string;
  };
  counts: { source_rows: number; matched: number; returned: number };
  records: SessionTurnoverRecord[];
  source: {
    resource: string;
    endpoint: string;
    attempts: number;
    row_count: number;
    size: number;
    age_seconds: number;
    stale: boolean;
    upstream_error: string | null;
  };
  cache: { ttl_seconds: number; refreshed: boolean; age_seconds: number };
  semantics: string;
}

export type FlowFollowupView =
  | 'margin'
  | 'northbound'
  | 'financing-model'
  | 'lending-model'
  | 'northbound-inflow-model'
  | 'northbound-purchase-model';

export interface FlowFollowupHistoryRecord {
  date: string;
  csi300_close: number | null;
  signal_available: boolean;
  usable_for_signal_analysis: boolean;
  data_status: string;
  financing_balance_100m_cny?: number | null;
  securities_lending_balance_100m_cny?: number | null;
  active_float_market_cap_10b_cny?: number | null;
  financing_rate_pct?: number | null;
  securities_lending_rate_pct?: number | null;
  reported_net_inflow_100m_cny?: number | null;
  reported_net_purchase_100m_cny?: number | null;
  net_inflow_100m_cny?: number | null;
  net_purchase_100m_cny?: number | null;
  forward_returns_pct: {
    days_1: number | null;
    days_3: number | null;
    days_5: number | null;
    days_10?: number | null;
  };
}

export interface FlowFollowupPerformance {
  days_1: { mean_return_pct: number | null; positive_ratio_pct: number | null };
  days_3: { mean_return_pct: number | null; positive_ratio_pct: number | null };
  days_5: { mean_return_pct: number | null; positive_ratio_pct: number | null };
  days_10?: { mean_return_pct: number | null; positive_ratio_pct: number | null };
}

export interface FlowFollowupBucket {
  bucket_code: string;
  bucket_label: string;
  bucket_unit: 'percentage-points' | '100-million-CNY';
  observations: number | null;
  is_current_bucket: boolean;
  usable_for_current_signal: boolean | null;
  csi300_forward_performance: FlowFollowupPerformance;
}

interface FlowFollowupDocumentBase {
  schema: 'tdx-flow-followup-native-v2';
  availability: 'live' | 'stale-cache';
  generated_at: string;
  view: FlowFollowupView;
  available_views: FlowFollowupView[];
  warnings: string[];
  cache: { hit: boolean; stale: boolean; age_seconds: number; ttl_seconds: number };
  raw_response_retained: false;
}

export interface FlowFollowupHistoryDocument extends FlowFollowupDocumentBase {
  view: 'margin' | 'northbound';
  parameters: { start_date: string; end_date: string; available_only: boolean };
  methodology: {
    index: 'CSI 300';
    forward_returns: string;
    association_not_causation: true;
    forecast: false;
    client_note: string;
  };
  summary: {
    observed_start_date: string | null;
    observed_end_date: string | null;
    last_signal_available_date: string | null;
    placeholder_start_date: string | null;
    latest: FlowFollowupHistoryRecord | null;
    forward_return_statistics: Record<string, unknown>;
  };
  counts: {
    upstream_rows: number;
    normalized: number;
    signal_available: number;
    upstream_placeholder: number;
    available_only_excluded: number;
    eligible: number;
    returned: number;
    truncated: boolean;
  };
  records: FlowFollowupHistoryRecord[];
  source: Record<string, unknown>;
}

export interface FlowFollowupCurrentSignal {
  date: string | null;
  signal_kind: string;
  signal_available: boolean;
  usable_for_signal_analysis: boolean;
  data_status: string;
  balance_100m_cny?: number | null;
  rate_pct?: number | null;
  flow_100m_cny?: number | null;
  csi300_forward_performance: FlowFollowupPerformance;
  upstream_text: string;
}

export interface FlowFollowupModelDocument extends FlowFollowupDocumentBase {
  view: Exclude<FlowFollowupView, 'margin' | 'northbound'>;
  view_title: string;
  parameters: { start_date: string; end_date: string; request_type: string };
  methodology: {
    population: string;
    index: 'CSI 300';
    forward_returns: string;
    positive_ratio: string;
    association_not_causation: true;
    investment_signal: false;
    upstream_calls_result_prediction: true;
  };
  current_signal: FlowFollowupCurrentSignal;
  current_bucket: FlowFollowupBucket | null;
  northbound_placeholder_audit: {
    date: string | null;
    reported_net_inflow_100m_cny: number | null;
    reported_net_purchase_100m_cny: number | null;
    exact_1040_0_pair: boolean;
    counterpart_signal_kind: string;
  } | null;
  counts: {
    upstream_bucket_rows: number;
    normalized_buckets: number;
    current_buckets: number;
    returned: number;
    truncated: boolean;
  };
  records: FlowFollowupBucket[];
  sources: Array<Record<string, unknown>>;
}

export type FlowFollowupDocument =
  | FlowFollowupHistoryDocument
  | FlowFollowupModelDocument;

export type BlockBacktestCategory = 'all' | 'industry' | 'concept' | 'style' | 'region';
export type BlockBacktestAdjustment = 'none' | 'forward' | 'backward';
export type BlockBacktestSort =
  | 'return'
  | 'net-inflow'
  | 'main-net-inflow'
  | 'max-drawdown'
  | 'turnover'
  | 'code';

export interface BlockBacktestBlockIdentity {
  entity_type: 'block';
  code: string;
  name: string;
  upstream_setcode: string;
  name_resolved: boolean;
  block_id: string | null;
  family: string | null;
  family_name: string | null;
  local_match_count: number;
}

export interface BlockBacktestSecurityIdentity {
  entity_type: 'security';
  market: 'sz' | 'sh' | 'bj';
  market_id: 0 | 1 | 2;
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
}

export interface BlockBacktestMetrics {
  rank: number;
  trade_date: string;
  base_close: number | null;
  high: number | null;
  low: number | null;
  close: number | null;
  return_pct: number | null;
  max_daily_gain_pct: number | null;
  amplitude_pct: number | null;
  max_drawdown_pct: number | null;
  volume: number | null;
  amount: number | null;
  turnover_pct: number | null;
  net_inflow: number | null;
  main_net_inflow: number | null;
}

export interface BlockBacktestBlockRecord extends BlockBacktestMetrics {
  block: BlockBacktestBlockIdentity;
}

export interface BlockBacktestMemberRecord extends BlockBacktestMetrics {
  security: BlockBacktestSecurityIdentity;
}

interface MarketBlockBacktestDocumentBase {
  schema: 'tdx-block-backtest-native-v1';
  availability: 'live' | 'stale-cache';
  generated_at: string;
  period: {
    requested_begin: string;
    requested_end: string;
    observed_trade_dates: string[];
    adjustment: BlockBacktestAdjustment;
  };
  sort: BlockBacktestSort;
  order: 'asc' | 'desc';
  summary: {
    gainers: number;
    losers: number;
    flat: number;
    positive_ratio_pct: number | null;
    average_return_pct: number | null;
    median_return_pct: number | null;
  };
  counts: {
    upstream_rows: number;
    normalized: number;
    identity_resolved: number;
    returned: number;
    truncated: boolean;
  };
  source: {
    transport: 'PBRPC reqformat=22';
    request_id: '200199';
    source_file: string;
    module: string;
    rpc_id: number;
    rounds: number;
    raw_size: number;
  };
  cache: {
    hit: boolean;
    stale: boolean;
    age_seconds: number;
    ttl_seconds: number;
    upstream_error?: string;
  };
  raw_response_retained: false;
}

export interface MarketBlockBacktestBlocksDocument extends MarketBlockBacktestDocumentBase {
  selection: {
    mode: 'blocks';
    category: BlockBacktestCategory;
    block_code: null;
    membership_basis: null;
    historical_membership_reconstructed: false;
  };
  records: BlockBacktestBlockRecord[];
}

export interface MarketBlockBacktestMembersDocument extends MarketBlockBacktestDocumentBase {
  selection: {
    mode: 'members';
    category: BlockBacktestCategory;
    block_code: string;
    membership_basis: 'upstream-server-selection';
    historical_membership_reconstructed: false;
  };
  records: BlockBacktestMemberRecord[];
}

export type MarketBlockBacktestDocument =
  | MarketBlockBacktestBlocksDocument
  | MarketBlockBacktestMembersDocument;

export type LimitReviewView = 'current' | 'history' | 'daily' | 'annual' | 'security';
export type LimitReviewCategory = 'all' | 'limit-up' | 'limit-down' | 'surge';

export interface LimitReviewSecurity {
  market: 'sz' | 'sh' | 'bj';
  market_id: 0 | 1 | 2;
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
}

export interface LimitReviewGene {
  past_year_limit_count: number | null;
  next_open_above_5pct_count: number | null;
  next_day_red_rate: number | null;
  next_day_mean_return_pct: number | null;
  first_board_seal_rate: number | null;
  continuation_rate: number | null;
}

export interface LimitReviewCurrentRecord {
  category: 'limit-up' | 'limit-down' | 'surge';
  direction: 'up' | 'down';
  security: LimitReviewSecurity;
  date: string;
  trigger_time?: string;
  reason: string;
  limit_type?: string;
  board_shape?: string;
  break_count?: number | null;
  first_time?: string;
  last_time?: string;
  streak_days?: number | null;
  limit_gene?: LimitReviewGene | null;
  raw: Record<string, unknown>;
}

export interface LimitReviewAnnualSide {
  close_count: number | null;
  intraday_count: number | null;
  total_count: number | null;
  average_turnover_pct: number | null;
  next_day_gap_up_rate_pct?: number | null;
  next_day_higher_close_rate_pct?: number | null;
  next_day_gap_down_rate_pct?: number | null;
  next_day_lower_close_rate_pct?: number | null;
}

export interface LimitReviewAnnualRecord {
  category: 'annual';
  security: LimitReviewSecurity;
  latest_date: string;
  newly_listed: string;
  limit_up: LimitReviewAnnualSide;
  limit_down: LimitReviewAnnualSide;
  raw: Record<string, unknown>;
}

export interface LimitReviewMarketHistoryRecord {
  category: 'market-history';
  date: string;
  shanghai_index_change_pct: number | null;
  market_turnover_100m_yuan: number | null;
  market_turnover_yuan: number | null;
  limit_up: {
    all_count: number | null;
    closed_count: number | null;
    broken_count: number | null;
    streak_count: number | null;
    one_price_count: number | null;
    total_seal_amount_yuan: number | null;
    max_seal_amount_yuan: number | null;
    total_turnover_yuan: number | null;
    at_limit_turnover_yuan: number | null;
    max_streak: number | null;
    seal_rate_pct: number | null;
    streak_distribution: Record<string, number | null>;
  };
  limit_down: {
    all_count: number | null;
    closed_count: number | null;
    intraday_count: number | null;
  };
  up_down_count_ratio: number | null;
  up_down_break_ratio: number | null;
  raw: Record<string, unknown>;
}

export interface LimitReviewDailyRecord {
  category: 'limit-up' | 'limit-down';
  direction: 'up' | 'down';
  date: string;
  security: LimitReviewSecurity;
  daily_change_pct: number | null;
  limit_category: string;
  reason: string;
  first_time: string;
  last_time: string;
  break_count: number | null;
  streak_days: number | null;
  raw: Record<string, unknown>;
}

export interface LimitReviewSecurityHistoryRecord {
  date: string;
  limit_type: string;
  reason: string;
  streak_days: number | null;
  explanation: string;
  raw: Record<string, unknown>;
}

export type LimitReviewRecord =
  | LimitReviewCurrentRecord
  | LimitReviewAnnualRecord
  | LimitReviewMarketHistoryRecord
  | LimitReviewDailyRecord;

export interface LimitReviewSource {
  resource: string;
  endpoint: string | null;
  row_count: number;
  size: number;
  attempts?: number;
  stale?: boolean;
  age_seconds?: number;
  upstream_error?: string | null;
  missing?: boolean;
}

export interface MarketLimitReviewDocument {
  schema: 'tdx-market-limit-review-native-v1';
  generated_at: string;
  view: LimitReviewView;
  category: LimitReviewCategory;
  market: 'sz' | 'sh' | 'bj' | null;
  code: string | null;
  date: string | null;
  availability: 'live' | 'stale-cache' | 'empty';
  counts: {
    source_rows: number;
    matched: number;
    returned: number;
    history: number;
    missing_sources: number;
  };
  records: LimitReviewRecord[];
  history: LimitReviewSecurityHistoryRecord[];
  sources: LimitReviewSource[];
  upstream_health: {
    stale: boolean;
    live_sources: number;
    stale_sources: number;
    max_attempts: number;
    oldest_age_seconds: number;
    upstream_errors: Array<{ resource: string | null; message: string }>;
  };
  cache: { ttl_seconds: number; refreshed: boolean };
  semantics: string;
}

export type HotHistoryMarket = 'sz' | 'sh' | 'bj';
export type HotHistorySort = 'start-date' | 'end-date' | 'return' | 'peak' | 'days' | 'code';

export interface HotHistorySecurity {
  market_id: 0 | 1 | 2;
  market: HotHistoryMarket;
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
  name_source: 'current-directory' | 'historical-compatibility' | 'unresolved';
}

export interface HotHistoryRecord {
  security: HotHistorySecurity;
  start_date: string;
  end_date: string;
  trading_days: number;
  theme: string;
  interval_return_pct: number;
  peak_return_pct: number;
  /** 完整恢复的说明；界面必须按纯文本呈现。 */
  analysis: string;
  native_client_analysis: string;
  analysis_contains_embedded_delimiter: boolean;
  native_host_eligible: boolean;
  source_line: number;
}

export interface MarketHotHistoryDocument {
  schema: 'tdx-market-hot-history-native-v1';
  generated_at: string;
  source_mode: 'local';
  mode: 'catalog' | 'security';
  availability: 'local' | 'empty';
  match_count: number;
  returned: number;
  has_more: boolean;
  next_offset: number | null;
  filters: {
    // The HTTP query accepts both canonical names and the native numeric
    // aliases, and echoes the selected spelling unchanged.
    market: 'all' | HotHistoryMarket | '0' | '1' | '2';
    code: string | null;
    q: string;
    from: string | null;
    to: string | null;
    sort: HotHistorySort;
    order: 'asc' | 'desc';
    offset: number;
    limit: number;
  };
  summary: {
    security_count: number;
    native_host_eligible_records: number;
    historical_name_fallback_records: number;
    unresolved_security_records: number;
    embedded_delimiter_records: number;
    earliest_start_date: string | null;
    latest_end_date: string | null;
    maximum_interval_return_pct: number | null;
    maximum_peak_return_pct: number | null;
  };
  records: HotHistoryRecord[];
  sources: Array<{
    file: string;
    path: string;
    size: number;
    row_count: number;
    endpoint: string;
  }>;
  semantics: {
    native_loader: string;
    native_record_size_bytes: number;
    interval: string;
    interval_return_pct: string;
    peak_return_pct: string;
    analysis_recovery: string;
    historical_name_fallback: string;
  };
  transport: { kind: 'local-files'; network_requests: 0 };
}

export interface CapitalStrengthRecord {
  period: CapitalStrengthPeriod;
  period_label: string;
  source_rank: number;
  security: ValuationSecurity;
  statistics_date: string;
  float_shares: number | null;
  period_return_pct: number | null;
  total_net_inflow_yuan: number | null;
  main_net_inflow_yuan: number | null;
  ddx_float_share_pct: number | null;
  raw: Record<string, unknown>;
}

export interface CapitalStrengthConfluenceRecord {
  security: ValuationSecurity;
  period_count: number;
  all_five_periods: boolean;
  periods: CapitalStrengthPeriod[];
  average_ddx_float_share_pct: number | null;
  minimum_ddx_float_share_pct: number | null;
  maximum_ddx_float_share_pct: number | null;
  best_source_rank: number | null;
  latest_statistics_date: string;
  observations: CapitalStrengthRecord[];
}

export interface CapitalStrengthCatalogRecord {
  period: CapitalStrengthPeriod;
  label: string;
  resource: string;
  source_limit: number;
  source_sort: string;
}

export interface MarketCapitalStrengthDocument {
  schema: string;
  generated_at: string;
  view: 'ranking' | 'confluence' | 'security' | 'catalog';
  period: CapitalStrengthPeriod | null;
  availability: 'live' | 'empty' | 'stale-cache' | 'catalog';
  filters: {
    market: string | null;
    code: string | null;
    query: string;
    min_periods: number;
    sort: string;
    order: string;
  };
  summary: {
    source_rows?: number;
    names_resolved?: number;
    positive_ddx_rows?: number;
    negative_total_net_inflow_rows?: number;
    negative_main_net_inflow_rows?: number;
    total_net_inflow_yuan?: number;
    main_net_inflow_yuan?: number;
    source_ddx_descending?: boolean;
    statistics_dates?: string[];
    unique_securities?: number;
    at_least_two_periods?: number;
    all_five_periods?: number | boolean;
    period_count_distribution?: Record<string, number>;
    periods_checked?: number;
    periods_matched?: number;
  };
  counts: { matched: number; returned: number; sources: number };
  records: Array<CapitalStrengthRecord | CapitalStrengthConfluenceRecord | CapitalStrengthCatalogRecord>;
  catalog: CapitalStrengthCatalogRecord[];
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string; attempts?: number; stale?: boolean }>;
  cache: { ttl_seconds: number; refreshed: boolean; resource_count: number };
}

export interface StrongStockIntervalRecord {
  interval_id: string;
  source_rank: number;
  security: ValuationSecurity;
  start_date: string;
  end_date: string;
  interval_statistics: string;
  trading_days: number | null;
  limit_up_days: number | null;
  stock_return_pct: number | null;
  index_return_pct: number | null;
  excess_return_pct: number | null;
  return_finalized: boolean;
  detail_resource: string;
  raw: Record<string, unknown>;
}

export interface StrongStockDetailRecord {
  interval_id: string;
  source_rank: number;
  date: string;
  security: ValuationSecurity;
  stock_return_pct: number | null;
  turnover_amount_yuan: number | null;
  limit_up_reason: string;
  market_limit_up_count: number | null;
  market_broken_limit_count: number | null;
  market_limit_down_count: number | null;
  market_seal_success_pct: number | null;
  index_return_pct: number | null;
  raw: Record<string, unknown>;
}

export interface StrongStockCatalogRecord {
  view: 'intervals' | 'security' | 'detail';
  label: string;
  resource: string;
  fields: string;
}

export interface MarketStrongStocksDocument {
  schema: string;
  generated_at: string;
  view: 'intervals' | 'security' | 'detail' | 'catalog';
  availability: 'live' | 'empty' | 'stale-cache' | 'catalog';
  filters: {
    market: string | null;
    code: string | null;
    interval_id: string | null;
    query: string;
    from: string | null;
    to: string | null;
    min_trading_days: number;
    min_limit_up_days: number;
    sort: string;
    order: string;
  };
  summary: {
    source_rows?: number;
    unique_securities?: number;
    securities_with_multiple_intervals?: number;
    names_resolved?: number;
    return_finalized_rows?: number;
    return_pending_rows?: number;
    first_start_date?: string | null;
    latest_start_date?: string | null;
    latest_end_date?: string | null;
    by_market?: Record<string, number>;
    days?: number;
    expected_trading_days?: number | null;
    complete?: boolean;
    first_date?: string | null;
    latest_date?: string | null;
    positive_stock_days?: number;
    reason_days?: number;
    turnover_amount_yuan?: number | null;
    market_limit_up_count_sum?: number;
    market_broken_limit_count_sum?: number;
    market_limit_down_count_sum?: number;
    average_market_seal_success_pct?: number | null;
    interval?: StrongStockIntervalRecord;
  };
  counts: { matched: number; returned: number; sources: number };
  records: Array<StrongStockIntervalRecord | StrongStockDetailRecord | StrongStockCatalogRecord>;
  catalog: StrongStockCatalogRecord[];
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string; attempts?: number; stale?: boolean }>;
  cache: { ttl_seconds: number; refreshed: boolean };
}

export interface CommodityQuoteRecord {
  commodity_id: string;
  quote_id: string;
  source_rank: number;
  name: string;
  latest_price: number | null;
  unit: string;
  associated_industry: string;
  quote_date: string;
  previous_price: number | null;
  price_5d: number | null;
  price_10d: number | null;
  price_30d: number | null;
  price_60d: number | null;
  day_change_amount: number | null;
  day_change_pct: number | null;
  change_5d_pct: number | null;
  change_10d_pct: number | null;
  change_30d_pct: number | null;
  change_60d_pct: number | null;
  description: string;
  stocks_resource: string;
  related_securities_resource: string;
  raw: Record<string, unknown>;
}

export interface CommodityStockRecord {
  security: ValuationSecurity;
  reference_price_3m: number | null;
  return_since_reference_pct: null;
  current_quote_available: false;
  investment_logic: string;
  description: string;
  raw: Record<string, unknown>;
}

export interface CommodityRelatedSecurityRecord {
  security: ValuationSecurity;
  quote_fields_available: false;
  raw: Record<string, unknown>;
}

export interface CommodityDetailRecord {
  commodity_id: string;
  quotes: CommodityQuoteRecord[];
  quote_count: number;
  counts: { stocks: number; related_securities: number };
  stocks: CommodityStockRecord[];
  related_securities: CommodityRelatedSecurityRecord[];
}

export interface PriceThemeRecord {
  theme_id: string;
  source_rank: number;
  name: string;
  stock_set: ValuationSecurity[];
  stock_count: number;
  trigger_date: string;
  logic: string;
  latest_driver_date: string;
  latest_driver_title: string;
  associated_commodity_id: string;
  stocks_resource: string;
  drivers_resource: string;
  raw: Record<string, unknown>;
}

export interface ThemeStockRecord {
  security: ValuationSecurity;
  trigger_price?: number | null;
  driver_price?: number | null;
  content?: string;
  return_since_trigger_pct?: null;
  return_since_driver_pct?: null;
  current_quote_available: false;
  raw: Record<string, unknown>;
}

export interface ThemeDriverRecord {
  driver_id: string;
  date: string;
  title: string;
  stock_set: ValuationSecurity[];
  stock_count: number;
  stocks_resource: string;
  raw: Record<string, unknown>;
}

export interface ThemeDetailRecord extends PriceThemeRecord {
  counts: { stocks: number; drivers: number };
  stocks: ThemeStockRecord[];
  drivers: ThemeDriverRecord[];
}

export interface DriverDetailRecord extends ThemeDriverRecord {
  theme: PriceThemeRecord;
  stocks: ThemeStockRecord[];
}

export interface CommoditySecurityAssociationRecord {
  security: ValuationSecurity;
  theme: PriceThemeRecord;
  theme_stock: ThemeStockRecord | null;
  drivers: ThemeDriverRecord[];
  associated_commodity_quotes: CommodityQuoteRecord[];
}

export interface MarketCommodityLinksDocument {
  schema: string;
  generated_at: string;
  view: 'commodities' | 'commodity' | 'themes' | 'theme' | 'driver' | 'security' | 'catalog';
  availability: 'live' | 'empty' | 'stale-cache' | 'catalog';
  filters: {
    commodity_id: string | null;
    theme_id: string | null;
    driver_id: string | null;
    market: string | null;
    code: string | null;
    query: string;
    sort: string;
    order: string;
  };
  summary: {
    quote_rows?: number;
    unique_commodity_ids?: number;
    themes?: number;
    selected_commodity_id?: string;
    selected_theme_id?: string;
    selected_driver_id?: string;
    security?: ValuationSecurity;
    theme_associations?: number;
    commodity_scan?: string;
  };
  counts: { matched: number; returned: number; sources: number };
  records: Array<
    CommodityQuoteRecord | CommodityDetailRecord | PriceThemeRecord |
    ThemeDetailRecord | DriverDetailRecord | CommoditySecurityAssociationRecord
  >;
  sources: Array<{ resource: string; size: number; row_count: number; endpoint: string; attempts?: number; stale?: boolean }>;
  cache: { ttl_seconds: number; refreshed: boolean };
}

export interface AnnouncementSignalRecord {
  signal_id: string;
  record_kind: 'selected' | 'risk' | 'history';
  source_rank: number;
  security: ValuationSecurity;
  date: string;
  title: string;
  pdf_url: string | null;
  direction: 'bullish' | 'bearish' | 'unknown';
  direction_raw: string;
  announcement_type: string;
  recent_3d_return_pct: number | null;
  recent_10d_return_pct: number | null;
  pre_3d_return_pct: number | null;
  post_3d_return_pct: number | null;
  raw: Record<string, unknown>;
}

export interface MarketAnnouncementSignalsDocument {
  schema: string;
  generated_at: string;
  view: 'selected' | 'risks' | 'security' | 'history' | 'catalog';
  availability: 'live' | 'empty' | 'stale-cache' | 'unavailable' | 'catalog';
  filters: {
    market: string | null;
    code: string | null;
    query: string;
    direction: string;
    announcement_type: string;
    from: string | null;
    to: string | null;
    include_history: boolean;
    sort: string;
    order: string;
  };
  summary: {
    rows?: number;
    unique_securities?: number;
    announcement_types?: number;
    bullish?: number;
    bearish?: number;
    unknown_direction?: number;
    pdf_links?: number;
    selected_rows?: number;
    risk_rows?: number;
    history_rows?: number;
    first_date?: string | null;
    latest_date?: string | null;
  };
  counts: { matched: number; returned: number; sources: number; warnings: number };
  records: AnnouncementSignalRecord[];
  sources: Array<{ resource: string; size?: number; row_count?: number; endpoint?: string; attempts?: number; stale?: boolean }>;
  warnings: Array<{ resource: string; message: string }>;
  cache: { ttl_seconds: number; refreshed: boolean };
}

export interface ReverseRepoRecord {
  repo_id: string;
  source_rank: number;
  security: ValuationSecurity & { category: 'repo' };
  term_days: number;
  interest_days: number;
  bonus_interest_days: number;
  fee_yuan_per_100k: number;
  settlement_date: string;
  funds_available_date: string;
  funds_withdrawable_date: string;
  principal_yuan: number;
  annualized_rate_pct: number | null;
  gross_interest_yuan: number | null;
  fee_yuan: number;
  net_interest_yuan: number | null;
  net_annualized_rate_pct: number | null;
  quote_available: boolean;
  quote_price_source: 'last-price' | 'pre-close' | 'unavailable';
  previous_rate_pct: number | null;
  open_rate_pct: number | null;
  high_rate_pct: number | null;
  low_rate_pct: number | null;
  rate_change_pct: number | null;
  turnover_amount_yuan: number | null;
  quote_time_raw: number | null;
  raw: Record<string, unknown>;
}

export interface MarketReverseRepoDocument {
  schema: string;
  generated_at: string;
  view: 'rates' | 'security' | 'catalog';
  availability: 'live' | 'schedule-only' | 'stale-cache' | 'empty' | 'catalog';
  filters: {
    market: string;
    code: string | null;
    query: string;
    min_term_days: number;
    max_term_days: number;
    principal_yuan: number;
    include_quotes: boolean;
    sort: string;
    order: string;
  };
  summary: {
    rows?: number;
    quoted_rows?: number;
    names_resolved?: number;
    principal_yuan?: number;
    minimum_term_days?: number;
    maximum_term_days?: number;
    schedule_date?: string | null;
    by_market?: { sz: number; sh: number };
    best_rate?: { security: ValuationSecurity; annualized_rate_pct: number } | null;
    best_net_rate?: { security: ValuationSecurity; net_annualized_rate_pct: number; net_interest_yuan: number } | null;
  };
  counts: { matched: number; returned: number; sources: number; warnings: number };
  records: ReverseRepoRecord[];
  quote_source: null | {
    command: string;
    endpoint: string;
    server_name: string;
    generated_at: string;
    requested: number;
    received: number;
    cache_refreshed: boolean;
  };
  warnings: Array<{ source: string; message: string }>;
  cache: {
    schedule_ttl_seconds: number;
    quote_ttl_seconds: number;
    schedule_refreshed: boolean;
    quote_refreshed: boolean;
  };
}

export interface ExchangeSupervisionRecord {
  supervision_id: string;
  record_kind: 'current' | 'history';
  source_rank: number;
  security: ValuationSecurity;
  start_date: string;
  end_date: string;
  start_price: number | null;
  end_price: number | null;
  last_price: number | null;
  period_return_pct: number | null;
  since_start_return_pct: number | null;
  pe_ttm: number | null;
  announcement_url: string | null;
  quote_available: boolean;
  quote_change_pct: number | null;
  turnover_amount_yuan: number | null;
  raw: Record<string, unknown>;
}

export interface MarketExchangeSupervisionDocument {
  schema: 'tdx-market-exchange-supervision-native-v1';
  generated_at: string;
  view: 'current' | 'history' | 'security' | 'catalog';
  availability: 'live' | 'records-only' | 'stale-cache' | 'empty' | 'catalog';
  filters: {
    market: string | null;
    code: string | null;
    query: string;
    from: string | null;
    to: string | null;
    pdf_only: boolean;
    include_quotes: boolean;
    sort: string;
    order: string;
  };
  summary: {
    rows?: number;
    unique_securities?: number;
    current_rows?: number;
    history_rows?: number;
    pdf_links?: number;
    quoted_rows?: number;
    first_start_date?: string | null;
    latest_end_date?: string | null;
  };
  counts: { matched: number; returned: number; sources: number; warnings: number };
  records: ExchangeSupervisionRecord[];
  quote_source: null | {
    command: string;
    endpoint: string;
    server_name: string;
    generated_at: string;
    requested: number;
    received: number;
    cache_refreshed: boolean;
  };
  sources: Array<{ resource: string; size?: number; row_count?: number; endpoint?: string; attempts?: number; stale?: boolean }>;
  warnings: Array<{ source: string; message: string }>;
}

export type InstitutionAnalysisLayout =
  | 'holdings'
  | 'pensions'
  | 'float-structure'
  | 'crowded-oversold'
  | 'exclusive-funds'
  | 'notable-private-funds'
  | 'national-team'
  | 'social-security-summary'
  | 'development-bank-holdings'
  | 'named-holding'
  | 'named-holding-summary'
  | 'stake-building';

export interface InstitutionAnalysisData {
  report_date?: string | null;
  holding_market_value?: number | null;
  institution_count?: number | null;
  institution_count_change?: number | null;
  holding_shares?: number | null;
  holding_shares_change?: number | null;
  holding_shares_change_ratio?: number | null;
  float_share_ratio?: number | null;
  float_share_ratio_change?: number | null;
  total_share_ratio?: number | null;
  total_share_ratio_change?: number | null;
  total_share_pct?: number | null;
  holding_shares_10k?: number | null;
  float_share_pct?: number | null;
  holding_market_value_10k?: number | null;
  net_profit?: number | null;
  net_profit_growth_pct?: number | null;
  report_total_shares?: number | null;
  report_float_shares?: number | null;
  free_float_shares?: number | null;
  free_float_to_float_ratio?: number | null;
  institution_shares?: number | null;
  institution_to_float_ratio?: number | null;
  major_holder_shares?: number | null;
  major_holder_to_float_ratio?: number | null;
  one_year_high_adjusted?: number | null;
  latest_close_adjusted?: number | null;
  drawdown_from_high_pct?: number | null;
  fund_management_company?: string | null;
  private_fund_manager?: string | null;
  china_securities_finance_ratio_pct?: number | null;
  central_huijin_ratio_pct?: number | null;
  combined_ratio_pct?: number | null;
  calculated_combined_ratio_pct?: number | null;
  combined_ratio_formula_matches?: boolean | null;
  holder_rank?: string | null;
  industry?: string | null;
  shareholder_position?: string | null;
  holding_detail?: string | null;
  holding_type?: string | null;
  announcement_date?: string | null;
  start_date?: string | null;
  end_date?: string | null;
  shareholder?: string | null;
  start_adjusted_close?: number | null;
  end_adjusted_close?: number | null;
  period_return_pct?: number | null;
  average_price?: number | null;
  increase_shares?: number | null;
  increase_total_pct?: number | null;
  post_holding_shares?: number | null;
  post_holding_total_pct?: number | null;
  further_increase?: string | null;
  insurance_capital?: string | null;
}

export interface InstitutionAnalysisRecord {
  market: string;
  market_id: number;
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
  data: InstitutionAnalysisData;
  raw: Record<string, unknown>;
}

export interface InstitutionAnalysisSection {
  view: string;
  label: string;
  layout: InstitutionAnalysisLayout;
  resource: string;
  source_rows: number;
  matched: number;
  returned: number;
  truncated: boolean;
  summary: {
    rows: number;
    unique_securities: number;
    names_resolved: number;
    report_dates: string[];
    fund_management_companies?: number;
    holding_shares?: number;
    duplicate_company_security_rows?: number;
    private_fund_managers?: number;
    industries?: number;
    holding_market_value?: number;
    duplicate_manager_security_rows?: number;
    china_securities_finance_securities?: number;
    central_huijin_securities?: number;
    both_securities?: number;
    combined_formula_checked?: number;
    combined_formula_mismatches?: number;
    further_increase_yes?: number;
    insurance_capital_yes?: number;
  };
  records: InstitutionAnalysisRecord[];
  source: { resource: string; size: number; row_count: number; endpoint: string; attempts?: number; stale?: boolean };
}

export interface MarketInstitutionAnalysisDocument {
  schema: string;
  generated_at: string;
  view: string;
  market: string | null;
  code: string | null;
  query: string;
  counts: { sections: number; matched: number; returned: number };
  sections: InstitutionAnalysisSection[];
  sources: InstitutionAnalysisSection['source'][];
  cache: { refreshed: boolean; ttl_seconds: number; resource_count: number };
}

export interface StrategicThemeCategory {
  block_id: string;
  name: string;
  config_name: string;
  resource: string;
  theme_count: number;
  theme_ids: string[];
}

export interface StrategicThemeRecord {
  theme_id: string;
  source_theme_id: string;
  name: string;
  description: string;
  categories: string[];
  category_block_ids: string[];
  master_declared_member_count: number;
  declared_member_count_available: boolean;
  master_raw_member_count: number;
  master_member_count: number;
  master_duplicate_member_count: number;
  count_matches_raw: boolean;
  detail_resource: string;
  detail_available?: boolean;
  member_count?: number;
}

export interface StrategicThemeDetail {
  security: ValuationSecurity;
  logic: string;
  description: string;
  reference_prices: Record<'3d' | '5d' | '20d' | '60d' | '3m', string>;
  raw: Record<string, unknown>;
}

export interface MarketStrategicThemesDocument {
  schema: 'tdx-market-strategic-themes-native-v1';
  generated_at: string;
  view: 'catalog' | 'categories' | 'themes' | 'theme' | 'security';
  availability: 'live' | 'partial' | 'stale-cache' | 'empty';
  categories: StrategicThemeCategory[];
  themes: StrategicThemeRecord[];
  selected_theme: StrategicThemeRecord | null;
  members: ValuationSecurity[];
  details: StrategicThemeDetail[];
  summary: {
    category_count: number;
    theme_count: number;
    category_theme_memberships: number;
    raw_theme_stock_memberships: number;
    theme_stock_memberships: number;
    security_count: number;
    duplicate_master_memberships: number;
    count_mismatch_count: number;
    selected_security_theme_count?: number;
  };
  counts: {
    matched: number;
    returned_categories: number;
    returned_themes: number;
    returned_members: number;
    returned_details: number;
  };
  errors: Array<{ resource: string; message: string }>;
  sources: Array<{ resource: string; size?: number; row_count?: number; endpoint?: string; attempts?: number; stale?: boolean }>;
}

export interface ThemeLibrarySourceOption {
  id: 'general' | 'region' | 'state-owned' | 'company' | 'holdings';
  name: string;
  resource: string;
  theme_count: number;
}

export interface ThemeLibraryRecord {
  record_id: string;
  theme_id: string;
  source: ThemeLibrarySourceOption['id'];
  source_name: string;
  client_instrument_id?: string | null;
  name: string;
  type: string;
  description: string;
  created_date: string;
  age_days: number | null;
  declared_member_count: number;
  raw_member_count: number;
  member_count: number;
  duplicate_member_count: number;
  count_matches_raw: boolean;
  limit_up_count: number | null;
  limit_down_count: number | null;
  broken_limit_count: number | null;
  detail_resource: string;
  chart_resource: string;
  active_member_count?: number;
  detail_available?: boolean;
  chart_available?: boolean;
}

export interface ThemeLibraryDetail {
  security: ValuationSecurity;
  reference_prices: Record<'3d' | '5d' | '20d' | '60d' | '3m', number | null>;
  leader_count: number | null;
  relation_strength: string;
  description: string;
  raw: Record<string, unknown>;
}

export interface ThemeLibraryChartPoint {
  date: string;
  value: number;
}

export interface MarketThemeLibraryDocument {
  schema: 'tdx-market-theme-library-native-v1';
  generated_at: string;
  view: 'catalog' | 'theme' | 'security';
  availability: 'live' | 'partial' | 'stale-cache' | 'empty';
  source_options: ThemeLibrarySourceOption[];
  themes: ThemeLibraryRecord[];
  selected_theme: ThemeLibraryRecord | null;
  members: ValuationSecurity[];
  details: ThemeLibraryDetail[];
  chart: ThemeLibraryChartPoint[];
  summary: {
    source_count: number;
    snapshot_count: number;
    unique_theme_id_count: number;
    theme_stock_memberships: number;
    security_count: number;
    count_mismatch_count: number;
  };
  counts: {
    matched: number;
    returned_themes: number;
    returned_members: number;
    returned_details: number;
    chart_points: number;
  };
  errors: Array<{ resource: string; message: string }>;
  sources: Array<{ resource: string; size?: number; row_count?: number; endpoint?: string; attempts?: number; stale?: boolean }>;
}

export interface BondReferenceSourceOption {
  group: 'rating' | 'rate' | 'category';
  bucket: string;
  name: string;
  resource: string;
}

export interface BondCouponPoint {
  date: string;
  rate_pct: number | null;
}

export interface BondReferenceRecord {
  security: ValuationSecurity;
  source_group: 'rating' | 'rate' | 'category';
  source_bucket: string;
  source_name: string;
  bond_type: string;
  bond_credit_rating: string;
  issuer_credit_rating: string;
  rate_type: string;
  rate_type_flag: string;
  accrual_start_date: string;
  maturity_date: string;
  next_coupon_date: string;
  last_coupon_date: string;
  remaining_years: number | null;
  current_coupon_rate_pct: number | null;
  coupon_frequency_months: number | null;
  face_value_yuan: number | null;
  issue_price_yuan: number | null;
  issue_size_yuan: number | null;
  issue_size_source_100m?: number | null;
  outstanding_balance_source_100m?: number | null;
  outstanding_balance_yuan?: number | null;
  source_scale_raw?: number | null;
  source_scale_semantics?: 'issue-size-yuan' | 'issue-size-100m-yuan' | 'outstanding-balance-100m-yuan' | 'client-master-hidden-unit';
  guarantee_status: string;
  underlying: ValuationSecurity | null;
  listing_date: string;
  conversion_start_date: string;
  conversion_end_date: string;
  conversion_price_yuan: number | null;
  revision_trigger_pct: number | null;
  put_trigger_pct: number | null;
  call_trigger_pct: number | null;
  remaining_coupon_count: number | null;
  coupon_dates: string[];
  coupon_schedule: BondCouponPoint[];
  remaining_coupon_schedule: BondCouponPoint[];
  source_resource: string;
  raw: Record<string, unknown>;
}

export interface MarketBondReferenceDocument {
  schema: 'tdx-market-bond-reference-native-v1';
  generated_at: string;
  availability: 'live' | 'stale-cache' | 'empty';
  source_options: BondReferenceSourceOption[];
  summary: {
    source_group: 'rating' | 'rate' | 'category';
    source_bucket: string;
    source_name: string;
    source_row_count: number;
    matched_count: number;
    matched_unique_security_count: number;
    credit_ratings: Array<{ name: string; count: number }>;
    rate_types: Array<{ name: string; count: number }>;
    bond_types: Array<{ name: string; count: number }>;
    earliest_maturity_date: string | null;
    latest_maturity_date: string | null;
  };
  match_count: number;
  returned: number;
  records: BondReferenceRecord[];
  sources: Array<{ resource: string; size?: number; row_count?: number; endpoint?: string; attempts?: number; stale?: boolean }>;
  projection_reconciliation?: {
    master_resource: string;
    master_row_count: number;
    master_count: number;
    projection_union_count: number;
    counts_match: boolean;
    exact_match: boolean;
    master_only: string[];
    projection_only: string[];
    projections: Array<{ resource: string; market: 'sh' | 'sz'; count: number }>;
    client_master_comparison?: {
      resource: string;
      row_count: number;
      security_count: number;
      counts_match: boolean;
      exact_match: boolean;
      selected_only: string[];
      client_master_only: string[];
      matching_projection_resources: string[];
      matches_single_market_projection: boolean;
    } | null;
  } | null;
}

export interface ThematicOpportunityGroup {
  group_id: string;
  type: 'industry' | 'region' | 'legacy-client-theme';
  type_name: string;
  name: string;
  category: string;
  raw_member_count: number;
  member_count: number;
  duplicate_member_count: number;
  detail_resource: string;
  source_resource: string;
  page_declared_name: string | null;
  semantic_mismatch: boolean;
  semantic_note: string;
  detail_available?: boolean;
}

export interface ThematicOpportunityDetail {
  security: ValuationSecurity;
  logic: string;
  description: string;
  reference_close_3d: number | null;
  reference_close_5d: number | null;
  reference_close_20d: number | null;
  three_month_adjusted_close: number | null;
  year_start_adjusted_close: number | null;
  limit_up_count: number | null;
  detail_variant: 'ydyl-opportunity' | 'legacy-client-theme';
  source_resource: string;
  raw: Record<string, unknown>;
}

export interface CompletedHypeRecord {
  record_id: string;
  status: 'completed';
  block_code: string;
  block_name: string;
  leader: ValuationSecurity;
  start_date: string;
  end_date: string;
  limit_pattern: string;
  interval_return_pct: number | null;
  analysis: string;
  concept_id: string;
  raw: Record<string, unknown>;
}

export interface ActiveHypeRecord {
  record_id: string;
  status: 'active';
  security: ValuationSecurity;
  start_date: string;
  end_date: string;
  interval_stat: string;
  stock_return_pct: number | null;
  shanghai_index_return_pct: number | null;
  relative_return_pct: number | null;
  raw: Record<string, unknown>;
}

export interface MarketThematicOpportunitiesDocument {
  schema: 'tdx-market-thematic-opportunities-native-v1';
  generated_at: string;
  view: 'catalog' | 'groups' | 'group' | 'security' | 'hype-completed' | 'hype-active';
  availability: 'live' | 'partial' | 'stale-cache' | 'empty';
  groups: ThematicOpportunityGroup[];
  selected_group: ThematicOpportunityGroup | null;
  details: ThematicOpportunityDetail[];
  completed_hype: CompletedHypeRecord[];
  active_hype: ActiveHypeRecord[];
  summary: {
    group_count: number;
    industry_group_count: number;
    region_group_count: number;
    legacy_client_theme_count: number;
    semantic_mismatch_count: number;
    group_memberships: number;
    security_count: number;
    duplicate_master_memberships: number;
    completed_hype_count: number;
    active_hype_count: number;
  };
  counts: {
    matched: number;
    returned_groups: number;
    returned_details: number;
    returned_completed_hype: number;
    returned_active_hype: number;
  };
  errors: Array<{ resource: string; message: string }>;
  sources: Array<{ resource: string; row_count?: number; endpoint?: string; attempts?: number; stale?: boolean }>;
}

export type TenderOfferStatusCategory =
  | 'preparing'
  | 'active'
  | 'paused'
  | 'completed'
  | 'failed'
  | 'other';

export interface TenderOfferRecord {
  offer_id: string;
  source_rank: number;
  security: ValuationSecurity;
  announcement_date: string;
  acquirer: string;
  status: string;
  status_category: TenderOfferStatusCategory;
  share_type: string;
  offer_price: number | null;
  planned_shares_10k: number | null;
  planned_shares: number | null;
  planned_total_pct: number | null;
  planned_funds_10k_yuan: number | null;
  planned_funds_yuan: number | null;
  currency: string;
  start_date: string;
  end_date: string;
  actual_shares_10k: number | null;
  actual_shares: number | null;
  actual_total_pct: number | null;
  actual_to_planned_pct: number | null;
  transfer_date: string;
  delisting: string;
  delisting_flag: boolean;
  purpose: string;
  raw: Record<string, unknown>;
}

export interface MarketTenderOffersDocument {
  schema: 'tdx-market-tender-offers-native-v1';
  generated_at: string;
  availability: 'live' | 'empty' | 'stale-cache';
  filters: {
    market: string | null;
    code: string | null;
    query: string;
    status: string;
    from: string | null;
    to: string | null;
    sort: string;
    order: string;
  };
  summary: {
    rows: number;
    unique_securities: number;
    names_resolved: number;
    by_status: Partial<Record<TenderOfferStatusCategory, number>>;
    live_rows: number;
    delisting_rows: number;
    first_announcement_date: string | null;
    latest_announcement_date: string | null;
    planned_shares: number;
    planned_funds_yuan: number;
    actual_shares: number;
  };
  counts: { matched: number; returned: number; sources: number; warnings: number };
  records: TenderOfferRecord[];
  sources: Array<{ resource: string; size?: number; row_count?: number; endpoint?: string; stale?: boolean }>;
  warnings: Array<{ resource: string; message: string }>;
  cache: { ttl_seconds: number; refreshed: boolean; age_seconds: number };
}

export interface DisclosureSecurity {
  market: string;
  market_id: number;
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
}

export interface DisclosureAnnouncementEvidence {
  issue_date?: string;
  title?: string;
  typecode?: string;
  typename?: string;
  url?: string;
  record_id?: string;
  source?: string;
  summary?: boolean;
}

export interface DisclosureRecord {
  kind: 'schedule' | 'express' | 'recent' | 'announcement-report' | string;
  region: string;
  security: DisclosureSecurity;
  report_period: string;
  report_type?: string;
  report_start?: string;
  scheduled_disclosure_date?: string;
  first_scheduled_date?: string;
  actual_disclosure_date?: string;
  available_from: string | null;
  status: 'scheduled' | 'rescheduled' | 'disclosed' | string;
  change_dates?: string[];
  change_status?: string;
  industry?: string;
  listing_board?: string;
  currency?: string;
  net_profit_yuan?: number | null;
  prior_net_profit_yuan?: number | null;
  net_profit?: number | null;
  prior_net_profit?: number | null;
  net_profit_yoy_pct?: number | null;
  revenue?: number | null;
  revenue_yoy_pct?: number | null;
  weighted_roe_pct?: number | null;
  basic_eps?: number | null;
  selected_announcement?: DisclosureAnnouncementEvidence;
  announcement_evidence?: DisclosureAnnouncementEvidence[];
  full_report_candidate_count?: number;
}

export interface MarketDisclosureDocument {
  schema: 'tdx-market-disclosures-native-v1';
  generated_at: string;
  view: 'all' | 'schedule' | 'express' | 'recent' | 'announcement';
  status: string;
  rows: DisclosureRecord[];
  summary: {
    all_rows: number;
    returned_rows: number;
    by_kind: Record<string, number>;
    by_status: Record<string, number>;
  };
  sources: Array<{
    resource?: string;
    entry?: string;
    endpoint?: string;
    row_count?: number;
    size?: number;
    kind?: string;
  }>;
  cache: {
    refreshed: boolean;
    age_seconds: number;
    announcement_refreshed?: boolean;
    announcement_age_seconds?: number;
  };
  availability_semantics: string;
  history_limit: string;
  announcement_backfill?: boolean;
}

export type PanoramaValue = string | number | boolean | null;

export interface PanoramaRecord {
  market: string;
  market_id: number;
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
  data: Record<string, PanoramaValue>;
  raw: Record<string, unknown>;
}

export interface PanoramaSource {
  resource?: string;
  size?: number;
  row_count?: number;
  endpoint?: string;
  server?: string;
  stale?: boolean;
  age_seconds?: number;
  transport?: Record<string, unknown>;
}

export interface PanoramaSection {
  view: string;
  label: string;
  resource: string;
  source_rows: number;
  matched: number;
  offset: number;
  limit: number;
  returned: number;
  truncated: boolean;
  records: PanoramaRecord[];
  source: PanoramaSource;
}

export interface MarketPanoramaDocument {
  schema: 'tdx-market-panorama-native-v1';
  generated_at: string;
  view: 'security';
  market: string;
  code: string;
  query: string;
  counts: { sections: number; matched: number; returned: number };
  sections: PanoramaSection[];
  sources: PanoramaSource[];
  upstream_health: {
    live_sources: number;
    stale_sources: number;
    stale: boolean;
    oldest_age_seconds: number;
    max_attempts: number;
    upstream_errors: unknown[];
  };
  cache: { refreshed: boolean; ttl_seconds: number; resource_count: number };
  semantics: string;
}

export interface TPoolHistoryRecord {
  market_id: number | null;
  market: string | null;
  code: string;
  security_id: string | null;
  entry_date: number | null;
  entry_date_text: string | null;
  entry_time: number | null;
  entry_time_text: string | null;
  entry_price: number | null;
  income?: number | null;
  current_price?: number | null;
  rise_pct?: number | null;
  volume?: number | null;
  maximum_rise_pct?: number | null;
  maximum_period?: number | null;
  maximum_date?: number | null;
  maximum_date_text?: string | null;
  maximum_price?: number | null;
  day_count?: number | null;
  missing_fields: string[];
  invalid_fields: string[];
  complete: boolean;
  raw: Record<string, string>;
}

export interface TPoolHistoryFile {
  schema: 'tdx-tpool-history-file-v1';
  source: string;
  pool: string;
  cell: string;
  kind: 'daily-snapshot' | 'daily-entry-log';
  history_date: number | null;
  history_date_text: string | null;
  size: number;
  sha256: string;
  record_count: number;
  complete_record_count: number;
  incomplete_record_count: number;
  duplicate_record_count: number;
  expected_fields: string[];
  records: TPoolHistoryRecord[];
  read_only: boolean;
}

export interface TPoolHistoryDocument {
  schema: 'tdx-tpool-history-catalog-v1';
  schema_version: number;
  read_only: boolean;
  dll_loaded: boolean;
  worker_started: boolean;
  path_scope: 'tdx-root-relative';
  file_count: number;
  matched_file_count: number;
  record_count: number;
  complete_record_count: number;
  incomplete_record_count: number;
  duplicate_record_count: number;
  security_count: number;
  truncated: boolean;
  order: 'date-desc';
  directories: Array<{ path: string; exists: boolean }>;
  filters: {
    pool: string | null;
    cell: string | null;
    kind: 'all' | 'snapshot' | 'entry';
    from_date: number | null;
    to_date: number | null;
    limit: number;
  };
  files: TPoolHistoryFile[];
}

export interface ExpansionInstrument {
  category: number;
  market_id: number;
  code: string;
  name: string;
  description: string;
  contract_multiplier: number;
  contract_multiplier_source: 'tdx-7727-0x23f5-offset56-u32';
  security: string;
}

export interface ExpansionInstrumentsDocument {
  schema: 'tdx-expansion-instruments-v1';
  total: number;
  start: number;
  requested: number;
  fetched: number;
  returned: number;
  next_start: number;
  has_more: boolean;
  directory_exhausted: boolean;
  reported_total_shortfall: number;
  market_filter: number | null;
  query: string;
  endpoint: string;
  server_name: string;
  transport: 'tdx-7727-0x23f0-0x23f5';
  market_counts_in_page: Record<string, number>;
  instruments: ExpansionInstrument[];
}

export interface ExpansionDepthLevel {
  level: number;
  price: number;
  volume: number;
}

export interface ExpansionQuoteDocument {
  schema: 'tdx-expansion-quote-v1';
  market_id: number;
  market: string;
  code: string;
  pre_close: number;
  pre_settlement: number;
  reference_price_semantics: string;
  open: number;
  high: number;
  low: number;
  price: number;
  opening_volume: number;
  volume: number;
  last_volume: number;
  inside_volume: number;
  outside_volume: number;
  open_interest: number;
  change: number;
  change_percent: number;
  unknown_u32: number[];
  bids: ExpansionDepthLevel[];
  asks: ExpansionDepthLevel[];
  response_bytes: number;
  endpoint: string;
  server_name: string;
  transport: 'tdx-7727-0x23fa';
  volume_unit: string;
}

export interface ExpansionTimelinePoint {
  index: number;
  wire_minute: number;
  minute_of_day: number;
  session_day_offset: number;
  valid_time: boolean;
  time: string | null;
  price: number;
  average_price: number;
  volume: number;
  open_interest: number;
}

export interface ExpansionTimelineDocument {
  schema: 'tdx-expansion-timeline-v1';
  market: string;
  market_id: number;
  code: string;
  security: string;
  mode: 'current' | 'historical';
  date: number | null;
  date_semantics: string;
  wire_count: number;
  header_bytes: number;
  response_bytes: number;
  invalid_time_count: number;
  response_market_id?: number;
  response_code?: string;
  points: ExpansionTimelinePoint[];
  endpoint: string;
  server_name: string;
  transport: 'tdx-7727-0x240b' | 'tdx-7727-0x240c';
  volume_unit: string;
}

export interface ExpansionTrade {
  index: number;
  absolute_index: number;
  minute_of_day: number;
  second: number;
  time: string;
  price_raw: number;
  price: number;
  volume: number;
  open_interest_change: number;
  nature_raw: number;
  nature_mark: number;
  direction: number;
  side: string;
  nature: string;
}

export interface ExpansionTradesDocument {
  schema: 'tdx-expansion-trades-v1';
  market: string;
  market_id: number;
  code: string;
  security: string;
  mode: 'current' | 'historical';
  date: number | null;
  date_semantics: string;
  start: number;
  page_size: number;
  pages_completed: number;
  downloaded: number;
  next_start: number;
  has_more: boolean;
  price_divisor: 1000;
  price_encoding: string;
  ordering_note?: string;
  volume_unit: string;
  nature_summary: Record<string, number>;
  trades: ExpansionTrade[];
  endpoint: string;
  server_name: string;
  transport: 'tdx-7727-0x23fc' | 'tdx-7727-0x2406';
}

export type OptionMarketKey = 'all' | 'czce' | 'dce' | 'shfe' | 'cffex' | 'gfex';
export type OptionType = 'call' | 'put';

export interface OptionInstrument {
  market_id: number;
  market: string;
  code: string;
  security: string;
  name: string;
  contract: string;
  type: OptionType;
  strike: number;
  underlying_market_id: number;
  underlying_code: string;
  underlying_security: string;
  exercise_style: 'american' | 'european';
  pricing_family: 'black_76' | 'black_scholes';
}

export interface OptionCatalogDocument {
  schema: 'tdx-option-catalog-v1';
  source_schema: string;
  reported_directory_total: number;
  scanned: number;
  matched: number;
  returned: number;
  truncated: boolean;
  market_filter: number | null;
  underlying_filter: string;
  contract_filter: string;
  type_filter: 'all' | OptionType;
  query: string;
  endpoint?: string;
  transport: 'tdx-7727-option-directory-normalized';
  options: OptionInstrument[];
}

export interface OptionChainLeg extends OptionInstrument {
  quote_status: 'priced' | 'no_positive_price' | 'unavailable';
  last_price: number | null;
  previous_settlement?: number | null;
  bid_price: number | null;
  ask_price: number | null;
  mark_price: number | null;
  mark_price_source?: string;
  implied_volatility: number | null;
  implied_volatility_percent: number | null;
  model_price: number | null;
  delta: number | null;
  gamma: number | null;
  theta: number | null;
  vega: number | null;
  rho: number | null;
  volume: number;
  open_interest: number;
  strike_over_underlying: number;
  intrinsic_value?: number;
  time_value?: number | null;
  implied_status: 'calculated' | 'price_unavailable';
}

export interface OptionChainStrike {
  strike: number;
  distance_to_underlying: number;
  call: OptionChainLeg | null;
  put: OptionChainLeg | null;
}

export interface OptionChainSummary {
  strike_count: number;
  contract_count: number;
  quoted_count: number;
  calculated_iv_count: number;
  call_volume: number;
  put_volume: number;
  call_open_interest: number;
  put_open_interest: number;
  put_call_volume_ratio: number | null;
  put_call_open_interest_ratio: number | null;
  call_open_interest_weighted_iv: number | null;
  put_open_interest_weighted_iv: number | null;
  max_pain: {
    strike: number;
    aggregate_intrinsic_open_interest: number;
    method: 'minimum_open_interest_weighted_intrinsic_value';
  } | null;
}

export interface OptionChainDocument {
  schema: 'tdx-option-chain-v1';
  path_scope: 'tdx-root-relative';
  market_id: number;
  market: string;
  contract: string;
  underlying_market_id: number;
  underlying_code: string;
  underlying_security: string;
  underlying_price: number;
  underlying_price_source: string;
  calculation_date: string;
  calculation_date_source: string;
  expiry: string;
  expiry_source: string;
  expiry_resolution: OptionExpiryDocument;
  calendar_days_to_expiry: number;
  time_to_expiry_years: number;
  historical_close_date: string;
  historical_volatility: number;
  historical_volatility_percent: number;
  lookback_requested: number;
  lookback_used: number;
  risk_free: number;
  catalog_refresh: boolean;
  catalog_cache_ttl_seconds: number;
  at_the_money: {
    strike: number;
    distance: number;
    call?: OptionChainLeg | null;
    put?: OptionChainLeg | null;
    put_minus_call_iv?: number | null;
  };
  summary: OptionChainSummary;
  strikes: OptionChainStrike[];
  model_source: string;
  transport: string;
  endpoint?: string;
  server_name?: string;
}

export interface OptionExpiryDocument {
  schema: 'tdx-option-expiry-v1';
  path_scope?: 'tdx-root-relative';
  option: OptionInstrument;
  product: string;
  contract_month: number;
  rules_source: string;
  holiday_source: string;
  resource_mode: string;
  expiry: string | null;
  expiry_raw?: number;
  status: string;
  cutoff_contract_month?: number;
  cutoff_expiry_raw?: number;
  rule_code?: number;
  rule_character?: string;
  rule_parameter?: number;
  rule_month?: number | null;
  holiday_count?: number;
}

export interface OptionVolatilityDocument {
  schema: 'tdx-option-volatility-v1';
  path_scope: 'tdx-root-relative';
  option: OptionInstrument;
  calculation_date: string;
  expiry: string;
  expiry_source: string;
  expiry_resolution: OptionExpiryDocument;
  calendar_days_to_expiry: number;
  time_to_expiry_years: number;
  annualization_days: number;
  day_count: string;
  lookback_requested: number;
  lookback_used: number;
  historical_volatility: number;
  historical_volatility_percent: number;
  initial_volatility: number;
  implied_volatility: number | null;
  implied_volatility_percent: number | null;
  implied_status: string;
  volatility_spread: number | null;
  volatility_spread_percent: number | null;
  underlying_price: number;
  underlying_price_source: string;
  option_price: number;
  option_price_source: string;
  model_price: number | null;
  model_source: string;
  delta: number | null;
  gamma: number | null;
  theta: number | null;
  vega: number | null;
  rho: number | null;
  risk_free: number;
  risk_free_source: string;
  greeks_source: string;
  greeks_volatility_source: string;
  catalog_lookup_used: boolean;
  transport: string;
}

export type HkActionKind =
  | 'all'
  | 'dividend'
  | 'bonus'
  | 'rights'
  | 'split'
  | 'consolidation'
  | 'mixed'
  | 'adjustment'
  | 'other';

export interface HkActionRecord {
  kind: Exclude<HkActionKind, 'all'>;
  flags: string[];
  security: {
    market: 'hk';
    code: string;
    security_id: string;
  };
  date: string;
  description: string;
  source_file: string;
  factors: {
    previous_cumulative_multiplier: number;
    previous_cumulative_offset: number;
    cumulative_multiplier: number;
    cumulative_offset: number;
    event_share_multiplier: number;
    event_additive_adjustment: number;
    changes_share_basis: boolean;
    changes_price_basis: boolean;
  };
}

export interface MarketHkActionsDocument {
  schema: 'tdx-market-hk-actions-native-v1';
  path_scope: 'tdx-root-relative';
  generated_at: string;
  source_mode: 'local';
  mode: 'catalog' | 'security';
  availability: 'local' | 'empty';
  kind: HkActionKind;
  order: 'asc' | 'desc';
  offset: number;
  match_count: number;
  returned: number;
  summary: {
    securities: number;
    earliest_date: string | null;
    latest_date: string | null;
    by_primary_kind: Record<string, number>;
    by_flag: Record<string, number>;
  };
  rows: HkActionRecord[];
  sources: LocalResourceSource[];
  native_semantics: Record<string, unknown>;
  transport: { kind: 'local-encrypted-files'; network_requests: 0 };
  semantics: string;
}

export interface HkFinanceRecord {
  code: string;
  report_date: string | null;
  listing_date: string | null;
  classification_code: string | null;
  finance: {
    report_date: string | null;
    listing_date: string | null;
    classification_code: string | null;
    shares: {
      total_10k: number | null;
      h_10k: number | null;
    };
    balance_sheet: {
      total_assets_10k: number | null;
      net_assets_10k: number | null;
      minority_interest_10k: number | null;
    };
    income_statement: {
      revenue_10k: number | null;
      net_profit_10k: number | null;
    };
    per_share: {
      dividend: number | null;
      earnings: number | null;
      net_assets: number | null;
    };
    valuation: {
      pe_ttm: number | null;
      pe_static: number | null;
    };
    currency_adjustment: {
      native_code: number | null;
      conversion_required: boolean | null;
    };
  };
  native_values: Record<string, string | number | null>;
  raw_fields: Array<string | null>;
}

export interface MarketHkFinanceDocument {
  schema: 'tdx-market-hk-finance-native-v1';
  path_scope: 'tdx-root-relative';
  generated_at: string;
  source_mode: 'local';
  mode: 'catalog' | 'security';
  availability: 'local' | 'empty';
  sort: 'code' | 'report_date' | 'listing_date' | 'classification';
  order: 'asc' | 'desc';
  offset: number;
  match_count: number;
  returned: number;
  summary: {
    source_records: number;
    earliest_report_date: string | null;
    latest_report_date: string | null;
    by_classification: Record<string, number>;
    source_missing_by_index: number[];
  };
  rows: HkFinanceRecord[];
  source: LocalResourceSource;
  native_schema: Record<string, unknown>;
  transport: { kind: 'local-encrypted-file'; network_requests: 0 };
  semantics: string;
}

export interface LocalResourceSource {
  file: string;
  path: string;
  endpoint: string;
  size: number | null;
  row_count: number;
  encrypted?: boolean;
  cipher?: string;
}

export interface HistoricalSecurityIdentity {
  market_id: number;
  market: 'sz' | 'sh' | 'bj';
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
}

export interface HistoricalSecurityRecord {
  record_id: string;
  security: HistoricalSecurityIdentity;
  compatibility_name: string;
  current_name: string | null;
  current_directory_present: boolean;
  name_differs_from_current: boolean;
  source_line: number;
}

export interface HistoricalSecuritiesDocument {
  schema: 'tdx-market-historical-securities-native-v1';
  path_scope: 'tdx-root-relative';
  availability: 'local' | 'empty';
  source_mode: 'local';
  match_count: number;
  returned: number;
  has_more: boolean;
  next_offset: number | null;
  filters: {
    market: 'all' | 'sz' | 'sh' | 'bj' | '0' | '1' | '2';
    code: string | null;
    q: string;
    presence: 'all' | 'current' | 'absent';
    sort: 'code' | 'name' | 'presence';
    order: 'asc' | 'desc';
    offset: number;
    limit: number;
  };
  summary: {
    shenzhen: number;
    shanghai: number;
    beijing: number;
    current_directory_present: number;
    absent_from_current_directory: number;
    name_differs_from_current: number;
  };
  records: HistoricalSecurityRecord[];
  sources: LocalResourceSource[];
  transport: { kind: 'local-files'; network_requests: 0 };
}

export type IndexEventBenchmark =
  | 'all'
  | 'shanghai-composite'
  | 'hang-seng'
  | 'nasdaq-composite';

export interface IndexEventRecord {
  record_id: string;
  benchmark: Exclude<IndexEventBenchmark, 'all'>;
  event_id: number;
  title: string;
  month_day: string;
  occurrence_date: string;
  chart_date: string;
  chart_date_adjusted: boolean;
  detail_url: string;
  native_detail_target: string;
  native_kind: 0 | 1 | 2;
  source_file: string;
  source_line: number;
}

export interface IndexEventsDocument {
  schema: 'tdx-market-index-events-native-v1';
  path_scope: 'tdx-root-relative';
  availability: 'local' | 'empty';
  source_mode: 'local';
  match_count: number;
  returned: number;
  has_more: boolean;
  next_offset: number | null;
  filters: {
    benchmark: IndexEventBenchmark;
    event_id: string | null;
    q: string;
    from: string | null;
    to: string | null;
    date_basis: 'chart' | 'occurrence';
    sort: 'date' | 'event-id' | 'title';
    order: 'asc' | 'desc';
    offset: number;
    limit: number;
  };
  summary: {
    shanghai_composite: number;
    hang_seng: number;
    nasdaq_composite: number;
    chart_date_adjusted: number;
    earliest_date: string | null;
    latest_date: string | null;
  };
  records: IndexEventRecord[];
  sources: LocalResourceSource[];
  transport: { kind: 'local-files'; network_requests: 0 };
}

export interface FundReferenceSecurity {
  market_id: 0 | 1;
  market: 'sz' | 'sh';
  code: string;
  security_id: string;
  name: string;
  name_resolved: boolean;
}

export interface FundReferenceInstrument {
  native_market_id: number;
  market: string;
  source_code: string;
  code: string;
  security_id: string;
  normalization: string | null;
}

interface FundReferenceRecordBase {
  kind: 'fund-snapshot' | 'etf-mapping' | 'lof-mapping';
  security: FundReferenceSecurity;
  source_file: string;
}

export interface FundSnapshotRecord extends FundReferenceRecordBase {
  kind: 'fund-snapshot';
  as_of_date: string;
  fund_units_10k: number | null;
  fund_units: number | null;
  unit_reference_value: number | null;
  unit_nav: number | null;
}

export interface FundMappingRecord extends FundReferenceRecordBase {
  kind: 'etf-mapping' | 'lof-mapping';
  reference_instrument: FundReferenceInstrument | null;
  native_catalog_id: string;
  dates?: {
    window_start: string | null;
    window_end: string | null;
  };
  native_lifecycle_status?: 1 | 2 | 3 | null;
  lifecycle_phase?: 'before-window' | 'in-window' | 'after-window' | null;
}

export type FundReferenceRecord = FundSnapshotRecord | FundMappingRecord;

export interface FundReferenceDocument {
  schema: 'tdx-market-fund-reference-native-v1';
  path_scope: 'tdx-root-relative';
  availability: 'local' | 'empty';
  source_mode: 'local';
  view: 'all' | 'snapshot' | 'etf-mapping' | 'lof-mapping';
  as_of_date: string;
  mode: 'catalog' | 'security';
  match_count: number;
  returned: number;
  summary: {
    fund_snapshots: number;
    etf_mappings: number;
    lof_mappings: number;
    reference_instruments: number;
    native_status_unset: number;
    native_status_1: number;
    native_status_2: number;
    native_status_3: number;
  };
  records: FundReferenceRecord[];
  sources: LocalResourceSource[];
  transport: { kind: 'local-files'; network_requests: 0 };
}
