<script lang="ts" module>
  export interface SegmentOption {
    id: string;
    label: string;
    /** 悬停提示，折叠或缩写时用。 */
    hint?: string;
    disabled?: boolean;
  }
</script>

<script lang="ts">
  /**
   * 分段控件。取代重构前散落的六七种药丸/标签变体
   * （nav-tab / tab-pill / period-btn / family-btn / data-tab-pill / view-btn），
   * 全站只此一种，靠 size 区分密度。
   */
  interface Props {
    options: SegmentOption[];
    value: string;
    onChange: (id: string) => void;
    /** sm 用于面板内工具条，md 用于页面级切换。 */
    size?: 'sm' | 'md';
    ariaLabel?: string;
  }

  const { options, value, onChange, size = 'sm', ariaLabel = '' }: Props = $props();
</script>

<div class="seg {size}" role="tablist" aria-label={ariaLabel}>
  {#each options as option (option.id)}
    <button
      type="button"
      role="tab"
      class:active={option.id === value}
      aria-selected={option.id === value}
      disabled={option.disabled}
      title={option.hint || undefined}
      onclick={() => onChange(option.id)}
    >{option.label}</button>
  {/each}
</div>

<style>
  .seg {
    display: inline-flex;
    flex-wrap: wrap;
    gap: 1px;
    padding: 1px;
    background: var(--bg-input);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  button {
    height: 20px;
    padding: 0 var(--sp-3);
    font-size: var(--fs-micro);
    color: var(--fg-dim);
    border-radius: 1px;
    white-space: nowrap;
  }

  .md button {
    height: 24px;
    padding: 0 var(--sp-4);
    font-size: var(--fs-body);
  }

  button:hover:not(:disabled):not(.active) {
    color: var(--fg);
    background: var(--bg-hover);
  }

  button.active {
    color: var(--fg);
    font-weight: 500;
    background: var(--bg-active);
  }

  button:disabled {
    color: var(--fg-mute);
    opacity: 0.5;
  }
</style>
