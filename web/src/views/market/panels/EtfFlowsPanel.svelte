<script lang="ts">
  import { queryString } from '../../../api';
  import { compact, count, date, percent, signedCompact, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Button from '../../../ui/Button.svelte';
  import Icon from '../../../ui/Icon.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { EtfFlowRow, MarketEtfFlowsDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const flows = new Resource<MarketEtfFlowsDocument>();
  const stock = $derived(flows.data?.selected ?? null);
  const industry = $derived(flows.data?.related_industry ?? null);

  function load(refresh = false) {
    void flows.load(`/api/v1/market/etf-flows?${queryString({ view: 'stocks', market, code, refresh: refresh ? 1 : 0 })}`);
  }

  function stats(row: EtfFlowRow | null): Stat[] {
    if (!row) return [];
    return [
      { label: '持有 ETF 数', value: count(row.etf_count), note: `截止 ${date(row.cutoff_date)}` },
      { label: 'ETF 持仓市值', value: compact(row.holding_market_value_yuan, '元'), note: compact(row.holding_shares, '股') },
      { label: '宽基净流入', value: signedCompact(row.broad_net_inflow_yuan, '元'), tone: tone(row.broad_net_inflow_yuan) },
      { label: '主题净流入', value: signedCompact(row.theme_net_inflow_yuan, '元'), tone: tone(row.theme_net_inflow_yuan) },
      { label: '当日净流入', value: signedCompact(row.total_net_inflow_yuan, '元'), note: `占成交额 ${percent(row.net_inflow_share_turnover_pct)}`, tone: tone(row.total_net_inflow_yuan) },
      { label: '一周净流入', value: signedCompact(row.weekly_net_inflow_yuan, '元'), tone: tone(row.weekly_net_inflow_yuan) }
    ];
  }

  $effect(() => { void market; void code; load(); });
</script>

<Panel
  title={`${name} · ETF 资金动向`}
  eyebrow="ETFSG · STOCK"
  subtitle={stock ? `持仓与申赎估算 · 截止 ${date(stock.cutoff_date)}` : ''}
  busy={flows.busy}
  error={flows.error}
  onRetry={() => load()}
  empty={flows.loaded && !flows.busy && !stock}
  emptyText="该证券不在当前 ETF 持股资金表中。"
>
  {#snippet actions()}<Button icon="refresh" busy={flows.busy} onclick={() => load(true)}>强制更新</Button>{/snippet}
  <StatGrid stats={stats(stock)} columns={3} />
</Panel>

{#if industry}
  <Panel title={`${industry.entity.name || industry.entity.code} · ETF 行业资金`} eyebrow="RESEARCH INDUSTRY · LEVEL 1" subtitle="由通达信一级研究行业关系关联">
    <StatGrid stats={stats(industry)} columns={3} />
  </Panel>
{/if}

<p class="note"><Icon name="info" size={12} /><span>这里展示的是 ETF 申赎对成分股的估算传导和 ETF 持仓快照，<strong>不是个股逐笔主力资金，也不是实时仓位</strong>。</span></p>

<style>
  .note { display: flex; align-items: flex-start; gap: var(--sp-2); padding: var(--sp-2) var(--sp-4); font-size: var(--fs-micro); color: var(--warn); background: var(--warn-soft); border: 1px solid var(--line); border-radius: var(--radius-lg); }
  .note strong { font-weight: 600; }
</style>
