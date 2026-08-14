<script lang="ts">
  /**
   * 系统状态。服务概览取自全局 health（App 启动时已拉过，这里只提供手动刷新），
   * 另外盘点原生功能目录、安装产物，以及从 openapi.json 读到的接口清单。
   */
  import { onMount } from 'svelte';
  import { count, date, text, time } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Split from '../../ui/Split.svelte';
  import type {
    Feature,
    FeatureDocument,
    InstallArtifact,
    InstallDocument,
    MarketStreamStatusDocument
  } from '../../types';

  /** openapi.json 只用到 info 与标准 HTTP operation 的摘要，其余字段忽略。 */
  interface OpenApiOperation {
    summary?: string;
    description?: string;
  }

  const openApiMethods = ['get', 'post', 'put', 'patch', 'delete', 'options', 'head', 'trace'] as const;
  type OpenApiMethod = (typeof openApiMethods)[number];

  interface OpenApiDocument {
    openapi?: string;
    info?: { title?: string; version?: string; description?: string };
    paths?: Record<string, Partial<Record<OpenApiMethod, OpenApiOperation>>>;
  }

  interface Endpoint {
    method: string;
    path: string;
    summary: string;
  }

  const spec = new Resource<OpenApiDocument>();
  const features = new Resource<FeatureDocument>();
  const install = new Resource<InstallDocument>();
  const stream = new Resource<MarketStreamStatusDocument>();

  const health = $derived(app.health);
  const featureRows = $derived(features.data?.features ?? []);
  const artifacts = $derived(install.data?.artifacts ?? []);

  const specRows = $derived.by<Endpoint[]>(() => {
    const paths = spec.data?.paths;
    if (!paths) return [];
    const rows: Endpoint[] = [];
    for (const [path, item] of Object.entries(paths)) {
      if (!path.startsWith('/')) continue;
      for (const method of openApiMethods) {
        const operation = item?.[method];
        if (!operation) continue;
        rows.push({ method: method.toUpperCase(), path, summary: text(operation.summary) });
      }
    }
    return rows.sort((a, b) => a.path.localeCompare(b.path) || a.method.localeCompare(b.method));
  });

  /** openapi 不可用时优先使用结构化 operation，兼容旧服务的 api_endpoint。 */
  const fallbackRows = $derived.by<Endpoint[]>(() => {
    const seen = new Map<string, Endpoint>();
    for (const feature of featureRows) {
      if (Array.isArray(feature.api_operations)) {
        for (const operation of feature.api_operations) {
          const method = operation.method.trim().toUpperCase();
          const path = operation.path.trim();
          if (!method || !path.startsWith('/')) continue;
          const key = `${method} ${path}`;
          if (!seen.has(key)) seen.set(key, { method, path, summary: feature.title });
        }
        continue;
      }
      if (feature.surface === 'cli-only') continue;
      for (const part of feature.api_endpoint.split(',')) {
        const path = part.trim();
        if (!path.startsWith('/')) continue;
        const key = `GET ${path}`;
        if (!seen.has(key)) seen.set(key, { method: 'GET', path, summary: feature.title });
      }
    }
    return [...seen.values()].sort(
      (a, b) => a.path.localeCompare(b.path) || a.method.localeCompare(b.method)
    );
  });

  const fallback = $derived(spec.loaded && specRows.length === 0);
  const endpoints = $derived(fallback ? fallbackRows : specRows);
  /** 两个来源都拿不到时才算真错误，否则降级展示。 */
  const endpointError = $derived(fallback && features.error ? features.error : '');

  const stats = $derived.by(() => {
    if (!health) return [];
    return [
      { label: '服务', value: text(health.service), note: health.native_cpp ? '原生 C++' : '非原生' },
      { label: '版本', value: text(health.version) },
      { label: 'API 版本', value: text(health.api_version) },
      { label: '证券', value: count(health.securities) },
      { label: '板块', value: count(health.blocks) },
      { label: '成分关系', value: count(health.memberships) },
      { label: '公式', value: count(health.formulas) },
      {
        label: 'JSN 数据',
        value: health.jsn_available ? '可用' : '不可用',
        tone: (health.jsn_available ? 'up' : 'down') as 'up' | 'down',
        note: text(health.jsn_root)
      },
      { label: 'TDX 根目录', value: text(health.tdx_root) }
    ];
  });

  const endpointColumns: Column<Endpoint>[] = [
    { key: 'method', label: '方法', width: '46px', value: (row) => row.method },
    { key: 'path', label: '路径', width: '250px', num: true, value: (row) => row.path },
    { key: 'summary', label: '说明', wrap: true, value: (row) => row.summary }
  ];

  function featureApiLabel(feature: Feature) {
    if (feature.api_operations?.length) {
      return feature.api_operations
        .map((operation) => `${operation.method.toUpperCase()} ${operation.path}`)
        .join(', ');
    }
    return feature.surface === 'cli-only' ? 'CLI only' : text(feature.api_endpoint);
  }

  const featureColumns: Column<Feature>[] = [
    { key: 'category', label: '分类', width: '84px', value: (row) => text(row.category) },
    {
      key: 'command',
      label: '命令',
      width: '150px',
      value: (row) => text(row.command),
      sub: (row) => featureApiLabel(row)
    },
    { key: 'title', label: '功能', width: '150px', value: (row) => text(row.title) },
    { key: 'description', label: '说明', wrap: true, value: (row) => text(row.description) },
    {
      key: 'network',
      label: '来源',
      width: '58px',
      value: (row) => (row.uses_network ? '联网' : '本地')
    }
  ];

  const artifactColumns: Column<InstallArtifact>[] = [
    { key: 'path', label: '文件', num: true, wrap: true, value: (row) => row.path },
    {
      key: 'size',
      label: '大小',
      align: 'right',
      num: true,
      value: (row) => `${count(row.size)} B`,
      sortValue: (row) => row.size ?? 0
    },
    {
      key: 'modified',
      label: '修改时间',
      width: '104px',
      num: true,
      value: (row) => date(row.modified),
      sub: (row) => time(row.modified),
      sortValue: (row) => row.modified ?? ''
    }
  ];

  function loadSpec() {
    void spec.load('/api/v1/openapi.json');
    void features.load('/api/v1/features');
  }

  function refresh() {
    void app.loadHealth();
    loadSpec();
    void install.load('/api/v1/install');
    void stream.load('/api/v1/market/stream/status');
  }

  onMount(() => {
    refresh();
    const timer = window.setInterval(
      () => void stream.load('/api/v1/market/stream/status', { silent: true }),
      5000
    );
    return () => window.clearInterval(timer);
  });
