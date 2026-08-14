<script lang="ts">
  /**
   * PBRPC 策略查询控制台（reqformat=22）。
   * 与 TQLEX 同构，额外把 protobuf 外层的校验信息（RpcID / 分片轮次 / 原始字节）
   * 摆到结果上方——这层编码是否解对，是本页最需要先确认的事。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import ResultSets from './ResultSets.svelte';
  import type { PbrpcConfig, PbrpcConfigsDocument, PbrpcQueryDocument } from '../../types';

  let entryFilter = $state('');
  let moduleFilter = $state('');
  let reqIdFilter = $state('');
  let query = $state('');

  let selectedId = $state('');
  let selectedSource = $state('');
  let setJson = $state('{}');
  let paramJson = $state('{}');
  let maxRounds = $state('32');
  let maxAssembledBytes = $state('134217728');
  let retryDelay = $state('150');
  let formError = $state('');

  const configs = new Resource<PbrpcConfigsDocument>();
  const result = new Resource<PbrpcQueryDocument>();

  const records = $derived(configs.data?.records ?? []);

  const entryOptions = $derived.by(() => {
    const names = [...new Set(records.map((item) => item.entry))].sort();
    return [{ id: '', label: '全部入口' }, ...names.map((name) => ({ id: name, label: name }))];
  });

  const moduleOptions = $derived.by(() => {
    const names = [...new Set(records.map((item) => item.module))].sort();
    return [{ id: '', label: '全部模块' }, ...names.map((name) => ({ id: name, label: name }))];
  });

  const filtered = $derived.by(() => {
    const needle = query.trim().toLowerCase();
    const reqNeedle = reqIdFilter.trim();
    return records.filter((item) => {
      if (entryFilter && item.entry !== entryFilter) return false;
      if (moduleFilter && item.module !== moduleFilter) return false;
      if (reqNeedle && !item.request_id.includes(reqNeedle)) return false;
      if (!needle) return true;
      return (
        item.request_id.includes(needle) ||
        item.entry.toLowerCase().includes(needle) ||
        item.module.toLowerCase().includes(needle) ||
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
      { label: '模块数', value: count(doc.module_count) },
      { label: 'ReqId 数', value: count(doc.request_id_count) },
      { label: '当前命中', value: count(filtered.length) }
    ];
  });

  /** 协议校验：外层 protobuf 解出来的字段，与结果表放在一起才有对照价值。 */
  const protocolStats = $derived.by(() => {
    const doc = result.data;
    if (!doc) return [];
    const response = doc.response;
    return [
      { label: 'RpcID', value: count(doc.rpc_id) },
      { label: '分片轮次', value: count(doc.rounds) },
      { label: '原始字节', value: count(doc.raw_size) },
      { label: '结果集数', value: count(response?.ResultSetNum ?? response?.ResultSets?.length ?? 0) },
      { label: 'ErrorCode', value: text(response?.ErrorCode ?? 0) },
      { label: '模块', value: text(doc.module) }
    ];
  });

  function loadConfigs() {
    void configs.load('/api/v1/pbrpc/configs');
  }

  function select(row: PbrpcConfig) {
    selectedId = row.request_id;
    selectedSource = row.source_file;
    formError = '';
    result.reset();
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

  function run() {
    if (!selected) return;
    formError = '';
    const setValue = parseMap(setJson, '占位符');
    if (setValue === null) return;
    const paramValue = parseMap(paramJson, '参数覆盖');
    if (paramValue === null) return;
    const budgetText = maxAssembledBytes.trim() || '134217728';
    if (!/^\d+$/.test(budgetText)) {
      formError = '组装字节上限必须是 1..134217728 的整数';
      return;
    }
    const maxAssembled = Number(budgetText);
    if (!Number.isSafeInteger(maxAssembled) || maxAssembled < 1 || maxAssembled > 134217728) {
      formError = '组装字节上限必须是 1..134217728 的整数';
      return;
    }
    void result.load(
      `/api/v1/pbrpc/query?${queryString({
        req_id: selected.request_id,
        entry: selected.entry,
        module: selected.module,
        source_file: selected.source_file,
        set: setValue,
        param: paramValue,
        max_rounds: maxRounds.trim() || '32',
        max_assembled_bytes: maxAssembled,
        retry_delay_ms: retryDelay.trim() || '150'
      })}`
    );
  }

  const columns: Column<PbrpcConfig>[] = [
    { key: 'req', label: 'ReqId', width: '70px', num: true, value: (row) => row.request_id },
    { key: 'module', label: '模块', width: '140px', value: (row) => text(row.module) },
    { key: 'entry', label: '入口', width: '140px', value: (row) => row.entry },
    { key: 'source', label: '源文件', width: '150px', value: (row) => row.source_file },
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
  eyebrow="PBRPC · reqformat=22"
  title="PBRPC 策略查询"
  description="reqformat=22 模板走 protobuf 外层封装、按 RpcID 分片续传；这里可以逐个模板发起查询并核对外层解码结果。"
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
        emptyText="没有匹配的模板；放宽模块、入口或 ReqId 过滤条件。"
      >
        {#snippet toolbar()}
          <Select
            value={moduleFilter}
            options={moduleOptions}
            label="模块"
            width="180px"
            onChange={(next) => (moduleFilter = next)}
          />
          <Select
            value={entryFilter}
            options={entryOptions}
            label="入口"
            width="180px"
            onChange={(next) => (entryFilter = next)}
          />
          <TextInput bind:value={reqIdFilter} width="110px" label="ReqId" placeholder="ReqId" />
          <TextInput
            bind:value={query}
            icon="search"
            width="220px"
            label="检索模板"
            placeholder="入口、模块或请求体片段"
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
        eyebrow="PBRPC RESPONSE"
        title="解码结果"
        response={result.data?.response ?? null}
        stats={protocolStats}
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
      subtitle={selected ? `${selected.module} · ${selected.entry}` : '先在左侧选择一个模板'}
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
          <TextInput bind:value={maxRounds} width="100px" label="最大分片轮次" placeholder="max_rounds" />
          <TextInput
            bind:value={maxAssembledBytes}
            width="150px"
            label="组装上限（字节）"
            placeholder="max_assembled_bytes"
          />
          <TextInput bind:value={retryDelay} width="100px" label="重试间隔" placeholder="retry_ms" />
        </div>

        {#if formError}<p class="fail">{formError}</p>{/if}

        <div class="row">
          <Button variant="primary" icon="external" busy={result.busy} onclick={run}>
            执行 PBRPC 查询
          </Button>
        </div>
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

  .fail {
    margin-bottom: var(--sp-3);
    padding: var(--sp-1) var(--sp-2);
    font-size: var(--fs-micro);
    color: var(--up);
    background: var(--up-soft);
    border-radius: var(--radius);
  }
</style>
