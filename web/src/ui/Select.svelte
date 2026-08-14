<script lang="ts">
  /**
   * 下拉选择。选项超过约 8 个时用它，少于 8 个用 Segmented。
   * 保留原生 select 以获得键盘与无障碍行为，只做外观统一。
   */
  interface Props {
    value: string;
    options: Array<{ id: string; label: string }>;
    label?: string;
    width?: string;
    onChange?: (id: string) => void;
  }

  let { value = $bindable(''), options, label = '', width = '160px', onChange }: Props = $props();

  function handle(event: Event) {
    const next = (event.currentTarget as HTMLSelectElement).value;
    value = next;
    onChange?.(next);
  }
</script>

<label class="field" style="width:{width}">
  {#if label}<span class="label">{label}</span>{/if}
  <select {value} onchange={handle}>
    {#each options as option (option.id)}
      <option value={option.id}>{option.label}</option>
    {/each}
  </select>
</label>

<style>
  .field {
    display: inline-flex;
    align-items: center;
    gap: var(--sp-2);
  }

  .label {
    flex: none;
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  select {
    width: 100%;
    height: var(--h-control);
    padding: 0 var(--sp-2);
    font-size: var(--fs-micro);
    color: var(--fg);
    background: var(--bg-input);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  select:focus {
    outline: none;
    border-color: var(--focus);
  }
</style>
