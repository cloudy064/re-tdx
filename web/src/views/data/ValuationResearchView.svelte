<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import ResearchSeriesChart from '../../charts/ResearchSeriesChart.svelte';
  import { count, text } from '../../lib/fmt';
  import { recordColumns, valueAt } from '../../lib/records';
  import { Resource } from '../../lib/resource.svelte';
  import { MARKET_FILTER_OPTIONS, MARKET_OPTIONS } from '../../lib/marketOptions';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import {
    EQUITY_VIEWS,
    MONTH_OPTIONS,
    PE_TYPE_OPTIONS,
    RELATIVE_BENCHMARKS,
    RELATIVE_INDEX_TYPES,
    RELATIVE_METHODS,
    VALUATION_DOMAINS,
    VALUATION_TABLES,
    VOLATILITY_WINDOWS,
    type EquityValuationView,
    type RelativeValuationDocument,
    type ValuationDomain,
    type ValuationRecord,
    type ValuationRecordsDocument,
    type VolatilityView
  } from '../../lib/valuationResearch';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';

  type ResearchDocument = RelativeValuationDocument | ValuationRecordsDocument;

  let domain = $state<ValuationDomain>('relative');
  let startDate = $state('');
  let endDate = $state('');

  let relativeCode = $state('');
  let indexType = $state('broad');
  let relativeBenchmark = $state('000001');
  let relativeMethod = $state('pe-ttm');

  let equityView = $state<EquityValuationView>('pe-industries');
  let securityMarket = $state('sz');
  let securityCode = $state('000001');
  let industryCode = $state('881001');
  let industryMarket = $state('1');
  let peType = $state('ttm');
  let requiredReturn = $state('3');

  let volatilityView = $state<VolatilityView>('catalog');
  let volatilityQuery = $state('');
  let indexMarket = $state('sh');
  let indexCode = $state('999999');
  let volatilityWindow = $state('5');

  let totalQuery = $state('');
  let totalMarket = $state('');
  let totalCode = $state('');
  let totalYear = $state(String(new Date().getFullYear()));
  let totalMonth = $state('0');

  const result = new Resource<ResearchDocument>();

  const tableKey = $derived(
    domain === 'relative'
      ? (relativeCode ? 'relative-history' : 'relative-master')
      : domain === 'equity'
        ? `equity-${equityView}`
        : domain === 'volatility'
          ? (volatilityView === 'catalog' ? 'volatility-catalog' : 'volatility-history')
          : 'total-return'
  );
  const definition = $derived(VALUATION_TABLES[tableKey]);

  const rows = $derived.by<ValuationRecord[]>(() => {
    const document = result.data;
    if (!document) return [];
    if (domain === 'relative') {
      const relative = document as RelativeValuationDocument;
      return relativeCode ? relative.history ?? [] : relative.indices ?? [];
    }
    const records = (document as ValuationRecordsDocument).records ?? [];
    if (domain === 'volatility' && volatilityView === 'security') {
      const history = records[0]?.history;
      return Array.isArray(history) ? history as ValuationRecord[] : [];
    }
    return records;
  });

  const columns = $derived<Column<ValuationRecord>[]>(
    recordColumns<ValuationRecord>(definition.columns)
  );

  const selectedIdentity = $derived(
    domain === 'relative' ? relativeCode || '目录'
      : domain === 'equity'
        ? (equityView === 'pe-security-history' ? `${securityMarket.toUpperCase()}${securityCode}`
          : equityView.endsWith('members') ? industryCode : '行业目录')
        : domain === 'volatility'
          ? (volatilityView === 'security' ? `${indexMarket.toUpperCase()}${indexCode}` : '指数目录')
          : totalCode || '全部指数'
  );

  const stats = $derived.by<Stat[]>(() => [
    { label: '研究模型', value: VALUATION_DOMAINS.find((item) => item.id === domain)?.label ?? domain },
    { label: '当前选择', value: selectedIdentity },
    { label: '显示记录', value: count(rows.length) },
    { label: '来源状态', value: result.data?.availability === 'live' ? '在线' : text(result.data?.availability) }
  ]);

  async function localError(message: string) {
    await result.loadWith(() => Promise.reject(new Error(message)));
  }

  async function load(refresh = false) {
    let path = '';
    if (domain === 'relative') {
      if (relativeCode && !/^\d{6}$/.test(relativeCode)) return localError('指数代码必须是六位数字');
      path = `/api/v1/market/relative-valuation?${queryString({
        code: relativeCode, index_type: indexType, benchmark: relativeBenchmark,
        method: relativeMethod, start: startDate, end: endDate,
        limit: 4000, refresh: refresh ? 1 : 0
      })}`;
    } else if (domain === 'equity') {
      const history = equityView === 'pe-security-history';
      const members = equityView.endsWith('members');
      if (history && !/^\d{6}$/.test(securityCode)) return localError('个股历史需要六位股票代码');
      if (members && !/^\d{6}$/.test(industryCode)) return localError('行业成分视图需要六位行业代码');
      path = `/api/v1/market/equity-valuation?${queryString({
        view: equityView,
        market: history ? securityMarket : '', code: history ? securityCode : '',
        industry: members ? industryCode : '', industry_market: members ? industryMarket : '',
        start: startDate, end: endDate, pe_type: peType,
        required_return_rate: requiredReturn, limit: 4000, refresh: refresh ? 1 : 0
      })}`;
    } else if (domain === 'volatility') {
      if (volatilityView === 'security' && !/^\d{6}$/.test(indexCode))
        return localError('波动率历史需要六位指数代码');
      path = `/api/v1/market/index-volatility?${queryString({
        view: volatilityView, q: volatilityQuery,
        market: volatilityView === 'security' ? indexMarket : '',
        code: volatilityView === 'security' ? indexCode : '',
        start: startDate, end: endDate, window: volatilityWindow,
        limit: 10000, refresh: refresh ? 1 : 0
      })}`;
    } else {
      if (totalCode && !/^\d{6}$/.test(totalCode)) return localError('价格指数代码必须是六位数字');
      path = `/api/v1/market/total-return-gap?${queryString({
        q: totalQuery, market: totalMarket, code: totalCode,
        year: totalYear, month: totalMonth, limit: 1000,
        refresh: refresh ? 1 : 0
      })}`;
    }
    await result.load(path);
  }

  function switchDomain(next: string) {
    domain = next as ValuationDomain;
    result.reset();
    void load();
  }

  function openSecurity(row: ValuationRecord): boolean {
    const market = String(valueAt(row, 'security.market') ?? '');
    const code = String(valueAt(row, 'security.code') ?? '');
    if (!market || !code) return false;
    const name = String(valueAt(row, 'security.name') ?? code);
    app.setStock({ market, code, name });
    router.go(stockPath(market, code));
    return true;
  }

  function selectRow(row: ValuationRecord) {
    if (domain === 'relative' && !relativeCode) {
      relativeCode = String(valueAt(row, 'security.code') ?? '');
      if (relativeCode) void load();
      return;
    }
    if (domain === 'volatility' && volatilityView === 'catalog') {
      indexMarket = String(valueAt(row, 'index.market') ?? 'sh');
      indexCode = String(valueAt(row, 'index.code') ?? '');
      if (indexCode) {
        volatilityView = 'security';
        void load();
      }
      return;
    }
    if (domain === 'equity' && (equityView === 'pe-industries' || equityView === 'pb-roe-industries')) {
      industryCode = String(valueAt(row, 'industry.code') ?? '');
      industryMarket = String(valueAt(row, 'industry.market_id') ?? '1');
      equityView = equityView === 'pe-industries' ? 'pe-industry-members' : 'pb-roe-members';
      void load();
      return;
    }
    if (domain === 'equity') openSecurity(row);
  }

  function clearDetail() {
    if (domain === 'relative') relativeCode = '';
    if (domain === 'volatility') volatilityView = 'catalog';
    result.reset();
    void load();
  }

  function rowKey(row: ValuationRecord, index: number): string {
    return String(
      valueAt(row, 'security.security_id') ?? valueAt(row, 'industry.industry_id') ??
      valueAt(row, 'index.index_id') ?? valueAt(row, 'pair_id') ??
      valueAt(row, 'date') ?? index
    );
  }

  onMount(() => void load());
