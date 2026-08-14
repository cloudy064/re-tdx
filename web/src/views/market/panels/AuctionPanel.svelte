<script lang="ts">
  /** 集合竞价逐点：开盘段与收盘段的虚拟撮合过程。 */
  import { onDestroy, onMount } from 'svelte';
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, fixed, text } from '../../../lib/fmt';
  import Button from '../../../ui/Button.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import AuctionChart from '../../../charts/AuctionChart.svelte';
  import type { AuctionDocument, AuctionSegmentSummary } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const auction = new Resource<AuctionDocument>();
  const record = $derived(auction.data?.records[0] ?? null);
  const points = $derived(record?.points ?? []);

  let timer: ReturnType<typeof setInterval> | undefined;

  function load(silent = false) {
    void auction.load(`/api/v1/market/auction?${queryString({ market, code })}`, { silent });
  }

  function segmentStats(segment: AuctionSegmentSummary | undefined): Stat[] {
    if (!segment) return [];
    return [
      { label: '采样点', value: String(segment.point_count) },
      { label: '时间窗', value: `${text(segment.start_time)} — ${text(segment.end_time)}` },
      { label: '首个价格', value: fixed(segment.first_price) },
      { label: '末次价格', value: fixed(segment.last_sample_price) },
      { label: '价格区间', value: `${fixed(segment.min_price)} — ${fixed(segment.max_price)}` },
      { label: '末次匹配量', value: compact(segment.last_sample_matched_volume_hand, '手') },
      { label: '末次匹配额', value: compact(segment.last_sample_matched_amount_yuan, '元') },
      {
        label: '末次未匹配',
        value: compact(segment.last_sample_unmatched_signed_hand, '手'),
        note: `方向翻转 ${segment.unmatched_direction_flips} 次`
      }
    ];
  }

  const openingStats = $derived(segmentStats(record?.summary.opening));
  const closingStats = $derived(segmentStats(record?.summary.closing));

  $effect(() => {
    void market;
    void code;
    load();
  });

  onMount(() => {
    // 竞价窗口内数据持续变化；页面不可见时不轮询
    timer = setInterval(() => {
      if (!document.hidden) load(true);
    }, 10000);
  });

  onDestroy(() => clearInterval(timer));
</script>

<Panel
  title="集合竞价逐点"
  eyebrow="0x056A · TCP 7709"
  subtitle={auction.data ? `交易日 ${auction.data.server_trade_date} · ${points.length} 个采样点` : ''}
  busy={auction.busy}
  error={auction.error}
  onRetry={() => load()}
  empty={auction.loaded && !auction.busy && points.length === 0}
  emptyText="该标的当日没有返回竞价采样点"
  fill
>
  {#snippet actions()}
    <Button icon="refresh" busy={auction.busy} onclick={() => load()}>刷新</Button>
  {/snippet}

  {#if points.length}
    <AuctionChart {points} tradeDate={auction.data?.server_trade_date ?? ''} />
  {/if}
</Panel>

{#if record}
  <div class="segments">
    <Panel title="开盘竞价" eyebrow="09:15 — 09:25" scroll>
      <StatGrid stats={openingStats} columns={2} />
    </Panel>
    <Panel title="收盘竞价" eyebrow="14:57 — 15:00" scroll>
      <StatGrid stats={closingStats} columns={2} />
    </Panel>
  </div>
{/if}

<style>
  .segments {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: var(--sp-2);
    flex: none;
  }

  @media (max-width: 900px) {
    .segments {
      grid-template-columns: minmax(0, 1fr);
    }
  }
</style>
