<script lang="ts">
  /**
   * 模态层。用于 Omnibox 和少数确需打断的详情。
   * 深色终端里不做毛玻璃，只用一层低透明度遮罩 + 硬边容器。
   */
  import type { Snippet } from 'svelte';

  interface Props {
    open: boolean;
    onClose: () => void;
    /** 顶部对齐（Omnibox 用）还是居中。 */
    align?: 'top' | 'center';
    width?: string;
    labelledBy?: string;
    children: Snippet;
  }

  const { open, onClose, align = 'center', width = '520px', labelledBy, children }: Props =
    $props();

  function onKeydown(event: KeyboardEvent) {
    if (event.key === 'Escape') {
      event.stopPropagation();
      onClose();
    }
  }
</script>

<svelte:window onkeydown={open ? onKeydown : undefined} />

{#if open}
  <!-- 遮罩本身可点击关闭；键盘用户走 Escape，故此处不需要额外 role -->
  <div
    class="scrim {align}"
    role="presentation"
    onclick={(event) => {
      if (event.target === event.currentTarget) onClose();
    }}
  >
    <div class="dialog" style="width:{width}" role="dialog" aria-modal="true" aria-labelledby={labelledBy}>
      {@render children()}
    </div>
  </div>
{/if}

<style>
  .scrim {
    position: fixed;
    inset: 0;
    z-index: 100;
    display: flex;
    justify-content: center;
    padding: var(--sp-6);
    background: var(--scrim);
  }

  .scrim.top {
    align-items: flex-start;
    padding-top: 14vh;
  }

  .scrim.center {
    align-items: center;
  }

  .dialog {
    max-width: 100%;
    max-height: 100%;
    display: flex;
    flex-direction: column;
    overflow: hidden;
    background: var(--bg-panel);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius-lg);
    box-shadow: var(--shadow-pop);
  }
</style>
