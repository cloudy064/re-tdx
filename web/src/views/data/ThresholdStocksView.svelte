<script lang="ts">
  /** 通达信 QYSZ 页：百元股 / 千亿市值的历史、日期名单与入围跌出。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    MarketThresholdStocksDocument,
    ThresholdHistoryRecord,
    ThresholdMemberRecord,
    ValuationSecurity
  } from '../../types';

  const UNIVERSES = [
    { id: 'high-price', label: '百元股' },
    { id: 'mega-cap', label: '千亿市值' }
  ];
  const STATUSES = [
    { id: 'all', label: '全部状态' },
    { id: 'entered', label: '当日入围' },
    { id: 'exited', label: '当日跌出' },
    { id: 'continuing', label: '继续在列' }
  ];

  let universe = $state<'high-price' | 'mega-cap'>('high-price');
  let selectedDate = $state('');
  let status = $state('all');
  let query = $state('');
  const historyResource = new Resource<MarketThresholdStocksDocument>();
  const membersResource = new Resource<MarketThresholdStocksDocument>();
  const historyDoc = $derived(historyResource.data);
  const memberDoc = $derived(membersResource.data);
  const historyRows = $derived((historyDoc?.records ?? []) as ThresholdHistoryRecord[]);
  const memberRows = $derived((memberDoc?.records ?? []) as ThresholdMemberRecord[]);
  const dateOptions = $derived(historyRows.map((row) => ({ id: row.date, label: date(row.date) })));
  const selected = $derived(memberDoc?.selected_period ?? historyRows.find((row) => row.date === selectedDate) ?? null);

  const stats = $derived.by<Stat[]>(() => {
    const summary = memberDoc?.summary.members;
    if (!selected) return [];
    return [
      { label: universe === 'high-price' ? '百元股' : '千亿市值', value: count(selected.total_count), note: date(selected.date) },
      { label: '当日入围', value: count(selected.entered_count), tone: selected.entered_count > 0 ? 'up' : '' },
      { label: '当日跌出', value: count(selected.exited_count), tone: selected.exited_count > 0 ? 'down' : '' },
      { label: '合计市值', value: compact(selected.aggregate_market_cap_yuan, '元'), note: `A 股占比 ${percent(selected.aggregate_market_share_pct)}` },
      { label: '名单对账', value: summary?.active_count_matches && summary?.entered_count_matches && summary?.exited_count_matches ? '一致' : '待校验', note: summary ? `${count(summary.rows)} 条明细` : '' },
      { label: '趋势长度', value: count(memberDoc?.counts.trend_points), note: `${count(memberDoc?.sources.length)} 条真实资源链` }
    ];
  });

  async function loadUniverse(refresh = false) {
    const result = await historyResource.load(`/api/v1/market/threshold-stocks?${queryString({
      view: 'history', universe, sort: 'date', order: 'desc', limit: 500,
      refresh: refresh ? 1 : 0
    })}`);
    const rows = (result?.records ?? []) as ThresholdHistoryRecord[];
    if (!rows.length) return;
    selectedDate = rows[0].date;
    status = 'all';
    query = '';
    await loadMembers(refresh);
  }

  async function loadMembers(refresh = false) {
    if (!selectedDate) return;
    await membersResource.load(`/api/v1/market/threshold-stocks?${queryString({
      view: 'members', universe, date: selectedDate, status,
      q: query.trim(), sort: 'threshold-value', order: 'desc', limit: 1000,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function openSecurity(security: ValuationSecurity) {
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code, 'threshold-stocks'));
  }

  const historyColumns: Column<ThresholdHistoryRecord>[] = [
    { key: 'date', label: '日期', width: '92px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'count', label: '家数', width: '68px', align: 'right', num: true, value: (row) => count(row.total_count), sortValue: (row) => row.total_count ?? 0 },
    { key: 'entered', label: '入围', width: '62px', align: 'right', num: true, value: (row) => count(row.entered_count), tone: (row) => row.entered_count > 0 ? 'up' : '' },
    { key: 'exited', label: '跌出', width: '62px', align: 'right', num: true, value: (row) => count(row.exited_count), tone: (row) => row.exited_count > 0 ? 'down' : '' },
    { key: 'share', label: '市值占比', align: 'right', num: true, value: (row) => percent(row.aggregate_market_share_pct), sortValue: (row) => row.aggregate_market_share_pct ?? 0 }
  ];

  const memberColumns = $derived.by<Column<ThresholdMemberRecord>[]>(() => [
    { key: 'security', label: '股票', width: '136px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'status', label: '名单状态', width: '82px', value: (row) => row.status_label, tone: (row) => row.status === 'entered' ? 'up' : row.status === 'exited' ? 'down' : '' },
    { key: 'change', label: '当日涨幅', width: '86px', align: 'right', num: true, value: (row) => delta(row.day_change_pct), tone: (row) => tone(row.day_change_pct), sortValue: (row) => row.day_change_pct ?? 0 },
    { key: 'value', label: universe === 'high-price' ? '收盘价' : '总市值', width: '110px', align: 'right', num: true, value: (row) => universe === 'high-price' ? fixed(row.close_price_yuan, 2) : `${fixed(row.market_cap_100m_yuan, 2)} 亿`, sortValue: (row) => universe === 'high-price' ? row.close_price_yuan ?? 0 : row.market_cap_100m_yuan ?? 0 },
    { key: 'increase', label: universe === 'high-price' ? '股价净增' : '市值净增', width: '110px', align: 'right', num: true, value: (row) => universe === 'high-price' ? fixed(row.price_net_change_yuan, 2) : `${fixed(row.market_cap_net_change_100m_yuan, 2)} 亿`, tone: (row) => tone(universe === 'high-price' ? row.price_net_change_yuan : row.market_cap_net_change_100m_yuan) },
    { key: 'region', label: '地区', width: '72px', value: (row) => text(row.region) },
    { key: 'controller', label: '控股股东', value: (row) => text(row.controlling_shareholder), wrap: true }
  ]);

  onMount(() => void loadUniverse());
</script>

<PageHeader eyebrow="709/1721 · QYSZ / BYGTJ1 / BYGTJ3" title="百元股与千亿市值" description="还原通达信千亿百元股页面：查看历史家数、每个交易日的股票名单，以及当日入围和跌出关系。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={historyResource.busy || membersResource.busy} onclick={() => void loadUniverse(true)}>刷新三段资源</Button>{/snippet}
</PageHeader>

<Split asideWidth="420px">
  {#snippet main()}
    <Panel title="日期股票名单" subtitle={selected ? `${date(selected.date)} · ${count(memberDoc?.counts.matched)} 条命中` : ''} busy={membersResource.busy} error={membersResource.error} onRetry={() => void loadMembers()} empty={membersResource.loaded && !membersResource.busy && memberRows.length === 0} emptyText="这个日期和筛选条件下没有股票。" flush scroll>
      {#snippet toolbar()}
        <Select options={UNIVERSES} value={universe} width="145px" label="口径" onChange={(next) => { universe = next as 'high-price' | 'mega-cap'; void loadUniverse(); }} />
        <Select options={dateOptions} value={selectedDate} width="155px" label="日期" onChange={(next) => { selectedDate = next; void loadMembers(); }} />
        <Select options={STATUSES} value={status} width="145px" label="状态" onChange={(next) => { status = next; void loadMembers(); }} />
        <TextInput bind:value={query} icon="search" width="220px" label="检索" placeholder="股票 / 地区 / 控股股东" onEnter={() => void loadMembers()} />
      {/snippet}
      <DataTable columns={memberColumns} rows={memberRows} stickyFirst numbered rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} sortKey="value" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel title="历史家数与变动" subtitle={historyDoc ? `${count(historyRows.length)} 个交易日 · 点击切换名单` : ''} busy={historyResource.busy} error={historyResource.error} onRetry={() => void loadUniverse()} empty={historyResource.loaded && historyRows.length === 0} flush scroll>
      <DataTable columns={historyColumns} rows={historyRows} rowKey={(row) => row.date} onRowClick={(row) => { selectedDate = row.date; void loadMembers(); }} isActive={(row) => row.date === selectedDate} sortKey="date" />
    </Panel>
  {/snippet}
</Split>
