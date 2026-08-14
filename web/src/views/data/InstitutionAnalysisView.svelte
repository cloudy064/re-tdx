<script lang="ts">
  /** 二十五张通达信机构与投资参股主表：分类持仓、具名机构持股、举牌、基金及浮筹结构。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    InstitutionAnalysisLayout,
    InstitutionAnalysisRecord,
    MarketInstitutionAnalysisDocument
  } from '../../types';

  const VIEWS: Array<{ id: string; label: string; layout: InstitutionAnalysisLayout }> = [
    { id: 'all', label: '全部机构', layout: 'holdings' },
    { id: 'public-funds', label: '公募基金', layout: 'holdings' },
    { id: 'exclusive-funds', label: '基金独门', layout: 'exclusive-funds' },
    { id: 'private-funds', label: '私募基金', layout: 'holdings' },
    { id: 'notable-private-funds', label: '知名私募', layout: 'notable-private-funds' },
    { id: 'social-security', label: '社保', layout: 'holdings' },
    { id: 'social-security-summary', label: '社保汇总', layout: 'social-security-summary' },
    { id: 'pensions', label: '养老金', layout: 'pensions' },
    { id: 'national-team', label: '汇金证金', layout: 'national-team' },
    { id: 'development-bank-holdings', label: '国开系参股', layout: 'development-bank-holdings' },
    { id: 'wutong-holdings', label: '梧桐树系持股', layout: 'named-holding' },
    { id: 'zhongke-huitong-holdings', label: '中科汇通持股', layout: 'named-holding-summary' },
    { id: 'stake-building', label: '被举牌', layout: 'stake-building' },
    { id: 'northbound', label: '北向资金', layout: 'holdings' },
    { id: 'brokers', label: '券商', layout: 'holdings' },
    { id: 'insurers', label: '保险', layout: 'holdings' },
    { id: 'banks', label: '银行', layout: 'holdings' },
    { id: 'qfii', label: 'QFII', layout: 'holdings' },
    { id: 'trusts', label: '信托', layout: 'holdings' },
    { id: 'annuities', label: '年金', layout: 'holdings' },
    { id: 'finance-companies', label: '财务公司', layout: 'holdings' },
    { id: 'general-corporates', label: '一般法人', layout: 'holdings' },
    { id: 'special-corporates', label: '特殊法人', layout: 'holdings' },
    { id: 'float-structure', label: '浮筹结构', layout: 'float-structure' },
    { id: 'crowded-oversold', label: '抱团股超跌', layout: 'crowded-oversold' }
  ];

  let view = $state('all');
  let query = $state('');
  const resource = new Resource<MarketInstitutionAnalysisDocument>();
  const doc = $derived(resource.data);
  const section = $derived(doc?.sections[0] ?? null);
  const rows = $derived(section?.records ?? []);
  const layout = $derived(VIEWS.find((item) => item.id === view)?.layout ?? 'holdings');

  const stats = $derived.by<Stat[]>(() => {
    const summary = section?.summary;
    if (!summary) return [];
    const base: Stat[] = [
      { label: '业务明细', value: count(summary.rows), note: `${count(summary.unique_securities)} 只股票` },
      { label: '名称解析', value: `${count(summary.names_resolved)} / ${count(summary.rows)}` },
      { label: '报告期', value: summary.report_dates.map(date).join('、') || '—' },
      { label: '来源', value: text(section?.resource), note: `${count(section?.source_rows)} 行` }
    ];
    if (layout === 'exclusive-funds') {
      base.splice(2, 0,
        { label: '基金公司', value: count(summary.fund_management_companies) },
        { label: '持股合计', value: compact(summary.holding_shares, '股'), note: '同股多公司不去重' });
    } else if (layout === 'notable-private-funds') {
      base.splice(2, 0,
        { label: '私募管理人', value: count(summary.private_fund_managers) },
        { label: '持仓市值', value: compact(summary.holding_market_value, '元'), note: `${count(summary.industries)} 个行业` });
    } else if (layout === 'national-team') {
      base.splice(2, 0,
        { label: '证金持股', value: count(summary.china_securities_finance_securities) },
        { label: '汇金持股', value: count(summary.central_huijin_securities) },
        { label: '合计公式', value: summary.combined_formula_mismatches === 0 ? '零偏差' : `${count(summary.combined_formula_mismatches)} 条偏差`, note: `${count(summary.combined_formula_checked)} 条已核验` });
    } else if (layout === 'stake-building') {
      base.splice(2, 0,
        { label: '明确继续增持', value: count(summary.further_increase_yes) },
        { label: '险资举牌', value: count(summary.insurance_capital_yes) });
    }
    return base;
  });

  function load(refresh = false) {
    void resource.load(`/api/v1/market/institution-analysis?${queryString({
      view, q: query.trim(), limit: 5000, refresh: refresh ? 1 : 0
    })}`);
  }

  function openSecurity(row: InstitutionAnalysisRecord) {
    app.setStock({ market: row.market, code: row.code, name: row.name });
    router.go(stockPath(row.market, row.code, 'institution-holdings'));
  }

  const columns = $derived.by<Column<InstitutionAnalysisRecord>[]>(() => {
    const security: Column<InstitutionAnalysisRecord> = {
      key: 'security', label: '股票', width: '138px',
      value: (row) => row.name || row.code, sub: (row) => row.security_id
    };
    const report: Column<InstitutionAnalysisRecord> = {
      key: 'report', label: '报告期', width: '92px', num: true,
      value: (row) => date(row.data.report_date), sortValue: (row) => row.data.report_date ?? ''
    };
    if (layout === 'exclusive-funds') return [security,
      { key: 'company', label: '持有基金管理公司', value: (row) => text(row.data.fund_management_company), wrap: true },
      report,
      { key: 'shares', label: '持股数量', align: 'right', num: true, value: (row) => compact(row.data.holding_shares, '股'), sortValue: (row) => row.data.holding_shares ?? 0 },
      { key: 'ratio', label: '占流通', align: 'right', num: true, value: (row) => percent(row.data.float_share_pct), sortValue: (row) => row.data.float_share_pct ?? 0 }
    ];
    if (layout === 'notable-private-funds') return [security,
      { key: 'manager', label: '知名私募 / 管理人', value: (row) => text(row.data.private_fund_manager), wrap: true },
      report,
      { key: 'value', label: '持仓市值', align: 'right', num: true, value: (row) => compact(row.data.holding_market_value, '元'), sortValue: (row) => row.data.holding_market_value ?? 0 },
      { key: 'ratio', label: '占流通', align: 'right', num: true, value: (row) => percent(row.data.float_share_pct), sortValue: (row) => row.data.float_share_pct ?? 0 },
      { key: 'industry', label: '行业', value: (row) => text(row.data.industry) }
    ];
    if (layout === 'national-team') return [security, report,
      { key: 'csf', label: '证金占比', align: 'right', num: true, value: (row) => percent(row.data.china_securities_finance_ratio_pct), sortValue: (row) => row.data.china_securities_finance_ratio_pct ?? 0 },
      { key: 'huijin', label: '汇金占比', align: 'right', num: true, value: (row) => percent(row.data.central_huijin_ratio_pct), sortValue: (row) => row.data.central_huijin_ratio_pct ?? 0 },
      { key: 'combined', label: '合计占比', align: 'right', num: true, value: (row) => percent(row.data.combined_ratio_pct), sortValue: (row) => row.data.combined_ratio_pct ?? 0 },
      { key: 'rank', label: '股东名次', width: '126px', value: (row) => text(row.data.holder_rank) },
      { key: 'industry', label: '行业', width: '100px', value: (row) => text(row.data.industry) }
    ];
    if (layout === 'social-security-summary') return [security, report,
      { key: 'institutions', label: '社保机构数', align: 'right', num: true, value: (row) => count(row.data.institution_count), sortValue: (row) => row.data.institution_count ?? 0 },
      { key: 'shares', label: '持股数量', align: 'right', num: true, value: (row) => compact(row.data.holding_shares, '股'), sortValue: (row) => row.data.holding_shares ?? 0 },
      { key: 'ratio', label: '占总股本', align: 'right', num: true, value: (row) => percent(row.data.total_share_pct), sortValue: (row) => row.data.total_share_pct ?? 0 }
    ];
    if (layout === 'development-bank-holdings') return [security, report,
      { key: 'position', label: '参股 / 股东名次', width: '360px', value: (row) => text(row.data.shareholder_position), wrap: true },
      { key: 'detail', label: '披露持仓详情', value: (row) => text(row.data.holding_detail), wrap: true }
    ];
    if (layout === 'named-holding') return [security, report,
      { key: 'position', label: '持股披露 / 股东名次', value: (row) => text(row.data.shareholder_position), wrap: true }
    ];
    if (layout === 'named-holding-summary') return [security, report,
      { key: 'type', label: '持股类型', value: (row) => text(row.data.holding_type) },
      { key: 'shares', label: '持股数量', align: 'right', num: true, value: (row) => compact(row.data.holding_shares, '股'), sortValue: (row) => row.data.holding_shares ?? 0 },
      { key: 'ratio', label: '占流通', align: 'right', num: true, value: (row) => percent(row.data.float_share_pct), sortValue: (row) => row.data.float_share_pct ?? 0 },
      { key: 'value', label: '持仓市值', align: 'right', num: true, value: (row) => compact(row.data.holding_market_value, '元'), sortValue: (row) => row.data.holding_market_value ?? 0 },
      { key: 'profit', label: '净利增长', align: 'right', num: true, value: (row) => percent(row.data.net_profit_growth_pct), tone: (row) => tone(row.data.net_profit_growth_pct) },
      { key: 'industry', label: '行业', value: (row) => text(row.data.industry) }
    ];
    if (layout === 'stake-building') return [security,
      { ...report, label: '公告日期' },
      { key: 'shareholder', label: '举牌股东', width: '190px', value: (row) => text(row.data.shareholder), wrap: true },
      { key: 'period', label: '增持期间', width: '190px', value: (row) => `${date(row.data.start_date)} → ${date(row.data.end_date)}` },
      { key: 'return', label: '期间股价', align: 'right', num: true, value: (row) => percent(row.data.period_return_pct, 2, true), tone: (row) => tone(row.data.period_return_pct), sortValue: (row) => row.data.period_return_pct ?? 0 },
      { key: 'increase', label: '增持股数 / 占比', align: 'right', num: true, value: (row) => compact(row.data.increase_shares, '股'), sub: (row) => percent(row.data.increase_total_pct) },
      { key: 'post', label: '增持后持股 / 占比', align: 'right', num: true, value: (row) => compact(row.data.post_holding_shares, '股'), sub: (row) => percent(row.data.post_holding_total_pct) },
      { key: 'flags', label: '后续 / 险资', value: (row) => `${text(row.data.further_increase)} / ${text(row.data.insurance_capital)}` }
    ];
    if (layout === 'pensions') return [security, report,
      { key: 'shares', label: '持股数量', align: 'right', num: true, value: (row) => `${fixed(row.data.holding_shares_10k, 2)} 万股`, sortValue: (row) => row.data.holding_shares_10k ?? 0 },
      { key: 'ratio', label: '占流通', align: 'right', num: true, value: (row) => percent(row.data.float_share_pct), sortValue: (row) => row.data.float_share_pct ?? 0 },
      { key: 'value', label: '持仓市值', align: 'right', num: true, value: (row) => `${fixed(row.data.holding_market_value_10k, 2)} 万元`, sortValue: (row) => row.data.holding_market_value_10k ?? 0 },
      { key: 'profit', label: '净利增长', align: 'right', num: true, value: (row) => percent(row.data.net_profit_growth_pct), tone: (row) => tone(row.data.net_profit_growth_pct) }
    ];
    if (layout === 'float-structure') return [security, report,
      { key: 'free', label: '自由流通股', align: 'right', num: true, value: (row) => compact(row.data.free_float_shares, '股'), sortValue: (row) => row.data.free_float_shares ?? 0 },
      { key: 'freeRatio', label: '自由流通占比', align: 'right', num: true, value: (row) => percent((row.data.free_float_to_float_ratio ?? 0) * 100), sortValue: (row) => row.data.free_float_to_float_ratio ?? 0 },
      { key: 'institution', label: '机构占流通', align: 'right', num: true, value: (row) => percent((row.data.institution_to_float_ratio ?? 0) * 100), sortValue: (row) => row.data.institution_to_float_ratio ?? 0 },
      { key: 'major', label: '大股东占流通', align: 'right', num: true, value: (row) => percent((row.data.major_holder_to_float_ratio ?? 0) * 100), sortValue: (row) => row.data.major_holder_to_float_ratio ?? 0 }
    ];
    const result: Column<InstitutionAnalysisRecord>[] = [security, report,
      { key: 'value', label: '持仓市值', align: 'right', num: true, value: (row) => compact(row.data.holding_market_value, '元'), sortValue: (row) => row.data.holding_market_value ?? 0 },
      { key: 'institutions', label: '机构家数', align: 'right', num: true, value: (row) => count(row.data.institution_count), sub: (row) => `环比 ${count(row.data.institution_count_change)}`, tone: (row) => tone(row.data.institution_count_change), sortValue: (row) => row.data.institution_count ?? 0 },
      { key: 'shares', label: '持股数量', align: 'right', num: true, value: (row) => compact(row.data.holding_shares, '股'), sub: (row) => `变化 ${compact(row.data.holding_shares_change, '股')}`, tone: (row) => tone(row.data.holding_shares_change), sortValue: (row) => row.data.holding_shares ?? 0 },
      { key: 'float', label: '占流通', align: 'right', num: true, value: (row) => percent((row.data.float_share_ratio ?? 0) * 100), sortValue: (row) => row.data.float_share_ratio ?? 0 },
      { key: 'total', label: '占总股本', align: 'right', num: true, value: (row) => percent((row.data.total_share_ratio ?? 0) * 100), sortValue: (row) => row.data.total_share_ratio ?? 0 }
    ];
    if (layout === 'crowded-oversold') result.push({ key: 'drawdown', label: '距高点', align: 'right', num: true, value: (row) => percent(row.data.drawdown_from_high_pct), tone: () => 'down', sortValue: (row) => row.data.drawdown_from_high_pct ?? 0 });
    return result;
  });

  onMount(() => load());
</script>

<PageHeader eyebrow="CGFX × 17 · SPECIAL TABLES × 8" title="机构持仓全景" description="二十五张通达信机构与投资参股主表统一查看：机构分类、社保汇总、国开/梧桐树/中科汇通持股、被举牌、基金独门、知名私募、汇金证金、养老金、北向、浮筹与抱团超跌。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新当前主表</Button>{/snippet}
</PageHeader>

<Panel title={VIEWS.find((item) => item.id === view)?.label ?? '机构持仓'} subtitle={section ? `${count(section.matched)} 条业务明细 · 点击股票进入个股反查` : ''} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="当前口径没有记录。" flush scroll fill>
  {#snippet toolbar()}
    <Select options={VIEWS} value={view} width="170px" label="机构口径" onChange={(next) => { view = next; query = ''; load(); }} />
    <TextInput bind:value={query} icon="search" width="260px" label="检索" placeholder="股票 / 基金公司 / 行业" onEnter={() => load()} />
  {/snippet}
  <DataTable {columns} {rows} stickyFirst numbered rowKey={(row, index) => `${row.security_id}:${row.data.fund_management_company ?? ''}:${index}`} onRowClick={openSecurity} />
</Panel>