</script>

<PageHeader
  eyebrow="TDX-TOOL · 127.0.0.1:8765"
  title="服务状态"
  description="本地只读服务的运行概况、原生功能目录、公开接口清单与通达信安装目录盘点。"
  {stats}
>
  {#snippet actions()}
    {#if app.healthError}
      <Badge tone="down">离线</Badge>
    {:else if health}
      <Badge tone="up">在线</Badge>
    {/if}
    <Button icon="refresh" busy={app.booting} onclick={refresh}>刷新状态</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="400px">
  {#snippet main()}
    <div class="stack">
      <Panel
        flush
        scroll
        eyebrow={fallback ? 'FALLBACK · /API/V1/FEATURES' : 'OPENAPI 3.0.3'}
        title="接口清单"
        subtitle={fallback
          ? `openapi.json 不可用（${text(spec.error || '未返回 paths')}），已回退到功能目录里登记的接口`
          : text(spec.data?.info?.title)}
        busy={spec.busy || features.busy}
        error={endpointError}
        empty={spec.loaded && endpoints.length === 0}
        emptyText="服务未公开任何接口描述。"
        onRetry={loadSpec}
      >
        {#snippet actions()}
          <span class="tag">{count(endpoints.length)} 个操作</span>
        {/snippet}
        <DataTable columns={endpointColumns} rows={endpoints} rowKey={(row) => `${row.method}:${row.path}`} />
      </Panel>

      <Panel
        flush
        scroll
        eyebrow="NATIVE C++ COMMANDS"
        title="功能目录"
        subtitle="原生实现的命令及其对应的 HTTP 接口"
        busy={features.busy}
        error={features.error}
        empty={features.loaded && featureRows.length === 0}
        emptyText="服务没有登记任何功能。"
        onRetry={() => void features.load('/api/v1/features')}
      >
        {#snippet actions()}
          <span class="tag">{count(features.data?.count ?? featureRows.length)} 项</span>
        {/snippet}
        <DataTable columns={featureColumns} rows={featureRows} rowKey={(row) => row.command} />
      </Panel>
    </div>
  {/snippet}

  {#snippet aside()}
    <div class="aside-stack">
      <Panel
        eyebrow="0x0547 · LOCAL SSE"
        title="行情推送 Hub"
        subtitle={stream.data?.upstream_mode ?? '共享 7709 L1 轮询状态'}
        busy={stream.busy}
        error={stream.error}
        onRetry={() => void stream.load('/api/v1/market/stream/status')}
      >
        {#snippet actions()}
          {#if stream.data?.consecutive_failures}
            <Badge tone="warn">连续失败 {count(stream.data.consecutive_failures)}</Badge>
          {:else if stream.data?.last_error}
            <Badge tone="down">异常</Badge>
          {:else if stream.data}
            <Badge tone={stream.data.subscriber_count ? 'up' : 'neutral'}>
              {stream.data.subscriber_count ? '推送中' : '空闲'}
            </Badge>
          {/if}
        {/snippet}

        {#if stream.data}
          <div class="stream-grid">
            <div><span>订阅者</span><strong>{count(stream.data.subscriber_count)}</strong></div>
            <div><span>活跃证券</span><strong>{count(stream.data.active_security_count)}</strong></div>
            <div><span>成功轮询</span><strong>{count(stream.data.successful_polls)}</strong></div>
            <div><span>失败轮询</span><strong>{count(stream.data.failed_polls)}</strong></div>
          </div>
          <dl class="stream-detail">
            <div>
              <dt>交易时段</dt>
              <dd>{stream.data.exchange_session_active ? '是' : '否'}</dd>
            </div>
            <div>
              <dt>有效间隔</dt>
              <dd>{count(stream.data.effective_interval_ms)} ms</dd>
            </div>
            <div>
              <dt>最近成功</dt>
              <dd>{stream.data.last_success_at ? `${date(stream.data.last_success_at)} ${time(stream.data.last_success_at)}` : '尚无'}</dd>
            </div>
            <div>
              <dt>活跃标的</dt>
              <dd>{stream.data.active_securities.length ? stream.data.active_securities.join('、') : '无'}</dd>
            </div>
          </dl>
          {#if stream.data.last_error}
            <p class="stream-error">{stream.data.last_error}</p>
          {/if}
          <p class="boundary">
            FastHQ.Subscribe 未调用；上游为公开 7709 有界轮询，下游仅向本机浏览器推送 SSE。
          </p>
        {/if}
      </Panel>

      <Panel
        flush
        scroll
        eyebrow="INSTALL INVENTORY"
        title="安装产物"
        subtitle={text(install.data?.root)}
        busy={install.busy}
        error={install.error}
        empty={install.loaded && artifacts.length === 0}
        emptyText="安装目录下没有可盘点的 EXE / DLL。"
        onRetry={() => void install.load('/api/v1/install')}
      >
        {#snippet actions()}
          <span class="tag">{count(install.data?.count ?? artifacts.length)} 个</span>
        {/snippet}
        <DataTable
          columns={artifactColumns}
          rows={artifacts}
          rowKey={(row) => row.path}
          sortKey="size"
        />
      </Panel>
    </div>
  {/snippet}
</Split>

<style>
  /* 两块主面板各自滚动，整页不出现纵向滚动条 */
  .stack {
    display: grid;
    grid-template-rows: minmax(0, 1fr) minmax(0, 1fr);
    gap: var(--sp-2);
    flex: 1;
    min-height: 0;
  }

  .aside-stack {
    display: grid;
    grid-template-rows: auto minmax(0, 1fr);
    gap: var(--sp-2);
    flex: 1;
    min-height: 0;
  }

  .stream-grid {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: 1px;
    overflow: hidden;
    border: 1px solid var(--line);
    border-radius: var(--radius);
    background: var(--line);
  }

  .stream-grid div {
    display: flex;
    min-width: 0;
    flex-direction: column;
    gap: 2px;
    padding: var(--sp-2);
    background: var(--bg-raised);
  }

  .stream-grid span,
  .stream-detail dt {
    font-size: 10px;
    color: var(--fg-mute);
  }

  .stream-grid strong,
  .stream-detail dd {
    font-family: var(--font-num);
    font-size: var(--fs-small);
    color: var(--fg);
  }

  .stream-detail {
    display: grid;
    gap: var(--sp-1);
    margin: var(--sp-3) 0 0;
  }

  .stream-detail div {
    display: grid;
    grid-template-columns: 64px minmax(0, 1fr);
    gap: var(--sp-2);
  }

  .stream-detail dt,
  .stream-detail dd {
    margin: 0;
  }

  .stream-detail dd {
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }

  .stream-error,
  .boundary {
    margin: var(--sp-3) 0 0;
    padding-top: var(--sp-2);
    border-top: 1px solid var(--line);
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  .stream-error {
    color: var(--down);
  }

  .tag {
    font-size: 10px;
    color: var(--fg-mute);
    font-family: var(--font-num);
  }
</style>
