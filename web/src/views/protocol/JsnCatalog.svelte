<script lang="ts">
  /**
   * JSN 资源目录：本地已编目的静态资源主表 + 资源详情 + 远端探测/下载。
   */
  import { onMount } from 'svelte';
  import { postActionJson, queryString } from '../../api';
  import { count, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    JsnCatalogDocument,
    JsnCandidate,
    JsnCandidateDocument,
    JsnDiscoveryDocument,
    JsnDiscoveryFile,
    JsnResource,
    JsnTransferResult
  } from '../../types';

  let query = $state('');
  let selectedName = $state('');
  /** 下载是写操作，走 POST，不能塞进只读的 Resource 里。 */
  let downloadBusy = $state(false);
  let downloadError = $state('');
  let downloadResult = $state<JsnTransferResult | null>(null);
  let baselineBusy = $state(false);
  let baselineError = $state('');
  let topMode = $state<'changes' | 'candidates'>('changes');
  let candidateFamily = $state('');
  let selectedCandidateName = $state('');

  const catalog = new Resource<JsnCatalogDocument>();
  const probe = new Resource<JsnTransferResult>();
  const discovery = new Resource<JsnDiscoveryDocument>();
  const candidates = new Resource<JsnCandidateDocument>();

  const resources = $derived(catalog.data?.resources ?? []);
  const discoveryRows = $derived(discovery.data?.priority_changes ?? []);
  const candidateRows = $derived(candidates.data?.candidates ?? []);

  const filtered = $derived.by(() => {
    const needle = query.trim().toLowerCase();
    if (!needle) return resources;
    return resources.filter(
      (item) =>
        item.resource.toLowerCase().includes(needle) ||
        item.columns.some((column) => column.toLowerCase().includes(needle))
    );
  });

  const selected = $derived(resources.find((item) => item.resource === selectedName) ?? null);
  const selectedCandidate = $derived(
    candidateRows.find((item) => item.resource === selectedCandidateName) ?? null
  );

  const stats = $derived.by(() => {
    const counts = catalog.data?.counts;
    if (!counts) return [];
    const result = [
      { label: '已编目资源', value: count(counts.resources) },
      { label: '累计行数', value: count(counts.rows) },
      { label: '占用字节', value: count(counts.bytes) },
      { label: '当前命中', value: count(filtered.length) }
    ];
    const changes = discovery.data?.summary;
    if (changes) {
      result.push({
        label: '本次资源变化',
        value: count(
          changes.added_file_count + changes.changed_file_count + changes.removed_file_count
        )
      });
      result.push({ label: '新字段', value: count(changes.new_column_count) });
    }
    if (candidates.data)
      result.push({
        label: '待探测动态资源',
        value: count(candidates.data.summary.unique_missing_resource_count)
      });
    return result;
  });

  const detailStats = $derived.by(() => {
    if (!selected && selectedCandidate) return [
      { label: '优先级', value: count(selectedCandidate.priority_score) },
      { label: '证据行', value: count(selectedCandidate.evidence_rows) },
      { label: '主表', value: count(selectedCandidate.source_resources.length) },
      { label: '动态键', value: count(Object.keys(selectedCandidate.key_values).length) }
    ];
    if (!selected) return [];
    return [
      { label: '行数', value: count(selected.rows) },
      { label: '分组', value: count(selected.groups) },
      { label: '字段', value: count(selected.columns.length) },
      { label: '字节', value: count(selected.size) },
      { label: '覆盖证券', value: count(selected.unique_securities) },
      { label: '成员证券', value: count(selected.unique_member_securities) }
    ];
  });

  const transferStats = $derived.by(() => {
    const result = downloadResult ?? probe.data;
    if (!result) return [];
    return [
      { label: '远端大小', value: count(result.size) },
      { label: 'MD5', value: result.has_md5 ? text(result.md5) : '无' },
      { label: '状态', value: text(result.status ?? '已探测') }
    ];
  });

  function loadCatalog() {
    void catalog.load('/api/v1/jsn/catalog');
  }

  function loadDiscovery(refresh = false) {
    baselineError = '';
    void discovery.load(`/api/v1/jsn/discovery?top=100${refresh ? '&refresh=1' : ''}`);
  }

  function loadCandidates(refresh = false) {
    void candidates.load(
      `/api/v1/jsn/candidates?${queryString({
        family: candidateFamily.trim(),
        missing_only: 1,
        limit: 100,
        refresh: refresh ? 1 : 0
      })}`
    );
  }

  function refreshAll(refresh = false) {
    loadCatalog();
    loadDiscovery(refresh);
    loadCandidates(refresh);
  }

  function select(row: JsnResource) {
    selectedName = row.resource;
    selectedCandidateName = '';
    probe.reset();
    downloadResult = null;
    downloadError = '';
  }

  function selectDiscovery(row: JsnDiscoveryFile) {
    query = row.resource;
    const local = resources.find((item) => item.resource === row.resource);
    if (local) select(local);
  }

  function selectCandidate(row: JsnCandidate) {
    selectedName = row.resource;
    selectedCandidateName = row.resource;
    query = row.resource;
    probe.reset();
    downloadResult = null;
    downloadError = '';
  }

  async function captureBaseline() {
    if (baselineBusy) return;
    baselineBusy = true;
    baselineError = '';
    try {
      await postActionJson<JsnDiscoveryDocument>(
        '/api/v1/jsn/discovery?action=capture&top=100',
        'jsn-discovery-baseline'
      );
      await discovery.load('/api/v1/jsn/discovery?top=100');
    } catch (err) {
      baselineError = err instanceof Error ? err.message : '基线捕获失败';
    } finally {
      baselineBusy = false;
    }
  }

  function runProbe() {
    if (!selectedName) return;
    downloadResult = null;
    void probe.load(
      `/api/v1/jsn/resource?${queryString({ resource: selectedName, action: 'probe' })}`
    );
  }

  async function runDownload() {
    if (!selectedName || downloadBusy) return;
    downloadBusy = true;
    downloadError = '';
    try {
      downloadResult = await postActionJson<JsnTransferResult>(
        `/api/v1/jsn/resource?${queryString({ resource: selectedName, action: 'download' })}`
      );
      refreshAll(true); // 后端下载后会重建索引，目录与候选队列需要一起刷新
    } catch (err) {
      downloadError = err instanceof Error ? err.message : '下载失败';
    } finally {
      downloadBusy = false;
    }
  }

  const columns: Column<JsnResource>[] = [
    {
      key: 'resource',
      label: '资源',
      width: '150px',
      value: (row) => row.resource,
      sub: (row) => `${row.columns.length} 字段`,
      sortValue: (row) => row.resource
    },
    {
      key: 'rows',
      label: '行数',
      align: 'right',
      num: true,
      value: (row) => count(row.rows),
      sortValue: (row) => row.rows ?? 0
    },
    {
      key: 'groups',
      label: '分组',
      align: 'right',
      num: true,
      value: (row) => count(row.groups),
      sortValue: (row) => row.groups ?? 0
    },
    {
      key: 'securities',
      label: '覆盖证券',
      align: 'right',
      num: true,
      value: (row) => count(row.unique_securities),
      sortValue: (row) => row.unique_securities ?? 0
    },
    {
      key: 'members',
      label: '成员证券',
      align: 'right',
      num: true,
      value: (row) => count(row.unique_member_securities),
      sortValue: (row) => row.unique_member_securities ?? 0
    },
    {
      key: 'size',
      label: '字节',
      align: 'right',
      num: true,
      value: (row) => count(row.size),
      sortValue: (row) => row.size ?? 0
    },
    {
      key: 'columns',
      label: '字段预览',
      wrap: true,
      value: (row) => row.columns.slice(0, 6).join(' · ')
    }
  ];

  const discoveryColumns: Column<JsnDiscoveryFile>[] = [
    {
      key: 'resource',
      label: '变化资源',
      width: '190px',
      value: (row) => row.resource,
      sub: (row) => row.template ?? '未识别模板',
      sortValue: (row) => row.resource
    },
    {
      key: 'status',
      label: '状态',
      value: (row) =>
        ({ added: '新增', changed: '变化', removed: '移除', unchanged: '未变', unbaselined: '待建基线' })[
          row.status
        ],
      sortValue: (row) => row.status
    },
    {
      key: 'coverage',
      label: '覆盖',
      value: (row) =>
        row.coverage === 'typed-command'
          ? 'C++ 已接入'
          : row.coverage === 'generic-only'
            ? '仅通用解析'
            : '新资源',
      sub: (row) => row.typed_commands.join(' · ')
    },
    {
      key: 'columns',
      label: '字段变化',
      wrap: true,
      value: (row) => [
        ...row.new_columns.map((item) => `+${item}`),
        ...row.removed_columns.map((item) => `-${item}`)
      ].join(' · ') || '—'
    },
    {
      key: 'score',
      label: '优先级',
      align: 'right',
      num: true,
      value: (row) => count(row.priority_score),
      sortValue: (row) => row.priority_score
    }
  ];

  const candidateColumns: Column<JsnCandidate>[] = [
    {
      key: 'resource',
      label: '动态候选',
      width: '190px',
      value: (row) => row.resource,
      sub: (row) => row.template,
      sortValue: (row) => row.resource
    },
    {
      key: 'family',
      label: '资源族',
      value: (row) => row.family,
      sortValue: (row) => row.family
    },
    {
      key: 'keys',
      label: '动态键',
      value: (row) => Object.entries(row.key_values).map(([key, value]) => `${key}=${value}`).join(' · ')
    },
    {
      key: 'evidence',
      label: '主表证据',
      wrap: true,
      value: (row) => row.source_resources.join(' · '),
      sub: (row) => `${count(row.evidence_rows)} 行 · ${row.origin}`,
      sortValue: (row) => row.evidence_rows
    },
    {
      key: 'score',
      label: '优先级',
      align: 'right',
      num: true,
      value: (row) => count(row.priority_score),
      sortValue: (row) => row.priority_score
    }
  ];

  onMount(() => refreshAll());
