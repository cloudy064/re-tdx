<script lang="ts">
  /**
   * TQLEX 云查询控制台（reqformat=2）。
   * 左上选模板，右侧填参数，左下看结果集。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import ResultSets from './ResultSets.svelte';
  import type { TqlexConfig, TqlexConfigsDocument, TqlexQueryDocument } from '../../types';

  let entryFilter = $state('');
  let reqIdFilter = $state('');
  let query = $state('');

  let selectedId = $state('');
  let selectedSource = $state('');
  let setJson = $state('{}');
  let paramJson = $state('{}');
  let page = $state('0');
  let pageSize = $state('20');
  let maxPages = $state('10');
  let allPages = $state('0');
  let formError = $state('');

  const configs = new Resource<TqlexConfigsDocument>();
  const result = new Resource<TqlexQueryDocument>();

  const records = $derived(configs.data?.records ?? []);

  const entryOptions = $derived.by(() => {
    const names = [...new Set(records.map((item) => item.entry))].sort();
    return [{ id: '', label: '全部入口' }, ...names.map((name) => ({ id: name, label: name }))];
  });

  const filtered = $derived.by(() => {
    const needle = query.trim().toLowerCase();
    const reqNeedle = reqIdFilter.trim();
    return records.filter((item) => {
      if (entryFilter && item.entry !== entryFilter) return false;
      if (reqNeedle && !item.request_id.includes(reqNeedle)) return false;
      if (!needle) return true;
      return (
        item.request_id.includes(needle) ||
        item.entry.toLowerCase().includes(needle) ||
        item.source_file.toLowerCase().includes(needle) ||
        item.body.toLowerCase().includes(needle)
      );
    });
  });

  const selected = $derived(
    records.find(
      (item) => item.request_id === selectedId && item.source_file === selectedSource
    ) ?? null
  );

  const stats = $derived.by(() => {
    const doc = configs.data;
    if (!doc) return [];
    return [
      { label: '模板总数', value: count(doc.config_count) },
      { label: '入口数', value: count(doc.entry_count) },
      { label: 'ReqId 数', value: count(doc.request_id_count) },
      { label: '当前命中', value: count(filtered.length) }
    ];
  });

  function loadConfigs() {
    void configs.load('/api/v1/tqlex/configs');
  }

  function select(row: TqlexConfig) {
    selectedId = row.request_id;
    selectedSource = row.source_file;
    formError = '';
    result.reset();
    // 占位符是模板必填项，预生成骨架比让人对着文档手敲要可靠
    const skeleton: Record<string, string> = {};
    for (const name of row.placeholders) skeleton[name] = '';
    setJson = JSON.stringify(skeleton, null, 2);
    paramJson = '{}';
  }

  function parseMap(raw: string, label: string): string | null {
    const trimmed = raw.trim();
    if (trimmed === '' || trimmed === '{}') return '';
    try {
      const parsed: unknown = JSON.parse(trimmed);
      if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
        formError = `${label} 必须是 JSON 对象`;
        return null;
      }
      return JSON.stringify(parsed);
    } catch {
      formError = `${label} 不是合法 JSON`;
      return null;
    }
  }

  function parseNonnegativeInteger(
    raw: string,
    label: string,
    minimum: number,
    maximum?: number
  ): number | null {
    const trimmed = raw.trim();
    if (!/^\d+$/.test(trimmed)) {
      formError = `${label} 必须是十进制整数`;
      return null;
    }
    const value = Number(trimmed);
    if (!Number.isSafeInteger(value) || value < minimum || (maximum !== undefined && value > maximum)) {
      formError = maximum === undefined
        ? `${label} 必须不小于 ${minimum}`
        : `${label} 必须在 ${minimum}..${maximum} 之间`;
      return null;
    }
    return value;
  }

  function run() {
    if (!selected) return;
    formError = '';
    const setValue = parseMap(setJson, '占位符');
    if (setValue === null) return;
    const paramValue = parseMap(paramJson, '参数覆盖');
    if (paramValue === null) return;
    const pageValue = parseNonnegativeInteger(page, '页码', 0);
    if (pageValue === null) return;
    const pageSizeValue = parseNonnegativeInteger(pageSize, '页大小', 1, 5000);
    if (pageSizeValue === null) return;
    const maxPagesValue = parseNonnegativeInteger(maxPages, '最大页数', 1, 20);
    if (maxPagesValue === null) return;
    const effectiveMaxPages = allPages === '1' ? maxPagesValue : 1;
    if (pageSizeValue * effectiveMaxPages > 50000) {
      formError = '页大小 × 最大页数不能超过 50,000 行浏览器合并预算';
      return;
    }
    void result.load(
      `/api/v1/tqlex/query?${queryString({
        req_id: selected.request_id,
        entry: selected.entry,
        source_file: selected.source_file,
        set: setValue,
        param: paramValue,
        page: String(pageValue),
        page_size: String(pageSizeValue),
        all_pages: allPages,
        max_pages: String(effectiveMaxPages)
      })}`
    );
  }

  const columns: Column<TqlexConfig>[] = [
    { key: 'req', label: 'ReqId', width: '70px', num: true, value: (row) => row.request_id },
    { key: 'entry', label: '入口', width: '150px', value: (row) => row.entry },
    { key: 'source', label: '源文件', width: '160px', value: (row) => row.source_file },
    {
      key: 'placeholders',
      label: '占位符',
      wrap: true,
      value: (row) => (row.placeholders.length ? row.placeholders.join(' · ') : '无')
    },
    { key: 'body', label: '请求体', wrap: true, value: (row) => row.body }
  ];

  onMount(() => loadConfigs());
</script>

<PageHeader
  eyebrow="TQLEX · reqformat=2"
  title="TQLEX 云查询"
  description="从安装目录解析出的 reqformat=2 请求模板，可替换占位符、覆盖请求字段并直接向云端发起 JSON 查询。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={configs.busy} onclick={loadConfigs}>重新解析模板</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="330px">
  {#snippet main()}
    <div class="stack">
      <Panel
        flush
        scroll
        title="请求模板"
        subtitle={configs.loaded ? `${count(filtered.length)} / ${count(records.length)} 条` : ''}
        busy={configs.busy}
        error={configs.error}
        onRetry={loadConfigs}
        empty={configs.loaded && !configs.busy && filtered.length === 0}
        emptyText="没有匹配的模板；放宽入口或 ReqId 过滤条件。"
      >
        {#snippet toolbar()}
          <Select
            value={entryFilter}
            options={entryOptions}
            label="入口"
            width="200px"
            onChange={(next) => (entryFilter = next)}
          />
          <TextInput bind:value={reqIdFilter} width="110px" label="ReqId" placeholder="ReqId" />
          <TextInput
            bind:value={query}
            icon="search"
            width="240px"
            label="检索模板"
            placeholder="入口、源文件或请求体片段"
          />
        {/snippet}

        <DataTable
          {columns}
          rows={filtered}
          rowKey={(row, index) => `${row.source_file}-${row.request_id}-${index}`}
          onRowClick={select}
          isActive={(row) =>
            row.request_id === selectedId && row.source_file === selectedSource}
          stickyFirst
        />
      </Panel>

      <ResultSets
        eyebrow="TQLEX RESPONSE"
        title="响应结果"
        response={result.data?.response ?? null}
        busy={result.busy}
        error={result.error}
        loaded={result.loaded}
        onRetry={run}
        emptyText="选择左上方的模板，在右栏填好占位符后执行查询。"
      />
    </div>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="REQUEST BUILDER"
      title={selected ? `ReqId ${selected.request_id}` : '参数与执行'}
      subtitle={selected ? selected.entry : '先在左侧选择一个模板'}
      empty={!selected}
      emptyText="选择模板后，这里会按占位符生成参数骨架。"
    >
      {#if selected}
        <p class="src">{selected.source_file}</p>

        <label class="field">
          <span>占位符替换 · set</span>
          <textarea bind:value={setJson} spellcheck="false" rows="6"></textarea>
        </label>

        <label class="field">
          <span>请求字段覆盖 · param</span>
          <textarea bind:value={paramJson} spellcheck="false" rows="4"></textarea>
        </label>

        <div class="row">
          <TextInput bind:value={page} width="90px" label="页码" placeholder="Page" />
          <TextInput bind:value={pageSize} width="90px" label="页大小" placeholder="PageSize" />
        </div>

        <div class="row">
          <span class="hint">全部分页</span>
          <Segmented
            options={[
              { id: '0', label: '单页' },
              { id: '1', label: '合并全部' }
            ]}
            value={allPages}
            onChange={(next) => (allPages = next)}
            ariaLabel="分页策略"
          />
          {#if allPages === '1'}
            <TextInput bind:value={maxPages} width="100px" label="最大页数" placeholder="最大页数" />
          {/if}
        </div>

        <p class="budget">
          合并查询最多 50,000 行（页大小 ≤ 5,000、最大 20 页）；完整响应保留，结果集在浏览器中另按 200 行窗口展示。
        </p>

        {#if formError}<p class="fail">{formError}</p>{/if}

        <div class="row">
          <Button variant="primary" icon="external" busy={result.busy} onclick={run}>
            执行 TQLEX 查询
          </Button>
        </div>

        {#if result.data}
          <p class="hint">已请求 {text(result.data.entry)} · {result.data.all_pages ? '全部分页' : '单页'}</p>
        {/if}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  /* 上下两块各占一格，整页不产生纵向滚动 */
  .stack {
    display: grid;
    grid-template-rows: minmax(0, 1fr) minmax(0, 1.25fr);
    gap: var(--sp-2);
    flex: 1;
    min-height: 0;
  }

  .src {
    margin-bottom: var(--sp-3);
    font-family: var(--font-num);
    font-size: 10px;
    color: var(--fg-mute);
    word-break: break-all;
  }

  .field {
    display: flex;
    flex-direction: column;
    gap: var(--sp-1);
    margin-bottom: var(--sp-3);
  }

  .field span {
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  textarea {
    width: 100%;
    padding: var(--sp-2);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
    line-height: var(--lh-body);
    color: var(--fg);
    background: var(--bg-input);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
    resize: vertical;
  }

  textarea:focus {
    outline: none;
    border-color: var(--focus);
  }

  .row {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: var(--sp-2);
    margin-bottom: var(--sp-3);
  }

  .hint {
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  .fail {
    margin-bottom: var(--sp-3);
    padding: var(--sp-1) var(--sp-2);
    font-size: var(--fs-micro);
    color: var(--up);
    background: var(--up-soft);
    border-radius: var(--radius);
  }
</style>
