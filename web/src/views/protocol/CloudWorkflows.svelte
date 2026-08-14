<script lang="ts">
  import { onMount } from 'svelte';
  import { count, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import type {
    CloudWorkflowCatalogItem,
    CloudWorkflowDocument,
    CloudWorkflowStepDocument,
    CloudWorkflowsDocument
  } from '../../types';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';

  interface StepSummary {
    key: string;
    role: string;
    transport: string;
    requestId: string;
    entry: string;
    sourceFile: string;
    resultSets: number;
    rows: number;
    error: string;
  }

  const catalog = new Resource<CloudWorkflowsDocument>();
  const execution = new Resource<CloudWorkflowDocument>();
  let workflowName = $state('');
  let parameterJson = $state('{}');
  let selectedKeys = $state('');
  let limit = $state('1');
  let pageSize = $state('50');
  let maxPages = $state('100');
  let timeoutMs = $state('15000');
  let masterAllPages = $state(false);
  let validationError = $state('');

  const catalogRows = $derived(catalog.data?.records ?? []);
  const selectedSpec = $derived(
    catalogRows.find((item) => item.name === workflowName) ?? null
  );
  const workflowOptions = $derived(catalogRows.map((item) => ({
    id: item.name,
    label: `${item.name} · ${item.description}`
  })));
  const stepRows = $derived.by((): StepSummary[] => {
    const document = execution.data;
    if (!document) return [];
    const rows: StepSummary[] = [summarizeStep(document.master, '主表', 'master')];
    for (const detail of document.details) {
      detail.children.forEach((step, index) => {
        rows.push(summarizeStep(step, `明细 · ${detail.key}`, `${detail.key}:${index}`));
      });
    }
    return rows;
  });
  const stats = $derived([
    { label: '内置流程', value: count(catalog.data?.count ?? 0) },
    { label: '当前流程', value: selectedSpec?.name ?? '—', note: selectedSpec?.description ?? '' },
    { label: '主表命中', value: count(execution.data?.selection.master_rows ?? 0) },
    { label: '展开明细', value: count(execution.data?.selection.selected_rows ?? 0) }
  ]);

  const catalogColumns: Column<CloudWorkflowCatalogItem>[] = [
    {
      key: 'name', label: '工作流', width: '190px',
      value: (row) => row.name, sub: (row) => row.description
    },
    {
      key: 'master', label: '主表', width: '150px', num: true,
      value: (row) => row.master.request_id,
      sub: (row) => `${row.master.transport} · key=${row.key_field}`
    },
    {
      key: 'details', label: '明细链', width: '240px', num: true,
      value: (row) => row.details.map((step) => step.request_id).join(' → '),
      sub: (row) => row.details.map((step) => step.transport).join(' / ')
    },
    {
      key: 'defaults', label: '默认参数', wrap: true,
      value: (row) => Object.entries(row.defaults).map(([key, value]) => `${key}=${value}`).join(' · ')
    }
  ];

  const stepColumns: Column<StepSummary>[] = [
    { key: 'role', label: '步骤', width: '170px', value: (row) => row.role },
    {
      key: 'request', label: '请求', width: '170px', num: true,
      value: (row) => row.requestId,
      sub: (row) => `${row.transport}${row.entry ? ` · ${row.entry}` : ''}`
    },
    {
      key: 'rows', label: '结果', width: '130px', num: true,
      value: (row) => `${count(row.rows)} 行`,
      sub: (row) => `${count(row.resultSets)} 个结果集`
    },
    {
      key: 'source', label: '模板来源', width: '210px', value: (row) => row.sourceFile
    },
    {
      key: 'error', label: '上游状态', wrap: true,
      value: (row) => row.error || '成功'
    }
  ];

  function responseRows(step: CloudWorkflowStepDocument): number {
    return (step.response.ResultSets ?? []).reduce((sum, set) => {
      const content = Array.isArray(set.Content) ? set.Content.length : 0;
      const declared = Number(set.RowNum ?? 0);
      return sum + (content || (Number.isFinite(declared) ? declared : 0));
    }, 0);
  }

  function summarizeStep(step: CloudWorkflowStepDocument, role: string, key: string): StepSummary {
    const resultSets = step.response.ResultSets ?? [];
    return {
      key,
      role,
      transport: step.transport,
      requestId: step.request_id || step.req_id,
      entry: step.entry,
      sourceFile: step.source_file,
      resultSets: resultSets.length,
      rows: responseRows(step),
      error: text(step.response.ErrorInfo)
    };
  }

  function applyWorkflow(name: string) {
    workflowName = name;
    const spec = catalogRows.find((item) => item.name === name);
    parameterJson = JSON.stringify(spec?.defaults ?? {}, null, 2);
    selectedKeys = '';
    validationError = '';
    execution.reset();
  }

  function scalarParameters(): Record<string, string | number | boolean | null> {
    const parsed: unknown = JSON.parse(parameterJson.trim() || '{}');
    if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed))
      throw new Error('参数覆盖必须是 JSON 对象');
    const result: Record<string, string | number | boolean | null> = {};
    for (const [key, value] of Object.entries(parsed as Record<string, unknown>)) {
      if (!key.trim() || !['string', 'number', 'boolean'].includes(typeof value) && value !== null)
        throw new Error('参数名不能为空，参数值只能是字符串、数字、布尔值或 null');
      result[key] = value as string | number | boolean | null;
    }
    return result;
  }

  function boundedInteger(raw: string, name: string, minimum: number, maximum: number): number {
    const value = Number(raw);
    if (!Number.isInteger(value) || value < minimum || value > maximum)
      throw new Error(`${name} 必须是 ${minimum}..${maximum} 的整数`);
    return value;
  }

  function runWorkflow() {
    validationError = '';
    try {
      if (!workflowName) throw new Error('请选择工作流');
      const keys = selectedKeys.split(',').map((item) => item.trim()).filter(Boolean);
      if (keys.length > 10) throw new Error('主键一次最多选择 10 个');
      const parameters = scalarParameters();
      const params = new URLSearchParams({
        name: workflowName,
        set: JSON.stringify(parameters),
        limit: String(boundedInteger(limit, '展开数', 1, 10)),
        page_size: String(boundedInteger(pageSize, '分页大小', 1, 500)),
        max_pages: String(boundedInteger(maxPages, '最大页数', 1, 100)),
        timeout_ms: String(boundedInteger(timeoutMs, '超时', 100, 30000))
      });
      if (keys.length) params.set('select', keys.join(','));
      if (masterAllPages) params.set('master_all_pages', '1');
      void execution.load(`/api/v1/cloud/workflow?${params.toString()}`);
    } catch (error) {
      validationError = error instanceof Error ? error.message : '参数无效';
    }
  }

  onMount(() => {
    void catalog.load('/api/v1/cloud/workflows').then((document) => {
      if (document?.records.length) applyWorkflow(document.records[0].name);
    });
  });
