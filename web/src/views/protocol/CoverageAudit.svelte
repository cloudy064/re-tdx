<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import type {
    CloudVariantCoverageDocument,
    CloudVariantCoverageRecord,
    JsnVariantCoverageDocument,
    JsnVariantCoverageResource,
    JsnVariantGapFamily
  } from '../../types';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import TextInput from '../../ui/TextInput.svelte';

  const cloud = new Resource<CloudVariantCoverageDocument>();
  const jsn = new Resource<JsnVariantCoverageDocument>();
  let cloudGapsOnly = $state(false);
  let jsnGapsOnly = $state(true);
  let top = $state('50');
  let validationError = $state('');

  const cloudRows = $derived(cloud.data?.variants ?? []);
  const jsnRows = $derived(jsn.data?.resources ?? []);
  const familyRows = $derived(jsn.data?.gap_families ?? []);
  const stats = $derived([
    {
      label: '云语义变体',
      value: count(cloud.data?.summary.semantic_variant_count ?? 0),
      note: `${count(cloud.data?.summary.fixed_variant_count ?? 0)} 个固定命令覆盖`
    },
    {
      label: '云协议缺口',
      value: count(cloud.data?.summary.generic_only_variant_count ?? 0),
      note: `${count(cloud.data?.summary.parse_error_count ?? 0)} 个解析错误`
    },
    {
      label: 'JSN 模板',
      value: count(jsn.data?.summary.resource_template_count ?? 0),
      note: `${count(jsn.data?.summary.typed_template_count ?? 0)} 个类型化覆盖`
    },
    {
      label: 'JSN 类型缺口',
      value: count(jsn.data?.summary.generic_only_template_count ?? 0),
      note: `${count(jsn.data?.summary.generic_downloaded_template_count ?? 0)} 个已有本地数据`
    }
  ]);

  const cloudColumns: Column<CloudVariantCoverageRecord>[] = [
    {
      key: 'route', label: '协议变体', width: '245px', num: true,
      value: (row) => row.request_id ?? row.entry,
      sub: (row) => `${row.transport.toUpperCase()} · ${row.entry}${row.module ? ` · ${row.module}` : ''}`
    },
    {
      key: 'coverage', label: '类型化覆盖', width: '220px', wrap: true,
      value: (row) => row.fixed_command ?? '仅通用协议能力',
      sub: (row) => row.coverage
    },
    {
      key: 'shape', label: '请求形状', width: '240px', wrap: true,
      value: (row) => row.request_fields.join(' · ') || '无字段',
      sub: (row) => Object.entries(row.selectors).map(([key, value]) => `${key}=${value}`).join(' · ') || row.selection_key
    },
    {
      key: 'source', label: '模板来源', width: '210px', wrap: true,
      value: (row) => row.source_files.join(' · '),
      sub: (row) => `${count(row.template_occurrences)} 次 · ${count(row.body_size)} B`
    },
    {
      key: 'error', label: '解析状态', wrap: true,
      value: (row) => row.parse_error ?? '正常'
    }
  ];

  const jsnColumns: Column<JsnVariantCoverageResource>[] = [
    {
      key: 'resource', label: '资源模板', width: '275px', wrap: true,
      value: (row) => row.resource,
      sub: (row) => `${row.family} · ${row.dynamic ? '动态模板' : '静态模板'}`
    },
    {
      key: 'coverage', label: '类型化命令', width: '230px', wrap: true,
      value: (row) => row.typed_commands.join(' · ') || '仅通用 JSN 能力',
      sub: (row) => row.api_endpoints.join(' · ') || row.coverage
    },
    {
      key: 'downloaded', label: '本地样本', width: '190px', num: true,
      value: (row) => `${count(row.downloaded.matched_files)} 文件 · ${count(row.downloaded.rows)} 行`,
      sub: (row) => `${count(row.downloaded.bytes)} B · ${count(row.downloaded.groups)} 组`
    },
    {
      key: 'shape', label: '选择与列', width: '260px', wrap: true,
      value: (row) => row.placeholders.join(' · ') || row.selection_key,
      sub: (row) => row.downloaded.columns.slice(0, 8).join(' · ') || '尚无可见列'
    },
    {
      key: 'gap', label: '缺口优先级', width: '120px', num: true,
      value: (row) => row.gap_score === null ? '已覆盖' : String(row.gap_score),
      sub: (row) => row.downloaded.security_related ? '证券相关' : ''
    }
  ];

  const familyColumns: Column<JsnVariantGapFamily>[] = [
    { key: 'family', label: '缺口族', width: '160px', value: (row) => row.family },
    {
      key: 'templates', label: '模板', width: '130px', num: true,
      value: (row) => count(row.resource_templates),
      sub: (row) => `${count(row.downloaded_templates)} 个已有样本`
    },
    {
      key: 'local', label: '本地数据', width: '170px', num: true,
      value: (row) => `${count(row.matched_files)} 文件 · ${count(row.rows)} 行`,
      sub: (row) => `${count(row.bytes)} B`
    },
    {
      key: 'samples', label: '代表资源', wrap: true,
      value: (row) => row.samples.join(' · ')
    }
  ];

  function loadCloud() {
    void cloud.load(`/api/v1/cloud/variants?${queryString({ gaps_only: cloudGapsOnly ? 1 : 0 })}`);
  }

  function loadJsn() {
    validationError = '';
    const parsedTop = Number(top);
    if (!Number.isInteger(parsedTop) || parsedTop < 0 || parsedTop > 1000) {
      validationError = '高价值缺口数量必须是 0..1000 的整数';
      return;
    }
    void jsn.load(`/api/v1/jsn/variants?${queryString({
      gaps_only: jsnGapsOnly ? 1 : 0,
      top: parsedTop
    })}`);
  }

  function refreshAll() {
    loadCloud();
    loadJsn();
  }

  onMount(refreshAll);
