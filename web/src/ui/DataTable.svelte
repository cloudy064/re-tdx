<script lang="ts" module>
  export interface Column<T> {
    key: string;
    label: string;
    align?: 'left' | 'right' | 'center';
    /** CSS 宽度，如 '90px' / '18%'。不给则自适应。 */
    width?: string;
    /** 单元格主文本。 */
    value?: (row: T, index: number) => string;
    /** 第二行小字，用于「变化量」「所属行业」这类附属信息。 */
    sub?: (row: T, index: number) => string;
    /** 涨跌语义着色。 */
    tone?: (row: T, index: number) => 'up' | 'down' | 'flat' | '';
    /** 用等宽数字排版。数值列一律置为 true，保证纵向对齐。 */
    num?: boolean;
    /** 允许换行。机构名、公告标题这类长文本用。 */
    wrap?: boolean;
    /** 本列交由调用方的 cell 片段渲染（放按钮、链接等）。 */
    slot?: boolean;
    /** 参与本地排序时的取值。不给则不可排序。 */
    sortValue?: (row: T) => number | string;
  }
</script>

<script lang="ts" generics="T">
  /**
   * 数据表。全站表格的唯一实现。
   *
   * 重构前每个视图各写一遍 <table> 与样式，行高从 9px 到 14px 不等、
   * 数值列没有等宽字体导致小数点不对齐、宽表没有粘性表头。
   */
  import type { Snippet } from 'svelte';
  import Icon from './Icon.svelte';

  interface Props {
    columns: Column<T>[];
    rows: T[];
    /** 行的稳定标识，避免 Svelte 复用错行。 */
    rowKey?: (row: T, index: number) => string | number;
    onRowClick?: (row: T, index: number) => void;
    isActive?: (row: T, index: number) => boolean;
    /** 首列吸左，宽表横向滚动时保留股票名。 */
    stickyFirst?: boolean;
    /** 显示序号列。 */
    numbered?: boolean;
    maxHeight?: string;
    /** 默认排序列 key。 */
    sortKey?: string;
    sortDesc?: boolean;
    /** 宽表的最小宽度；小容器内改为横向滚动，避免长中文列被压成逐字换行。 */
    minWidth?: string;
    cell?: Snippet<[{ row: T; column: Column<T>; index: number }]>;
  }

  const {
    columns,
    rows,
    rowKey,
    onRowClick,
    isActive,
    stickyFirst = false,
    numbered = false,
    maxHeight,
    sortKey = '',
    sortDesc = true,
    minWidth,
    cell
  }: Props = $props();

  // sortKey / sortDesc 只作为初始值：排序之后由表头交互接管，
  // 上游再改这两个 prop 不应该覆盖用户当前的排序选择。
  // svelte-ignore state_referenced_locally
  let activeSort = $state(sortKey);
  // svelte-ignore state_referenced_locally
  let desc = $state(sortDesc);

  const sorted = $derived.by(() => {
    const column = columns.find((item) => item.key === activeSort);
    if (!column?.sortValue) return rows;
    const factor = desc ? -1 : 1;
    return [...rows].sort((a, b) => {
      const left = column.sortValue!(a);
      const right = column.sortValue!(b);
      if (typeof left === 'number' && typeof right === 'number') {
        return (left - right) * factor;
      }
      return String(left).localeCompare(String(right), 'zh-CN') * factor;
    });
  });

  function toggleSort(column: Column<T>) {
    if (!column.sortValue) return;
    if (activeSort === column.key) {
      desc = !desc;
    } else {
      activeSort = column.key;
      desc = true;
    }
  }

  function key(row: T, index: number): string | number {
    return rowKey ? rowKey(row, index) : index;
  }
</script>

