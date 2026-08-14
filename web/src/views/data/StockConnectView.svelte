<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    MarketStockConnectDocument,
    SouthboundIndustryMemberRecord,
    SouthboundIndustryTrendRecord,
    StockConnectActiveRecord,
    StockConnectActivityRecord,
    StockConnectFlowRecord,
    StockConnectHoldingRecord,
    StockConnectIndustryRecord
  } from '../../types';

  const CATEGORIES = [
    { id: 'northbound-total', label: '陆股通日度合计', view: 'flows' },
    { id: 'southbound-total', label: '港股通日度合计', view: 'flows' },
    { id: 'sh-northbound', label: '沪股通日度', view: 'flows' },
    { id: 'sz-northbound', label: '深股通日度', view: 'flows' },
    { id: 'sh-southbound', label: '港股通沪通道', view: 'flows' },
    { id: 'sz-southbound', label: '港股通深通道', view: 'flows' },
    { id: 'northbound-weekly', label: '陆股通长期周度', view: 'flows' },
    { id: 'southbound-weekly', label: '港股通长期周度', view: 'flows' },
    { id: 'mainland-quarterly', label: '陆股通季度持仓', view: 'holdings' },
    { id: 'mainland-current', label: '陆股通当前持仓', view: 'holdings' },
    { id: 'hong-kong-current', label: '港股通当前持仓', view: 'holdings' },
    { id: 'hong-kong-removed', label: '港股通调出历史', view: 'holdings' },
    { id: 'mainland-removed', label: '陆股通调出快照（旧）', view: 'holdings' },
    { id: 'daily-increase', label: '增仓榜（历史源）', view: 'activity' },
    { id: 'daily-decrease', label: '减仓榜（历史源）', view: 'activity' },
    { id: 'continuous-increase', label: '连续增仓（历史源）', view: 'activity' },
    { id: 'continuous-decrease', label: '连续减仓（历史源）', view: 'activity' },
    { id: 'five-day-increase', label: '五日增仓（历史源）', view: 'activity' },
    { id: 'five-day-decrease', label: '五日减仓（历史源）', view: 'activity' },
    { id: 'frequent-increase', label: '频繁增仓（历史源）', view: 'activity' },
    { id: 'northbound-all', label: '陆股通分类总览', view: 'industry' },
    { id: 'northbound-industry', label: '陆股通行业', view: 'industry' },
    { id: 'northbound-concept', label: '陆股通概念', view: 'industry' },
    { id: 'northbound-style', label: '陆股通风格', view: 'industry' },
    { id: 'southbound-industry', label: '港股通行业', view: 'industry' },
    { id: 'active-sh', label: '沪股通活跃股（指定日）', view: 'active-stocks' },
    { id: 'active-sz', label: '深股通活跃股（指定日）', view: 'active-stocks' }
  ];
  let category = $state('northbound-total');
  let activeDate = $state(new Date().toLocaleDateString('sv-SE').replace(/-/g, ''));
  const resource = new Resource<MarketStockConnectDocument>();
  const detail = new Resource<MarketStockConnectDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selectedView = $derived(CATEGORIES.find((item) => item.id === category)?.view ?? 'flows');
  const holdingMode = $derived(selectedView === 'holdings');
  const activityMode = $derived(selectedView === 'activity');
  const industryMode = $derived(selectedView === 'industry');
  const activeMode = $derived(selectedView === 'active-stocks');
  const industryTrend = $derived((detail.data?.trend ?? []) as SouthboundIndustryTrendRecord[]);
  const selectedGroup = $derived(detail.data?.group_id ?? '');
  const latest = $derived(doc?.latest as StockConnectFlowRecord | StockConnectHoldingRecord | null);
  const stats = $derived([
    { label: '数据行', value: count(doc?.count), note: `缓存 ${count(doc?.cache.age_seconds)}s` },
    { label: '最新日期', value: date(latest?.date) },
    { label: holdingMode ? '持股数量' : '成交总额', value: compact(holdingMode ? (latest as StockConnectHoldingRecord)?.holding_shares : (latest as StockConnectFlowRecord)?.turnover_yuan, holdingMode ? '股' : '元') },
    { label: holdingMode ? '持股比例' : '净流入', value: holdingMode ? percent((latest as StockConnectHoldingRecord)?.holding_ratio_pct) : compact((latest as StockConnectFlowRecord)?.net_inflow_yuan, '元'), tone: holdingMode ? undefined : tone((latest as StockConnectFlowRecord)?.net_inflow_yuan) }
  ]);
  const flowColumns: Column<StockConnectFlowRecord>[] = [
    { key: 'date', label: '日期', width: '96px', value: (row) => date(row.date) },
    { key: 'turnover', label: '成交总额', align: 'right', num: true, value: (row) => compact(row.turnover_yuan, '元'), sortValue: (row) => row.turnover_yuan ?? 0 },
    { key: 'net', label: '净流入', align: 'right', num: true, value: (row) => compact(row.net_inflow_yuan ?? row.net_buy_turnover_yuan, '元'), tone: (row) => tone(row.net_inflow_yuan ?? row.net_buy_turnover_yuan) },
    { key: 'buy', label: '买入成交', align: 'right', num: true, value: (row) => compact(row.buy_turnover_yuan, '元') },
    { key: 'sell', label: '卖出成交', align: 'right', num: true, value: (row) => compact(row.sell_turnover_yuan, '元') },
    { key: 'index', label: '参考指数', align: 'right', num: true, value: (row) => text(row.reference_index), sub: (row) => percent(row.reference_index_change_pct, 2, true), tone: (row) => tone(row.reference_index_change_pct) }
  ];
  const holdingColumns: Column<StockConnectHoldingRecord>[] = [
    { key: 'security', label: '证券', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => `${row.security.security_id} · ${row.channel}` },
    { key: 'date', label: '数据日', width: '96px', value: (row) => date(row.date) },
    { key: 'shares', label: '持股数量', align: 'right', num: true, value: (row) => compact(row.holding_shares, '股'), sortValue: (row) => row.holding_shares ?? 0 },
    { key: 'ratio', label: '占自由流通股', align: 'right', num: true, value: (row) => percent(row.holding_ratio_pct), sortValue: (row) => row.holding_ratio_pct ?? 0 },
    { key: 'value', label: '持股市值', align: 'right', num: true, value: (row) => compact(row.holding_market_value_yuan, '元'), sortValue: (row) => row.holding_market_value_yuan ?? 0 },
    { key: 'change', label: '持股变动', align: 'right', num: true, value: (row) => compact(row.holding_share_change, '股'), tone: (row) => tone(row.holding_share_change) },
    { key: 'fresh', label: '时效', width: '72px', slot: true }
  ];
  const activityColumns: Column<StockConnectActivityRecord>[] = [
    { key: 'security', label: '证券', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'date', label: '快照日', width: '96px', value: (row) => date(row.date) },
    { key: 'shares', label: '持股数', align: 'right', num: true, value: (row) => compact(row.holding_shares, '股') },
    { key: 'change', label: '持股变动', align: 'right', num: true, value: (row) => compact(row.holding_share_change, '股'), tone: (row) => tone(row.holding_share_change) },
    { key: 'pct', label: '变动幅度', align: 'right', num: true, value: (row) => percent(row.holding_change_pct, 2, true), tone: (row) => tone(row.holding_change_pct) },
    { key: 'amount', label: '净买金额', align: 'right', num: true, value: (row) => compact(row.net_buy_amount, '元'), tone: (row) => tone(row.net_buy_amount) },
    { key: 'fresh', label: '时效', width: '78px', value: (row) => row.snapshot_freshness === 'current' ? '当前' : '历史快照' }
  ];
  const industryColumns: Column<StockConnectIndustryRecord>[] = [
    { key: 'code', label: '分类代码', width: '130px', value: (row) => row.classification_code, sub: (row) => `市场 ${row.classification_market}` },
    { key: 'date', label: '快照日', width: '96px', value: (row) => date(row.date) },
    { key: 'daily', label: '当日净流入', align: 'right', num: true, value: (row) => compact(row.daily_net_inflow, '元'), tone: (row) => tone(row.daily_net_inflow) },
    { key: 'five', label: '五日净流入', align: 'right', num: true, value: (row) => compact(row.five_day_net_inflow, '元'), tone: (row) => tone(row.five_day_net_inflow) },
    { key: 'month', label: '一月净流入', align: 'right', num: true, value: (row) => compact(row.one_month_net_inflow, '元'), tone: (row) => tone(row.one_month_net_inflow) },
    { key: 'five-pct', label: '五日涨跌', align: 'right', num: true, value: (row) => percent(row.five_day_change_pct, 2, true), tone: (row) => tone(row.five_day_change_pct) },
    { key: 'month-pct', label: '一月涨跌', align: 'right', num: true, value: (row) => percent(row.one_month_change_pct, 2, true), tone: (row) => tone(row.one_month_change_pct) }
  ];
  const activeColumns: Column<StockConnectActiveRecord>[] = [
    { key: 'security', label: '证券', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'close', label: '收盘', align: 'right', num: true, value: (row) => text(row.close), sub: (row) => percent(row.change_pct, 2, true), tone: (row) => tone(row.change_pct) },
    { key: 'net', label: '净买入', align: 'right', num: true, value: (row) => compact(row.net_buy_10k_yuan, '万元'), tone: (row) => tone(row.net_buy_10k_yuan) },
    { key: 'buy', label: '买入', align: 'right', num: true, value: (row) => compact(row.buy_10k_yuan, '万元') },
    { key: 'sell', label: '卖出', align: 'right', num: true, value: (row) => compact(row.sell_10k_yuan, '万元') },
    { key: 'turnover', label: '通道成交', align: 'right', num: true, value: (row) => compact(row.connect_turnover_yuan, '元') },
    { key: 'share', label: '净买/总成交', align: 'right', num: true, value: (row) => percent(row.net_buy_total_turnover_pct) }
  ];
  const memberColumns: Column<SouthboundIndustryMemberRecord>[] = [
    { key: 'security', label: '港股', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'daily', label: '当日净流入', align: 'right', num: true, value: (row) => compact(row.daily_net_inflow_yuan, '元'), tone: (row) => tone(row.daily_net_inflow_yuan) },
    { key: 'five', label: '五日净流入', align: 'right', num: true, value: (row) => compact(row.five_day_net_inflow_yuan, '元'), tone: (row) => tone(row.five_day_net_inflow_yuan) },
    { key: 'month', label: '一月净流入', align: 'right', num: true, value: (row) => compact(row.one_month_net_inflow_yuan, '元'), tone: (row) => tone(row.one_month_net_inflow_yuan) },
    { key: 'five-pct', label: '五日涨跌', align: 'right', num: true, value: (row) => percent(row.five_day_change_pct, 2, true), tone: (row) => tone(row.five_day_change_pct) },
    { key: 'month-pct', label: '一月涨跌', align: 'right', num: true, value: (row) => percent(row.one_month_change_pct, 2, true), tone: (row) => tone(row.one_month_change_pct) }
  ];
  function load(refresh = false) {
    const selected = CATEGORIES.find((item) => item.id === category) ?? CATEGORIES[0];
    detail.reset();
    void resource.load(`/api/v1/market/stock-connect?${queryString({
      view: selected.view,
      category,
      date: selected.view === 'active-stocks' ? activeDate : '',
      channel: category === 'active-sh' ? 'sh-northbound' : category === 'active-sz' ? 'sz-northbound' : '',
      limit: 5000,
      refresh: refresh ? 1 : 0
    })}`);
  }
  function openSecurity(row: StockConnectHoldingRecord | StockConnectActivityRecord | StockConnectActiveRecord) {
    if (!['sz', 'sh', 'bj'].includes(row.security.market)) return;
    app.setStock({ market: row.security.market, code: row.security.code, name: row.security.name });
    router.go(stockPath(row.security.market, row.security.code, 'leverage'));
  }
  function openIndustry(row: StockConnectIndustryRecord) {
    if (category !== 'southbound-industry') return;
    void detail.load(`/api/v1/market/stock-connect?${queryString({ view: 'industry-detail', group_id: `${row.classification_market}${row.classification_code}`, limit: 5000 })}`);
  }
  onMount(() => load());
