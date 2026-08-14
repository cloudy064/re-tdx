<script lang="ts">
  import { onMount } from 'svelte';
  import FundStatisticsChart from '../../charts/FundStatisticsChart.svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, money, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    FundStatisticsRecord,
    MarketFundStatisticsDocument
  } from '../../types';

  type View = MarketFundStatisticsDocument['view'];
  const VIEWS = [
    { id: 'equity-fund-performance', label: '股票基金收益' },
    { id: 'new-funds', label: '新发基金' },
    { id: 'fund-dividends', label: '基金分红' },
    { id: 'listed-funds', label: '新上市基金' },
    { id: 'fund-market-size', label: '基金规模' },
    { id: 'etf-market-size', label: 'ETF规模申赎' },
    { id: 'etf-weekly', label: 'ETF周度' }
  ];

  let view = $state<View>('equity-fund-performance');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketFundStatisticsDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);
  const chartView = $derived(['fund-market-size', 'etf-market-size', 'etf-weekly'].includes(view));

  const stats = $derived.by<Stat[]>(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return [
      { label: '股票基金收益', value: count(summary.equity_fund_performance) },
      { label: '新发基金', value: count(summary.new_funds) },
      { label: '基金分红', value: count(summary.fund_dividends) },
      { label: '新上市基金', value: count(summary.listed_funds) },
      { label: '覆盖基金', value: count(summary.unique_funds) },
      { label: '最新业务日期', value: date(summary.latest_date) }
    ];
  });

  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/fund-statistics?${queryString({
      view, q: query.trim(), limit: 5000, refresh: refresh ? 1 : 0
    })}`);
  }

  function switchView(next: string) {
    view = next as View;
    load();
  }

  function fundName(row: FundStatisticsRecord): string {
    return row.security?.name || row.security?.code || '全市场';
  }

  const performanceColumns: Column<FundStatisticsRecord>[] = [
    { key: 'fund', label: '股票型基金', width: '190px', value: fundName, sub: (row) => `${row.security?.security_id ?? ''} · ${text(row.manager)}` },
    { key: 'date', label: '净值日期', width: '100px', num: true, value: (row) => date(row.snapshot_date) },
    { key: 'nav', label: '单位 / 累计净值', align: 'right', num: true, value: (row) => fixed(row.unit_nav, 4), sub: (row) => fixed(row.cumulative_nav, 4) },
    { key: '1m', label: '近1月', align: 'right', num: true, value: (row) => delta(row.return_1m_pct), tone: (row) => tone(row.return_1m_pct), sortValue: (row) => row.return_1m_pct ?? -Infinity },
    { key: '3m', label: '近3月', align: 'right', num: true, value: (row) => delta(row.return_3m_pct), tone: (row) => tone(row.return_3m_pct) },
    { key: '6m', label: '近6月', align: 'right', num: true, value: (row) => delta(row.return_6m_pct), tone: (row) => tone(row.return_6m_pct) },
    { key: '1y', label: '近1年', align: 'right', num: true, value: (row) => delta(row.return_1y_pct), tone: (row) => tone(row.return_1y_pct) },
    { key: 'ytd', label: '今年以来', align: 'right', num: true, value: (row) => delta(row.return_ytd_pct), tone: (row) => tone(row.return_ytd_pct) },
    { key: 'manager', label: '基金经理', width: '150px', value: (row) => text(row.fund_managers) }
  ];

  const newFundColumns: Column<FundStatisticsRecord>[] = [
    { key: 'fund', label: '新发基金', width: '210px', value: fundName, sub: (row) => `${row.security?.security_id ?? ''} · ${text(row.manager)}` },
    { key: 'period', label: '发行起止', width: '190px', num: true, value: (row) => `${date(row.offering_start)} — ${date(row.offering_end)}` },
    { key: 'type', label: '基金 / 产品类型', width: '180px', value: (row) => text(row.fund_class), sub: (row) => text(row.product_type) },
    { key: 'fee', label: '认购 / 申购费', align: 'right', num: true, value: (row) => `${percent(row.subscription_fee_pct)} / ${percent(row.purchase_fee_pct)}` },
    { key: 'min', label: '认购起点', align: 'right', num: true, value: (row) => count(row.minimum_subscription_amount) },
    { key: 'index', label: '跟踪指数', width: '180px', value: (row) => text(row.tracking_index_name), sub: (row) => text(row.tracking_index_code) },
    { key: 'manager', label: '基金经理', width: '150px', value: (row) => text(row.fund_managers) }
  ];

  const dividendColumns: Column<FundStatisticsRecord>[] = [
    { key: 'fund', label: '分红基金', width: '190px', value: fundName, sub: (row) => `${row.security?.security_id ?? ''} · ${text(row.manager)}` },
    { key: 'announce', label: '公告日', width: '100px', num: true, value: (row) => date(row.announcement_date) },
    { key: 'record', label: '登记 / 除息', width: '190px', num: true, value: (row) => `${date(row.record_date)} / ${date(row.ex_dividend_date)}` },
    { key: 'payment', label: '派发日', width: '100px', num: true, value: (row) => date(row.payment_date) },
    { key: 'description', label: '分红说明', width: '260px', value: (row) => text(row.distribution_description) },
    { key: 'nav', label: '单位 / 累计净值', align: 'right', num: true, value: (row) => fixed(row.unit_nav, 4), sub: (row) => fixed(row.cumulative_nav, 4) }
  ];

  const listedColumns: Column<FundStatisticsRecord>[] = [
    { key: 'fund', label: '新上市基金', width: '190px', value: fundName, sub: (row) => row.security?.security_id ?? '' },
    { key: 'date', label: '上市日期', width: '100px', num: true, value: (row) => date(row.listing_date), sub: (row) => `公告 ${date(row.listing_announcement_date)}` },
    { key: 'raised', label: '募集份额', align: 'right', num: true, value: (row) => compact(row.raised_units, '份') },
    { key: 'tradable', label: '上市交易份额', align: 'right', num: true, value: (row) => compact(row.listed_tradable_units, '份') },
    { key: 'nav', label: '上市日净值', align: 'right', num: true, value: (row) => fixed(row.listing_day_nav, 4) },
    { key: 'holders', label: '持有人户数', align: 'right', num: true, value: (row) => count(row.holder_households) },
    { key: 'style', label: '投资风格', width: '140px', value: (row) => text(row.investment_style) },
    { key: 'manager', label: '管理人与经理', width: '220px', value: (row) => text(row.manager), sub: (row) => text(row.fund_managers) }
  ];

  const fundSizeColumns: Column<FundStatisticsRecord>[] = [
    { key: 'date', label: '截止日期', width: '110px', num: true, value: (row) => date(row.snapshot_date) },
    { key: 'count', label: '基金 / 公司数', align: 'right', num: true, value: (row) => `${count(row.all_fund_count)} / ${count(row.fund_company_count)}` },
    { key: 'nav', label: '全部基金净值', align: 'right', num: true, value: (row) => money(row.all_fund_nav_yuan) },
    { key: 'units', label: '全部基金份额', align: 'right', num: true, value: (row) => compact(row.all_fund_units, '份') },
    { key: 'equity', label: '持股市值', align: 'right', num: true, value: (row) => money(row.equity_holdings_yuan) },
    { key: 'open', label: '开放式净值', align: 'right', num: true, value: (row) => money(row.open_fund_nav_yuan), sub: (row) => `${count(row.open_fund_count)} 只` },
    { key: 'closed', label: '封闭式净值', align: 'right', num: true, value: (row) => money(row.closed_fund_nav_yuan), sub: (row) => `${count(row.closed_fund_count)} 只` }
  ];

  const etfSizeColumns: Column<FundStatisticsRecord>[] = [
    { key: 'date', label: '日期', width: '110px', num: true, value: (row) => date(row.snapshot_date) },
    { key: 'total', label: 'ETF总规模', align: 'right', num: true, value: (row) => money(row.total_market_size_yuan) },
    { key: 'sh', label: '沪市 / 深市规模', align: 'right', num: true, value: (row) => money(row.sh_market_size_yuan), sub: (row) => money(row.sz_market_size_yuan) },
    { key: 'net', label: '总申赎净量', align: 'right', num: true, value: (row) => compact(row.total_net_subscription_units, '份'), tone: (row) => tone(row.total_net_subscription_units) },
    { key: 'split', label: '沪市 / 深市净量', align: 'right', num: true, value: (row) => compact(row.sh_net_subscription_units, '份'), sub: (row) => compact(row.sz_net_subscription_units, '份') },
    { key: 'index', label: '上证指数', align: 'right', num: true, value: (row) => fixed(row.sh_composite_close, 2), sub: (row) => delta(row.sh_composite_change_pct) }
  ];

  const weeklyColumns: Column<FundStatisticsRecord>[] = [
    { key: 'date', label: '周截止日', width: '110px', num: true, value: (row) => date(row.week_end_date) },
    { key: 'turnover', label: 'ETF成交额', align: 'right', num: true, value: (row) => money(row.turnover_yuan), sub: (row) => `变动 ${money(row.turnover_change_yuan)}` },
    { key: 'units', label: 'ETF总份额', align: 'right', num: true, value: (row) => compact(row.total_units, '份'), sub: (row) => `变动 ${compact(row.total_units_change, '份')}` },
    { key: 'financing', label: '融资余额', align: 'right', num: true, value: (row) => money(row.financing_balance_yuan), sub: (row) => `变动 ${money(row.financing_balance_change_yuan)}` },
    { key: 'lending', label: '融券余量', align: 'right', num: true, value: (row) => compact(row.securities_lending_units, '份'), sub: (row) => `变动 ${compact(row.securities_lending_change_units, '份')}` },
    { key: 'index', label: '上证周涨幅', align: 'right', num: true, value: (row) => delta(row.sh_composite_weekly_change_pct), tone: (row) => tone(row.sh_composite_weekly_change_pct) }
  ];

  const columns = $derived.by<Column<FundStatisticsRecord>[]>(() => {
    if (view === 'new-funds') return newFundColumns;
    if (view === 'fund-dividends') return dividendColumns;
    if (view === 'listed-funds') return listedColumns;
    if (view === 'fund-market-size') return fundSizeColumns;
    if (view === 'etf-market-size') return etfSizeColumns;
    if (view === 'etf-weekly') return weeklyColumns;
    return performanceColumns;
  });

  const detailStats = $derived.by<Stat[]>(() => {
    if (!selected) return [];
    if (selected.kind === 'equity-fund-performance') return [
      { label: '近1月', value: delta(selected.return_1m_pct), tone: tone(selected.return_1m_pct) },
      { label: '近3月', value: delta(selected.return_3m_pct), tone: tone(selected.return_3m_pct) },
      { label: '近1年', value: delta(selected.return_1y_pct), tone: tone(selected.return_1y_pct) },
      { label: '今年以来', value: delta(selected.return_ytd_pct), tone: tone(selected.return_ytd_pct) }
    ];
    if (selected.kind === 'new-funds') return [
      { label: '发行开始', value: date(selected.offering_start) },
      { label: '发行截止', value: date(selected.offering_end) },
      { label: '认购费', value: percent(selected.subscription_fee_pct) },
      { label: '申购费', value: percent(selected.purchase_fee_pct) }
    ];
    return [
      { label: '业务日期', value: date(selected.date) },
      { label: '源市场', value: selected.security?.market ?? '全市场' },
      { label: '原始字段', value: count(Object.keys(selected.raw).length) },
      { label: '原始资源', value: selected.source_resource }
    ];
  });

  onMount(() => load());
</script>

<PageHeader
  eyebrow="JJTJ · 9 LIVE SOURCES"
  title="基金统计"
  description="通达信客户端的新发基金、分红、股票型基金收益、基金/ETF规模、申赎、融资融券和新上市基金；市场33保留为场外基金，不误归类为A股。"
  {stats}
>
  {#snippet actions()}
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="基金统计视图" />
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

{#if chartView && rows.length}
  <Panel title={view === 'fund-market-size' ? '基金资产净值走势' : view === 'etf-market-size' ? 'ETF市场规模走势' : 'ETF周成交额走势'} subtitle="客户端历史序列 · 可缩放">
    <FundStatisticsChart {rows} {view} />
  </Panel>
{/if}

<Panel title="基金统计主表" subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.sources.length)} 个原始资源` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="当前筛选下没有命中记录。" flush scroll>
  {#snippet toolbar()}
    <TextInput bind:value={query} icon="search" width="280px" label="检索基金" placeholder="代码、名称、管理人或基金经理" onEnter={() => load()} />
  {/snippet}
  <DataTable {columns} {rows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedId = row.event_id)} isActive={(row) => row.event_id === selected?.event_id} stickyFirst numbered minWidth="1080px" />
</Panel>

{#if selected && !chartView}
  <Panel eyebrow={selected.kind_label} title={fundName(selected)} subtitle={selected.source_resource}>
    <StatGrid stats={detailStats} inline />
    {#if selected.kind === 'fund-dividends'}
      <p>{text(selected.distribution_description)}；登记日 {date(selected.record_date)}，除息日 {date(selected.ex_dividend_date)}，红利派发日 {date(selected.payment_date)}。</p>
    {:else if selected.kind === 'new-funds'}
      <p>{text(selected.fund_class)} · {text(selected.product_type)}；管理人 {text(selected.manager)}，基金经理 {text(selected.fund_managers)}。</p>
    {:else if selected.kind === 'listed-funds'}
      <p>管理人 {text(selected.manager)}，基金经理 {text(selected.fund_managers)}；投资风格 {text(selected.investment_style)}。</p>
    {/if}
    <p class="note">“亿”字段已同时换算成元或份；无法从 CFG 唯一确认的分红比例只保留原值和分红说明，不擅自改造成现金口径。</p>
  </Panel>
{/if}

<style>
  p { font-size: var(--fs-xs); line-height: 1.7; }
  .note { color: var(--fg-mute); font-size: 10px; }
</style>