</script>

<PageHeader
  eyebrow="TQLEX / PBRPC / JSN · LOCAL COVERAGE"
  title="协议覆盖审计"
  description="把模板、语义变体、本地样本与类型化 C++ 命令放在同一张只读审计面上；通用查询或下载能力不会被计作业务能力已补齐。"
  {stats}
>
  {#snippet actions()}
    <Badge tone={cloud.data?.summary.fully_fixed ? 'up' : 'warn'}>云协议 {cloud.data?.summary.fully_fixed ? '已覆盖' : '仍有缺口'}</Badge>
    <Badge tone={jsn.data?.summary.fully_fixed ? 'up' : 'warn'}>JSN {jsn.data?.summary.fully_fixed ? '已覆盖' : '仍有缺口'}</Badge>
    <Button icon="refresh" busy={cloud.busy || jsn.busy} onclick={refreshAll}>重新审计</Button>
  {/snippet}
</PageHeader>

<div class="coverage-layout">
  <Panel
    title="云模板语义变体"
    subtitle={cloud.data?.semantics ?? 'TQLEX / PBRPC 模板去重与固定命令覆盖'}
    busy={cloud.busy}
    error={cloud.error}
    onRetry={loadCloud}
    empty={cloud.loaded && cloudRows.length === 0}
    emptyText={cloudGapsOnly ? '当前没有 generic-only 云变体。' : '没有发现云模板。'}
    flush
    scroll
  >
    {#snippet toolbar()}
      <label class="check"><input type="checkbox" bind:checked={cloudGapsOnly} />仅显示缺口</label>
      <Button icon="search" busy={cloud.busy} onclick={loadCloud}>应用</Button>
      <span class="summary-line">
        模板 {count(cloud.data?.summary.template_count ?? 0)} · 去重变体 {count(cloud.data?.summary.semantic_variant_count ?? 0)} · 重复模板 {count(cloud.data?.summary.duplicate_template_count ?? 0)}
      </span>
    {/snippet}
    <DataTable columns={cloudColumns} rows={cloudRows} numbered rowKey={(row) => `${row.transport}|${row.variant_id}`} minWidth="1135px" />
  </Panel>

  <Panel
    title="JSN 资源覆盖"
    subtitle={jsn.data?.semantics ?? '静态/动态资源、本地样本与类型化命令覆盖'}
    busy={jsn.busy}
    error={jsn.error}
    onRetry={loadJsn}
    empty={jsn.loaded && jsnRows.length === 0}
    emptyText={jsnGapsOnly ? '当前没有 generic-only JSN 模板。' : '没有发现 JSN 模板。'}
    flush
    scroll
  >
    {#snippet toolbar()}
      <label class="check"><input type="checkbox" bind:checked={jsnGapsOnly} />仅显示缺口</label>
      <TextInput bind:value={top} label="高价值缺口" width="130px" onEnter={loadJsn} />
      <Button icon="search" busy={jsn.busy} onclick={loadJsn}>应用</Button>
      <span class="summary-line">
        本地文件 {count(jsn.data?.summary.downloaded_file_count ?? 0)} · 已匹配 {count(jsn.data?.summary.matched_downloaded_file_count ?? 0)} · 解析错误 {count(jsn.data?.summary.parse_error_template_count ?? 0)}
      </span>
    {/snippet}
    {#if validationError}<p class="validation-error">{validationError}</p>{/if}
    <DataTable columns={jsnColumns} rows={jsnRows} numbered rowKey={(row) => row.resource} minWidth="1145px" />
  </Panel>

  <Panel
    title="JSN 缺口族"
    subtitle={`${count(familyRows.length)} 个族 · ${count(jsn.data?.high_value_gaps.length ?? 0)} 个高价值候选`}
    empty={jsn.loaded && familyRows.length === 0}
    emptyText="当前没有 JSN 覆盖缺口族。"
    flush
  >
    <DataTable columns={familyColumns} rows={familyRows} rowKey={(row) => row.family} minWidth="760px" />
  </Panel>

  <p class="boundary">
    本页只读取安装目录模板与已配置的本地 JSN 下载目录，不执行云请求、不下载文件、不写基线，也不触碰账户、交易或 Level2 授权。缺口分数只是本地实现优先级，不代表协议可用性或数据授权。
  </p>
</div>

<style>
  .coverage-layout { display: flex; flex: 1; flex-direction: column; gap: var(--sp-3); min-height: 0; }
  .check { display: inline-flex; align-items: center; gap: var(--sp-1); color: var(--fg-dim); font-size: var(--fs-micro); white-space: nowrap; }
  .check input { accent-color: var(--focus); }
  .summary-line { color: var(--fg-mute); font-size: var(--fs-micro); }
  .validation-error { margin: 0; padding: var(--sp-2) var(--sp-4); color: var(--negative); font-size: var(--fs-micro); border-bottom: 1px solid var(--line); }
  .boundary { margin: 0; padding: var(--sp-2) var(--sp-4) var(--sp-3); color: var(--fg-mute); font-size: var(--fs-micro); line-height: 1.6; }
</style>