</script>

<PageHeader
  eyebrow="TQLEX / PBRPC · MASTER → DETAIL"
  title="关联工作流"
  description="把固定主表—明细关系作为可审计工作流执行；只读访问现有云查询端点，不写安装目录，不触发账户、交易或 Level2 授权。"
  {stats}
>
  {#snippet actions()}
    <Badge tone="warn">会访问公开云查询端点</Badge>
    <Button icon="refresh" busy={catalog.busy} onclick={() => void catalog.load('/api/v1/cloud/workflows')}>刷新目录</Button>
  {/snippet}
</PageHeader>

<div class="workflow-layout">
  <Panel
    title="工作流目录"
    subtitle={`${count(catalogRows.length)} 条固定协议链`}
    busy={catalog.busy}
    error={catalog.error}
    onRetry={() => void catalog.load('/api/v1/cloud/workflows')}
    empty={catalog.loaded && catalogRows.length === 0}
    emptyText="没有内置云工作流。"
    flush
    scroll
  >
    <DataTable
      columns={catalogColumns}
      rows={catalogRows}
      rowKey={(row) => row.name}
      onRowClick={(row) => applyWorkflow(row.name)}
      isActive={(row) => row.name === workflowName}
      minWidth="840px"
    />
  </Panel>

  <Panel title="执行参数" subtitle={selectedSpec?.description ?? '选择一条工作流'}>
    <div class="controls">
      <Select
        bind:value={workflowName}
        options={workflowOptions}
        label="工作流"
        width="min(100%, 520px)"
        onChange={applyWorkflow}
      />
      <TextInput bind:value={selectedKeys} label="指定主键" placeholder="逗号分隔，留空取首行" width="310px" />
      <TextInput bind:value={limit} label="展开数" width="105px" />
      <TextInput bind:value={pageSize} label="分页" width="105px" />
      <TextInput bind:value={maxPages} label="最多页" width="105px" />
      <TextInput bind:value={timeoutMs} label="超时 ms" width="125px" />
      <label class="check"><input type="checkbox" bind:checked={masterAllPages} />完整读取主表分页</label>
      <Button variant="primary" icon="external" busy={execution.busy} disabled={!workflowName} onclick={runWorkflow}>执行工作流</Button>
    </div>
    <label class="json-field">
      <span>参数覆盖 JSON</span>
      <textarea bind:value={parameterJson} spellcheck="false" rows="6"></textarea>
    </label>
    {#if validationError}<p class="validation-error">{validationError}</p>{/if}
    <p class="boundary">选择主键时服务端会完整分页主表以验证键存在；最多 10 个主键、10 个展开行、500 条/页、100 页。响应只展示结构化结果，不提供任意 URL、模板路径或请求正文入口。</p>
  </Panel>

  <Panel
    title="执行结果"
    subtitle={execution.data ? `${execution.data.workflow} · ${count(stepRows.length)} 个步骤` : '尚未执行'}
    busy={execution.busy}
    error={execution.error}
    onRetry={runWorkflow}
    empty={execution.loaded && !execution.data}
    emptyText="工作流没有返回结果。"
    flush
  >
    {#if execution.data}
      <div class="selection-summary">
        <span>主键字段 <strong>{execution.data.selection.key_field}</strong></span>
        <span>主表 {count(execution.data.selection.master_rows)} 行</span>
        <span>选中 {count(execution.data.selection.selected_rows)} 行</span>
      </div>
      <DataTable columns={stepColumns} rows={stepRows} rowKey={(row) => row.key} minWidth="850px" />
      {#if execution.data.details.length}
        <div class="details">
          {#each execution.data.details as detail (detail.key)}
            <details>
              <summary>{detail.key} · {count(detail.children.length)} 个明细步骤</summary>
              <pre>{JSON.stringify(detail.master_row, null, 2)}</pre>
            </details>
          {/each}
        </div>
      {/if}
    {/if}
  </Panel>
</div>

<style>
  .workflow-layout { display: flex; flex: 1; flex-direction: column; gap: var(--sp-3); min-height: 0; }
  .controls { display: flex; flex-wrap: wrap; align-items: center; gap: var(--sp-2); padding: var(--sp-3) var(--sp-4); }
  .check { display: inline-flex; align-items: center; gap: var(--sp-1); font-size: var(--fs-micro); color: var(--fg-dim); }
  .check input { accent-color: var(--focus); }
  .json-field { display: flex; flex-direction: column; gap: var(--sp-1); padding: 0 var(--sp-4) var(--sp-3); font-size: var(--fs-micro); color: var(--fg-mute); }
  textarea { width: 100%; resize: vertical; min-height: 92px; padding: var(--sp-2); font: var(--fs-micro)/1.5 var(--font-num); color: var(--fg); background: var(--bg-input); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  textarea:focus { outline: none; border-color: var(--focus); }
  .validation-error { margin: 0 var(--sp-4) var(--sp-2); color: var(--negative); font-size: var(--fs-micro); }
  .boundary { margin: 0; padding: 0 var(--sp-4) var(--sp-3); color: var(--fg-mute); font-size: var(--fs-micro); line-height: 1.6; }
  .selection-summary { display: flex; flex-wrap: wrap; gap: var(--sp-4); padding: var(--sp-2) var(--sp-4); color: var(--fg-dim); font-size: var(--fs-micro); border-bottom: 1px solid var(--line); }
  .selection-summary strong { color: var(--fg); }
  .details { display: grid; gap: var(--sp-2); padding: var(--sp-3) var(--sp-4); }
  details { border: 1px solid var(--line); border-radius: var(--radius); background: var(--bg-raised); }
  summary { padding: var(--sp-2) var(--sp-3); cursor: pointer; color: var(--fg); font-size: var(--fs-micro); }
  pre { max-height: 240px; overflow: auto; margin: 0; padding: var(--sp-3); color: var(--fg-dim); font: var(--fs-micro)/1.5 var(--font-num); white-space: pre-wrap; overflow-wrap: anywhere; border-top: 1px solid var(--line); }
</style>
