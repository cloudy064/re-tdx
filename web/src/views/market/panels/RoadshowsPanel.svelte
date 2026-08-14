<script lang="ts">
  /** 当前证券路演历史；使用无 ReqId 的固定 TQLEX key。 */
  import { queryString } from '../../../api';
  import { count, date, text } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { RoadshowDocument, RoadshowRecord } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<RoadshowDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);

  function safeUrl(value: string | null): string | null {
    if (!value) return null;
    try {
      const parsed = new URL(value);
      return parsed.protocol === 'http:' || parsed.protocol === 'https:' ? parsed.href : null;
    } catch {
      return null;
    }
  }

  function load(refresh = false) {
    void resource.load(`/api/v1/market/roadshows?${queryString({
      market,
      code,
      offset: 0,
      limit: 1000,
      refresh: refresh ? 1 : 0
    })}`);
  }

  const columns: Column<RoadshowRecord>[] = [
    { key: 'date', label: '日期', width: '100px', num: true, value: (row) => date(row.start_date), sub: (row) => `${text(row.start_time)} — ${text(row.end_time)}` },
    { key: 'type', label: '活动类型', width: '116px', value: (row) => text(row.roadshow_type) },
    { key: 'title', label: '标题', width: '360px', wrap: true, slot: true },
    { key: 'summary', label: '摘要', width: '460px', wrap: true, value: (row) => text(row.summary) }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  eyebrow="CWSearch.tzx_rcache · ly:market_code · NO REQID"
  title={`${name} · 路演历史`}
  subtitle={doc ? `${count(doc.counts.matched)} 条 · ${count(doc.types.length)} 类 · ${doc.cache.hit ? '缓存命中' : '在线请求'}` : '业绩说明会、发布会及其他路演；刷新失败可回退陈旧缓存'}
  busy={resource.busy}
  error={resource.error}
  onRetry={() => load()}
  empty={resource.loaded && rows.length === 0}
  emptyText="当前证券没有路演历史。"
  flush scroll fill
>
  {#snippet actions()}
    {#if doc?.availability === 'stale-cache'}<Badge tone="warn">陈旧缓存</Badge>{/if}
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新上游</Button>
  {/snippet}
  <DataTable {columns} {rows} numbered stickyFirst minWidth="1040px" rowKey={(row) => row.event_id}>
    {#snippet cell({ row })}
      {#if safeUrl(row.url)}
        <a class="roadshow-link" href={safeUrl(row.url) ?? undefined} target="_blank" rel="noreferrer">{row.title}</a>
      {:else}
        <span>{row.title}</span>
      {/if}
    {/snippet}
  </DataTable>
</Panel>

<p class="boundary-note">
  活动标题和摘要按纯文本显示，外链只接受 http(s)；固定路由不允许调用方替换 TQLEX entry、key 或请求体。
</p>

<style>
  .roadshow-link { color: var(--focus); }
  .roadshow-link:hover { text-decoration: underline; }
  .boundary-note { margin: var(--sp-3) var(--sp-1) 0; color: var(--fg-mute); font-size: var(--fs-small); line-height: 1.6; }
</style>
