<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import {
    ABNORMAL_TYPE_OPTIONS,
    ANOMALY_MODES,
    BOARD_OPTIONS,
    WARNING_OPTIONS,
    WARNING_SCOPE_OPTIONS,
    anomalyModeDefinition,
    type AbnormalDetailDocument,
    type AnomalyDocument,
    type AnomalyMode,
    type AnomalyRecord
  } from '../../lib/anomalyRisk';
  import { compact, count, date, delta, percent, text } from '../../lib/fmt';
  import { MARKET_FILTER_OPTIONS } from '../../lib/marketOptions';
  import { recordColumns, valueAt } from '../../lib/records';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';

  let mode = $state<AnomalyMode>('moves');
  let board = $state('all');
  let anomalyType = $state('all');
  let eventDate = $state('');
  let query = $state('');
  let market = $state('');
  let code = $state('');
  let warning = $state('all');
  let warningScope = $state('all');
  let minimumSafety = $state('60');
  let selectedId = $state('');

  const listing = new Resource<AnomalyDocument>();
  const summary = new Resource<AbnormalDetailDocument>();
  const explanation = new Resource<AbnormalDetailDocument>();
  const definition = $derived(anomalyModeDefinition(mode));
  const rows = $derived(listing.data?.records ?? []);
  const columns = $derived<Column<AnomalyRecord>[]>(
    recordColumns<AnomalyRecord>(definition.columns)
  );
  const selectedRow = $derived(
    rows.find((row) => anomalyRowId(row) === selectedId) ?? null
  );

  function anomalyRowId(row: AnomalyRecord): string {
    return `${valueAt(row, 'security.security_id') ?? ''}:${valueAt(row, 'anomaly.code') ?? ''}`;
  }

  const stats = $derived.by<Stat[]>(() => {
    if (mode === 'moves') return [
      { label: '当前异动', value: count(listing.data?.counts.returned ?? rows.length) },
      { label: '上游报告', value: count(valueAt(summary.data?.data ?? {}, 'reported_count')) },
      { label: '刷新时间', value: text(valueAt(summary.data?.data ?? {}, 'refresh_time')) },
      { label: '交易日期', value: date(listing.data?.parameters.resolved_date) }
    ];
    return [
      { label: '当前视图', value: definition.label },
      { label: '返回记录', value: count(listing.data?.counts.returned ?? rows.length) },
      { label: '证券筛选', value: code ? `${market.toUpperCase()}${code}` : '全市场' },
      { label: '来源状态', value: listing.data?.availability === 'live' ? '在线' : text(listing.data?.availability) }
    ];
  });

  async function localError(message: string) {
    await listing.loadWith(() => Promise.reject(new Error(message)));
  }

  function baseMoveParameters(refresh: boolean) {
    return {
      board, type: anomalyType, date: eventDate,
      refresh: refresh ? 1 : 0
    };
  }

  async function loadExplanation(row: AnomalyRecord, refresh = false) {
    const selectedMarket = String(valueAt(row, 'security.market') ?? '');
    const selectedCode = String(valueAt(row, 'security.code') ?? '');
    const selectedType = String(valueAt(row, 'anomaly.code') ?? anomalyType);
    if (!selectedMarket || !selectedCode) return;
    selectedId = anomalyRowId(row);
    await explanation.load(`/api/v1/market/abnormal-details?${queryString({
      view: 'explanation', board, type: selectedType, date: eventDate,
      market: selectedMarket, code: selectedCode, refresh: refresh ? 1 : 0
    })}`);
  }

  async function load(refresh = false) {
    selectedId = '';
    explanation.reset();
    if (mode === 'moves') {
      const parameters = baseMoveParameters(refresh);
      const [document] = await Promise.all([
        listing.load(`/api/v1/market/abnormal-moves?${queryString({
          ...parameters, limit: 5000
        })}`),
        summary.load(`/api/v1/market/abnormal-details?${queryString({
          view: 'summary', ...parameters
        })}`)
      ]);
      const first = document?.records.find((row) =>
        valueAt(row, 'anomaly.possible_suspension_check') === true
      ) ?? document?.records[0];
      if (first) await loadExplanation(first, refresh);
      return;
    }
    summary.reset();
    if ((market || code) && (!market || !/^\d{6}$/.test(code))) {
      await localError('单票筛选需要同时提供市场和六位证券代码');
      return;
    }
    if (mode === 'profit-gaps') {
      const safety = Number(minimumSafety);
      if (!Number.isInteger(safety) || safety < 0 || safety > 100) {
        await localError('最低安全分必须是 0..100 的整数');
        return;
      }
      await listing.load(`/api/v1/market/profit-gaps?${queryString({
        q: query, market, code, minimum_safety: safety,
        limit: 5000, refresh: refresh ? 1 : 0
      })}`);
      return;
    }
    await listing.load(`/api/v1/market/anomaly-risk?${queryString({
      view: mode, q: query, market, code, warning,
      warnings_only: warningScope === 'active' ? 1 : 0,
      limit: 5000, refresh: refresh ? 1 : 0
    })}`);
  }

  function switchMode(next: string) {
    mode = next as AnomalyMode;
    query = '';
    listing.reset();
    summary.reset();
    explanation.reset();
    void load();
  }

  function openSecurity(row: AnomalyRecord) {
    const selectedMarket = String(valueAt(row, 'security.market') ?? '');
    const selectedCode = String(valueAt(row, 'security.code') ?? '');
    if (!selectedMarket || !selectedCode) return;
    const name = String(valueAt(row, 'security.name') ?? selectedCode);
    app.setStock({ market: selectedMarket, code: selectedCode, name });
    router.go(stockPath(selectedMarket, selectedCode));
  }

  function inspectRisk(next: 'statistics' | 'suspension-risk') {
    if (!selectedRow) return;
    market = String(valueAt(selectedRow, 'security.market') ?? '');
    code = String(valueAt(selectedRow, 'security.code') ?? '');
    mode = next;
    listing.reset();
    summary.reset();
    explanation.reset();
    void load();
  }

  function rowKey(row: AnomalyRecord, index: number): string {
    return String(
      valueAt(row, 'event_id') ??
      `${valueAt(row, 'security.security_id') ?? index}:${valueAt(row, 'anomaly.code') ?? mode}`
    );
  }

  onMount(() => void load());
