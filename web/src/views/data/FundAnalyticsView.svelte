<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import ResearchSeriesChart from '../../charts/ResearchSeriesChart.svelte';
  import { count, text } from '../../lib/fmt';
  import {
    FUND_ANALYTICS_VIEWS,
    FUND_BENCHMARK_OPTIONS,
    FUND_STYLE_OPTIONS,
    fundViewDefinition,
    type FundAnalyticsDocument,
    type FundAnalyticsRecord,
    type FundAnalyticsView
  } from '../../lib/fundAnalytics';
  import { recordColumns, valueAt } from '../../lib/records';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';

  const DETAIL_BY_MASTER: Partial<Record<FundAnalyticsView, FundAnalyticsView>> = {
    risk: 'risk-history',
    'monthly-risk': 'monthly-history',
    'reported-holdings': 'reported-holding-securities',
    'holdings-stability': 'holding-history'
  };

  const VIEW_OPTIONS = FUND_ANALYTICS_VIEWS.map((item) => ({
    id: item.id,
    label: `${item.group} · ${item.label}`
  }));

  let view = $state<FundAnalyticsView>('risk');
  let fundCode = $state('000711');
  let query = $state('');
  let style = $state('005001');
  let benchmark = $state('0');
  let startDate = $state('');
  let endDate = $state('');
  let reportDate = $state('');
  let estimateDate = $state('');

  const result = new Resource<FundAnalyticsDocument>();
  const definition = $derived(fundViewDefinition(view));
  const rows = $derived(result.data?.records ?? []);
  const selectedFund = $derived(
    definition.requiresFund
      ? fundCode
      : rows.find((row) => row.fund?.code === fundCode)?.fund?.code ?? ''
  );

  const columns = $derived<Column<FundAnalyticsRecord>[]>(
    recordColumns<FundAnalyticsRecord>(definition.columns)
  );

  const stats = $derived.by<Stat[]>(() => [
    { label: '当前视图', value: definition.label, note: definition.group },
    { label: '返回记录', value: count(result.data?.counts.returned ?? rows.length) },
    { label: '基金代码', value: selectedFund || '未选择', note: definition.requiresFund ? '明细视图' : '点击基金可下钻' },
    { label: '来源状态', value: result.data?.availability === 'live' ? '在线' : text(result.data?.availability) }
  ]);

  async function load(refresh = false) {
    if (definition.requiresFund && !/^\d{6}$/.test(fundCode.trim())) {
      await result.loadWith(() => Promise.reject(new Error('基金明细需要六位基金代码')));
      return;
    }
    await result.load(`/api/v1/market/fund-analytics?${queryString({
      view,
      q: query.trim(),
      fund_code: definition.requiresFund ? fundCode.trim() : '',
      style,
      benchmark,
      start: definition.usesRange ? startDate.trim() : '',
      end: definition.usesRange ? endDate.trim() : '',
      report_date: definition.usesReportDate ? reportDate.trim() : '',
      estimate_date: definition.usesEstimateDate ? estimateDate.trim() : '',
      limit: 10000,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function switchView(next: string) {
    view = next as FundAnalyticsView;
    result.reset();
    void load();
  }

  function selectRow(row: FundAnalyticsRecord) {
    const code = row.fund?.code;
    if (!code) return;
    fundCode = code;
    const detail = DETAIL_BY_MASTER[view];
    if (detail) {
      view = detail;
      result.reset();
      void load();
    }
  }

  function rowKey(row: FundAnalyticsRecord, index: number): string {
    return row.fund?.fund_id || String(valueAt(row, 'security_code') ?? '') ||
      String(valueAt(row, 'date') ?? valueAt(row, 'month') ?? valueAt(row, 'report_date') ?? index);
  }

  onMount(() => void load());
</script>

<PageHeader
  eyebrow="module_risk_return.dll · TQLEX 500030—500062"
  title="基金风险与持仓分析"
  description="十三类公开基金风险、择时选股、报告期持仓、持仓稳定性和仓位估算视图。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={result.busy} onclick={() => void load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Panel
  title={definition.label}
  subtitle={`${definition.description}${definition.requiresFund ? ` · 基金 ${fundCode}` : ' · 点击基金行进入对应明细'}`}
  busy={result.busy}
  error={result.error}
  onRetry={() => void load()}
  empty={result.loaded && rows.length === 0}
  emptyText="当前条件没有返回基金分析记录。"
  fill
>
  {#snippet toolbar()}
    <Select options={VIEW_OPTIONS} value={view} width="285px" label="分析视图" onChange={switchView} />
    <TextInput bind:value={fundCode} width="115px" label="基金代码" placeholder="基金代码" onEnter={() => void load()} />
    {#if !definition.requiresFund && view !== 'market-position-history'}
      <TextInput bind:value={query} icon="search" width="190px" label="检索" placeholder="基金/公司/经理" onEnter={() => void load()} />
      <Select options={FUND_STYLE_OPTIONS} value={style} width="150px" label="基金类型" onChange={(next) => { style = next; void load(); }} />
    {/if}
    {#if ['risk', 'risk-history', 'monthly-risk', 'monthly-history', 'selection-skill'].includes(view)}
      <Select options={FUND_BENCHMARK_OPTIONS} value={benchmark} width="145px" label="比较基准" onChange={(next) => { benchmark = next; void load(); }} />
    {/if}
    {#if definition.usesRange}
      <TextInput bind:value={startDate} width="112px" label="开始日期" placeholder="开始 YYYYMMDD" onEnter={() => void load()} />
      <TextInput bind:value={endDate} width="112px" label="结束日期" placeholder="结束 YYYYMMDD" onEnter={() => void load()} />
    {:else if definition.usesReportDate}
      <TextInput bind:value={reportDate} width="128px" label="报告期" placeholder="报告期 YYYYMMDD" onEnter={() => void load()} />
    {:else if definition.usesEstimateDate}
      <TextInput bind:value={estimateDate} width="128px" label="估算日期" placeholder="估算日 YYYYMMDD" onEnter={() => void load()} />
    {/if}
    <Button variant="primary" onclick={() => void load()}>查询</Button>
  {/snippet}

  <div class:with-chart={Boolean(definition.chart)} class="stage">
    {#if definition.chart}
      <div class="chart-host">
        <ResearchSeriesChart {rows} definition={definition.chart} title={definition.label} />
      </div>
    {/if}
    <div class="table-host">
      <DataTable
        {columns}
        {rows}
        {rowKey}
        numbered
        stickyFirst
        minWidth="860px"
        maxHeight={definition.chart ? '38vh' : 'calc(100vh - 270px)'}
        onRowClick={selectRow}
      />
    </div>
  </div>
</Panel>

<style>
  .stage { display: flex; flex: 1; min-height: 0; flex-direction: column; }
  .stage.with-chart { display: grid; grid-template-rows: minmax(260px, 1.35fr) minmax(190px, 1fr); }
  .chart-host { display: flex; min-height: 0; padding: var(--sp-3); border-bottom: 1px solid var(--line); }
  .table-host { min-height: 0; overflow: auto; }
</style>
