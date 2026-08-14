<script lang="ts">
  /**
   * Level 2 五档盘口。
   *
   * 相对旧版的三列纯文本，这里在每行背后叠了一条按量归一化的深度条——
   * 盘口最需要一眼看出的是「哪一档挂了大单」，纯数字做不到。
   */
  import { compact, price, tone } from '../../../lib/fmt';
  import Panel from '../../../ui/Panel.svelte';
  import type { DepthRecord } from '../../../types';

  interface Props {
    record: DepthRecord | null;
    busy: boolean;
    error: string;
    onRetry: () => void;
  }

  const { record, busy, error, onRetry }: Props = $props();

  const sells = $derived([...(record?.sell_levels ?? [])].reverse());
  const buys = $derived(record?.buy_levels ?? []);

  /** 深度条按买卖两侧的最大挂单量统一归一，两侧才可比。 */
  const peak = $derived(
    Math.max(
      1,
      ...(record?.sell_levels ?? []).map((level) => level.volume_hand),
      ...(record?.buy_levels ?? []).map((level) => level.volume_hand)
    )
  );

  /** 委比：正数买盘占优。这是老版本没有、但盘口里最常看的一个派生量。 */
  const imbalance = $derived.by(() => {
    if (!record) return null;
    const bid = record.buy_levels.reduce((sum, level) => sum + level.volume_hand, 0);
    const ask = record.sell_levels.reduce((sum, level) => sum + level.volume_hand, 0);
    if (bid + ask === 0) return null;
    return ((bid - ask) / (bid + ask)) * 100;
  });

  function width(volumeHand: number): string {
    return `${Math.min(100, (volumeHand / peak) * 100)}%`;
  }
</script>

<Panel
  title="Level 2 盘口"
  eyebrow="TCP 7709 · DEPTH"
  subtitle="每 5 秒刷新"
  {busy}
  {error}
  {onRetry}
  empty={!record && !busy && !error}
  emptyText="暂无盘口数据"
  flush
>
  {#if record}
    <div class="book">
      {#each sells as level, index (index)}
        <div class="row">
          <span class="tag">卖{sells.length - index}</span>
          <span class="bar down" style="width:{width(level.volume_hand)}"></span>
          <span class="px num down">{price(level.price)}</span>
          <span class="qty num">{level.volume_hand}</span>
        </div>
      {/each}

      <div class="mid">
        <span class="mid-label">现价</span>
        <span class="mid-price num {tone(record.change_pct)}">{price(record.last_price)}</span>
        {#if imbalance !== null}
          <span class="mid-imb num {tone(imbalance)}">
            委比 {imbalance > 0 ? '+' : ''}{imbalance.toFixed(1)}%
          </span>
        {/if}
      </div>

      {#each buys as level, index (index)}
        <div class="row">
          <span class="tag">买{index + 1}</span>
          <span class="bar up" style="width:{width(level.volume_hand)}"></span>
          <span class="px num up">{price(level.price)}</span>
          <span class="qty num">{level.volume_hand}</span>
        </div>
      {/each}
    </div>

    <dl class="foot">
      <div><dt>委买一额</dt><dd class="num up">{compact(record.bid1_amount_yuan, '元')}</dd></div>
      <div><dt>委卖一额</dt><dd class="num down">{compact(record.ask1_amount_yuan, '元')}</dd></div>
      <div><dt>现手</dt><dd class="num">{compact(record.current_hand)}</dd></div>
      <div><dt>开盘金额</dt><dd class="num">{compact(record.open_amount_yuan, '元')}</dd></div>
    </dl>
  {/if}
</Panel>

<style>
  .book {
    display: flex;
    flex-direction: column;
  }

  .row {
    position: relative;
    display: grid;
    grid-template-columns: 30px minmax(0, 1fr) 52px;
    align-items: center;
    height: 22px;
    padding: 0 var(--sp-3);
    font-size: var(--fs-micro);
  }

  /* 深度条贴右侧生长，视觉上和右对齐的数量列同向 */
  .bar {
    position: absolute;
    top: 2px;
    right: 0;
    bottom: 2px;
    z-index: 0;
    opacity: 0.16;
    pointer-events: none;
  }

  .bar.up {
    background: var(--up);
  }

  .bar.down {
    background: var(--down);
  }

  .tag,
  .px,
  .qty {
    position: relative;
    z-index: 1;
  }

  .tag {
    color: var(--fg-mute);
  }

  .px {
    text-align: right;
    padding-right: var(--sp-3);
  }

  .qty {
    color: var(--fg-dim);
    text-align: right;
  }

  .mid {
    display: flex;
    align-items: baseline;
    gap: var(--sp-2);
    height: 26px;
    padding: 0 var(--sp-3);
    background: var(--bg-raised);
    border-top: 1px solid var(--line);
    border-bottom: 1px solid var(--line);
  }

  .mid-label {
    font-size: 10px;
    color: var(--fg-mute);
  }

  .mid-price {
    font-size: var(--fs-title);
    font-weight: 600;
  }

  .mid-imb {
    margin-left: auto;
    font-size: 10px;
  }

  .foot {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: var(--sp-1) var(--sp-3);
    margin: 0;
    padding: var(--sp-2) var(--sp-3);
    border-top: 1px solid var(--line);
  }

  .foot div {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: var(--sp-2);
  }

  dt {
    font-size: 10px;
    color: var(--fg-mute);
  }

  dd {
    margin: 0;
    font-size: 10px;
  }
</style>