</script>

<PageHeader
  eyebrow="PBRPC / TQLEX · 200002—500107"
  title="交易异动与风险"
  description="从十九类盘中异动下钻上榜原因，再关联异常波动、停牌风险和利润断层。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={listing.busy || summary.busy || explanation.busy} onclick={() => void load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<div class="mode-bar">
  <Segmented options={ANOMALY_MODES} value={mode} onChange={switchMode} ariaLabel="异常风险视图" />
</div>

{#if mode === 'moves'}
  <Split asideWidth="36%">
    {#snippet main()}
      <Panel
        title={definition.title}
        subtitle={definition.description}
        busy={listing.busy}
        error={listing.error}
        onRetry={() => void load()}
        empty={listing.loaded && rows.length === 0}
        emptyText="当前日期和板块没有交易异动。"
        flush scroll
      >
        {#snippet toolbar()}
          <Select options={BOARD_OPTIONS} value={board} width="135px" label="板块" onChange={(next) => { board = next; void load(); }} />
          <Select options={ABNORMAL_TYPE_OPTIONS} value={anomalyType} width="205px" label="异动类型" onChange={(next) => { anomalyType = next; void load(); }} />
          <TextInput bind:value={eventDate} width="128px" label="交易日期" placeholder="日期 YYYYMMDD" onEnter={() => void load()} />
          <Button variant="primary" onclick={() => void load()}>查询</Button>
        {/snippet}
        <DataTable {columns} {rows} {rowKey} numbered stickyFirst minWidth="980px" onRowClick={(row) => void loadExplanation(row)} isActive={(row) => anomalyRowId(row) === selectedId} />
      </Panel>
    {/snippet}
    {#snippet aside()}
      <Panel
        title={selectedRow ? `${text(valueAt(selectedRow, 'security.name'))} · 上榜原因` : '异动详情'}
        subtitle={selectedRow ? text(valueAt(selectedRow, 'anomaly.name')) : '从左侧选择证券'}
        busy={explanation.busy}
        error={explanation.error}
        onRetry={() => selectedRow && void loadExplanation(selectedRow)}
        empty={!selectedRow || (explanation.loaded && !explanation.data?.data)}
        emptyText={selectedRow
          ? '该条异动当前没有结构化区间原因；单日偏离类和盘中未结算记录可能合法为空。'
          : '选择一条异动记录查看结构化上榜原因。'}
      >
        {#if selectedRow && explanation.data}
          <div class="detail">
            <p class="reason">{text(valueAt(explanation.data.data, 'reason'))}</p>
            <StatGrid stats={[
              { label: '累计偏离', value: percent(valueAt(explanation.data.data, 'cumulative_deviation_pct')) },
              { label: '累计成交量', value: compact(valueAt(explanation.data.data, 'cumulative_volume_shares'), '股') },
              { label: '累计成交额', value: compact(valueAt(explanation.data.data, 'cumulative_amount_yuan'), '元') },
              { label: '异常开始', value: date(valueAt(explanation.data.data, 'period.start_date')) },
              { label: '异常结束', value: date(valueAt(explanation.data.data, 'period.end_date')) },
              { label: '当日涨跌', value: delta(valueAt(selectedRow, 'change_pct')) }
            ]} />
            <div class="detail-actions">
              <Button onclick={() => inspectRisk('statistics')}>查看波动统计</Button>
              <Button onclick={() => inspectRisk('suspension-risk')}>查看停牌风险</Button>
              <Button variant="primary" onclick={() => openSecurity(selectedRow)}>进入个股</Button>
            </div>
          </div>
        {/if}
      </Panel>
    {/snippet}
  </Split>
{:else}
  <Panel
    title={definition.title}
    subtitle={`${definition.description} · 点击股票进入个股工作台`}
    busy={listing.busy}
    error={listing.error}
    onRetry={() => void load()}
    empty={listing.loaded && rows.length === 0}
    emptyText="当前筛选没有返回风险记录。"
    flush scroll fill
  >
    {#snippet toolbar()}
      <TextInput bind:value={query} icon="search" width="210px" label="检索" placeholder="股票、板块或原因" onEnter={() => void load()} />
      <Select options={MARKET_FILTER_OPTIONS} value={market} width="125px" label="市场" onChange={(next) => market = next} />
      <TextInput bind:value={code} width="105px" label="证券代码" placeholder="证券代码" onEnter={() => void load()} />
      {#if mode === 'profit-gaps'}
        <TextInput bind:value={minimumSafety} width="105px" label="最低安全分" placeholder="安全分" onEnter={() => void load()} />
      {:else}
        <Select options={WARNING_OPTIONS} value={warning} width="145px" label="预警类型" onChange={(next) => { warning = next; void load(); }} />
        <Select options={WARNING_SCOPE_OPTIONS} value={warningScope} width="135px" label="预警范围" onChange={(next) => { warningScope = next; void load(); }} />
      {/if}
      <Button variant="primary" onclick={() => void load()}>查询</Button>
    {/snippet}
    <DataTable {columns} {rows} {rowKey} numbered stickyFirst minWidth="940px" onRowClick={openSecurity} />
  </Panel>
{/if}

<style>
  .mode-bar { display: flex; flex: none; align-items: center; }
  .detail { display: flex; flex-direction: column; gap: var(--sp-4); }
  .reason { padding: var(--sp-3); font-size: var(--fs-body); line-height: 1.7; color: var(--fg); background: var(--bg-raised); border: 1px solid var(--line); border-radius: var(--radius); }
  .detail-actions { display: flex; flex-wrap: wrap; gap: var(--sp-2); }
</style>
