<script lang="ts">
  /** 固定十类 GX/AQFPH 数据全景；一个 security 请求，不读取或展示 raw JSON。 */
  import { untrack } from 'svelte';
  import { queryString } from '../../../api';
  import { count, date, fixed, percent, text } from '../../../lib/fmt';
  import {
    PANORAMA_SECTIONS,
    type PanoramaFieldKind,
    type PanoramaSectionMeta
  } from '../../../lib/panorama';
  import { Resource } from '../../../lib/resource.svelte';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    MarketPanoramaDocument,
    PanoramaRecord,
    PanoramaSection,
    PanoramaValue
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketPanoramaDocument>();
  const doc = $derived(resource.data);
  const sections = $derived(new Map(
    (doc?.sections ?? []).map((section) => [section.view, section])
  ));
  const populated = $derived(PANORAMA_SECTIONS.filter(
    (meta) => (sections.get(meta.id)?.records.length ?? 0) > 0
  ).length);

  function load(refresh = false) {
    void resource.load(`/api/v1/market/panorama?${queryString({
      view: 'security',
      market,
      code,
      limit: 200,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function renderValue(value: PanoramaValue | undefined,
                       kind: PanoramaFieldKind = 'text'): string {
    if (kind === 'date') return date(value);
    if (kind === 'percent') return percent(value);
    if (kind === 'price') return fixed(value, 2);
    if (kind === 'number') return count(value);
    return text(value);
  }

  function sortValue(value: PanoramaValue | undefined): string | number {
    if (typeof value === 'number') return value;
    if (typeof value === 'string' && value.trim() !== '') {
      const parsed = Number(value);
      if (Number.isFinite(parsed)) return parsed;
      return value;
    }
    return '';
  }

  function columnsFor(meta: PanoramaSectionMeta): Column<PanoramaRecord>[] {
    return meta.fields.map((field) => {
      const numeric = field.kind === 'number' || field.kind === 'percent' ||
        field.kind === 'price';
      return {
        key: field.key,
        label: field.label,
        align: numeric ? 'right' : 'left',
        num: numeric || field.kind === 'date',
        wrap: field.kind === 'text',
        width: field.kind === 'text' ? '180px' : field.kind === 'date' ? '108px' : '132px',
        value: (row) => renderValue(row.data[field.key], field.kind),
        sortValue: (row) => sortValue(row.data[field.key])
      };
    });
  }

  function minimumWidth(meta: PanoramaSectionMeta): string {
    return `${Math.max(720, meta.fields.length * 136)}px`;
  }

  function sectionSubtitle(meta: PanoramaSectionMeta,
                           section: PanoramaSection | undefined): string {
    if (!section) return `${meta.description} · 响应未包含此 section`;
    return `${meta.description} · 命中 ${count(section.matched)} / 来源 ${count(section.source_rows)} 行`;
  }

  const stats = $derived.by<Stat[]>(() => [
    {
      label: '固定分类',
      value: `${count(doc?.counts.sections ?? PANORAMA_SECTIONS.length)} / ${count(PANORAMA_SECTIONS.length)}`,
      note: 'GX / AQFPH'
    },
    { label: '有数据分类', value: count(populated), note: `空 ${count(PANORAMA_SECTIONS.length - populated)} 类` },
    { label: '匹配记录', value: count(doc?.counts.matched) },
    { label: '实时来源', value: count(doc?.upstream_health.live_sources), note: `共 ${count(doc?.cache.resource_count)} 个资源` },
    {
      label: '上游状态',
      value: doc?.upstream_health.stale ? '含陈旧缓存' : '当前可用',
      tone: doc?.upstream_health.stale ? 'down' : 'up'
    },
    {
      label: '缓存',
      value: doc?.cache.refreshed ? '本次刷新' : '命中缓存',
      note: `${count(doc?.cache.ttl_seconds)} 秒 TTL`
    }
  ]);

  $effect(() => {
    void market; void code;
    untrack(() => load());
  });
</script>

<div class="stack">
  <Panel
    title={`${name} · 数据全景`}
    eyebrow="GX / AQFPH · 10 SECTIONS · READ ONLY"
    subtitle="一次请求汇总十类通达信静态个性数据；只展示类型化投影，不重复原始 JSON。"
    busy={resource.busy}
    error={resource.error}
    onRetry={() => load()}
  >
    {#snippet actions()}
      {#if doc}
        <Badge tone={doc.upstream_health.stale ? 'warn' : 'up'}>
          {doc.upstream_health.stale ? '部分缓存' : '来源正常'}
        </Badge>
      {/if}
    {/snippet}
    {#snippet toolbar()}
      <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新十类数据</Button>
    {/snippet}
    {#if doc}<StatGrid {stats} columns={6} />{/if}
  </Panel>

  {#if doc}
    {#each PANORAMA_SECTIONS as meta (meta.id)}
      {@const section = sections.get(meta.id)}
      {@const records = section?.records ?? []}
      <Panel
        title={meta.label}
        eyebrow={`${meta.resource} · ${count(records.length)} RECORDS`}
        subtitle={sectionSubtitle(meta, section)}
        empty={records.length === 0}
        emptyText={section
          ? '该证券在当前全市场快照中未命中此类数据；其他分类仍可正常查看。'
          : '本次响应缺少此固定分类；已返回的其他分类仍可正常查看。'}
        flush
      >
        <DataTable
          columns={columnsFor(meta)}
          rows={records}
          rowKey={(row, index) => `${row.security_id}:${meta.id}:${index}`}
          sortKey={meta.fields[0]?.key ?? ''}
          minWidth={minimumWidth(meta)}
          maxHeight={records.length > 6 ? '320px' : undefined}
        />
      </Panel>
    {/each}
  {/if}
</div>

<style>
  .stack {
    display: flex;
    min-height: 0;
    flex: 1;
    flex-direction: column;
    gap: var(--sp-2);
    overflow: auto;
  }
</style>
