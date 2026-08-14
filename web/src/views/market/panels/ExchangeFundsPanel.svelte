<script lang="ts">
  import { queryString } from '../../../api';
  import { compact, count, date, delta, fixed, money, percent, text, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { ExchangeFundRecord, MarketExchangeFundsDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketExchangeFundsDocument>();
  const rows = $derived(resource.data?.records ?? []);

  function load(refresh = false) {
    void resource.load(`/api/v1/market/exchange-funds?${queryString({
      view: 'all', market, code, include_quotes: 1, limit: 50,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function metric(row: ExchangeFundRecord): string {
    if (row.kind === 'etf-performance') return delta(row.change_5d_pct);
    if (row.kind === 'cash-arbitrage') return delta(row.buy_redeem_annualized_pct, 3);
    if (row.kind === 'cash-yield') return percent(row.seven_day_annualized_pct, 3);
    return fixed(row.subscription_price, 3);
  }

  const columns: Column<ExchangeFundRecord>[] = [
    { key: 'kind', label: '场内基金口径', width: '148px', value: (row) => row.kind_label, sub: (row) => row.source_resource },
    { key: 'date', label: '快照 / 询价日', width: '92px', num: true, value: (row) => date(row.snapshot_date) },
    { key: 'metric', label: '核心指标', align: 'right', num: true, value: metric, tone: (row) => tone(row.kind === 'etf-performance' ? row.change_5d_pct : row.kind === 'cash-arbitrage' ? row.buy_redeem_annualized_pct : null) },
    { key: 'current', label: '现价', align: 'right', num: true, value: (row) => fixed(row.current_quote?.last_price, 3), sub: (row) => delta(row.current_quote?.change_pct), tone: (row) => tone(row.current_quote?.change_pct) },
    { key: 'amount', label: '成交 / 发售规模', align: 'right', num: true, value: (row) => row.kind === 'etf-performance' ? money(row.turnover_5d_yuan) : compact(row.offering_total_units, '份') }
  ];

  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 场内基金`} eyebrow="ETF · CASH · REITs" subtitle={`${count(rows.length)} 条命中`} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="该证券当前不在ETF表现、货币基金或REITs发行表中。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.event_id} minWidth="820px" />
</Panel>

{#each rows.filter((row) => row.kind === 'reit-issued' || row.kind === 'reit-pipeline') as row (row.event_id)}
  <Panel title={`${row.security.name || row.security.code} · ${text(row.status)}`} eyebrow={row.source_resource}>
    <p>{text(row.project_description)}</p>
    <p class="note">询价 {text(row.inquiry_price_range)} · 认购价 {fixed(row.subscription_price, 3)} · 期限 {text(row.term)}</p>
  </Panel>
{/each}

<style>
  p { white-space: pre-wrap; font-size: var(--fs-xs); line-height: 1.7; }
  .note { margin-top: var(--sp-2); color: var(--fg-mute); }
</style>
