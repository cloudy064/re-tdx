<script lang="ts">
  /**
   * TQLEX / PBRPC 共用的响应渲染器。
   *
   * 两条协议的响应外壳完全一致（ErrorCode + ResultSets），列定义在 ColDes 里
   * 动态下发、行数据是裸数组，所以列必须在运行时从 ColDes 推导，不能写死。
   */
  import { count, text } from '../../lib/fmt';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import type { TqlexResponse } from '../../types';

  interface Props {
    response: TqlexResponse | null;
    /** 协议校验指标，排在结果表上方。 */
    stats?: Stat[];
    title?: string;
    eyebrow?: string;
    busy?: boolean;
    error?: string;
    /** 是否已执行过一次查询，用于区分「没跑过」和「跑完是空的」。 */
    loaded?: boolean;
    onRetry?: () => void;
    emptyText?: string;
  }

  const {
    response,
    stats = [],
    title = '响应结果',
    eyebrow = '',
    busy = false,
    error = '',
    loaded = false,
    onRetry,
    emptyText = '选择模板并执行查询后，这里显示服务端返回的结果集。'
  }: Props = $props();

  const sets = $derived(response?.ResultSets ?? []);
  const PAGE_SIZE = 200;

  let active = $state(0);
  let page = $state(0);

  // 换一次响应就退回第一个结果集和第一页，否则旧下标可能越界到空表。
  $effect(() => {
    void response;
    active = 0;
    page = 0;
  });

  const options = $derived(
    sets.map((set, index) => ({
      id: String(index),
      label: `结果集 ${index + 1}`,
      hint: `${count(set.RowNum ?? set.Content?.length ?? 0)} 行 · ${count(
        set.ColNum ?? set.ColDes?.length ?? 0
      )} 列`
    }))
  );

  const current = $derived(sets[active] ?? null);
  const rows = $derived((current?.Content ?? []) as unknown[][]);
  const rowCount = $derived(rows.length);
  const pageStart = $derived(page * PAGE_SIZE);
  const pageEnd = $derived(Math.min(pageStart + PAGE_SIZE, rowCount));
  const visibleRows = $derived(rows.slice(pageStart, pageEnd));
  // 列类型在固定首个样本窗口内判断，避免列数 × 全量行数的额外扫描。
  const typeSample = $derived(rows.slice(0, PAGE_SIZE));

  function selectResultSet(next: string) {
    active = Number(next);
    page = 0;
  }

  function previousPage() {
    if (page > 0) page -= 1;
  }

  function nextPage() {
    if (pageEnd < rowCount) page += 1;
  }

  const columns = $derived.by(() => {
    const described = current?.ColDes ?? [];
    return described.map((column, index): Column<unknown[]> => {
      // 服务端不标注列类型，只能靠样本值判断是否按数值列排版
      const numeric = typeSample.some((row) => typeof row[index] === 'number');
      return {
        key: `col-${index}`,
        label: column.Caption || column.Name || `列 ${index + 1}`,
        align: numeric ? 'right' : 'left',
        num: numeric,
        value: (row: unknown[]) => text(row[index]),
        sortValue: numeric
          ? (row: unknown[]) => Number(row[index]) || 0
          : (row: unknown[]) => String(row[index] ?? '')
      };
    });
  });

  const errorCode = $derived(String(response?.ErrorCode ?? ''));
  const failed = $derived(errorCode !== '' && errorCode !== '0');
</script>

<Panel
  scroll
  {title}
  {eyebrow}
  {busy}
  {error}
  {onRetry}
  subtitle={response
    ? failed
      ? `ErrorCode ${errorCode} · ${text(response.ErrorInfo)}`
      : `${sets.length} 个结果集`
    : ''}
  empty={!busy && !error && (!loaded || sets.length === 0)}
  emptyText={loaded && sets.length === 0 ? '服务端返回了空结果集' : emptyText}
>
  {#snippet toolbar()}
    {#if options.length > 1}
      <Segmented
        options={options}
        value={String(active)}
        onChange={selectResultSet}
        ariaLabel="结果集"
      />
    {/if}
  {/snippet}

  {#if stats.length}
    <StatGrid {stats} />
  {/if}

  {#if failed}
    <p class="fail">ErrorCode {errorCode} · {text(response?.ErrorInfo)}</p>
  {/if}

  {#if current}
    <div class="pager">
      <span>
        返回 {count(rowCount)} 行 · 当前 {rowCount === 0 ? '0' : `${count(pageStart + 1)}–${count(pageEnd)}`}
        · 浏览器每页最多 {count(PAGE_SIZE)} 行
      </span>
      <div class="pager-actions">
        <Button variant="ghost" disabled={page === 0} onclick={previousPage}>上一页</Button>
        <Button variant="ghost" disabled={pageEnd >= rowCount} onclick={nextPage}>下一页</Button>
      </div>
    </div>
    <div class="table">
      <DataTable {columns} rows={visibleRows} numbered />
    </div>
  {/if}
</Panel>

<style>
  .fail {
    margin-top: var(--sp-2);
    padding: var(--sp-1) var(--sp-2);
    font-size: var(--fs-micro);
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }

  .table {
    margin-top: var(--sp-2);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    overflow: hidden;
  }

  .pager,
  .pager-actions {
    display: flex;
    align-items: center;
  }

  .pager {
    justify-content: space-between;
    gap: var(--sp-2);
    margin-top: var(--sp-3);
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  .pager-actions {
    flex: none;
    gap: var(--sp-1);
  }
</style>