</script>

<PageHeader eyebrow="HSGT / HSGTCG / GGTHY · MASTER + DETAIL + CHART" title="沪深港通" description="覆盖日周资金、持仓、指定日活跃股、分类资金和南向行业成分；停止更新的增减仓源明确标为历史快照。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>{/snippet}
</PageHeader>
<Panel flush scroll fill title={CATEGORIES.find((item) => item.id === category)?.label ?? '沪深港通'} busy={resource.busy} error={resource.error} empty={resource.loaded && rows.length === 0} emptyText="该口径当前没有记录。" onRetry={() => load()}>
  {#snippet toolbar()}
    <Select options={CATEGORIES} value={category} width="230px" label="口径" onChange={(next) => { category = next; load(); }} />
    {#if activeMode}<TextInput bind:value={activeDate} width="110px" label="交易日" placeholder="YYYYMMDD" onEnter={() => load()} /><Button onclick={() => load()}>查询日期</Button>{/if}
  {/snippet}
  {#if holdingMode}
    <DataTable columns={holdingColumns} rows={rows as StockConnectHoldingRecord[]} rowKey={(row, index) => `${row.security.security_id}-${row.date}-${index}`} onRowClick={openSecurity} stickyFirst numbered>
      {#snippet cell({ row, column })}{#if column.key === 'fresh'}<Badge tone={row.snapshot_freshness === 'current' ? 'up' : 'warn'}>{row.snapshot_freshness === 'current' ? '当前' : '旧快照'}</Badge>{/if}{/snippet}
    </DataTable>
  {:else if activityMode}
    <DataTable columns={activityColumns} rows={rows as StockConnectActivityRecord[]} rowKey={(row, index) => `${row.security.security_id}-${row.date}-${index}`} onRowClick={openSecurity} stickyFirst numbered />
  {:else if activeMode}
    <DataTable columns={activeColumns} rows={rows as StockConnectActiveRecord[]} rowKey={(row, index) => `${row.security.security_id}-${row.date}-${index}`} onRowClick={openSecurity} stickyFirst numbered />
  {:else if industryMode}
    <DataTable columns={industryColumns} rows={rows as StockConnectIndustryRecord[]} rowKey={(row, index) => `${row.classification_market}${row.classification_code}-${index}`} onRowClick={openIndustry} isActive={(row) => `${row.classification_market}${row.classification_code}` === selectedGroup} numbered />
    {#if detail.loaded || detail.busy}
      <section class="industry-detail">
        <div class="detail-title"><strong>行业 {selectedGroup}</strong><span>{count(detail.data?.count)} 只成分 · {count(industryTrend.length)} 个历史点</span></div>
        {#if detail.error}<p>{detail.error}</p>{:else}<DataTable columns={memberColumns} rows={(detail.data?.records ?? []) as SouthboundIndustryMemberRecord[]} rowKey={(row, index) => `${row.security.security_id}-${index}`} numbered />{/if}
      </section>
    {/if}
  {:else}
    <DataTable columns={flowColumns} rows={rows as StockConnectFlowRecord[]} rowKey={(row, index) => `${row.date}-${index}`} numbered />
  {/if}
</Panel>

<style>
  .industry-detail { margin: var(--sp-3); border: 1px solid var(--line-strong); border-radius: var(--radius); overflow: hidden; }
  .detail-title { display: flex; justify-content: space-between; padding: var(--sp-2) var(--sp-3); background: var(--surface-2); }
  .detail-title span, .industry-detail p { color: var(--fg-mute); font-size: var(--fs-micro); }
  .industry-detail p { padding: var(--sp-3); }
</style>
