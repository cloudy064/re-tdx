<script lang="ts">
  import { queryString } from '../../../api';
  import { count, date, fixed, money, percent, price, text } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type {
    MarketSpecialAttentionDocument,
    SpecialAttentionRecord
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketSpecialAttentionDocument>();
  const rows = $derived(resource.data?.records ?? []);

  function load(refresh = false) {
    void resource.load(`/api/v1/market/special-attention?${queryString({
      view: 'all', market, code, limit: 200, refresh: refresh ? 1 : 0
    })}`);
  }

  function primary(row: SpecialAttentionRecord): string {
    if (row.kind === 'investigations') return text(row.reason);
    if (row.kind === 'st-risk') return text(row.risk_type);
    if (row.kind === 'star-cap-removal') return text(row.status_or_forecast);
    if (row.kind === 'goodwill-risk') return money(row.goodwill_current_yuan);
    return text(row.largest_shareholder);
  }

  function secondary(row: SpecialAttentionRecord): string {
    if (row.kind === 'investigations') return `${text(row.progress)} · 立案收盘 ${price(row.filing_close)}`;
    if (row.kind === 'st-risk') return text(row.risk_reason);
    if (row.kind === 'star-cap-removal') return text(row.explanation_excerpt);
    if (row.kind === 'goodwill-risk') return `同比 ${percent(row.goodwill_change_pct)} · 占净利润 ${percent(row.goodwill_to_net_profit_pct)}`;
    return `${text(row.industry)} · 持股 ${percent(row.largest_shareholder_pct)}`;
  }

  const columns: Column<SpecialAttentionRecord>[] = [
    { key: 'kind', label: '关注类型', width: '150px', value: (row) => row.kind_label, sub: (row) => row.source_resource },
    { key: 'date', label: '业务日期', width: '110px', num: true, value: (row) => date(row.date) },
    { key: 'primary', label: '核心信息', width: '260px', value: primary },
    { key: 'detail', label: '补充信息', width: '360px', value: secondary }
  ];

  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 特别关注`} eyebrow="TBGZ · PUBLIC SNAPSHOTS" subtitle={`${count(rows.length)} 条风险或股权快照`} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="该证券当前未命中股权分散、*ST、摘星摘帽、立案调查或商誉风险表。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.event_id} minWidth="880px" />
</Panel>

{#each rows.filter((row) => row.kind === 'investigations') as row (row.event_id)}
  <Panel title={text(row.reason)} eyebrow={`立案调查 · ${date(row.filing_date)}`}>
    <p class="copy">{text(row.case_detail)}</p>
    <p>进展：{text(row.progress)}；十年记录 {count(row.occurrence_count_10y)} 次；处罚披露日 {date(row.penalty_date)}。</p>
  </Panel>
{/each}

{#each rows.filter((row) => row.kind === 'star-cap-removal') as row (row.event_id)}
  <Panel title={text(row.status_or_forecast)} eyebrow={`摘星摘帽 · ${date(row.implementation_date)}`}>
    <p class="copy">{text(row.explanation)}</p>
    <p>本期净利润 {money(row.current_net_profit_yuan)}；上期 {money(row.prior_net_profit_yuan)}；PB {fixed(row.price_to_book_ratio, 3)}。</p>
  </Panel>
{/each}

<style>
  p { font-size: var(--fs-xs); line-height: 1.7; }
  .copy { white-space: pre-wrap; color: var(--fg-dim); }
</style>
