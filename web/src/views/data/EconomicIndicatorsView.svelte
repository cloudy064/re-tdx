<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import EconomicIndicatorChart from '../../charts/EconomicIndicatorChart.svelte';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { compact, count, date, fixed, percent, text, tone } from '../../lib/fmt';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    EconomicIndicatorRecord,
    EconomicIndicatorRelatedSecurity,
    MarketEconomicIndicatorsDocument
  } from '../../types';

  let query = $state('');
  let selectedId = $state('');
  const catalog = new Resource<MarketEconomicIndicatorsDocument>();
  const detail = new Resource<MarketEconomicIndicatorsDocument>();
  const indicators = $derived(catalog.data?.indicators ?? []);
  const doc = $derived(detail.data);
  const selected = $derived(doc?.selected_indicator ?? indicators.find((row) => row.indicator_id === selectedId) ?? null);

  const stats = $derived.by<Stat[]>(() => [
    { label: '经济指标', value: count(catalog.data?.summary.indicator_count), note: '价格 / 景气指数' },
    { label: '日 / 周 / 季', value: (catalog.data?.summary.by_frequency ?? []).map((row) => `${row.name}${row.count}`).join(' · ') },
    { label: '最新更新', value: date(catalog.data?.summary.last_update_date) },
    { label: '历史点', value: count(doc?.counts.history_points), note: doc?.history.length ? `${date(doc.history[0].date)} 起` : '' },
    { label: '相关股票', value: count(doc?.summary.related_security_count), note: `${count(doc?.quote_source?.received)} 条行情` },
    { label: '动态源', value: doc ? `${count(doc.sources.length)} / 3` : '—', note: doc?.errors.length ? `${doc.errors.length} 个错误` : '主表 / 历史 / 关系' }
  ]);

  async function loadCatalog(refresh = false) {
    const result = await catalog.load(`/api/v1/market/economic-indicators?${queryString({ view: 'catalog', q: query.trim(), sort: 'update-date', order: 'desc', limit: 500, refresh: refresh ? 1 : 0 })}`);
    if (result?.indicators.length && !selectedId) select(result.indicators[0]);
  }

  function select(row: EconomicIndicatorRecord) {
    selectedId = row.indicator_id;
    void detail.load(`/api/v1/market/economic-indicators?${queryString({ view: 'indicator', indicator_id: row.indicator_id, limit: 500, history_limit: 5000, include_quotes: 1 })}`);
  }

  const indicatorColumns: Column<EconomicIndicatorRecord>[] = [
    { key: 'name', label: '指标', width: '205px', wrap: true, value: (row) => row.name, sub: (row) => `${row.indicator_id} · ${row.frequency}` },
    { key: 'value', label: '当前值', width: '95px', align: 'right', num: true, value: (row) => fixed(row.current_value, 2), sub: (row) => row.unit, sortValue: (row) => row.current_value ?? 0 },
    { key: 'mom', label: '环比', width: '70px', align: 'right', num: true, value: (row) => percent(row.month_on_month_pct), tone: (row) => tone(row.month_on_month_pct), sortValue: (row) => row.month_on_month_pct ?? 0 },
    { key: 'yoy', label: '同比', width: '70px', align: 'right', num: true, value: (row) => percent(row.year_on_year_pct), tone: (row) => tone(row.year_on_year_pct), sortValue: (row) => row.year_on_year_pct ?? 0 },
    { key: 'date', label: '更新', width: '88px', num: true, value: (row) => date(row.update_date), sub: (row) => text(row.indicator_type), sortValue: (row) => row.update_date }
  ];

  const relatedColumns: Column<EconomicIndicatorRelatedSecurity>[] = [
    { key: 'security', label: '相关股票', width: '145px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'industry', label: '细分行业', width: '130px', value: (row) => text(row.industry) },
    { key: 'price', label: '现价', width: '80px', align: 'right', num: true, value: (row) => fixed(row.last_price, 2) },
    { key: 'change', label: '涨跌', width: '75px', align: 'right', num: true, value: (row) => percent(row.change_pct), tone: (row) => tone(row.change_pct), sortValue: (row) => row.change_pct ?? 0 },
    { key: 'amount', label: '成交额', width: '100px', align: 'right', num: true, value: (row) => compact(row.amount_yuan, '元'), sortValue: (row) => row.amount_yuan ?? 0 }
  ];

  onMount(() => void loadCatalog());
</script>

<PageHeader eyebrow="JJZB · 72 INDICATORS · HISTORY + STOCKS" title="经济指标与相关股票" description="通达信价格/景气指标当前值、环比同比、历史曲线和相关股票；行情列来自公开 L1，不把宿主列伪装成 JSN 原始字段。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={catalog.busy || detail.busy} onclick={() => void loadCatalog(true)}>刷新指标目录</Button>{/snippet}
</PageHeader>

<Split asideWidth="475px">
  {#snippet main()}
    <div class="stage">
      <Panel title={`${selected?.name ?? '经济指标'} · 历史走势`} eyebrow={selected?.indicator_id ?? 'JJZB1'} subtitle={selected ? `${fixed(selected.current_value, 2)} ${selected.unit} · 环比 ${percent(selected.month_on_month_pct)} · 同比 ${percent(selected.year_on_year_pct)}` : ''} busy={detail.busy} error={detail.error} onRetry={() => selected && select(selected)} empty={detail.loaded && !detail.busy && (doc?.history.length ?? 0) === 0} emptyText="该指标没有返回历史序列" fill>
        {#if doc?.history.length}<EconomicIndicatorChart history={doc.history} title={selected?.name ?? '指标值'} unit={selected?.unit ?? ''} />{/if}
      </Panel>
      <Panel title="相关股票" eyebrow="JJZB2" subtitle={doc ? `${count(doc.summary.related_security_count)} 只 · 当前返回 ${count(doc.counts.returned_related_securities)} 只` : ''} busy={detail.busy} empty={detail.loaded && !detail.busy && (doc?.related_securities.length ?? 0) === 0} emptyText="该指标没有返回相关股票" flush scroll>
        <DataTable columns={relatedColumns} rows={doc?.related_securities ?? []} stickyFirst numbered rowKey={(row) => row.security.security_id} onRowClick={(row) => router.go(stockPath(row.security.market, row.security.code))} sortKey="change" />
      </Panel>
    </div>
  {/snippet}
  {#snippet aside()}
    <Panel title="指标目录" subtitle={`${count(catalog.data?.counts.matched)} 条`} busy={catalog.busy} error={catalog.error} onRetry={() => void loadCatalog()} empty={catalog.loaded && !catalog.busy && indicators.length === 0} flush scroll>
      {#snippet toolbar()}<TextInput bind:value={query} icon="search" width="260px" label="检索" placeholder="铜、原油、钢材、景气指数" onEnter={() => void loadCatalog()} />{/snippet}
      <DataTable columns={indicatorColumns} rows={indicators} stickyFirst numbered rowKey={(row) => row.indicator_id} onRowClick={select} isActive={(row) => row.indicator_id === selectedId} sortKey="date" />
    </Panel>
  {/snippet}
</Split>

<style>
  .stage { display: grid; grid-template-rows: minmax(260px, 0.9fr) minmax(250px, 1.1fr); gap: var(--sp-2); flex: 1; min-height: 0; }
</style>