</script>

<PageHeader
  eyebrow="TQLEX / PBRPC · 200000—200305"
  title="估值与指数研究"
  description="相对估值、个股/行业估值、已实现波动率和全收益指数差的统一研究工作台。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={result.busy} onclick={() => void load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<div class="domain-bar">
  <Segmented options={VALUATION_DOMAINS} value={domain} onChange={switchDomain} ariaLabel="估值研究模型" />
</div>

<Panel
  title={definition.title}
  subtitle={definition.description}
  busy={result.busy}
  error={result.error}
  onRetry={() => void load()}
  empty={result.loaded && rows.length === 0}
  emptyText="当前参数没有返回估值研究记录。"
  fill
>
  {#snippet toolbar()}
    {#if domain === 'relative'}
      <Select options={RELATIVE_INDEX_TYPES} value={indexType} width="125px" label="指数类型" onChange={(next) => { indexType = next; relativeCode = ''; void load(); }} />
      <Select options={RELATIVE_BENCHMARKS} value={relativeBenchmark} width="145px" label="比较基准" onChange={(next) => { relativeBenchmark = next; relativeCode = ''; void load(); }} />
      <Select options={RELATIVE_METHODS} value={relativeMethod} width="125px" label="估值方法" onChange={(next) => { relativeMethod = next; relativeCode = ''; void load(); }} />
      <TextInput bind:value={startDate} width="112px" label="开始日期" placeholder="开始 YYYYMMDD" onEnter={() => void load()} />
      <TextInput bind:value={endDate} width="112px" label="结束日期" placeholder="结束 YYYYMMDD" onEnter={() => void load()} />
      {#if relativeCode}<Button onclick={clearDetail}>返回目录</Button>{/if}
    {:else if domain === 'equity'}
      <Select options={EQUITY_VIEWS} value={equityView} width="170px" label="估值视图" onChange={(next) => { equityView = next as EquityValuationView; void load(); }} />
      <Select options={PE_TYPE_OPTIONS} value={peType} width="125px" label="PE口径" onChange={(next) => { peType = next; void load(); }} />
      {#if equityView === 'pe-security-history'}
        <Select options={MARKET_OPTIONS} value={securityMarket} width="110px" label="市场" onChange={(next) => securityMarket = next} />
        <TextInput bind:value={securityCode} width="105px" label="股票代码" placeholder="股票代码" onEnter={() => void load()} />
      {:else if equityView.endsWith('members')}
        <TextInput bind:value={industryCode} width="115px" label="行业代码" placeholder="行业代码" onEnter={() => void load()} />
        <TextInput bind:value={industryMarket} width="85px" label="行业市场" placeholder="市场号" onEnter={() => void load()} />
        {#if equityView === 'pe-industry-members'}
          <TextInput bind:value={requiredReturn} width="105px" label="要求回报率" placeholder="回报率%" onEnter={() => void load()} />
        {/if}
      {/if}
      {#if equityView !== 'pe-industries'}
        <TextInput bind:value={startDate} width="112px" label="开始日期" placeholder="开始 YYYYMMDD" onEnter={() => void load()} />
        <TextInput bind:value={endDate} width="112px" label="结束日期" placeholder="结束 YYYYMMDD" onEnter={() => void load()} />
      {/if}
    {:else if domain === 'volatility'}
      <Select options={VOLATILITY_WINDOWS} value={volatilityWindow} width="110px" label="滚动窗口" onChange={(next) => { volatilityWindow = next; void load(); }} />
      {#if volatilityView === 'catalog'}
        <TextInput bind:value={volatilityQuery} icon="search" width="210px" label="指数检索" placeholder="指数名称或代码" onEnter={() => void load()} />
      {:else}
        <Select options={MARKET_OPTIONS} value={indexMarket} width="110px" label="市场" onChange={(next) => indexMarket = next} />
        <TextInput bind:value={indexCode} width="105px" label="指数代码" placeholder="指数代码" onEnter={() => void load()} />
        <TextInput bind:value={startDate} width="112px" label="开始日期" placeholder="开始 YYYYMMDD" onEnter={() => void load()} />
        <TextInput bind:value={endDate} width="112px" label="结束日期" placeholder="结束 YYYYMMDD" onEnter={() => void load()} />
        <Button onclick={clearDetail}>返回目录</Button>
      {/if}
    {:else}
      <TextInput bind:value={totalQuery} icon="search" width="190px" label="指数检索" placeholder="指数名称或代码" onEnter={() => void load()} />
      <Select options={MARKET_FILTER_OPTIONS} value={totalMarket} width="125px" label="市场" onChange={(next) => { totalMarket = next; void load(); }} />
      <TextInput bind:value={totalCode} width="105px" label="指数代码" placeholder="指数代码" onEnter={() => void load()} />
      <TextInput bind:value={totalYear} width="85px" label="年份" placeholder="年份" onEnter={() => void load()} />
      <Select options={MONTH_OPTIONS} value={totalMonth} width="130px" label="月份" onChange={(next) => { totalMonth = next; void load(); }} />
    {/if}
    <Button variant="primary" onclick={() => void load()}>查询</Button>
  {/snippet}

  <div class:with-chart={Boolean(definition.chart)} class="stage">
    {#if definition.chart}
      <div class="chart-host">
        <ResearchSeriesChart {rows} definition={definition.chart} title={definition.title} />
      </div>
    {/if}
    <div class="table-host">
      <DataTable
        {columns}
        {rows}
        {rowKey}
        numbered
        stickyFirst
        minWidth="820px"
        maxHeight={definition.chart ? '38vh' : 'calc(100vh - 300px)'}
        onRowClick={selectRow}
      />
    </div>
  </div>
</Panel>

<style>
  .domain-bar { display: flex; flex: none; align-items: center; }
  .stage { display: flex; flex: 1; min-height: 0; flex-direction: column; }
  .stage.with-chart { display: grid; grid-template-rows: minmax(260px, 1.35fr) minmax(190px, 1fr); }
  .chart-host { display: flex; min-height: 0; padding: var(--sp-3); border-bottom: 1px solid var(--line); }
  .table-host { min-height: 0; overflow: auto; }
</style>
