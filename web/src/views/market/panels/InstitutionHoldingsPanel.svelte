<script lang="ts">
  /** 单票二十五类机构与投资参股主表反查，重点展示具名持股、举牌及基金关系。 */
  import { queryString } from '../../../api';
  import { compact, count, date, percent, text } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid from '../../../ui/StatGrid.svelte';
  import type {
    InstitutionAnalysisRecord,
    InstitutionAnalysisSection,
    MarketInstitutionAnalysisDocument
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketInstitutionAnalysisDocument>();
  const doc = $derived(resource.data);
  const hitSections = $derived((doc?.sections ?? []).filter((section) => section.matched > 0));
  const exclusive = $derived(hitSections.find((section) => section.view === 'exclusive-funds') ?? null);
  const notable = $derived(hitSections.find((section) => section.view === 'notable-private-funds') ?? null);
  const national = $derived(hitSections.find((section) => section.view === 'national-team') ?? null);
  const stake = $derived(hitSections.find((section) => section.view === 'stake-building') ?? null);
  const namedHoldings = $derived(hitSections.filter((section) => ['development-bank-holdings', 'wutong-holdings', 'zhongke-huitong-holdings'].includes(section.view)));
  const ordinary = $derived(hitSections.filter((section) => !['exclusive-funds', 'notable-private-funds', 'national-team', 'stake-building', 'development-bank-holdings', 'wutong-holdings', 'zhongke-huitong-holdings'].includes(section.view)));

  function load(refresh = false) {
    void resource.load(`/api/v1/market/institution-analysis?${queryString({
      view: 'security', market, code, limit: 5000, refresh: refresh ? 1 : 0
    })}`);
  }

  const exclusiveColumns: Column<InstitutionAnalysisRecord>[] = [
    { key: 'company', label: '基金管理公司', value: (row) => text(row.data.fund_management_company), wrap: true },
    { key: 'report', label: '报告期', width: '92px', num: true, value: (row) => date(row.data.report_date) },
    { key: 'shares', label: '持股数量', width: '120px', align: 'right', num: true, value: (row) => compact(row.data.holding_shares, '股'), sortValue: (row) => row.data.holding_shares ?? 0 },
    { key: 'ratio', label: '占流通', width: '90px', align: 'right', num: true, value: (row) => percent(row.data.float_share_pct), sortValue: (row) => row.data.float_share_pct ?? 0 }
  ];

  const ordinaryColumns: Column<InstitutionAnalysisSection>[] = [
    { key: 'view', label: '机构口径', value: (row) => row.label, sub: (row) => row.layout },
    { key: 'report', label: '报告期', width: '100px', num: true, value: (row) => date(row.records[0]?.data.report_date) },
    { key: 'value', label: '持仓市值', align: 'right', num: true, value: (row) => compact(row.records[0]?.data.holding_market_value, '元'), sortValue: (row) => row.records[0]?.data.holding_market_value ?? 0 },
    { key: 'count', label: '机构家数', align: 'right', num: true, value: (row) => count(row.records[0]?.data.institution_count), sortValue: (row) => row.records[0]?.data.institution_count ?? 0 },
    { key: 'shares', label: '持股数量', align: 'right', num: true, value: (row) => compact(row.records[0]?.data.holding_shares, '股'), sortValue: (row) => row.records[0]?.data.holding_shares ?? 0 }
  ];

  const notableColumns: Column<InstitutionAnalysisRecord>[] = [
    { key: 'manager', label: '知名私募 / 管理人', value: (row) => text(row.data.private_fund_manager), wrap: true },
    { key: 'report', label: '统计期', width: '92px', num: true, value: (row) => date(row.data.report_date) },
    { key: 'value', label: '持仓市值', width: '130px', align: 'right', num: true, value: (row) => compact(row.data.holding_market_value, '元'), sortValue: (row) => row.data.holding_market_value ?? 0 },
    { key: 'ratio', label: '占流通', width: '90px', align: 'right', num: true, value: (row) => percent(row.data.float_share_pct), sortValue: (row) => row.data.float_share_pct ?? 0 },
    { key: 'industry', label: '行业', width: '110px', value: (row) => text(row.data.industry) }
  ];
  const stakeColumns: Column<InstitutionAnalysisRecord>[] = [
    { key: 'date', label: '公告日', width: '92px', num: true, value: (row) => date(row.data.announcement_date) },
    { key: 'shareholder', label: '举牌股东', value: (row) => text(row.data.shareholder), wrap: true },
    { key: 'period', label: '增持期间', width: '190px', value: (row) => `${date(row.data.start_date)} → ${date(row.data.end_date)}` },
    { key: 'return', label: '期间股价', width: '88px', align: 'right', num: true, value: (row) => percent(row.data.period_return_pct, 2, true) },
    { key: 'increase', label: '增持股数 / 占比', width: '135px', align: 'right', num: true, value: (row) => compact(row.data.increase_shares, '股'), sub: (row) => percent(row.data.increase_total_pct) },
    { key: 'post', label: '增持后持股 / 占比', width: '145px', align: 'right', num: true, value: (row) => compact(row.data.post_holding_shares, '股'), sub: (row) => percent(row.data.post_holding_total_pct) },
    { key: 'flags', label: '继续增持 / 险资', width: '110px', value: (row) => `${text(row.data.further_increase)} / ${text(row.data.insurance_capital)}` }
  ];
  const namedHoldingColumns: Column<InstitutionAnalysisRecord>[] = [
    { key: 'report', label: '报告期', width: '92px', num: true, value: (row) => date(row.data.report_date) },
    { key: 'position', label: '持股披露 / 类型', value: (row) => text(row.data.shareholder_position ?? row.data.holding_type), wrap: true },
    { key: 'detail', label: '持仓详情', value: (row) => text(row.data.holding_detail), wrap: true },
    { key: 'shares', label: '持股数量', width: '125px', align: 'right', num: true, value: (row) => compact(row.data.holding_shares, '股') },
    { key: 'ratio', label: '占流通', width: '82px', align: 'right', num: true, value: (row) => percent(row.data.float_share_pct) }
  ];

  $effect(() => {
    market; code;
    load();
  });
</script>

<div class="stack">
  <Panel title={`${name} · 机构持仓命中`} subtitle={doc ? `二十五张主表命中 ${count(hitSections.length)} 类 · ${count(doc.counts.matched)} 条` : '查询二十五张公开机构与投资参股主表'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && hitSections.length === 0} emptyText="当前证券未命中二十五张机构与投资参股主表。">
    {#snippet toolbar()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新</Button>{/snippet}
    {#if national?.records[0]}
      {@const row = national.records[0]}
      <StatGrid inline stats={[
        { label: '报告期', value: date(row.data.report_date), note: text(row.data.holder_rank) },
        { label: '证金占比', value: percent(row.data.china_securities_finance_ratio_pct) },
        { label: '汇金占比', value: percent(row.data.central_huijin_ratio_pct) },
        { label: '合计占比', value: percent(row.data.combined_ratio_pct), note: row.data.combined_ratio_formula_matches ? '证金 + 汇金对账一致' : '待核验' }
      ]} />
    {:else if doc}
      <p class="muted">当前证券不在汇金证金持仓表。</p>
    {/if}
  </Panel>

  <Panel title="被举牌与增持披露" subtitle={stake ? `${count(stake.records.length)} 条举牌关系 · 区间收益按客户端公式复现` : ''} empty={!resource.busy && Boolean(doc) && !stake} emptyText="当前证券不在被举牌主表。" flush scroll>
    <DataTable columns={stakeColumns} rows={stake?.records ?? []} rowKey={(row, index) => `${row.data.announcement_date}:${row.data.shareholder}:${index}`} />
  </Panel>

  {#each namedHoldings as holding (holding.view)}
    <Panel title={holding.label} subtitle={`${count(holding.records.length)} 条具名机构持股披露`} flush scroll>
      <DataTable columns={namedHoldingColumns} rows={holding.records} rowKey={(row, index) => `${holding.view}:${row.data.report_date}:${index}`} />
    </Panel>
  {/each}

  <Panel title="知名私募持仓" subtitle={notable ? `${count(notable.records.length)} 条管理人—股票关系` : ''} empty={!resource.busy && Boolean(doc) && !notable} emptyText="当前证券不在知名私募持仓表。" flush scroll>
    <DataTable columns={notableColumns} rows={notable?.records ?? []} rowKey={(row, index) => `${row.data.private_fund_manager}:${index}`} />
  </Panel>

  <Panel title="基金独门持仓" subtitle={exclusive ? `${count(exclusive.records.length)} 家基金公司 · 同股多公司完整保留` : ''} empty={!resource.busy && Boolean(doc) && !exclusive} emptyText="当前证券不在基金独门持仓表。" flush scroll>
    <DataTable columns={exclusiveColumns} rows={exclusive?.records ?? []} rowKey={(row, index) => `${row.data.fund_management_company}:${index}`} />
  </Panel>

  <Panel title="其他机构分类命中" subtitle={`${count(ordinary.length)} 类公开主表`} empty={!resource.busy && Boolean(doc) && ordinary.length === 0} emptyText="其他十七类机构主表暂无命中。" flush scroll>
    <DataTable columns={ordinaryColumns} rows={ordinary} rowKey={(row) => row.view} />
  </Panel>
</div>

<style>
  .stack { display: flex; min-height: 0; flex-direction: column; gap: var(--sp-2); overflow: auto; }
  .muted { margin: 0; color: var(--fg-mute); font-size: var(--fs-small); }
</style>
