<script lang="ts">
  /** 单票两融日序列与陆股通季度持仓；均按证券键直取，不依赖当前榜单命中。 */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, percent, tone } from '../../../lib/fmt';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { MarginRecord, MarketMarginDocument, MarketStockConnectDocument, StockConnectHistoryRecord } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();
  const margin = new Resource<MarketMarginDocument>();
  const connect = new Resource<MarketStockConnectDocument>();
  const marginRows = $derived(margin.data?.records ?? []);
  const connectRows = $derived((connect.data?.records ?? []) as StockConnectHistoryRecord[]);
  const latestMargin = $derived(marginRows[0] ?? null);
  const latestConnect = $derived(connectRows[0] ?? null);

  function load(refresh = false) {
    const query = { market, code, refresh: refresh ? 1 : 0 };
    void margin.load(`/api/v1/market/margin?${queryString({ ...query, view: 'security' })}`);
    void connect.load(`/api/v1/market/stock-connect?${queryString({ ...query, view: 'security' })}`);
  }
  const marginStats = $derived.by<Stat[]>(() => latestMargin ? [
    { label: '数据日', value: date(latestMargin.date), note: `${count(marginRows.length)} 个交易日` },
    { label: '融资余额', value: compact(latestMargin.financing_balance_yuan, '元') },
    { label: '融资占流通市值', value: percent(latestMargin.financing_balance_float_market_pct) },
    { label: '当日融资净买', value: compact(latestMargin.financing_net_buy_yuan, '元'), tone: tone(latestMargin.financing_net_buy_yuan) },
    { label: '融券余量', value: compact(latestMargin.short_balance_shares, '股') },
    { label: '融券占流通股', value: percent(latestMargin.short_balance_float_shares_pct) }
  ] : []);
  const connectStats = $derived.by<Stat[]>(() => latestConnect ? [
    { label: '报告期', value: date(latestConnect.date), note: `${count(connectRows.length)} 个季度` },
    { label: '陆股通持股', value: compact(latestConnect.holding_shares, '股') },
    { label: '占自由流通股', value: percent(latestConnect.holding_ratio_pct) },
    { label: '持股市值', value: compact(latestConnect.holding_market_value_yuan, '元') },
    { label: '持股变动', value: compact(latestConnect.holding_share_change, '股'), tone: tone(latestConnect.holding_share_change) },
    { label: '市值变动', value: compact(latestConnect.market_value_change_yuan, '元'), tone: tone(latestConnect.market_value_change_yuan) }
  ] : []);
  const marginColumns: Column<MarginRecord>[] = [
    { key: 'date', label: '日期', width: '94px', value: (row) => date(row.date) },
    { key: 'financing', label: '融资余额', align: 'right', num: true, value: (row) => compact(row.financing_balance_yuan, '元') },
    { key: 'net', label: '融资净买', align: 'right', num: true, value: (row) => compact(row.financing_net_buy_yuan, '元'), tone: (row) => tone(row.financing_net_buy_yuan) },
    { key: 'ratio', label: '融资占流通市值', align: 'right', num: true, value: (row) => percent(row.financing_balance_float_market_pct) },
    { key: 'short', label: '融券余量', align: 'right', num: true, value: (row) => compact(row.short_balance_shares, '股') },
    { key: 'difference', label: '两融差额', align: 'right', num: true, value: (row) => compact(row.financing_short_difference_yuan, '元') }
  ];
  const connectColumns: Column<StockConnectHistoryRecord>[] = [
    { key: 'date', label: '报告期', width: '94px', value: (row) => date(row.date) },
    { key: 'shares', label: '持股数量', align: 'right', num: true, value: (row) => compact(row.holding_shares, '股') },
    { key: 'change', label: '持股变动', align: 'right', num: true, value: (row) => compact(row.holding_share_change, '股'), tone: (row) => tone(row.holding_share_change) },
    { key: 'ratio', label: '占自由流通股', align: 'right', num: true, value: (row) => percent(row.holding_ratio_pct) },
    { key: 'value', label: '持股市值', align: 'right', num: true, value: (row) => compact(row.holding_market_value_yuan, '元') },
    { key: 'value-change', label: '市值变动', align: 'right', num: true, value: (row) => compact(row.market_value_change_yuan, '元'), tone: (row) => tone(row.market_value_change_yuan) }
  ];
  $effect(() => { void market; void code; load(); });
</script>

<Panel title="融资融券" eyebrow="RZRQ1/2 · 证券键直取" subtitle="近三月日度明细，不要求当前排行命中" busy={margin.busy} error={margin.error} onRetry={load} empty={margin.loaded && marginRows.length === 0} emptyText="该证券当前没有可下载的融资融券历史。" scroll>
  <StatGrid stats={marginStats} columns={3} />
  <DataTable columns={marginColumns} rows={marginRows} rowKey={(row) => row.date} />
</Panel>
<Panel title="陆股通持股历史" eyebrow="HSGTCG1 · JD SECURITY KEY" subtitle="季度主表的 jd<代码> 动态键已反向绑定到真实证券" busy={connect.busy} error={connect.error} onRetry={load} empty={connect.loaded && connectRows.length === 0} emptyText="该证券没有陆股通或港股通持股历史。" scroll>
  <StatGrid stats={connectStats} columns={3} />
  <DataTable columns={connectColumns} rows={connectRows} rowKey={(row) => row.date} />
</Panel>
