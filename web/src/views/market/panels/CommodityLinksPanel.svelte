<script lang="ts">
  /** 单票反查涨价题材、驱动事件和题材所映射的商品报价。 */
  import { queryString } from '../../../api';
  import { count, date, fixed, percent, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import { router } from '../../../lib/router.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid from '../../../ui/StatGrid.svelte';
  import type {
    CommodityQuoteRecord,
    CommoditySecurityAssociationRecord,
    MarketCommodityLinksDocument,
    ThemeDriverRecord
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketCommodityLinksDocument>();
  const records = $derived((resource.data?.records ?? []) as CommoditySecurityAssociationRecord[]);
  let selected = $state<CommoditySecurityAssociationRecord | null>(null);

  async function load(refresh = false) {
    const result = await resource.load(`/api/v1/market/commodity-links?${queryString({
      view: 'security', market, code, limit: 100, refresh: refresh ? 1 : 0
    })}`);
    const rows = (result?.records ?? []) as CommoditySecurityAssociationRecord[];
    selected = rows[0] ?? null;
  }

  const associationColumns: Column<CommoditySecurityAssociationRecord>[] = [
    { key: 'theme', label: '涨价题材', width: '110px', value: (row) => row.theme.name, sub: (row) => `ID ${row.theme.theme_id}` },
    { key: 'price', label: '触发参考价', width: '90px', align: 'right', num: true, value: (row) => fixed(row.theme_stock?.trigger_price, 2) },
    { key: 'latest', label: '最新驱动', width: '390px', wrap: true, value: (row) => row.theme.latest_driver_title, sub: (row) => date(row.theme.latest_driver_date), sortValue: (row) => row.theme.latest_driver_date },
    { key: 'events', label: '命中事件', width: '76px', align: 'right', num: true, value: (row) => count(row.drivers.length), sortValue: (row) => row.drivers.length },
    { key: 'commodity', label: '关联商品', width: '140px', value: (row) => row.associated_commodity_quotes.map((quote) => quote.name).join(' / ') || '—' }
  ];
  const driverColumns: Column<ThemeDriverRecord>[] = [
    { key: 'date', label: '日期', width: '90px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'title', label: '该票命中的驱动事件', width: '480px', wrap: true, value: (row) => row.title },
    { key: 'stocks', label: '事件股票', width: '74px', align: 'right', num: true, value: (row) => count(row.stock_count) }
  ];
  const quoteColumns: Column<CommodityQuoteRecord>[] = [
    { key: 'name', label: '商品', width: '118px', value: (row) => row.name, sub: (row) => row.commodity_id },
    { key: 'price', label: '最新价', align: 'right', num: true, value: (row) => `${fixed(row.latest_price, 2)} ${row.unit}` },
    { key: 'day', label: '当日', align: 'right', num: true, value: (row) => percent(row.day_change_pct), tone: (row) => tone(row.day_change_pct) },
    { key: '30d', label: '30 日', align: 'right', num: true, value: (row) => percent(row.change_30d_pct), tone: (row) => tone(row.change_30d_pct) },
    { key: 'date', label: '报价日', num: true, value: (row) => date(row.quote_date) }
  ];

  $effect(() => { market; code; void load(); });
</script>

<div class="stack">
  <Panel title={`${name} · 商股联动`} subtitle="从完整涨价题材股票集合反查；未命中是正常关系" busy={resource.busy} error={resource.error} onRetry={() => void load()} empty={resource.loaded && records.length === 0} emptyText="当前证券不在通达信 21 个涨价题材的关联股票集合中。">
    {#snippet actions()}
      <Button onclick={() => router.go('/data/commodity-links')}>打开关系中心</Button>
      <Button icon="refresh" busy={resource.busy} onclick={() => void load(true)}>刷新</Button>
    {/snippet}
    {#if records.length}
      <StatGrid inline stats={[
        { label: '命中题材', value: count(records.length) },
        { label: '历史驱动', value: count(records.reduce((sum, row) => sum + row.drivers.length, 0)) },
        { label: '关联商品', value: count(new Set(records.flatMap((row) => row.associated_commodity_quotes.map((quote) => quote.commodity_id))).size) },
        { label: '来源', value: count(resource.data?.counts.sources), note: '按命中题材按需展开' }
      ]} />
    {/if}
  </Panel>

  {#if records.length}
    <Panel title="题材关系" subtitle="点击查看该题材下本票命中的历史驱动与商品报价" flush scroll>
      <DataTable columns={associationColumns} rows={records} rowKey={(row) => row.theme.theme_id} onRowClick={(row) => (selected = row)} isActive={(row) => row.theme.theme_id === selected?.theme.theme_id} sortKey="latest" />
    </Panel>
  {/if}

  <div class="pair">
    <Panel title={selected ? `${selected.theme.name} · 历史驱动命中` : '历史驱动命中'} subtitle="主表中的事件股票集合已足够完成单票反查" busy={resource.busy} empty={!selected || selected.drivers.length === 0} emptyText="该题材的历史驱动没有精确命中当前证券。" flush scroll fill>
      <DataTable columns={driverColumns} rows={selected?.drivers ?? []} rowKey={(row) => row.driver_id} sortKey="date" />
    </Panel>
    <Panel title="题材映射商品" subtitle="商品 ID 可继续展开关联股、行业与 ETF" busy={resource.busy} empty={!selected || selected.associated_commodity_quotes.length === 0} emptyText="该题材没有映射到当前商品报价主表。" flush scroll fill>
      <DataTable columns={quoteColumns} rows={selected?.associated_commodity_quotes ?? []} rowKey={(row) => row.quote_id} />
    </Panel>
  </div>
</div>

<style>
  .stack { display: flex; min-height: 0; flex: 1; flex-direction: column; gap: var(--sp-2); overflow: hidden; }
  .pair { display: grid; min-height: 0; flex: 1; grid-template-columns: minmax(0, 1.15fr) minmax(300px, .85fr); gap: var(--sp-2); }
  @media (max-width: 1050px) { .pair { grid-template-columns: 1fr; } }
</style>
