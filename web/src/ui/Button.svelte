<script lang="ts">
  /** 按钮。三档视觉权重，全站不再出现渐变或发光按钮。 */
  import type { Snippet } from 'svelte';
  import Icon, { type IconName } from './Icon.svelte';
  import Spinner from './Spinner.svelte';

  interface Props {
    variant?: 'default' | 'primary' | 'ghost';
    icon?: IconName;
    disabled?: boolean;
    busy?: boolean;
    title?: string;
    type?: 'button' | 'submit';
    onclick?: () => void;
    children?: Snippet;
  }

  const {
    variant = 'default',
    icon,
    disabled = false,
    busy = false,
    title,
    type = 'button',
    onclick,
    children
  }: Props = $props();
</script>

<button
  class="btn {variant}"
  class:icon-only={!children}
  {type}
  {title}
  disabled={disabled || busy}
  {onclick}
>
  {#if busy}
    <Spinner size={11} />
  {:else if icon}
    <Icon name={icon} size={12} />
  {/if}
  {@render children?.()}
</button>

<style>
  .btn {
    display: inline-flex;
    flex: none;
    align-items: center;
    justify-content: center;
    gap: var(--sp-1);
    height: var(--h-control);
    padding: 0 var(--sp-3);
    font-size: var(--fs-micro);
    white-space: nowrap;
    border: 1px solid transparent;
    border-radius: var(--radius);
  }

  .btn.icon-only {
    width: var(--h-control);
    padding: 0;
  }

  .default {
    color: var(--fg);
    background: var(--bg-raised);
    border-color: var(--line-strong);
  }

  .default:hover:not(:disabled) {
    background: var(--bg-hover);
  }

  .primary {
    color: var(--on-solid);
    background: var(--focus);
    border-color: var(--focus);
  }

  .primary:hover:not(:disabled) {
    filter: brightness(1.1);
  }

  .ghost {
    color: var(--fg-dim);
  }

  .ghost:hover:not(:disabled) {
    color: var(--fg);
    background: var(--bg-hover);
  }

  .btn:disabled {
    color: var(--fg-mute);
    background: var(--bg-raised);
    border-color: var(--line);
  }
</style>
