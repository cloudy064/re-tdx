<script lang="ts">
  import { onMount } from 'svelte';
  import { postJson, queryString } from '../../api';
  import { count, date, percent, price, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    TPoolHistoryDocument,
    TPoolHistoryFile,
    TPoolHistoryRecord
  } from '../../types';

  interface PoolInspection {
    source: string;
    size: number;
    sha256: string;
    flow_count: number;
    function_count: number;
    stock_count: number;
    security_count: number;
    cell_count: number;
    execution_ready_function_count: number;
    referenced_formulas: string[];
    flow_graph: { acyclic: boolean; references_valid: boolean; enabled_edge_count: number; dangling_edges: unknown[] };
    compatibility: { safe_to_inspect: boolean; safe_to_evaluate_individual_rules: boolean; safe_to_execute: boolean; execution_boundary: string };
  }
  interface PoolCatalog {
    schema: 'tdx-tpool-catalog-v1';
    path_scope: 'tdx-root-relative';
    pool_count: number;
    directories: Array<{ path: string; exists: boolean }>;
    pools: PoolInspection[];
  }
  interface EvaluatedRule { rule_index: number; formula: string; operator: string; period: string; cell_id: string; status: string; matched: boolean | null; message: string }
  interface EvaluatedSecurity { security_id: string; name: string; cell_id: string; any_rule_matched: boolean; seed_cell_any_rule_matched: boolean; rules: EvaluatedRule[] }
  interface FlowTransition { start_cell_id: string; end_cell_id: string; candidate_count: number; matched_count: number; matched_securities: string[] }
  interface PoolEvaluation {
    source: string;
    inline_source?: boolean;
    available_security_count: number;
    evaluated_security_count: number;
    evaluated_rule_count: number;
    matched_rule_count: number;
    filtered_rule_count: number;
    filter_unavailable_rule_count: number;
    flow_graph_evaluated: boolean;
    scope: string;
    securities: EvaluatedSecurity[];
    flow_projection: { evaluated: boolean; runtime_semantics_verified: boolean; semantics: string; message: string; transitions: FlowTransition[] };
  }

  const catalog = new Resource<PoolCatalog>();
  const evaluation = new Resource<PoolEvaluation>();
  const history = new Resource<TPoolHistoryDocument>();
  let selectedSource = $state('');
  let inlineSourceName = $state('inline-pool.xml');
  let inlineXml = $state('');
  let inlineXmlFileInput: HTMLInputElement;
  let historyPool = $state('');
  let historyCell = $state('');
  let historyKind = $state('all');
  let historyFrom = $state('');
  let historyTo = $state('');
  let historyLimit = $state('100');
  let selectedHistorySource = $state('');
  const pools = $derived(catalog.data?.pools ?? []);
  const selected = $derived(pools.find((pool) => pool.source === selectedSource) ?? null);
  const historyFiles = $derived(history.data?.files ?? []);
  const selectedHistoryFile = $derived(
    historyFiles.find((file) => file.source === selectedHistorySource) ?? null
  );
  const historyRecords = $derived(selectedHistoryFile?.records ?? []);
  const stats = $derived.by(() => [
    { label: 'XML 股票池', value: count(catalog.data?.pool_count ?? 0) },
    { label: '已选证券', value: count(selected?.security_count ?? evaluation.data?.available_security_count ?? 0) },
    { label: '规则 / 可计算', value: `${count(selected?.function_count ?? 0)} / ${count(selected?.execution_ready_function_count ?? 0)}` },
    { label: 'Cell / 启用边', value: `${count(selected?.cell_count ?? 0)} / ${count(selected?.flow_graph.enabled_edge_count ?? 0)}` },
    { label: '本次命中', value: count(evaluation.data?.matched_rule_count ?? 0), note: evaluation.data?.flow_graph_evaluated ? '已生成流程候选' : '尚未生成流程候选' },
    { label: '历史文件 / 成员', value: `${count(history.data?.file_count ?? 0)} / ${count(history.data?.record_count ?? 0)}`, note: history.data?.truncated ? `匹配 ${count(history.data.matched_file_count)}，已截断` : '原生日记录' }
  ]);

  const historyKindOptions = [
    { id: 'all', label: '全部历史' },
    { id: 'snapshot', label: '每日状态 .dat' },
    { id: 'entry', label: '每日入池 .log' }
  ];

  const poolColumns: Column<PoolInspection>[] = [
    { key: 'source', label: '股票池 XML', wrap: true, num: true, value: (row) => row.source, sub: (row) => row.sha256?.slice(0, 12) ?? '' },
    { key: 'stocks', label: '证券', align: 'right', num: true, value: (row) => count(row.security_count), sortValue: (row) => row.security_count },
    { key: 'rules', label: '规则', align: 'right', num: true, value: (row) => `${count(row.execution_ready_function_count)} / ${count(row.function_count)}`, sortValue: (row) => row.function_count },
    { key: 'graph', label: '流程图', width: '120px', value: (row) => row.flow_graph.references_valid && row.flow_graph.acyclic ? '可投影' : '需修复', sub: (row) => `${row.cell_count} cell · ${row.flow_graph.enabled_edge_count} edge` },
    { key: 'formulas', label: '指标', width: '220px', wrap: true, value: (row) => row.referenced_formulas?.join(' · ') || '—' }
  ];
  const securityColumns: Column<EvaluatedSecurity>[] = [
    { key: 'security', label: '证券', width: '130px', num: true, value: (row) => row.name || row.security_id, sub: (row) => `${row.security_id} · cell ${row.cell_id}` },
    { key: 'matched', label: '种子 Cell 命中', width: '100px', value: (row) => row.seed_cell_any_rule_matched ? '命中' : '未命中' },
    { key: 'evaluated', label: '已计算规则', align: 'right', num: true, value: (row) => count(row.rules.filter((rule) => rule.status === 'evaluated').length) },
    { key: 'filtered', label: '过滤 / 未知', align: 'right', num: true, value: (row) => `${row.rules.filter((rule) => rule.status === 'filtered').length} / ${row.rules.filter((rule) => rule.status === 'filter-unavailable').length}` },
    { key: 'rules', label: '规则结果', wrap: true, value: (row) => row.rules.filter((rule) => rule.cell_id === row.cell_id).map((rule) => `${rule.formula}:${rule.status}${rule.matched === true ? '✓' : ''}`).join(' · ') || '无规则' }
  ];
  const transitionColumns: Column<FlowTransition>[] = [
    { key: 'edge', label: '启用边', width: '140px', num: true, value: (row) => `${row.start_cell_id} → ${row.end_cell_id}` },
    { key: 'candidate', label: '输入候选', align: 'right', num: true, value: (row) => count(row.candidate_count) },
    { key: 'matched', label: '转移候选', align: 'right', num: true, value: (row) => count(row.matched_count) },
    { key: 'ids', label: '证券', wrap: true, num: true, value: (row) => row.matched_securities.join(' · ') || '—' }
  ];
  const historyFileColumns: Column<TPoolHistoryFile>[] = [
    { key: 'source', label: '根目录内文件', width: '285px', wrap: true, num: true, value: (row) => row.source, sub: (row) => row.sha256?.slice(0, 16) ?? '' },
    { key: 'kind', label: '类型', width: '112px', value: (row) => row.kind === 'daily-snapshot' ? '状态快照 .dat' : '入池记录 .log' },
    { key: 'scope', label: '池 / Cell', width: '160px', wrap: true, value: (row) => `${row.pool} / ${row.cell}` },
    { key: 'date', label: '历史日', width: '105px', num: true, value: (row) => date(row.history_date_text ?? row.history_date), sortValue: (row) => row.history_date ?? 0 },
    { key: 'records', label: '完整 / 总记录', align: 'right', num: true, value: (row) => `${count(row.complete_record_count)} / ${count(row.record_count)}`, sortValue: (row) => row.record_count },
    { key: 'duplicates', label: '重复证券', align: 'right', num: true, value: (row) => count(row.duplicate_record_count), sortValue: (row) => row.duplicate_record_count },
    { key: 'size', label: '字节', align: 'right', num: true, value: (row) => count(row.size), sortValue: (row) => row.size }
  ];
  const historyRecordColumns: Column<TPoolHistoryRecord>[] = [
    { key: 'security', label: '证券', width: '125px', num: true, value: (row) => row.security_id ?? row.code, sub: (row) => row.market ?? '' },
    { key: 'entry', label: '进入时间', width: '155px', num: true, value: (row) => `${date(row.entry_date_text ?? row.entry_date)} ${text(row.entry_time_text ?? row.entry_time)}` },
    { key: 'entry-price', label: '进入价', align: 'right', num: true, value: (row) => price(row.entry_price), sortValue: (row) => row.entry_price ?? Number.NEGATIVE_INFINITY },
    { key: 'current', label: '当日状态', align: 'right', num: true, value: (row) => price(row.current_price), sub: (row) => row.rise_pct === undefined ? '' : `涨幅 ${percent(row.rise_pct)}` },
    { key: 'maximum', label: '最高表现', align: 'right', num: true, value: (row) => row.maximum_rise_pct === undefined ? '—' : percent(row.maximum_rise_pct), sub: (row) => row.maximum_date_text ? `${date(row.maximum_date_text)} · ${price(row.maximum_price)}` : '' },
    { key: 'quality', label: '字段状态', width: '215px', wrap: true, value: (row) => row.complete ? '完整' : '不完整', sub: (row) => [...row.missing_fields.map((field) => `缺 ${field}`), ...row.invalid_fields.map((field) => `错 ${field}`)].join(' · ') }
  ];

  async function loadHistory() {
    selectedHistorySource = '';
    const result = await history.load(`/api/v1/pools/history?${queryString({
      pool: historyPool.trim(),
      cell: historyCell.trim(),
      kind: historyKind,
      from: historyFrom.trim(),
      to: historyTo.trim(),
      limit: historyLimit.trim()
    })}`);
    if (result) selectedHistorySource = result.files[0]?.source ?? '';
  }
  function load() {
    void catalog.load('/api/v1/pools');
    void loadHistory();
  }
  function choose(pool: PoolInspection) { selectedSource = pool.source; evaluation.reset(); }
  function chooseHistory(file: TPoolHistoryFile) { selectedHistorySource = file.source; }
  function evaluate() {
    if (!selectedSource) return;
    void evaluation.load(`/api/v1/pools/evaluate?${queryString({ source: selectedSource, limit: 200, pages: 1, page_size: 800 })}`);
  }
  async function evaluateInline() {
    if (!inlineXml.trim()) return;
    const submittedInlineXml = inlineXml;
    selectedSource = '';
    const result = await evaluation.loadWith(() => postJson<PoolEvaluation>(
      '/api/v1/pools/evaluate',
      {
        source_name: inlineSourceName.trim() || 'inline-pool.xml',
        xml: submittedInlineXml,
        limit: 200,
        pages: 1,
        page_size: 800,
        timeout_ms: 10000
      },
      'pool-evaluate'
    ));
    if (result && inlineXml === submittedInlineXml) inlineXml = '';
  }
  async function importXml(event: Event) {
    const input = event.currentTarget as HTMLInputElement;
    const file = input.files?.[0];
    try {
      if (!file) return;
      if (file.size > 512 * 1024) {
        evaluation.error = '内联股票池 XML 不能超过 512 KiB；更大的文件请放入通达信 tpool 目录后重新扫描。';
        return;
      }
      inlineXml = await file.text();
      const safeName = file.name.replace(/[^A-Za-z0-9._-]/g, '_');
      inlineSourceName = safeName.toLowerCase().endsWith('.xml') ? safeName : `${safeName}.xml`;
    } catch (error) {
      evaluation.error = error instanceof Error ? error.message : '读取 XML 文件失败';
    } finally {
      input.value = '';
    }
  }
  function clearInlineXml() {
    inlineXml = '';
    if (inlineXmlFileInput) inlineXmlFileInput.value = '';
  }
  onMount(load);
