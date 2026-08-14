<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, delta, money, percent } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { MarketSpecializedMetricsDocument, SpecializedMetricsRecord } from '../../types';

  type View = MarketSpecializedMetricsDocument['view'];
  const VIEWS = [
    { id: 'banks', label: '银行' },
    { id: 'securities', label: '券商' },
    { id: 'insurers', label: '保险' },
    { id: 'all', label: '全部' }
  ];

  let view = $state<View>('banks');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketSpecializedMetricsDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);

  const stats = $derived.by<Stat[]>(() => doc ? [
    { label: '银行', value: count(doc.summary.banks) },
    { label: '券商', value: count(doc.summary.securities) },
    { label: '保险', value: count(doc.summary.insurers) },
    { label: '覆盖证券', value: count(doc.summary.unique_securities) },
    { label: '最新期间', value: date(doc.summary.latest_date) }
  ] : []);

  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/specialized-metrics?${queryString({
      view, q: query.trim(), limit: 5000, refresh: refresh ? 1 : 0
    })}`);
  }
  function switchView(next: string) { view = next as View; load(); }
  function name(row: SpecializedMetricsRecord) { return row.security.name || row.security.code; }
  function openStock() {
    if (selected) router.go(stockPath(selected.security.market, selected.security.code, 'specialized-metrics'));
  }

  const bankColumns: Column<SpecializedMetricsRecord>[] = [
    { key: 'security', label: '银行', width: '160px', value: name, sub: (row) => row.security.security_id },
    { key: 'date', label: '报告期', width: '100px', num: true, value: (row) => date(row.date) },
    { key: 'capital', label: '资本净额', align: 'right', num: true, value: (row) => money(row.capital_net_yuan), sub: (row) => `充足率 ${percent(row.capital_adequacy_ratio_pct)}` },
    { key: 'core', label: '核心一级资本', align: 'right', num: true, value: (row) => money(row.core_tier1_capital_net_yuan), sub: (row) => percent(row.core_tier1_adequacy_ratio_pct) },
    { key: 'deposits', label: '存款 / 贷款', align: 'right', num: true, value: (row) => money(row.deposits_yuan), sub: (row) => money(row.loans_yuan) },
    { key: 'npl', label: '不良 / 拨备覆盖', align: 'right', num: true, value: (row) => percent(row.nonperforming_loan_ratio_pct), sub: (row) => percent(row.provision_coverage_ratio_pct) },
    { key: 'margin', label: '净息差 / 净利差', align: 'right', num: true, value: (row) => percent(row.net_interest_margin_ratio_pct), sub: (row) => percent(row.net_interest_spread_ratio_pct) }
  ];
  const brokerColumns: Column<SpecializedMetricsRecord>[] = [
    { key: 'security', label: '券商', width: '160px', value: name, sub: (row) => row.security.security_id },
    { key: 'date', label: '经营月份', width: '100px', num: true, value: (row) => date(row.date) },
    { key: 'revenue', label: '月营收', align: 'right', num: true, value: (row) => money(row.monthly_revenue_yuan), sub: (row) => delta(row.monthly_revenue_yoy_pct) },
    { key: 'profit', label: '月净利润', align: 'right', num: true, value: (row) => money(row.monthly_net_profit_yuan), sub: (row) => delta(row.monthly_net_profit_yoy_pct) },
    { key: 'capital', label: '净资本 / 净资产', align: 'right', num: true, value: (row) => money(row.net_capital_yuan), sub: (row) => money(row.net_assets_yuan) },
    { key: 'commission', label: '手续费佣金', align: 'right', num: true, value: (row) => money(row.commission_income_yuan), sub: (row) => `经纪 ${money(row.brokerage_income_yuan)}` },
    { key: 'aum', label: '受托资产规模', align: 'right', num: true, value: (row) => money(row.entrusted_asset_scale_yuan) }
  ];
  const insurerColumns: Column<SpecializedMetricsRecord>[] = [
    { key: 'security', label: '保险公司', width: '160px', value: name, sub: (row) => row.security.security_id },
    { key: 'date', label: '报告期', width: '100px', num: true, value: (row) => date(row.date) },
    { key: 'ev', label: '内含价值', align: 'right', num: true, value: (row) => money(row.embedded_value_yuan), sub: (row) => `新业务 ${money(row.new_business_value_after_cost_yuan)}` },
    { key: 'solvency', label: '核心 / 综合偿付率', align: 'right', num: true, value: (row) => percent(row.core_solvency_adequacy_ratio_pct), sub: (row) => percent(row.combined_solvency_adequacy_ratio_pct) },
    { key: 'persistency', label: '13 / 25月继续率', align: 'right', num: true, value: (row) => percent(row.persistency_13m_ratio_pct), sub: (row) => percent(row.persistency_25m_ratio_pct) },
    { key: 'yield', label: '净 / 总投资收益率', align: 'right', num: true, value: (row) => percent(row.net_investment_yield_ratio_pct), sub: (row) => percent(row.total_investment_yield_ratio_pct) },
    { key: 'assets', label: '投资资产', align: 'right', num: true, value: (row) => money(row.total_investments_yuan), sub: (row) => `权益 ${money(row.equity_investments_yuan)}` }
  ];
  const allColumns: Column<SpecializedMetricsRecord>[] = [
    { key: 'kind', label: '类别', width: '130px', value: (row) => row.kind_label },
    { key: 'security', label: '证券', width: '170px', value: name, sub: (row) => row.security.security_id },
    { key: 'date', label: '业务期间', width: '110px', num: true, value: (row) => date(row.date) },
    { key: 'fields', label: '已还原指标数', align: 'right', num: true, value: (row) => count(Object.keys(row).filter((key) => !['raw', 'security'].includes(key)).length) },
    { key: 'source', label: '客户端资源', width: '230px', value: (row) => row.source_resource }
  ];
  const columns = $derived(view === 'banks' ? bankColumns : view === 'securities' ? brokerColumns : view === 'insurers' ? insurerColumns : allColumns);

  const detailStats = $derived.by<Stat[]>(() => {
    if (!selected) return [];
    if (selected.kind === 'banks') return [
      { label: '资本充足率', value: percent(selected.capital_adequacy_ratio_pct) },
      { label: '不良贷款率', value: percent(selected.nonperforming_loan_ratio_pct) },
      { label: '拨备覆盖率', value: percent(selected.provision_coverage_ratio_pct) },
      { label: '净息差', value: percent(selected.net_interest_margin_ratio_pct) }
    ];
    if (selected.kind === 'securities') return [
      { label: '月营收', value: money(selected.monthly_revenue_yuan) },
      { label: '营收同比', value: delta(selected.monthly_revenue_yoy_pct) },
      { label: '月净利润', value: money(selected.monthly_net_profit_yuan) },
      { label: '净利润同比', value: delta(selected.monthly_net_profit_yoy_pct) }
    ];
    return [
      { label: '内含价值', value: money(selected.embedded_value_yuan) },
      { label: '核心偿付率', value: percent(selected.core_solvency_adequacy_ratio_pct) },
      { label: '综合偿付率', value: percent(selected.combined_solvency_adequacy_ratio_pct) },
      { label: '净投资收益率', value: percent(selected.net_investment_yield_ratio_pct) }
    ];
  });

  onMount(() => load());
</script>

<PageHeader eyebrow="HYJYFX · 3 LIVE SOURCES" title="金融行业专项指标" description="直接还原客户端银行、券商、保险三套专用经营表。金额统一为元，比例同时提供源小数和明确百分比口径。" {stats}>
  {#snippet actions()}
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="专项指标视图" />
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="390px">
  {#snippet main()}
    <Panel title="专项经营指标" subtitle={doc ? `${count(doc.match_count)} 家 · ${count(doc.sources.length)} 个客户端资源` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选下没有记录。" flush scroll>
      {#snippet toolbar()}<TextInput bind:value={query} icon="search" width="260px" label="检索公司" placeholder="代码或名称" onEnter={() => load()} />{/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedId = row.event_id)} isActive={(row) => row.event_id === selected?.event_id} stickyFirst numbered minWidth="1050px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.kind_label ?? 'SPECIALIZED METRICS'} title={selected ? name(selected) : '指标详情'} subtitle={selected?.source_resource ?? '选择左侧公司'} empty={!selected && !resource.busy}>
      {#if selected}
        <button type="button" onclick={openStock}>在个股工作台打开</button>
        <StatGrid stats={detailStats} inline />
        <p>报告期 {date(selected.date)}；原始字段 {count(Object.keys(selected.raw).length)} 个。原始行随 API 返回，可逐字段审计。</p>
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  button { width: 100%; height: 24px; margin-bottom: var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  p { font-size: var(--fs-xs); line-height: 1.7; color: var(--fg-mute); }
</style>
