<script lang="ts">
  /**
   * 页面头。所有数据页共用：来源标注 + 标题 + 一句话说明 + 右侧操作，
   * 可选下挂一排摘要指标。高度紧凑，不抢工作区。
   */
  import type { Snippet } from 'svelte';
  import StatGrid, { type Stat } from './StatGrid.svelte';

  interface Props {
    /** 数据来源，例如 `ZCJC + GQZY · 709/1721`。让人一眼知道这页背后是哪条协议。 */
    eyebrow?: string;
    title: string;
    description?: string;
    stats?: Stat[];
    actions?: Snippet;
  }

  const { eyebrow = '', title, description = '', stats, actions }: Props = $props();
</script>

<header class="page-head">
  <div class="line">
    <div class="titles">
      {#if eyebrow}<span class="eyebrow">{eyebrow}</span>{/if}
      <h1>{title}</h1>
      {#if description}<p>{description}</p>{/if}
    </div>
    {#if actions}
      <div class="actions">{@render actions()}</div>
    {/if}
  </div>

  {#if stats?.length}
    <StatGrid {stats} />
  {/if}
</header>

<style>
  .page-head {
    display: flex;
    flex: none;
    flex-direction: column;
    gap: var(--sp-2);
  }

  .line {
    display: flex;
    align-items: flex-end;
    justify-content: space-between;
    gap: var(--sp-4);
  }

  .titles {
    display: flex;
    min-width: 0;
    flex-direction: column;
    gap: 1px;
  }

  h1 {
    font-size: var(--fs-lead);
    font-weight: 600;
    line-height: var(--lh-tight);
  }

  p {
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  .actions {
    display: flex;
    flex: none;
    align-items: center;
    gap: var(--sp-2);
  }
</style>
