<script lang="ts">
  import { queryString } from '../../../api';
  import { count, date, fixed, money, percent, text } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { CuratedDataRecord, MarketCuratedDataDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketCuratedDataDocument>();
  const rows = $derived(resource.data?.records ?? []);

  function load(refresh = false) {
    void resource.load(`/api/v1/market/curated-data?${queryString({
      view: 'all', market, code, limit: 100, refresh: refresh ? 1 : 0
    })}`);
  }

  function primary(row: CuratedDataRecord): string {
    if (row.kind === 'low-valuation-smallcap') return `PEG ${fixed(row.estimated_peg, 3)}`;
    if (row.kind === 'high-dividend') return percent(row.payout_ratio_pct);
    if (row.kind === 'below-book-soe') return `PB ${fixed(row.price_to_book_ratio, 3)}`;
    if (row.kind === 'dividend-fundraising') return `分红/募资 ${fixed(row.dividend_fundraising_ratio, 2)}`;
    if (row.kind === 'high-refinancing-lending') return money(row.refinancing_lending_balance_yuan);
    if (row.kind === 'media-entertainment') return text(row.title);
    return '—';
  }

  function secondary(row: CuratedDataRecord): string {
    if (row.kind === 'low-valuation-smallcap') return `PE ${fixed(row.pe, 2)} · EPS增速 ${percent(row.forecast_eps_growth_pct)}`;
    if (row.kind === 'high-dividend') return `分红 ${money(row.dividend_yuan)}`;
    if (row.kind === 'below-book-soe') return text(row.controlling_shareholder);
    if (row.kind === 'dividend-fundraising') return `股息率 ${percent(row.dividend_yield_pct)}`;
    if (row.kind === 'high-refinancing-lending') return row.has_lending_data ? `占比 ${percent(row.balance_ratio_pct)}` : '源余额为空';
    if (row.kind === 'media-entertainment') return `${count(row.movie_count)} 部电影 · ${count(row.drama_count)} 部剧`;
    return row.source_resource;
  }

  const columns: Column<CuratedDataRecord>[] = [
    { key: 'kind', label: '客户端入选功能', width: '170px', value: (row) => row.kind_label, sub: (row) => row.source_resource },
    { key: 'date', label: '快照 / 业务日期', width: '110px', num: true, value: (row) => date(row.date) },
    { key: 'metric', label: '核心指标', align: 'right', num: true, value: primary },
    { key: 'detail', label: '补充信息', width: '300px', value: secondary }
  ];

  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 客户端精选`} eyebrow="TDX CURATED" subtitle={`${count(rows.length)} 条入选关系`} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="该证券当前未命中低估值、高分红、破净国企、转融券或传媒娱乐等客户端精选表。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.event_id} minWidth="820px" />
</Panel>

{#each rows.filter((row) => row.kind === 'below-book-soe') as row (row.event_id)}
  <Panel title="国企控制关系" eyebrow={row.source_resource}>
    <p>控股股东：{text(row.controlling_shareholder)}（{text(row.controlling_shareholder_nature)}，{percent(row.controlling_shareholder_pct)}）</p>
    <p>实际控制人：{text(row.actual_controller)}（{text(row.actual_controller_nature)}，{percent(row.actual_controller_pct)}）</p>
  </Panel>
{/each}

{#each rows.filter((row) => row.kind === 'media-entertainment') as row (row.event_id)}
  <Panel title={text(row.title)} eyebrow={`传媒娱乐 · ${date(row.release_date)}`}>
    <p class="copy">{text(row.background)}</p>
  </Panel>
{/each}

<style>
  p { font-size: var(--fs-xs); line-height: 1.7; }
  .copy { white-space: pre-wrap; color: var(--fg-dim); }
</style>
