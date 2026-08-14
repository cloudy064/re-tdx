<script lang="ts">
  /**
   * 数据关系 / 原始 JSN 命中（/api/v1/jsn/security）。
   *
   * 这是给逆向研究看原始字段的页面：左表列出命中该证券的每一条 JSN 记录，
   * 右侧原样铺开选中记录的全部键值，不做任何语义化改写，
   * 并标注命中路径（matched_by）以便回溯是哪个字段匹配上的。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { DASH, count } from '../../../lib/fmt';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import TextInput from '../../../ui/TextInput.svelte';
  import type { JsnMatch, JsnSecurityDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const jsn = new Resource<JsnSecurityDocument>();
  const doc = $derived(jsn.data);

  let query = $state('');
  let pickedKey = $state('');

  function load() {
    void jsn.load(`/api/v1/jsn/security?${queryString({ market, code, limit: 500 })}`);
  }

  /** 同一资源同组可能有多行，只有三者合起来才唯一。 */
  function keyOf(match: JsnMatch): string {
    return `${match.resource}#${match.group}#${match.row_index}`;
  }

  function pathOf(match: JsnMatch): string {
    return match.matched_by.map((path) => path.join('.')).join(' · ');
  }

  function display(value: unknown): string {
    if (value === null || value === undefined || value === '') return DASH;
    if (typeof value === 'object') return JSON.stringify(value);
    return String(value);
  }

  // func_gx_hyzt 是行业树本身，对个股没有信息量，沿用旧版的过滤
  const matches = $derived<JsnMatch[]>(
    (doc?.matches ?? []).filter((match) => !match.resource.includes('func_gx_hyzt'))
  );

  const filtered = $derived.by<JsnMatch[]>(() => {
    const needle = query.trim().toLowerCase();
    if (!needle) return matches;
    return matches.filter(
      (match) =>
        match.resource.toLowerCase().includes(needle) ||
        pathOf(match).toLowerCase().includes(needle) ||
        JSON.stringify(match.record).toLowerCase().includes(needle)
    );
  });

  // 过滤后旧选中项可能已被排除，回落到首条而不是留空
  const selected = $derived<JsnMatch | null>(
    filtered.find((match) => keyOf(match) === pickedKey) ?? filtered[0] ?? null
  );

  interface Field {
    key: string;
    value: unknown;
  }

  const fields = $derived<Field[]>(
    Object.entries(selected?.record ?? {}).map(([key, value]) => ({ key, value }))
  );

  const matchColumns: Column<JsnMatch>[] = [
    {
      key: 'resource',
      label: '资源',
      wrap: true,
      value: (row) => row.resource,
      sub: (row) => `组 ${row.group} · 第 ${row.row_index + 1} 行`
    },
    { key: 'matched', label: '命中路径', wrap: true, value: (row) => pathOf(row) || DASH },
    {
      key: 'fields',
      label: '字段',
      width: '54px',
      align: 'right',
      num: true,
      value: (row) => count(Object.keys(row.record).length)
    }
  ];

  const fieldColumns: Column<Field>[] = [
    { key: 'name', label: '字段', width: '150px', num: true, value: (row) => row.key },
    { key: 'value', label: '值', wrap: true, value: (row) => display(row.value) }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="个股 JSN / F10 命中"
  eyebrow="JSN SECURITY INDEX"
  subtitle={doc
    ? `${count(doc.resource_count)} 个资源 · ${count(doc.match_count)} 条命中${doc.truncated ? ' · 已截断' : ''}`
    : '横向查询本地 JSN 资源里与该证券相关的全部记录'}
  busy={jsn.busy}
  error={jsn.error}
  onRetry={() => load()}
  empty={jsn.loaded && !jsn.busy && filtered.length === 0}
  emptyText={matches.length ? '当前过滤条件没有命中记录' : '没有匹配的本地 JSN/F10 记录'}
  flush
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={jsn.busy} onclick={() => load()}>重新读取</Button>
  {/snippet}

  {#snippet toolbar()}
    <TextInput
      bind:value={query}
      icon="search"
      width="260px"
      label="过滤命中记录"
      placeholder="资源名、命中路径或记录内容"
    />
    <span class="mute">{count(filtered.length)} / {count(matches.length)} 条（已排除 func_gx_hyzt）</span>
  {/snippet}

  <div class="cols">
    <div class="list">
      <DataTable
        columns={matchColumns}
        rows={filtered}
        rowKey={(row) => keyOf(row)}
        onRowClick={(row) => (pickedKey = keyOf(row))}
        isActive={(row) => selected !== null && keyOf(row) === keyOf(selected)}
        maxHeight="420px"
      />
    </div>

    <div class="detail">
      {#if selected}
        <header class="detail-head">
          <div class="ident">
            <span class="eyebrow">RESOURCE</span>
            <strong class="num">{selected.resource}</strong>
            <span class="mute">组 {selected.group} · 第 {selected.row_index + 1} 行</span>
          </div>
          <div class="paths">
            {#each selected.matched_by as path, index (index)}
              <Badge tone="focus">{path.join('.')}</Badge>
            {/each}
          </div>
        </header>
        <DataTable columns={fieldColumns} rows={fields} rowKey={(row) => row.key} maxHeight="360px" />
      {:else}
        <p class="hint">在左表选择一条命中记录，查看它的完整原始字段。</p>
      {/if}
    </div>
  </div>
</Panel>

<style>
  .cols {
    display: grid;
    grid-template-columns: minmax(0, 340px) minmax(0, 1fr);
  }

  .list {
    min-width: 0;
    border-right: 1px solid var(--line);
  }

  .detail {
    display: flex;
    flex-direction: column;
    min-width: 0;
  }

  .detail-head {
    display: flex;
    flex-direction: column;
    gap: var(--sp-2);
    padding: var(--sp-3) var(--sp-4);
    background: var(--bg-raised);
    border-bottom: 1px solid var(--line);
  }

  .ident {
    display: flex;
    flex-direction: column;
    gap: 1px;
  }

  .ident strong {
    font-size: var(--fs-body);
    font-weight: 600;
    color: var(--fg);
  }

  .ident .mute,
  .paths {
    font-size: var(--fs-micro);
  }

  .paths {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-1);
  }

  .hint {
    padding: var(--sp-6);
    font-size: var(--fs-body);
    color: var(--fg-dim);
    text-align: center;
  }

  @media (max-width: 1180px) {
    .cols {
      grid-template-columns: minmax(0, 1fr);
    }

    .list {
      border-right: 0;
      border-bottom: 1px solid var(--line);
    }
  }
</style>