</script>

<PageHeader
  eyebrow="TPOOL · /api/v1/pools"
  title="TPool 股票池实验室"
  description="只读解析 XML，并按 cell 计算规则、过滤与跨证券排名；流程结果是拓扑投影，不启动原版 worker、不写回股票池。"
  {stats}
>
  {#snippet actions()}
    <Badge tone="up">只读</Badge>
    <Button icon="refresh" busy={catalog.busy || history.busy} onclick={load}>重新扫描</Button>
    <Button icon="market" variant="primary" disabled={!selected} busy={evaluation.busy} onclick={evaluate}>计算所选池</Button>
  {/snippet}
</PageHeader>

<div class="stack">
  <Panel title="内联股票池" subtitle="服务端不保留 XML；浏览器在计算成功后自动清空，失败时保留以便重试。">
    <div class="inline-pool">
      <div class="inline-actions">
        <input class="source-name" bind:value={inlineSourceName} aria-label="内联股票池名称" maxlength="128" />
        <label class="file-picker">导入 XML<input bind:this={inlineXmlFileInput} type="file" accept=".xml,text/xml,application/xml" onchange={importXml} /></label>
        <Button icon="market" variant="primary" disabled={!inlineXml.trim()} busy={evaluation.busy} onclick={evaluateInline}>计算内联池</Button>
        <Button onclick={clearInlineXml}>清除 XML</Button>
        <span>{count(new TextEncoder().encode(inlineXml).length)} / 524,288 B</span>
      </div>
      <textarea bind:value={inlineXml} spellcheck="false" placeholder="粘贴包含 flow、cell、func、stk 的 TPool XML，或从本机选择 XML 文件。"></textarea>
    </div>
  </Panel>

  <Panel flush scroll title="本地股票池" subtitle={catalog.data ? catalog.data.directories.map((item) => `${item.exists ? '✓' : '×'} ${item.path}`).join(' · ') : ''} busy={catalog.busy} error={catalog.error} onRetry={load} empty={catalog.loaded && pools.length === 0} emptyText="当前安装的 tpool、T0002/tpool、userdata/tpool 目录中没有 XML 股票池。">
    <DataTable columns={poolColumns} rows={pools} rowKey={(row) => row.source} onRowClick={choose} isActive={(row) => row.source === selectedSource} />
  </Panel>

  <Panel
    flush
    scroll
    title="每日历史文件"
    subtitle={history.data ? `${count(history.data.file_count)} 个文件 · ${count(history.data.record_count)} 条成员记录 · 路径均相对 TDX 根目录` : '只扫描服务启动时配置的 TDX 根目录'}
    busy={history.busy}
    error={history.error}
    onRetry={loadHistory}
    empty={history.loaded && !history.busy && historyFiles.length === 0}
    emptyText="当前筛选下没有 .dat 状态快照或 .log 入池记录；这不是接口错误。"
  >
    {#snippet toolbar()}
      <TextInput bind:value={historyPool} width="145px" label="股票池目录" placeholder="池目录（可空）" onEnter={loadHistory} />
      <TextInput bind:value={historyCell} width="145px" label="Cell 目录" placeholder="Cell（可空）" onEnter={loadHistory} />
      <Select bind:value={historyKind} options={historyKindOptions} width="150px" label="类型" />
      <TextInput bind:value={historyFrom} width="118px" label="起始日期" placeholder="YYYYMMDD" onEnter={loadHistory} />
      <TextInput bind:value={historyTo} width="118px" label="结束日期" placeholder="YYYYMMDD" onEnter={loadHistory} />
      <TextInput bind:value={historyLimit} width="88px" label="文件上限" placeholder="1..10000" onEnter={loadHistory} />
      <Button icon="filter" variant="primary" busy={history.busy} onclick={loadHistory}>查询历史</Button>
    {/snippet}
    <DataTable
      columns={historyFileColumns}
      rows={historyFiles}
      rowKey={(row) => row.source}
      onRowClick={chooseHistory}
      isActive={(row) => row.source === selectedHistorySource}
      sortKey="date"
      minWidth="1040px"
      maxHeight="330px"
    />
  </Panel>

  <Panel
    flush
    scroll
    title="历史成员"
    subtitle={selectedHistoryFile ? `${selectedHistoryFile.source} · ${count(selectedHistoryFile.record_count)} 条 · SHA-256 ${selectedHistoryFile.sha256.slice(0, 16)}…` : '从上方选择一个历史文件'}
    empty={history.loaded && (!selectedHistoryFile || historyRecords.length === 0)}
    emptyText={historyFiles.length === 0 ? '没有历史文件可供选择。' : '所选历史文件真实存在，但没有成员记录。'}
  >
    <DataTable
      columns={historyRecordColumns}
      rows={historyRecords}
      rowKey={(row, index) => `${selectedHistorySource}|${row.security_id ?? row.code}|${index}`}
      minWidth="980px"
      maxHeight="380px"
    />
  </Panel>

  {#if selected || evaluation.data}
    <Panel title="执行边界" subtitle={selected?.compatibility.execution_boundary ?? '内联 XML 由纯 C++ 解释器只读计算；请求正文未保留。'}>
      <div class="guards">
        {#if selected}
          <Badge tone={selected.flow_graph.references_valid ? 'up' : 'down'}>引用{selected.flow_graph.references_valid ? '有效' : '异常'}</Badge>
          <Badge tone={selected.flow_graph.acyclic ? 'up' : 'warn'}>{selected.flow_graph.acyclic ? '无环' : '含循环'}</Badge>
        {:else}
          <Badge tone="up">内联只读</Badge>
        {/if}
        <Badge tone="warn">原版定时/循环/写回未执行</Badge>
        <span>{text(evaluation.data?.flow_projection.semantics)}</span>
      </div>
    </Panel>
  {/if}

  <Panel flush scroll title="证券规则结果" subtitle={evaluation.data?.scope ?? '选择股票池后开始计算'} busy={evaluation.busy} error={evaluation.error} onRetry={evaluate} empty={evaluation.loaded && (evaluation.data?.securities.length ?? 0) === 0} emptyText="该池没有可计算证券。">
    <DataTable columns={securityColumns} rows={evaluation.data?.securities ?? []} rowKey={(row) => `${row.cell_id}|${row.security_id}`} />
  </Panel>

  {#if evaluation.data}
    <Panel flush title="流程转移候选" subtitle={evaluation.data.flow_projection.message}>
      <DataTable columns={transitionColumns} rows={evaluation.data.flow_projection.transitions ?? []} rowKey={(row) => `${row.start_cell_id}|${row.end_cell_id}`} />
      <p class="watch">持续告警可使用 <code>tdx-tool pool watch --input FILE</code>，输出规则与流程候选的进入/退出 JSONL。</p>
    </Panel>
  {/if}
</div>

<style>
  .stack { display: flex; flex: 1; flex-direction: column; gap: var(--sp-3); min-height: 0; }
  .inline-pool { display: grid; gap: var(--sp-2); padding: 0 var(--sp-3) var(--sp-3); }
  .inline-actions { display: flex; flex-wrap: wrap; align-items: center; gap: var(--sp-2); }
  .inline-actions span { color: var(--fg-mute); font-size: var(--fs-micro); }
  .source-name, textarea { border: 1px solid var(--line); border-radius: var(--radius-sm); background: var(--surface-2); color: var(--fg); }
  .source-name { width: 190px; padding: 7px 9px; }
  textarea { min-height: 130px; resize: vertical; padding: var(--sp-2); font: 12px/1.5 var(--font-mono); }
  .file-picker { cursor: pointer; border: 1px solid var(--line); border-radius: var(--radius-sm); padding: 7px 10px; color: var(--fg-mute); font-size: var(--fs-micro); }
  .file-picker input { display: none; }
  .guards { display: flex; flex-wrap: wrap; align-items: center; gap: var(--sp-2); padding: 0 var(--sp-3) var(--sp-3); font-size: var(--fs-micro); color: var(--fg-mute); }
  .guards span { flex: 1; min-width: 280px; }
  .watch { margin: var(--sp-2) var(--sp-3) var(--sp-3); font-size: var(--fs-micro); color: var(--fg-mute); }
  code { color: var(--fg); }
</style>
