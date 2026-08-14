<script lang="ts">
  /** 单行文本输入。表单控件全站统一 24px 高，与按钮、分段控件对齐。 */
  import Icon from './Icon.svelte';

  interface Props {
    value: string;
    placeholder?: string;
    /** 前置图标，通常是 search。 */
    icon?: 'search' | 'filter' | 'clock';
    width?: string;
    label?: string;
    onInput?: (value: string) => void;
    onEnter?: () => void;
  }

  let {
    value = $bindable(''),
    placeholder = '',
    icon,
    width = '180px',
    label = '',
    onInput,
    onEnter
  }: Props = $props();

  function handleInput(event: Event) {
    const next = (event.currentTarget as HTMLInputElement).value;
    value = next;
    onInput?.(next);
  }

  function handleKey(event: KeyboardEvent) {
    if (event.key === 'Enter') onEnter?.();
  }
</script>

<label class="field" style="width:{width}">
  {#if label}<span class="sr-only">{label}</span>{/if}
  {#if icon}<span class="lead"><Icon name={icon} size={12} /></span>{/if}
  <input
    type="text"
    class:with-icon={Boolean(icon)}
    {placeholder}
    {value}
    oninput={handleInput}
    onkeydown={handleKey}
  />
</label>

<style>
  .field {
    position: relative;
    display: inline-flex;
    align-items: center;
  }

  .lead {
    position: absolute;
    left: var(--sp-2);
    display: flex;
    color: var(--fg-mute);
    pointer-events: none;
  }

  input {
    width: 100%;
    height: var(--h-control);
    padding: 0 var(--sp-2);
    font-size: var(--fs-micro);
    color: var(--fg);
    background: var(--bg-input);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  input.with-icon {
    padding-left: 22px;
  }

  input::placeholder {
    color: var(--fg-mute);
  }

  input:focus {
    outline: none;
    border-color: var(--focus);
  }
</style>
