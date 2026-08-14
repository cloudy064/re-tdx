<script lang="ts">
  import { queryString } from '../../../api';
  import { count, date, delta, fixed, money, price, text, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { MarketSpecialSituationsDocument, SpecialSituationRecord } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketSpecialSituationsDocument>();
  const rows = $derived(resource.data?.records ?? []);

  function load(refresh = false) {
    void resource.load(`/api/v1/market/special-situations?${queryString({
      view: 'all', market, code, include_quotes: 1, limit: 100,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function relation(row: SpecialSituationRecord): string {
    if (row.kind === 'market-cap-risk') return text(row.sample_index);
    if (row.kind.startsWith('major-') || row.kind === 'ordinary-merger-plan') return text(row.acquiring_party);
    if (row.kind === 'neeq-transfer-plan') return text(row.target_board || row.adviser);
    if (row.kind === 'neeq-transfer-completed') return `${text(row.listing_venue_before)} → ${text(row.listing_venue_after)}`;
    if (row.kind === 'neeq-regulation') return text(row.regulation_reason);
    return row.related_security?.name || row.related_security?.code || '—';
  }

  function reference(row: SpecialSituationRecord): string {
    if (row.transaction_amount_yuan !== undefined) return money(row.transaction_amount_yuan);
    if (row.net_profit_yuan !== undefined) return money(row.net_profit_yuan);
    if (row.kind === 'neeq-regulation') return text(row.regulation_measure);
    if (row.kind === 'market-cap-risk') return price(row.one_year_high_price);
    return fixed(row.kind === 'b-to-h' ? row.cash_option_price : row.absorber_exchange_price, row.kind === 'b-to-h' ? 3 : 2);
  }

  function deviation(row: SpecialSituationRecord): string {
    if (row.kind.startsWith('major-') || row.kind === 'ordinary-merger-plan') return text(row.transaction_type);
    if (row.kind === 'neeq-transfer-plan') return money(row.revenue_yuan);
    if (row.kind === 'neeq-transfer-completed') return date(row.listing_date);
    if (row.kind === 'neeq-regulation') return date(row.date);
    return row.kind === 'market-cap-risk' ? delta(row.high_to_current_change_pct) : delta(row.kind === 'b-to-h' ? row.cash_option_premium_pct : row.absorber_exchange_premium_pct);
  }

  const columns: Column<SpecialSituationRecord>[] = [
    { key: 'kind', label: '事项', width: '104px', value: (row) => row.kind_label, sub: (row) => text(row.status) },
    { key: 'relation', label: '关联对象', width: '170px', value: relation, sub: (row) => row.related_security?.security_id || text(row.trigger_type) },
    { key: 'current', label: '现价', align: 'right', num: true, value: (row) => price(row.primary_quote?.last_price), sub: (row) => delta(row.primary_quote?.change_pct), tone: (row) => tone(row.primary_quote?.change_pct) },
    { key: 'reference', label: '参考值', align: 'right', num: true, value: reference },
    { key: 'deviation', label: '类型 / 偏离', align: 'right', num: true, value: deviation },
    { key: 'date', label: '统计日', width: '86px', num: true, value: (row) => date(row.date) }
  ];

  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 并购、转板与特殊事项`} eyebrow="AGTL · CDGC · QXFA · XSBTJ" subtitle={`${count(rows.length)} 条关系`} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="该证券当前不在并购重组、转板、自律监管或市值管理预警表中。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.event_id} minWidth="840px" />
</Panel>

{#each rows as row (row.event_id)}
  {#if row.description}
    <Panel title={`${row.kind_label} · ${text(row.status)}`} eyebrow={row.source_resource}>
      <p class="copy">{row.description}</p>
      {#if row.announcement_url}<a href={row.announcement_url} target="_blank" rel="noreferrer">打开预案公告</a>{/if}
    </Panel>
  {/if}
{/each}

<style>
  .copy { white-space: pre-wrap; font-size: var(--fs-xs); line-height: 1.7; }
  a { display: inline-block; margin-top: var(--sp-2); color: var(--focus); font-size: var(--fs-xs); }
</style>
