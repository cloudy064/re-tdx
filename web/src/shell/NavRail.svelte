<script lang="ts">
  /**
   * 左侧导航。
   *
   * 重构前 13 个数据页是一条横向滚动的药丸条——屏幕外的项目完全不可见，
   * 也无法表达「资金 / 股东 / 研究 / 估值」的分组关系。这里全部展开为
   * 「区 → 组 → 项」的静态树：一屏放得下，无需展开收起，可见即可达。
   */
  import { NAV, sectionOf } from '../lib/nav';
  import { router } from '../lib/router.svelte';
  import { app } from '../lib/store.svelte';
  import Icon from '../ui/Icon.svelte';

  const activePath = $derived(router.path);
  const activeSection = $derived(sectionOf(activePath));

  /** 个股工作台路径带参数（/stock/sz/000001/funds），前缀匹配才能保持高亮。 */
  function isActive(path: string): boolean {
    return activePath === path || activePath.startsWith(`${path}/`);
  }
</script>

<nav class="rail" class:collapsed={app.navCollapsed} aria-label="主导航">
  {#each NAV as section (section.id)}
    {#if app.navCollapsed}
      <button
        class="icon-item"
        class:on={activeSection === section.id}
        type="button"
        title={section.label}
        onclick={() => router.go(section.groups[0].items[0].path)}
      >
        <Icon name={section.icon} size={16} />
      </button>
    {:else}
      <div class="section">
        <div class="section-head">
          <Icon name={section.icon} size={13} />
          <span>{section.label}</span>
        </div>
        {#each section.groups as group (group.label || section.id)}
          {#if group.label}
            <div class="group-label">{group.label}</div>
          {/if}
          {#each group.items as item (item.path)}
            <button
              class="item"
              class:on={isActive(item.path)}
              type="button"
              title={item.hint}
              onclick={() => router.go(item.path)}
            >
              {item.label}
            </button>
          {/each}
        {/each}
      </div>
    {/if}
  {/each}
</nav>

<style>
  .rail {
    display: flex;
    flex: none;
    flex-direction: column;
    gap: var(--sp-1);
    width: var(--w-nav);
    padding: var(--sp-3) var(--sp-2);
    overflow-y: auto;
    background: var(--bg-panel);
    border-right: 1px solid var(--line-strong);
  }

  .rail.collapsed {
    width: var(--w-nav-collapsed);
    align-items: center;
    gap: var(--sp-2);
    padding: var(--sp-3) 0;
  }

  .section {
    display: flex;
    flex-direction: column;
    margin-bottom: var(--sp-3);
  }

  .section-head {
    display: flex;
    align-items: center;
    gap: var(--sp-2);
    height: 22px;
    padding: 0 var(--sp-2);
    font-size: var(--fs-micro);
    font-weight: 600;
    color: var(--fg-dim);
  }

  .group-label {
    padding: var(--sp-2) var(--sp-2) 2px calc(var(--sp-2) + 21px);
    font-size: 9px;
    letter-spacing: 0.06em;
    color: var(--fg-mute);
  }

  .item {
    display: block;
    height: 22px;
    /* 与 section-head 的图标对齐：图标 13px + 间距 6px + 内边距 6px */
    padding: 0 var(--sp-2) 0 calc(var(--sp-2) + 21px);
    font-size: var(--fs-micro);
    color: var(--fg-dim);
    text-align: left;
    border-radius: var(--radius);
  }

  .item:hover {
    color: var(--fg);
    background: var(--bg-hover);
  }

  .item.on {
    color: var(--fg);
    font-weight: 500;
    background: var(--bg-active);
    box-shadow: inset 2px 0 0 var(--focus);
  }

  .icon-item {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 28px;
    height: 28px;
    color: var(--fg-dim);
    border-radius: var(--radius);
  }

  .icon-item:hover {
    color: var(--fg);
    background: var(--bg-hover);
  }

  .icon-item.on {
    color: var(--fg);
    background: var(--bg-active);
  }
</style>