</script>

<PageHeader
  eyebrow="JSN · /api/v1/jsn/catalog"
  title="JSN 资源目录"
  description="通达信离线静态资源的本地编目：行数、分组、字段与覆盖证券，可对单个资源探测远端版本或重新下载。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={catalog.busy || discovery.busy || candidates.busy} onclick={() => refreshAll(true)}>刷新扫描</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="360px">
  {#snippet main()}
    <div class="main-stack">
      <Panel
        flush
        title={topMode === 'changes' ? '增量能力发现' : '动态资源候选'}
        subtitle={topMode === 'changes'
          ? discovery.data?.baseline.available
            ? `基线 ${text(discovery.data.baseline.captured_at)} · ${count(discoveryRows.length)} 个优先变化`
            : '尚未建立基线；当前资源先按识别价值排序'
          : candidates.data
            ? `${count(candidates.data.summary.unique_missing_resource_count)} 个本地缺失资源 · 仅来自同业务 refunit 主表`
            : '从客户端主从单元关系生成，不做跨模板笛卡尔积'}
        busy={topMode === 'changes' ? discovery.busy : candidates.busy}
        error={topMode === 'changes' ? discovery.error || baselineError : candidates.error}
        onRetry={() => topMode === 'changes' ? loadDiscovery() : loadCandidates()}
        empty={topMode === 'changes'
          ? discovery.loaded && !discovery.busy && discoveryRows.length === 0
          : candidates.loaded && !candidates.busy && candidateRows.length === 0}
        emptyText={topMode === 'changes'
          ? '相对基线没有资源或字段变化。'
          : '当前筛选下没有待探测的动态资源。'}
      >
        {#snippet actions()}
          <Button variant={topMode === 'changes' ? 'primary' : 'default'} onclick={() => topMode = 'changes'}>资源变化</Button>
          <Button variant={topMode === 'candidates' ? 'primary' : 'default'} onclick={() => topMode = 'candidates'}>动态候选</Button>
          {#if topMode === 'changes'}
            <Button busy={baselineBusy} onclick={() => void captureBaseline()}>设为当前基线</Button>
          {/if}
        {/snippet}
        {#snippet toolbar()}
          {#if topMode === 'candidates'}
            <TextInput
              bind:value={candidateFamily}
              icon="search"
              width="180px"
              label="资源族"
              placeholder="如 gqzy / jjzb1"
            />
            <Button onclick={() => loadCandidates(true)}>筛选候选</Button>
          {/if}
        {/snippet}
        {#if topMode === 'changes'}
          <DataTable
            columns={discoveryColumns}
            rows={discoveryRows}
            rowKey={(row) => `${row.status}:${row.resource}`}
            onRowClick={selectDiscovery}
            maxHeight="190px"
            sortKey="score"
            stickyFirst
          />
        {:else}
          <DataTable
            columns={candidateColumns}
            rows={candidateRows}
            rowKey={(row) => `${row.template}:${row.resource}`}
            onRowClick={selectCandidate}
            isActive={(row) => row.resource === selectedCandidateName}
            maxHeight="190px"
            sortKey="score"
            stickyFirst
          />
        {/if}
      </Panel>

      <Panel
        flush
        scroll
        title="已编目资源"
        subtitle={catalog.loaded ? `${count(filtered.length)} / ${count(resources.length)} 个资源` : ''}
        busy={catalog.busy}
        error={catalog.error}
        onRetry={loadCatalog}
        empty={catalog.loaded && !catalog.busy && filtered.length === 0}
        emptyText="没有匹配的 JSN 资源；清空检索词或确认后端已配置 --jsn-root。"
      >
        {#snippet toolbar()}
          <TextInput
            bind:value={query}
            icon="search"
            width="260px"
            label="检索资源"
            placeholder="资源名或字段名，如 lhb / cgfx / ztt"
          />
        {/snippet}

        <DataTable
          {columns}
          rows={filtered}
          rowKey={(row) => row.resource}
          onRowClick={select}
          isActive={(row) => row.resource === selectedName}
          sortKey="rows"
          stickyFirst
        />
      </Panel>
    </div>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="RESOURCE DETAIL"
      title={selected?.resource ?? selectedCandidate?.resource ?? '资源详情'}
      subtitle={selected
        ? `${count(selected.columns.length)} 个字段`
        : selectedCandidate
          ? `${selectedCandidate.family} · ${selectedCandidate.origin}`
          : '点击左侧任意资源'}
      empty={!selected && !selectedCandidate}
      emptyText="点击左侧任意资源，查看字段结构并探测远端版本。"
    >
      {#if selected || selectedCandidate}
        <StatGrid stats={detailStats} inline />

        <div class="acts">
          <Button icon="search" busy={probe.busy} onclick={runProbe}>探测远端</Button>
          <Button
            variant="primary"
            icon="download"
            busy={downloadBusy}
            onclick={() => void runDownload()}
          >
            下载并重建索引
          </Button>
        </div>

        {#if probe.error}<p class="fail">{probe.error}</p>{/if}
        {#if downloadError}<p class="fail">{downloadError}</p>{/if}

        {#if transferStats.length}
          <StatGrid stats={transferStats} inline />
          <p class="path">{text((downloadResult ?? probe.data)?.remote_path)}</p>
          <p class="path">{text((downloadResult ?? probe.data)?.server)}</p>
        {/if}

        {#if selected}
          <h3>字段</h3>
          <ul class="fields">
            {#each selected.columns as column (column)}
              <li>{column}</li>
            {/each}
          </ul>
        {:else if selectedCandidate}
          <h3>动态键</h3>
          <ul class="fields">
            {#each Object.entries(selectedCandidate.key_values) as [key, value] (`${key}:${value}`)}
              <li>{key}={value}</li>
            {/each}
          </ul>
          <h3>来源主表</h3>
          {#each selectedCandidate.source_resources as source (source)}
            <p class="path">{source}</p>
          {/each}
        {/if}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .main-stack {
    display: flex;
    min-height: 0;
    height: 100%;
    flex-direction: column;
    gap: var(--sp-3);
  }

  .acts {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-2);
    margin: var(--sp-3) 0;
  }

  .fail {
    padding: var(--sp-1) var(--sp-2);
    margin-bottom: var(--sp-2);
    font-size: var(--fs-micro);
    color: var(--up);
    background: var(--up-soft);
    border-radius: var(--radius);
  }

  .path {
    font-family: var(--font-num);
    font-size: 10px;
    color: var(--fg-mute);
    word-break: break-all;
  }

  h3 {
    margin-top: var(--sp-4);
    margin-bottom: var(--sp-2);
    font-size: var(--fs-micro);
    font-weight: 600;
    color: var(--fg-dim);
  }

  .fields {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-1);
    margin: 0;
    padding: 0;
    list-style: none;
  }

  .fields li {
    padding: 1px var(--sp-2);
    font-family: var(--font-num);
    font-size: 10px;
    color: var(--fg-dim);
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }
</style>
