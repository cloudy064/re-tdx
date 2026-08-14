<script lang="ts">
  /**
   * 分时段主力资金。数据链是 200340 → 200341：先按本地研究行业树定位一级行业，
   * 再只请求该行业明细，因此页面同时呈现「个股」与「所属行业」两个口径。
   */
  import { onDestroy, onMount } from 'svelte';
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, fixed, num, percent, tone } from '../../../lib/fmt';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { IntradayFundPeriod, IntradayFundsDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const funds = new Resource<IntradayFundsDocument>();
  const doc = $derived(funds.data);
  const record = $derived(doc?.funds ?? null);

  let timer: ReturnType<typeof setInterval> | undefined;

  function load(silent = false, refresh = false) {
    void funds.load(
      `/api/v1/market/funds?${queryString({ market, code, refresh: refresh ? 1 : 0 })}`,
      { silent }
    );
  }

  type PeriodRow = IntradayFundPeriod & { key: string };

  const rows = $derived<PeriodRow[]>(
    Object.entries(record?.periods ?? {}).map(([key, value]) => ({ ...value, key }))
  );

  const industryRows = $derived<PeriodRow[]>(
    Object.entries(doc?.industry.summary.periods ?? {}).map(([key, value]) => ({ ...value, key }))
  );

  const columns: Column<PeriodRow>[] = [
    { key: 'name', label: '时段', width: '78px', value: (row) => row.name },
    {
      key: 'net',
      label: '主力净流入',
      align: 'right',
      num: true,
      value: (row) => compact(row.net_main_inflow, '元'),
      tone: (row) => tone(num(row.net_main_inflow))
    },
    {
      key: 'turnover',
      label: '成交额',
      align: 'right',
      num: true,
      value: (row) => compact(row.turnover, '元')
    },
    {
      key: 'share',
      label: '主力占比',
      align: 'right',
      num: true,
      value: (row) => percent(row.net_main_share_pct),
      tone: (row) => tone(num(row.net_main_share_pct))
    },
    {
      key: 'change',
      label: '阶段涨幅',
      align: 'right',
      num: true,
      value: (row) => percent(row.change_pct, 2, true),
      tone: (row) => tone(num(row.change_pct))
    },
    {
      key: 'volume',
      label: '成交量',
      align: 'right',
      num: true,
      value: (row) => compact(row.volume, '手')
    },
    {
      key: 'relative',
      label: '相对量',
      align: 'right',
      num: true,
      value: (row) => fixed(row.relative_volume)
    }
  ];

  const stats = $derived.by<Stat[]>(() => {
    if (!record) return [];
    return [
      { label: '现价', value: fixed(record.quote.last), tone: tone(num(record.quote.change_pct)) },
      {
        label: '涨跌幅',
        value: percent(record.quote.change_pct, 2, true),
        tone: tone(num(record.quote.change_pct))
      },
      { label: '涨跌额', value: fixed(record.quote.change) },
      { label: '前 5 日分钟均量', value: compact(record.quote.previous_5day_minute_volume, '手') }
    ];
  });

  $effect(() => {
    void market;
    void code;
    load();
  });

  onMount(() => {
    timer = setInterval(() => {
      if (!document.hidden) load(true);
    }, 15000);
  });

  onDestroy(() => clearInterval(timer));
</script>

<Panel
  title="个股分时段主力资金"
  eyebrow="PBRPC · 200340 → 200341"
  subtitle={doc?.found ? `缓存 ${doc.cache.detail_age_seconds}s / TTL ${doc.cache.ttl_seconds}s` : ''}
  busy={funds.busy}
  error={funds.error}
  onRetry={() => load()}
  empty={funds.loaded && !funds.busy && !record}
  emptyText="该标的不在当前行业资金明细中"
  flush
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={funds.busy} onclick={() => load(false, true)}>强制更新</Button>
  {/snippet}

  {#if record}
    <div class="pad"><StatGrid stats={stats} columns={4} /></div>
    <DataTable {columns} rows={rows} rowKey={(row) => row.key} />
  {/if}
</Panel>

{#if doc?.industry}
  <Panel
    title="所属一级行业"
    eyebrow="RESEARCH INDUSTRY"
    subtitle={`${doc.industry.name} ${doc.industry.code} · ${doc.industry.component_count} 只成分`}
    flush
    scroll
  >
    <DataTable {columns} rows={industryRows} rowKey={(row) => row.key} />
  </Panel>
{/if}

<style>
  .pad {
    padding: var(--sp-3) var(--sp-4);
    border-bottom: 1px solid var(--line);
  }
</style>
