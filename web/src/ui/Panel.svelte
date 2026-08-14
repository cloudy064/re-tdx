<script lang="ts">
  /**
   * 面板容器。全站唯一的内容分区外框。
   *
   * 同时承担四态呈现（加载 / 出错 / 空 / 有数据）——重构前这四态在各视图里
   * 写法各异，很多视图干脆没有空态，数据为空时只剩一个孤零零的表头。
   */
  import type { Snippet } from 'svelte';
  import Icon from './Icon.svelte';
  import Spinner from './Spinner.svelte';

  interface Props {
    title?: string;
    /** 数据来源标注，例如 `0x054B · TCP 7709`。用等宽小号字排在标题上方。 */
    eyebrow?: string;
    subtitle?: string;
    busy?: boolean;
    error?: string;
    /** 为真时展示空态；调用方自行判断（如 rows.length === 0）。 */
    empty?: boolean;
    emptyText?: string;
    /** 内容区是否自身滚动。表格类面板置为 true。 */
    scroll?: boolean;
    /** 内容区撑满剩余高度并作为 flex 容器。图表类面板置为 true。 */
    fill?: boolean;
    /** 去掉内容内边距，让表格贴边。 */
    flush?: boolean;
    onRetry?: () => void;
    actions?: Snippet;
    toolbar?: Snippet;
    children?: Snippet;
  }

  const {
    title = '',
    eyebrow = '',
    subtitle = '',
    busy = false,
    error = '',
    empty = false,
    emptyText = '当前条件下没有数据',
    scroll = false,
    fill = false,
    flush = false,
    onRetry,
    actions,
    toolbar,
    children
  }: Props = $props();

  const hasHeader = $derived(Boolean(title || eyebrow || subtitle || actions));

  /**
   * fill 与 scroll 都表示「内容区应当占满剩余高度」——前者给图表，后者给长表格。
   * 但面板作为 flex 子项默认按内容收缩，不在根节点也声明 flex: 1，
   * body 的 flex: 1 就没有空间可填：图表会被压成几十像素，
   * 表格则会一直长到视口之外且无法滚动（body 是 overflow: hidden）。
   *
   * 空态和错误态例外——它们的内容是固定高度的提示块。让它们照样瓜分高度的话，
   * 多面板页面上几个「尚未选择」的占位就会把真正有数据的表挤没。
   */
  const grow = $derived((fill || scroll) && !empty && !error);
</script>

<section class="panel" class:grow>
  {#if hasHeader}
    <header class="head">
      <div class="titles">
        {#if eyebrow}<span class="eyebrow">{eyebrow}</span>{/if}
        {#if title}<h2 class="title">{title}</h2>{/if}
        {#if subtitle}<p class="subtitle">{subtitle}</p>{/if}
      </div>
      <div class="actions">
        {#if busy}<Spinner />{/if}
        {@render actions?.()}
      </div>
    </header>
  {/if}

  {#if toolbar}
    <div class="toolbar">{@render toolbar()}</div>
  {/if}

  <div class="body" class:scroll class:flush class:fill>
    {#if error}
      <div class="state">
        <span class="state-icon error"><Icon name="alert" size={16} /></span>
        <p class="state-text">{error}</p>
        {#if onRetry}
          <button class="retry" type="button" onclick={onRetry}>
            <Icon name="refresh" size={12} />重试
          </button>
        {/if}
      </div>
    {:else if busy && !children}
      <div class="state"><Spinner /><p class="state-text">加载中</p></div>
    {:else if empty}
      <div class="state">
        <span class="state-icon"><Icon name="inbox" size={16} /></span>
        <p class="state-text">{emptyText}</p>
      </div>
    {:else}
      {@render children?.()}
    {/if}
  </div>
</section>

<style>
  .panel {
    display: flex;
    flex-direction: column;
    min-width: 0;
    min-height: 0;
    background: var(--bg-panel);
    border: 1px solid var(--line);
    border-radius: var(--radius-lg);
  }

  /* fill 表示内容区要撑满剩余高度；但面板本身作为 flex 子项默认按内容收缩，
     不在这里也声明 flex: 1，body 的 flex: 1 就没有空间可填，图表会被压成几十像素。 */
  .panel.grow {
    flex: 1;
  }
  .head {
    display: flex;
    flex: none;
    align-items: flex-start;
    justify-content: space-between;
    gap: var(--sp-4);
    padding: var(--sp-3) var(--sp-4);
    border-bottom: 1px solid var(--line);
  }

  .titles {
    display: flex;
    min-width: 0;
    flex-direction: column;
    gap: 1px;
  }

  .title {
    font-size: var(--fs-title);
    font-weight: 600;
    line-height: var(--lh-tight);
    color: var(--fg);
  }

  .subtitle {
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  .actions {
    display: flex;
    flex: none;
    align-items: center;
    gap: var(--sp-2);
  }

  .toolbar {
    display: flex;
    flex: none;
    flex-wrap: wrap;
    align-items: center;
    gap: var(--sp-2);
    padding: var(--sp-2) var(--sp-4);
    border-bottom: 1px solid var(--line);
    background: var(--bg-raised);
  }

  .body {
    min-width: 0;
    min-height: 0;
    padding: var(--sp-4);
  }

  .body.flush {
    padding: 0;
  }

  .body.scroll {
    overflow: auto;
    flex: 1;
  }

  .body.fill {
    display: flex;
    flex: 1;
    flex-direction: column;
    gap: var(--sp-2);
  }

  .state {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: var(--sp-2);
    /* 占位提示只有一行字，给到 120px + 24px 内边距会在多面板页面上挖出大洞 */
    min-height: 64px;
    padding: var(--sp-4);
    text-align: center;
  }

  .state-icon {
    color: var(--fg-mute);
  }

  .state-icon.error {
    color: var(--up);
  }

  .state-text {
    max-width: 46ch;
    font-size: var(--fs-body);
    color: var(--fg-dim);
  }

  .retry {
    display: inline-flex;
    align-items: center;
    gap: var(--sp-1);
    height: var(--h-control);
    padding: 0 var(--sp-3);
    font-size: var(--fs-micro);
    color: var(--fg);
    background: var(--bg-raised);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  .retry:hover {
    background: var(--bg-hover);
  }
</style>
