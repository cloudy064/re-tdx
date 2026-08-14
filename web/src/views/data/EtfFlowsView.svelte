<script lang="ts">
  /** ETF 持仓与申赎资金。股票、一级行业使用相同字段，便于横向比较。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, percent, signedCompact, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import Icon from '../../ui/Icon.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { EtfFlowRow, MarketEtfFlowsDocument } from '../../types';

  const VIEWS = [
    { id: 'stocks', label: '个股', hint: 'ETF 持有的股票' },
    { id: 'industries', label: '一级行业', hint: 'ETF 持有股票汇总到行业' }
  ];
  const DIRECTIONS = [
    { id: 'all', label: '全部' },
    { id: 'inflow', label: '净流入' },
    { id: 'outflow', label: '净流出' },
    { id: 'flat', label: '持平' }
  ];
  const SORTS = [
    { id: 'total-flow', label: '当日净流入' },
    { id: 'weekly-flow', label: '一周净流入' },
    { id: 'holding-value', label: 'ETF 持仓市值' },
    { id: 'etf-count', label: '持有 ETF 数' }
  ];

  let view = $state('stocks');
  let direction = $state('all');
  let sort = $state('total-flow');
  let query = $state('');
  const flows = new Resource<MarketEtfFlowsDocument>();
  const doc = $derived(flows.data);
  const rows = $derived(doc?.rows ?? []);
  const summary = $derived(doc?.summaries[view as 'stocks' | 'industries']);

  const stats = $derived.by(() => {
    if (!summary) return [];
    return [
      { label: view === 'stocks' ? '覆盖股票' : '一级行业', value: count(summary.rows), note: date(summary.cutoff_dates[summary.cutoff_dates.length - 1]) },
      { label: '流入 / 流出', value: `${count(summary.inflow)} / ${count(summary.outflow)}` },
      { label: 'ETF 持仓关系', value: count(summary.etf_relationships) },
      { label: '持仓市值', value: compact(summary.holding_market_value_yuan, '元') },
      { label: '当日净流入', value: signedCompact(summary.total_net_inflow_yuan, '元'), tone: tone(summary.total_net_inflow_yuan) },
      { label: '一周净流入', value: signedCompact(summary.weekly_net_inflow_yuan, '元'), tone: tone(summary.weekly_net_inflow_yuan) }
    ];
  });

  function load(refresh = false) {
    void flows.load(`/api/v1/market/etf-flows?${queryString({
      view, direction, sort, q: query.trim(), limit: 10000, refresh: refresh ? 1 : 0
    })}`);
  }

  function open(row: EtfFlowRow) {
    if (row.entity.type !== 'security') return;
    app.setStock({ market: row.entity.market, code: row.entity.code, name: row.entity.name });
    router.go(stockPath(row.entity.market, row.entity.code, 'etf-flows'));
  }

  const columns: Column<EtfFlowRow>[] = [
    {
      key: 'entity', label: '股票 / 行业', width: '142px',
      value: (row) => row.entity.name || row.entity.code,
      sub: (row) => row.entity.type === 'security' ? row.entity.security_id : `行业 ${row.entity.code}`
    },
    { key: 'count', label: 'ETF 数', align: 'right', num: true, value: (row) => count(row.etf_count), sortValue: (row) => row.etf_count ?? 0 },
    { key: 'holding', label: '持仓市值', align: 'right', num: true, value: (row) => compact(row.holding_market_value_yuan, '元'), sortValue: (row) => row.holding_market_value_yuan ?? 0 },
    { key: 'shares', label: '持仓股数', align: 'right', num: true, value: (row) => compact(row.holding_shares, '股'), sortValue: (row) => row.holding_shares ?? 0 },
    { key: 'broad', label: '宽基净流入', align: 'right', num: true, value: (row) => signedCompact(row.broad_net_inflow_yuan, '元'), tone: (row) => tone(row.broad_net_inflow_yuan), sortValue: (row) => row.broad_net_inflow_yuan ?? 0 },
    { key: 'theme', label: '主题净流入', align: 'right', num: true, value: (row) => signedCompact(row.theme_net_inflow_yuan, '元'), tone: (row) => tone(row.theme_net_inflow_yuan), sortValue: (row) => row.theme_net_inflow_yuan ?? 0 },
    { key: 'total', label: '当日净流入', align: 'right', num: true, value: (row) => signedCompact(row.total_net_inflow_yuan, '元'), sub: (row) => `成交额占比 ${percent(row.net_inflow_share_turnover_pct)}`, tone: (row) => tone(row.total_net_inflow_yuan), sortValue: (row) => row.total_net_inflow_yuan ?? 0 },
    { key: 'weekly', label: '一周净流入', align: 'right', num: true, value: (row) => signedCompact(row.weekly_net_inflow_yuan, '元'), tone: (row) => tone(row.weekly_net_inflow_yuan), sortValue: (row) => row.weekly_net_inflow_yuan ?? 0 },
    { key: 'date', label: '截止日', width: '82px', num: true, value: (row) => date(row.cutoff_date), sortValue: (row) => row.cutoff_date }
  ];

  onMount(() => load());
</script>

<PageHeader
  eyebrow="ETFSG · func_etfsg101 / 102"
  title="ETF 资金动向"
  description="从 ETF 持股、申赎净流入与成交额估算个股及一级行业的资金方向。"
  {stats}
>
  {#snippet actions()}<Button icon="refresh" busy={flows.busy} onclick={() => load(true)}>刷新源数据</Button>{/snippet}
</PageHeader>

<Panel
  title={view === 'stocks' ? 'ETF 持股资金排行' : 'ETF 一级行业资金排行'}
  subtitle={doc ? `${count(doc.counts.returned)} 行 · 数据源 ${doc.sources.map((source) => source.endpoint).join(' / ')}` : ''}
  busy={flows.busy}
  error={flows.error}
  onRetry={() => load()}
  empty={flows.loaded && !flows.busy && rows.length === 0}
  emptyText="当前筛选条件下没有 ETF 资金记录。"
  flush
  scroll
>
  {#snippet toolbar()}
    <Segmented options={VIEWS} value={view} ariaLabel="ETF 资金视图" onChange={(next) => { view = next; load(); }} />
    <Segmented options={DIRECTIONS} value={direction} ariaLabel="资金方向" onChange={(next) => { direction = next; load(); }} />
    <Select options={SORTS} value={sort} width="170px" label="服务端排序" onChange={(next) => { sort = next; load(); }} />
    <TextInput bind:value={query} icon="search" width="220px" label="检索" placeholder="股票、代码或行业" onEnter={() => load()} />
    <p class="notice"><Icon name="info" size={11} />宽基与主题流入为 ETF 申赎传导的估算口径，不等同于个股实时主力资金。</p>
  {/snippet}

  <DataTable {columns} {rows} stickyFirst numbered rowKey={(row) => row.entity.security_id} onRowClick={open} sortKey={sort === 'weekly-flow' ? 'weekly' : sort === 'holding-value' ? 'holding' : sort === 'etf-count' ? 'count' : 'total'} />
</Panel>

<style>
  .notice { display: inline-flex; align-items: center; gap: var(--sp-1); margin-left: auto; font-size: 10px; color: var(--warn); }
</style>
