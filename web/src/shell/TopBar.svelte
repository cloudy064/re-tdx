<script lang="ts">
  /**
   * 顶栏（40px）。承载：导航折叠、品牌、全局检索入口、当前标的、后端状态、主题。
   * 高度固定且不随内容变化，任何时候都不抢工作区的垂直空间。
   */
  import { app } from '../lib/store.svelte';
  import { router, stockPath } from '../lib/router.svelte';
  import { theme } from '../lib/theme.svelte';
  import { count } from '../lib/fmt';
  import Icon from '../ui/Icon.svelte';

  interface Props {
    onOpenSearch: () => void;
  }

  const { onOpenSearch }: Props = $props();

  const hotkey = $derived(
    typeof navigator !== 'undefined' && /Mac|iPhone|iPad/.test(navigator.platform) ? '⌘K' : 'Ctrl K'
  );

  function openStock() {
    if (!app.stock.code) return;
    router.go(stockPath(app.stock.market, app.stock.code));
  }
</script>

<header class="bar">
  <button
    class="ico"
    type="button"
    title={app.navCollapsed ? '展开导航' : '折叠导航'}
    onclick={() => app.toggleNav()}
  >
    <Icon name="panel" size={14} />
  </button>

  <span class="brand">TDX <span class="brand-dim">工作台</span></span>

  <button class="omni" type="button" onclick={onOpenSearch}>
    <Icon name="search" size={12} />
    <span class="omni-text">搜索证券或跳转页面</span>
    <kbd>{hotkey}</kbd>
  </button>

  {#if app.stock.name}
    <button class="ctx" type="button" onclick={openStock} title="回到个股工作台">
      <span class="ctx-name">{app.stock.name}</span>
      <span class="ctx-code num">{app.stock.market.toUpperCase()}{app.stock.code}</span>
    </button>
  {/if}

  <div class="spacer"></div>

  <div class="status" role="status">
    {#if app.booting}
      <span class="dot idle"></span><span class="status-text">连接中</span>
    {:else if app.health?.ok}
      <span class="dot live"></span>
      <span class="status-text num">
        {count(app.health.securities)} 证券 · {count(app.health.blocks)} 板块
      </span>
    {:else}
      <span class="dot dead"></span><span class="status-text">后端离线</span>
    {/if}
  </div>

  <button
    class="ico"
    type="button"
    title={theme.current === 'dark' ? '切换到浅色' : '切换到深色'}
    onclick={() => theme.toggle()}
  >
    <Icon name={theme.current === 'dark' ? 'sun' : 'moon'} size={14} />
  </button>
</header>

<style>
  .bar {
    display: flex;
    flex: none;
    align-items: center;
    gap: var(--sp-3);
    height: var(--h-topbar);
    padding: 0 var(--sp-3);
    background: var(--bg-panel);
    border-bottom: 1px solid var(--line-strong);
  }

  .ico {
    display: flex;
    flex: none;
    align-items: center;
    justify-content: center;
    width: 26px;
    height: 26px;
    color: var(--fg-dim);
    border-radius: var(--radius);
  }

  .ico:hover {
    color: var(--fg);
    background: var(--bg-hover);
  }

  .brand {
    flex: none;
    font-size: var(--fs-title);
    font-weight: 600;
    letter-spacing: 0.02em;
  }

  .brand-dim {
    font-weight: 400;
    color: var(--fg-mute);
  }

  .omni {
    display: flex;
    flex: 0 1 300px;
    align-items: center;
    gap: var(--sp-2);
    height: 24px;
    margin-left: var(--sp-3);
    padding: 0 var(--sp-2);
    color: var(--fg-mute);
    background: var(--bg-input);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  .omni:hover {
    border-color: var(--fg-mute);
  }

  .omni-text {
    flex: 1;
    overflow: hidden;
    font-size: var(--fs-micro);
    text-align: left;
    white-space: nowrap;
    text-overflow: ellipsis;
  }

  kbd {
    flex: none;
    padding: 0 3px;
    font-family: var(--font-num);
    font-size: 9px;
    color: var(--fg-mute);
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: 2px;
  }

  .ctx {
    display: flex;
    flex: none;
    align-items: baseline;
    gap: var(--sp-2);
    height: 24px;
    padding: 0 var(--sp-2);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .ctx:hover {
    background: var(--bg-hover);
  }

  .ctx-name {
    font-size: var(--fs-micro);
    color: var(--fg);
  }

  .ctx-code {
    font-size: 10px;
    color: var(--fg-mute);
  }

  .spacer {
    flex: 1;
  }

  .status {
    display: flex;
    flex: none;
    align-items: center;
    gap: var(--sp-2);
  }

  .status-text {
    font-size: 10px;
    color: var(--fg-mute);
  }

  .dot {
    width: 6px;
    height: 6px;
    border-radius: 50%;
  }

  /* 在线用蓝色而不是绿色：本项目里绿色专指「下跌」，
     状态色一旦借用涨跌色，扫一眼会误读成行情信号。 */
  .dot.live {
    background: var(--focus);
  }

  .dot.idle {
    background: var(--warn);
  }

  .dot.dead {
    background: var(--up);
  }

  @media (max-width: 900px) {
    .ctx,
    .status-text {
      display: none;
    }
  }
</style>
