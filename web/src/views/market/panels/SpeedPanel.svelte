<script lang="ts">
  /** 0x053E 服务端涨速与五档快照；与右栏 0x0547/SSE 深度流保持独立。 */
  import { onDestroy, onMount } from 'svelte';
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, delta, fixed, price, tone } from '../../../lib/fmt';
  import Button from '../../../ui/Button.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { DepthLevel, SpeedDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const speed = new Resource<SpeedDocument>();
  const record = $derived(speed.data?.records[0] ?? null);
  let timer: ReturnType<typeof setInterval> | undefined;

  function load(silent = false) {
    void speed.load(`/api/v1/market/speed?${queryString({ market, code })}`, { silent });
  }

  const stats = $derived.by<Stat[]>(() => {
    if (!record) return [];
    return [
      {
        label: '服务端涨速',
        value: delta(record.rise_speed_pct),
        tone: tone(record.rise_speed_pct),
        note: `raw ${record.rise_speed_raw}`
      },
      {
        label: '最新 / 涨跌幅',
        value: `${price(record.last_price)} / ${delta(record.change_pct)}`,
        tone: tone(record.change_pct)
      },
      { label: '今开 / 昨收', value: `${price(record.open_price)} / ${price(record.pre_close_price)}` },
      { label: '最高 / 最低', value: `${price(record.high_price)} / ${price(record.low_price)}` },
      { label: '成交量', value: compact(record.total_hand, '手') },
      { label: '现手', value: compact(record.current_hand, '手') },
      { label: '成交额', value: compact(record.amount, '元') },
      { label: '内盘 / 外盘', value: `${compact(record.inside_dish, '手')} / ${compact(record.outer_disc, '手')}` },
      { label: '开盘金额', value: compact(record.open_amount_yuan, '元') },
      { label: '基金 IOPV', value: record.fund_iopv === null ? '—' : fixed(record.fund_iopv, 4) }
    ];
  });

  const rawStats = $derived.by<Stat[]>(() => {
    if (!record) return [];
    return [
      { label: '时间 raw', value: String(record.time_raw) },
      { label: '状态 raw', value: String(record.status_raw) },
      { label: '活跃值 raw', value: String(record.active) },
      { label: '尾字段 raw', value: String(record.tail_raw) },
      { label: '辅助价差 raw', value: String(record.auxiliary_price_delta_raw) },
      { label: '竞价失衡手数 raw', value: String(record.auction_imbalance_hand_raw) },
      { label: '扩展标记 raw', value: String(record.extension_marker_raw) },
      { label: '扩展值 raw', value: record.extension_values_raw.join(' / ') || '—' }
    ];
  });

  function levelRows(levels: DepthLevel[], side: 'buy' | 'sell') {
    return side === 'sell' ? [...levels].reverse() : levels;
  }

  $effect(() => {
    void market;
    void code;
    speed.reset();
    load();
  });

  onMount(() => {
    timer = setInterval(() => {
      if (!document.hidden) load(true);
    }, 5000);
  });

  onDestroy(() => clearInterval(timer));
</script>

<Panel
  title="服务端涨速快照"
  eyebrow="0x053E · TCP 7709"
  subtitle={speed.data
    ? `${speed.data.server_name} · ${speed.data.endpoint} · ${speed.data.generated_at} · 5 秒刷新`
    : '独立于右栏 0x0547/SSE 深度流'}
  busy={speed.busy}
  error={speed.error}
  onRetry={() => load()}
  empty={speed.loaded && !speed.busy && !record}
  emptyText="服务端没有返回该证券的涨速记录"
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={speed.busy} onclick={() => load()}>刷新</Button>
  {/snippet}

  {#if record}
    <StatGrid {stats} columns={5} />

    <div class="books">
      <section class="book" aria-label="卖盘五档">
        <h3>卖盘</h3>
        {#each levelRows(record.sell_levels, 'sell') as level, index (index)}
          <div class="level">
            <span class="side down">卖{record.sell_levels.length - index}</span>
            <span class="num down">{price(level.price)}</span>
            <span class="num">{compact(level.volume_hand, '手')}</span>
            <span class="num mute">{compact(level.amount_yuan, '元')}</span>
          </div>
        {/each}
      </section>

      <section class="book" aria-label="买盘五档">
        <h3>买盘</h3>
        {#each levelRows(record.buy_levels, 'buy') as level, index (index)}
          <div class="level">
            <span class="side up">买{index + 1}</span>
            <span class="num up">{price(level.price)}</span>
            <span class="num">{compact(level.volume_hand, '手')}</span>
            <span class="num mute">{compact(level.amount_yuan, '元')}</span>
          </div>
        {/each}
      </section>
    </div>

    <div class="raw">
      <h3>原生尾字段</h3>
      <p>未恢复业务含义的字段只按 raw 展示，不推断方向、状态或竞价语义。</p>
      <StatGrid stats={rawStats} columns={4} />
    </div>
  {/if}
</Panel>

<style>
  .books {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: var(--sp-3);
    margin-top: var(--sp-4);
  }

  .book,
  .raw {
    min-width: 0;
    padding: var(--sp-3);
    border: 1px solid var(--line);
    border-radius: var(--radius-md);
    background: var(--bg-raised);
  }

  h3 {
    margin-bottom: var(--sp-2);
    font-size: var(--fs-caption);
    color: var(--fg-mute);
  }

  .level {
    display: grid;
    grid-template-columns: 38px minmax(62px, 0.7fr) minmax(72px, 1fr) minmax(84px, 1fr);
    gap: var(--sp-2);
    align-items: center;
    min-height: 28px;
    border-top: 1px solid var(--line-soft);
    font-size: var(--fs-micro);
  }

  .level:first-of-type {
    border-top: 0;
  }

  .side {
    font-weight: 600;
  }

  .mute,
  .raw p {
    color: var(--fg-mute);
  }

  .raw {
    margin-top: var(--sp-3);
  }

  .raw p {
    margin: 0 0 var(--sp-3);
    font-size: var(--fs-micro);
  }

  @media (max-width: 760px) {
    .books {
      grid-template-columns: minmax(0, 1fr);
    }
  }
</style>
