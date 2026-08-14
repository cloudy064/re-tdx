<script lang="ts">
  /** 状态徽标。语义色只有五种，禁止在调用处自定义颜色。 */
  import type { Snippet } from 'svelte';

  interface Props {
    /** 'flat' 是 fmt.tone() 的平盘返回值，与 neutral 同样式，允许直接透传。 */
    tone?: 'neutral' | 'flat' | 'up' | 'down' | 'warn' | 'focus';
    /** 实心：用于「在线」这类需要抢眼的状态；默认描边。 */
    solid?: boolean;
    children: Snippet;
  }

  const { tone = 'neutral', solid = false, children }: Props = $props();
</script>

<span class="badge {tone}" class:solid>{@render children()}</span>

<style>
  .badge {
    display: inline-flex;
    align-items: center;
    gap: 3px;
    height: 16px;
    padding: 0 5px;
    font-size: 10px;
    font-weight: 500;
    line-height: 1;
    white-space: nowrap;
    border: 1px solid;
    border-radius: var(--radius);
  }

  .neutral,
  .flat {
    color: var(--fg-dim);
    background: var(--flat-soft);
    border-color: var(--line-strong);
  }

  .up {
    color: var(--up);
    background: var(--up-soft);
    border-color: var(--up-soft);
  }

  .down {
    color: var(--down);
    background: var(--down-soft);
    border-color: var(--down-soft);
  }

  .warn {
    color: var(--warn);
    background: var(--warn-soft);
    border-color: var(--warn-soft);
  }

  .focus {
    color: var(--focus);
    background: var(--focus-soft);
    border-color: var(--focus-soft);
  }

  .solid.up {
    color: var(--on-solid);
    background: var(--up);
    border-color: var(--up);
  }

  .solid.down {
    color: var(--on-solid);
    background: var(--down);
    border-color: var(--down);
  }
</style>
