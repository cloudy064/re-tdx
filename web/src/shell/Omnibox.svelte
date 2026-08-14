<script lang="ts">
  /**
   * 全局检索。Ctrl/Cmd+K 唤起。
   * 同时检索本地证券库与导航项——重构前搜索只能搜股票，想去某个数据页
   * 必须先记住它在 13 个横滚药丸里的位置。
   */
  import { getJson, queryString } from '../api';
  import { NAV_ITEMS } from '../lib/nav';
  import { router, stockPath } from '../lib/router.svelte';
  import { app } from '../lib/store.svelte';
  import type { SecuritySearchDocument, SecuritySearchRecord } from '../types';
  import Icon from '../ui/Icon.svelte';
  import Modal from '../ui/Modal.svelte';
  import Spinner from '../ui/Spinner.svelte';

  interface Props {
    open: boolean;
    onClose: () => void;
  }

  const { open, onClose }: Props = $props();

  type Hit =
    | { kind: 'stock'; label: string; hint: string; market: string; code: string }
    | { kind: 'nav'; label: string; hint: string; path: string };

  let query = $state('');
  let busy = $state(false);
  let securities = $state<SecuritySearchRecord[]>([]);
  let cursor = $state(0);
  let input = $state<HTMLInputElement | null>(null);
  let seq = 0;

  const navHits = $derived.by<Hit[]>(() => {
    const q = query.trim().toLowerCase();
    const pool = NAV_ITEMS.filter(
      (item) =>
        q === '' ||
        item.label.toLowerCase().includes(q) ||
        item.hint.toLowerCase().includes(q) ||
        item.section.toLowerCase().includes(q)
    );
    return pool.slice(0, q === '' ? 5 : 6).map((item) => ({
      kind: 'nav' as const,
      label: item.label,
      hint: `${item.section} · ${item.hint}`,
      path: item.path
    }));
  });

  const stockHits = $derived.by<Hit[]>(() => {
    // 无输入时展示最近访问，让空态也有用
    if (query.trim() === '') {
      return app.recent.map((item) => ({
        kind: 'stock' as const,
        label: item.name,
        hint: `${item.market.toUpperCase()}${item.code} · 最近访问`,
        market: item.market,
        code: item.code
      }));
    }
    return securities.map((item) => ({
      kind: 'stock' as const,
      label: item.name,
      hint: `${item.market.toUpperCase()}${item.code}`,
      market: item.market,
      code: item.code
    }));
  });

  const hits = $derived<Hit[]>([...stockHits, ...navHits]);

  $effect(() => {
    if (!open) return;
    query = '';
    securities = [];
    cursor = 0;
    // 等待 Modal 挂载后再聚焦
    queueMicrotask(() => input?.focus());
  });

  $effect(() => {
    const q = query.trim();
    if (!open || q === '') {
      securities = [];
      busy = false;
      return;
    }
    const ticket = ++seq;
    busy = true;
    const timer = setTimeout(async () => {
      try {
        const result = await getJson<SecuritySearchDocument>(
          `/api/v1/market/securities?${queryString({ market: 'all', q, limit: 12 })}`
        );
        if (ticket === seq) securities = result.securities ?? [];
      } catch {
        if (ticket === seq) securities = [];
      } finally {
        if (ticket === seq) busy = false;
      }
    }, 120);
    return () => clearTimeout(timer);
  });

  // 结果集变化后把光标收回合法范围
  $effect(() => {
    if (cursor > hits.length - 1) cursor = Math.max(0, hits.length - 1);
  });

  function choose(hit: Hit) {
    if (hit.kind === 'stock') {
      app.setStock({ market: hit.market, code: hit.code, name: hit.label });
      router.go(stockPath(hit.market, hit.code));
    } else {
      router.go(hit.path);
    }
    onClose();
  }

  function onKeydown(event: KeyboardEvent) {
    if (event.key === 'ArrowDown') {
      event.preventDefault();
      cursor = (cursor + 1) % Math.max(1, hits.length);
    } else if (event.key === 'ArrowUp') {
      event.preventDefault();
      cursor = (cursor - 1 + hits.length) % Math.max(1, hits.length);
    } else if (event.key === 'Enter') {
      event.preventDefault();
      const hit = hits[cursor];
      if (hit) choose(hit);
    }
  }
</script>

<Modal {open} {onClose} align="top" width="560px">
  <div class="search">
    <Icon name="search" size={14} />
    <input
      bind:this={input}
      bind:value={query}
      type="text"
      placeholder="搜索证券代码 / 名称，或跳转到任意页面"
      onkeydown={onKeydown}
      autocomplete="off"
      spellcheck="false"
    />
    {#if busy}<Spinner />{/if}
    <kbd>Esc</kbd>
  </div>

  <div class="results">
    {#if hits.length === 0}
      <p class="none">没有匹配项</p>
    {:else}
      {#each hits as hit, index (hit.kind + hit.label + index)}
        <button
          class="hit"
          class:on={index === cursor}
          type="button"
          onmouseenter={() => (cursor = index)}
          onclick={() => choose(hit)}
        >
          <span class="mark">{hit.kind === 'stock' ? '证券' : '页面'}</span>
          <span class="label">{hit.label}</span>
          <span class="hint">{hit.hint}</span>
        </button>
      {/each}
    {/if}
  </div>

  <footer class="foot">
    <span><kbd>↑</kbd><kbd>↓</kbd> 选择</span>
    <span><kbd>Enter</kbd> 打开</span>
  </footer>
</Modal>

<style>
  .search {
    display: flex;
    flex: none;
    align-items: center;
    gap: var(--sp-3);
    padding: 0 var(--sp-4);
    height: 40px;
    color: var(--fg-mute);
    border-bottom: 1px solid var(--line);
  }

  .search input {
    flex: 1;
    min-width: 0;
    height: 100%;
    font-size: var(--fs-title);
    color: var(--fg);
    background: none;
    border: 0;
    outline: none;
  }

  .search input::placeholder {
    color: var(--fg-mute);
  }

  .results {
    overflow-y: auto;
    max-height: 46vh;
    padding: var(--sp-1);
  }

  .none {
    padding: var(--sp-5);
    font-size: var(--fs-micro);
    color: var(--fg-mute);
    text-align: center;
  }

  .hit {
    display: flex;
    align-items: baseline;
    gap: var(--sp-3);
    width: 100%;
    padding: var(--sp-2) var(--sp-3);
    text-align: left;
    border-radius: var(--radius);
  }

  .hit.on {
    background: var(--bg-hover);
  }

  .mark {
    flex: none;
    width: 26px;
    font-size: 9px;
    color: var(--fg-mute);
  }

  .label {
    flex: none;
    font-size: var(--fs-body);
    color: var(--fg);
  }

  .hint {
    overflow: hidden;
    font-size: var(--fs-micro);
    color: var(--fg-mute);
    white-space: nowrap;
    text-overflow: ellipsis;
  }

  .foot {
    display: flex;
    flex: none;
    gap: var(--sp-4);
    padding: var(--sp-2) var(--sp-4);
    font-size: 10px;
    color: var(--fg-mute);
    border-top: 1px solid var(--line);
    background: var(--bg-raised);
  }

  kbd {
    display: inline-block;
    min-width: 15px;
    margin-right: 2px;
    padding: 0 3px;
    font-family: var(--font-num);
    font-size: 9px;
    text-align: center;
    color: var(--fg-dim);
    background: var(--bg-input);
    border: 1px solid var(--line-strong);
    border-radius: 2px;
  }
</style>