<div class="wrap" style={maxHeight ? `max-height:${maxHeight}` : undefined}>
  <table class:sticky-first={stickyFirst} style={minWidth ? `min-width:${minWidth}` : undefined}>
    <thead>
      <tr>
        {#if numbered}<th class="idx" scope="col">#</th>{/if}
        {#each columns as column (column.key)}
          <th
            scope="col"
            class:right={column.align === 'right'}
            class:center={column.align === 'center'}
            class:sortable={Boolean(column.sortValue)}
            style={column.width ? `width:${column.width}` : undefined}
          >
            {#if column.sortValue}
              <button class="sort" type="button" onclick={() => toggleSort(column)}>
                {column.label}
                {#if activeSort === column.key}
                  <Icon name={desc ? 'arrow-down' : 'arrow-up'} size={10} width={2} />
                {/if}
              </button>
            {:else}
              {column.label}
            {/if}
          </th>
        {/each}
      </tr>
    </thead>
    <tbody>
      {#each sorted as row, index (key(row, index))}
        <tr
          class:clickable={Boolean(onRowClick)}
          class:active={isActive?.(row, index)}
          onclick={onRowClick ? () => onRowClick(row, index) : undefined}
        >
          {#if numbered}<td class="idx num">{index + 1}</td>{/if}
          {#each columns as column (column.key)}
            <td
              class:right={column.align === 'right'}
              class:center={column.align === 'center'}
              class:num={column.num}
              class:wrap={column.wrap}
              class:up={column.tone?.(row, index) === 'up'}
              class:down={column.tone?.(row, index) === 'down'}
            >
              {#if column.slot}
                {@render cell?.({ row, column, index })}
              {:else}
                {column.value?.(row, index) ?? ''}
                {#if column.sub}
                  {@const extra = column.sub(row, index)}
                  {#if extra}<small>{extra}</small>{/if}
                {/if}
              {/if}
            </td>
          {/each}
        </tr>
      {/each}
    </tbody>
  </table>
</div>

<style>
  .wrap {
    width: 100%;
    /* 上限贴合父级（Panel 的 body 在 scroll 模式下有确定高度），
       让垂直滚动只发生在这一层。否则外层 body 滚、本层不滚，
       而 th 的 sticky 粘的是本层——表头就会跟着内容一起滑走。
       父级高度不确定时 100% 解析不出来，等同于不设上限，不影响其它用法。 */
    max-height: 100%;
    overflow: auto;
  }

  table {
    width: 100%;
    font-size: var(--fs-micro);
    white-space: nowrap;
  }

  th {
    position: sticky;
    top: 0;
    z-index: 2;
    height: 24px;
    padding: 0 var(--sp-3);
    font-weight: 500;
    color: var(--fg-dim);
    text-align: left;
    background: var(--bg-raised);
    border-bottom: 1px solid var(--line-strong);
  }

  td {
    height: var(--h-row);
    padding: 0 var(--sp-3);
    color: var(--fg);
    border-bottom: 1px solid var(--line);
    vertical-align: middle;
  }

  tbody tr:last-child td {
    border-bottom: 0;
  }

  .right {
    text-align: right;
  }

  .center {
    text-align: center;
  }

  .num {
    font-family: var(--font-num);
    font-variant-numeric: tabular-nums;
  }

  td.wrap {
    max-width: 320px;
    white-space: normal;
    line-height: var(--lh-tight);
  }

  td.up {
    color: var(--up);
  }

  td.down {
    color: var(--down);
  }

  td small {
    display: block;
    margin-top: 1px;
    font-size: 9px;
    line-height: 1.2;
    color: var(--fg-mute);
  }

  .idx {
    width: 34px;
    color: var(--fg-mute);
    text-align: right;
  }

  tbody tr.clickable {
    cursor: pointer;
  }

  tbody tr:hover td {
    background: var(--bg-hover);
  }

  tbody tr.active td {
    background: var(--focus-soft);
    box-shadow: inset 2px 0 0 var(--focus);
  }

  .sticky-first td:first-child,
  .sticky-first th:first-child {
    position: sticky;
    left: 0;
    z-index: 1;
    background: var(--bg-panel);
  }

  .sticky-first th:first-child {
    z-index: 3;
    background: var(--bg-raised);
  }

  .sticky-first tbody tr:hover td:first-child {
    background: var(--bg-hover);
  }

  .sort {
    display: inline-flex;
    align-items: center;
    gap: 3px;
    padding: 0;
    font: inherit;
    color: inherit;
  }

  th.sortable:hover .sort {
    color: var(--fg);
  }

  th.right .sort {
    flex-direction: row-reverse;
  }
</style>
