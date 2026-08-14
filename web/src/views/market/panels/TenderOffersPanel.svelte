<script lang="ts">
  /** 该股历次要约收购事件（YYSG101）。 */
  import { queryString } from '../../../api';
  import { compact, count, date, fixed, money, percent, text } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { MarketTenderOffersDocument, TenderOfferRecord } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketTenderOffersDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);

  function load(refresh = false) {
    void resource.load(`/api/v1/market/tender-offers?${queryString({
      market, code, limit: 100, sort: 'announcement-date', order: 'desc',
      refresh: refresh ? 1 : 0
    })}`);
  }

  const stats = $derived.by<Stat[]>(() => [
    { label: '历史要约', value: count(doc?.summary.rows), note: name || code },
    { label: '最新公告', value: date(doc?.summary.latest_announcement_date) },
    { label: '拟要约资金', value: money(doc?.summary.planned_funds_yuan) },
    { label: '拟要约股数', value: compact(doc?.summary.planned_shares, '股') },
    { label: '实际受让股数', value: compact(doc?.summary.actual_shares, '股') },
    { label: '数据状态', value: doc?.availability === 'stale-cache' ? '缓存降级' : doc?.availability === 'live' ? '在线' : '无记录' }
  ]);

  const columns: Column<TenderOfferRecord>[] = [
    { key: 'announcement', label: '最新公告', width: '92px', num: true, value: (row) => date(row.announcement_date) },
    { key: 'status', label: '转让进度', width: '86px', value: (row) => row.status },
    { key: 'acquirer', label: '收购人', width: '185px', wrap: true, value: (row) => text(row.acquirer) },
    { key: 'price', label: '要约价', align: 'right', num: true, value: (row) => fixed(row.offer_price, 2), sub: (row) => row.currency },
    { key: 'planned', label: '拟要约', align: 'right', num: true, value: (row) => compact(row.planned_shares, '股'), sub: (row) => `${percent(row.planned_total_pct)} · ${money(row.planned_funds_yuan)}` },
    { key: 'actual', label: '实际受让', align: 'right', num: true, value: (row) => compact(row.actual_shares, '股'), sub: (row) => row.actual_shares == null ? '尚无结果' : `${percent(row.actual_total_pct)} · 完成 ${percent(row.actual_to_planned_pct)}` },
    { key: 'window', label: '要约窗口', width: '176px', num: true, value: (row) => `${date(row.start_date)} — ${date(row.end_date)}`, sub: (row) => row.transfer_date ? `过户 ${date(row.transfer_date)}` : '' },
    { key: 'purpose', label: '要约目的', width: '360px', wrap: true, value: (row) => text(row.purpose) }
  ];

  $effect(() => {
    market; code;
    load();
  });
</script>

<Panel title="要约收购" eyebrow="YYSG101 · SECURITY HISTORY" subtitle="按事件保留该股历次收购人、转让进度、拟定与实际规模及完整收购目的" busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="该证券当前没有命中通达信要约收购主表。" scroll>
  {#snippet actions()}
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}
  <StatGrid {stats} columns={6} />
</Panel>

<Panel title="历次要约事件" subtitle={`${count(rows.length)} 条；空值表示上游尚未披露实际受让结果`} empty={resource.loaded && rows.length === 0} flush scroll>
  <DataTable columns={columns} rows={rows} rowKey={(row) => row.offer_id} minWidth="1220px" />
</Panel>
