<script lang="ts" module>
  export interface Stat {
    label: string;
    value: string;
    /** 涨跌着色。 */
    tone?: 'up' | 'down' | 'flat' | '';
    /** 补充说明，排在数值下方。 */
    note?: string;
  }
</script>

<script lang="ts">
  /**
   * 指标网格。用于个股概览、榜单摘要、系统状态这类「标签 + 数值」的成组呈现。
   * 数值一律等宽，列宽自适应但保持基线对齐。
   */
  interface Props {
    stats: Stat[];
    /** 每行列数。不给则按容器宽度自动排布。 */
    columns?: number;
    /** 紧凑模式：标签与数值同一行，用于侧栏。 */
    inline?: boolean;
  }

  const { stats, columns, inline = false }: Props = $props();
</script>

<dl
  class="grid"
  class:inline
  style={columns ? `grid-template-columns:repeat(${columns},minmax(0,1fr))` : undefined}
>
  {#each stats as stat (stat.label)}
    <div class="cell">
      <dt>{stat.label}</dt>
      <dd class="num" class:up={stat.tone === 'up'} class:down={stat.tone === 'down'}>
        {stat.value}
      </dd>
      {#if stat.note}<span class="note">{stat.note}</span>{/if}
    </div>
  {/each}
</dl>

<style>
  .grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(104px, 1fr));
    gap: 1px;
    margin: 0;
    background: var(--line);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    overflow: hidden;
  }

  .cell {
    display: flex;
    flex-direction: column;
    gap: 1px;
    padding: var(--sp-2) var(--sp-3);
    background: var(--bg-panel);
  }

  dt {
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  dd {
    margin: 0;
    font-size: var(--fs-body);
    font-weight: 500;
    line-height: var(--lh-tight);
    color: var(--fg);
  }

  .note {
    font-size: 9px;
    color: var(--fg-mute);
  }

  .up {
    color: var(--up);
  }

  .down {
    color: var(--down);
  }

  /* 侧栏紧凑模式：改为两列对齐的定义列表，纵向更省空间 */
  .inline {
    display: block;
    background: transparent;
    border: 0;
  }

  .inline .cell {
    flex-direction: row;
    align-items: baseline;
    justify-content: space-between;
    gap: var(--sp-3);
    padding: var(--sp-1) 0;
    background: transparent;
    border-bottom: 1px solid var(--line);
  }

  .inline .cell:last-child {
    border-bottom: 0;
  }

  .inline dd {
    font-size: var(--fs-micro);
  }

  .inline .note {
    display: none;
  }
</style>
