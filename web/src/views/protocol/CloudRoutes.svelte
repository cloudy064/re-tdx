<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import TextInput from '../../ui/TextInput.svelte';

  interface RouteRecord {
    source_file: string;
    entry: string;
    entry_family: string;
    request_format: string;
    body: string;
    placeholders: string[];
    same_entry_other_formats: string[];
    route_assessment: string;
    reason: string;
    safe_next_action: string;
  }
  interface RoutesDocument {
    schema: string;
    request_format_counts: Record<string, number>;
    reqformat1_record_count: number;
    reqformat1_unique_entry_count: number;
    route_assessments: Record<string, number>;
    compatibility: {
      tpdata_path: string;
      tpdata_exists: boolean;
      tpdata_architecture: string;
      tool_process_architecture: string;
      load_in_process_possible: boolean;
      message_loop_required: boolean;
      diagnostic_boundary: string;
    };
    observed_probes: Array<{ entry: string; request_format: string; anonymous_result: string; verified_at: string }>;
    records: RouteRecord[];
  }

  const routes = new Resource<RoutesDocument>();
  let entry = $state('');
  let source = $state('');
  const rows = $derived(routes.data?.records ?? []);
  const stats = $derived.by(() => {
    const doc = routes.data;
    if (!doc) return [];
    return [
      { label: 'reqformat=1', value: count(doc.request_format_counts['1'] ?? 0), note: `${count(doc.reqformat1_unique_entry_count)} 个唯一入口` },
      { label: 'reqformat=2', value: count(doc.request_format_counts['2'] ?? 0) },
      { label: '11 / 20 / 22', value: `${count(doc.request_format_counts['11'] ?? 0)} / ${count(doc.request_format_counts['20'] ?? 0)} / ${count(doc.request_format_counts['22'] ?? 0)}` },
      { label: '当前返回', value: count(doc.reqformat1_record_count) },
      { label: 'TPData / 工具', value: `${doc.compatibility.tpdata_architecture} / ${doc.compatibility.tool_process_architecture}`, note: doc.compatibility.load_in_process_possible ? '位数兼容' : '不能同进程加载' }
    ];
  });

  const columns: Column<RouteRecord>[] = [
    { key: 'entry', label: '旧服务入口', width: '250px', num: true, value: (row) => row.entry, sub: (row) => row.entry_family, sortValue: (row) => row.entry },
    { key: 'source', label: '配置文件', width: '190px', value: (row) => row.source_file, sortValue: (row) => row.source_file },
    { key: 'assessment', label: '诊断', width: '170px', value: (row) => row.route_assessment, sub: (row) => row.same_entry_other_formats.length ? `同名格式 ${row.same_entry_other_formats.join('/')}` : '无同名现代路由' },
    { key: 'body', label: '请求 Body', wrap: true, num: true, value: (row) => text(row.body) },
    { key: 'reason', label: '证据边界', wrap: true, value: (row) => row.reason }
  ];

  function load() {
    void routes.load(`/api/v1/cloud/routes?${queryString({ entry: entry.trim(), source: source.trim() })}`);
  }
  onMount(load);
</script>

<PageHeader
  eyebrow="REQFORMAT=1 · /api/v1/cloud/routes"
  title="旧云路由诊断"
  description="盘点旧 TPData 服务名并与现代请求格式做精确关联；只读，不加载 DLL、不发送网络请求。"
  {stats}
>
  {#snippet actions()}
    <Badge tone={routes.data?.compatibility.load_in_process_possible ? 'up' : 'warn'}>
      {routes.data?.compatibility.load_in_process_possible ? '位数兼容' : 'x86 / x64 隔离'}
    </Badge>
    <Button icon="refresh" busy={routes.busy} onclick={load}>重新扫描</Button>
  {/snippet}
</PageHeader>

<div class="stack">
  <Panel title="宿主与已验证探针" subtitle={routes.data?.compatibility.diagnostic_boundary ?? ''} busy={routes.busy} error={routes.error} onRetry={load}>
    <div class="evidence">
      <p><strong>TPData：</strong>{text(routes.data?.compatibility.tpdata_path)}</p>
      {#each routes.data?.observed_probes ?? [] as probe (probe.entry + probe.request_format)}
        <p><code>{probe.entry}</code><span>RF={probe.request_format}</span><span>{probe.anonymous_result}</span><small>{probe.verified_at}</small></p>
      {/each}
    </div>
  </Panel>

  <Panel flush scroll title="reqformat=1 配置" subtitle={`${count(rows.length)} 条`} busy={routes.busy} error={routes.error} onRetry={load} empty={routes.loaded && rows.length === 0} emptyText="没有符合筛选条件的旧路由。">
    {#snippet toolbar()}
      <TextInput bind:value={entry} icon="search" width="260px" label="服务名" placeholder="pcwebcall / CWServ / cfg_" onEnter={load} />
      <TextInput bind:value={source} width="210px" label="配置文件" placeholder="cloud_cfg 文件名" onEnter={load} />
      <Button icon="search" busy={routes.busy} onclick={load}>筛选</Button>
    {/snippet}
    <DataTable {columns} {rows} numbered rowKey={(row) => `${row.source_file}|${row.entry}|${row.body}`} />
  </Panel>
</div>

<style>
  .stack { display: flex; flex: 1; flex-direction: column; gap: var(--sp-3); min-height: 0; }
  .evidence { display: flex; flex-direction: column; gap: var(--sp-2); padding: 0 var(--sp-3) var(--sp-3); font-size: var(--fs-micro); color: var(--fg-dim); }
  .evidence p { display: flex; flex-wrap: wrap; gap: var(--sp-3); margin: 0; }
  .evidence strong, .evidence code { color: var(--fg); }
  .evidence small { color: var(--fg-mute); }
</style>
