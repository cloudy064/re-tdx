<script lang="ts">
  /** 全市场上市公司路演；服务端分页，链接仅允许 http(s)。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type { RoadshowDocument, RoadshowRecord, RoadshowSecurity } from '../../types';

  const PAGE_LIMITS = [50, 100, 200, 500].map((value) => ({
    id: String(value), label: `${value} 条 / 页`
  }));

  let query = $state('');
  let type = $state('');
  let status = $state('');
  let start = $state('');
  let end = $state('');
  let limit = $state('100');
  let offset = $state(0);
  let knownTypes = $state<Array<{ name: string | null; count: number }>>([]);

  const resource = new Resource<RoadshowDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const typeOptions = $derived.by(() => [
    { id: '', label: '全部类型' },
    ...knownTypes
      .filter((item) => item.name)
      .map((item) => ({ id: item.name!, label: `${item.name} · ${count(item.count)}` }))
  ]);
  const stats = $derived.by<Stat[]>(() => [
    { label: '匹配路演', value: count(doc?.counts.matched) },
    { label: '上游记录', value: count(doc?.counts.upstream_rows) },
    { label: '活动类型', value: count(knownTypes.length) },
    { label: '当前来源', value: doc?.cache.stale ? '陈旧缓存' : doc?.cache.hit ? '内存缓存' : doc ? '在线请求' : '—' },
    { label: 'ReqId', value: '无' }
  ]);

  function safeUrl(value: string | null): string | null {
    if (!value) return null;
    try {
      const parsed = new URL(value);
      return parsed.protocol === 'http:' || parsed.protocol === 'https:' ? parsed.href : null;
    } catch {
      return null;
    }
  }

  function canOpenSecurity(security: RoadshowSecurity): boolean {
    return (security.market === 'sz' || security.market === 'sh' || security.market === 'bj') &&
      /^\d{6}$/.test(security.code);
  }

  function openSecurity(security: RoadshowSecurity) {
    if (!canOpenSecurity(security)) return;
    app.setStock({
      market: security.market,
      code: security.code,
      name: security.name || security.security_id
    });
    router.go(stockPath(security.market, security.code, 'roadshows'));
  }

  function load(reset = true, refresh = false) {
    if (reset) offset = 0;
    return resource.load(`/api/v1/market/roadshows?${queryString({
      q: query.trim(),
      type,
      status: status.trim(),
      start: start.trim(),
      end: end.trim(),
      offset,
      limit: Number(limit),
      refresh: refresh ? 1 : 0
    })}`);
  }

  function previous() {
    offset = Math.max(0, offset - Number(limit));
    void load(false);
  }

  function next() {
    if (!doc?.counts.has_more) return;
    offset += Number(limit);
    void load(false);
  }

  const columns: Column<RoadshowRecord>[] = [
    { key: 'security', label: '证券', width: '154px', slot: true },
    { key: 'date', label: '日期', width: '98px', num: true, value: (row) => date(row.start_date), sub: (row) => `${text(row.start_time)} — ${text(row.end_time)}` },
    { key: 'type', label: '活动类型', width: '116px', value: (row) => text(row.roadshow_type), sub: (row) => row.listing_status_code ? `状态 ${row.listing_status_code}` : '' },
    { key: 'title', label: '活动标题', width: '360px', wrap: true, slot: true },
    { key: 'summary', label: '摘要', width: '440px', wrap: true, value: (row) => text(row.summary) }
  ];

  $effect(() => {
    if (doc && !doc.filters.type) knownTypes = doc.types;
  });

  onMount(() => void load());
</script>

<PageHeader
  eyebrow="CWSearch.tzx_rcache · NO REQUEST ID"
  title="上市公司路演"
  description="汇总全市场业绩说明会、发布会、IPO/再融资与重大事项路演，并可进入单票历史；上游模板只有 entry 与 key，没有 ReqId。"
  {stats}
>
  {#snippet actions()}
    {#if doc?.availability === 'stale-cache'}
      <Badge tone="warn">陈旧缓存</Badge>
    {:else if doc}
      <Badge tone={doc.cache.hit ? 'focus' : 'up'}>{doc.cache.hit ? '缓存命中' : '在线'}</Badge>
    {/if}
    <Button icon="refresh" busy={resource.busy} onclick={() => void load(false, true)}>刷新上游</Button>
  {/snippet}
</PageHeader>

<Panel
  title="路演日历与历史"
  subtitle={doc ? `${count(doc.counts.returned)} / ${count(doc.counts.matched)} · ${doc.source.key} · 尝试 ${count(doc.source.attempts)} 次` : '市场主表按日期倒序；刷新失败时可回退陈旧缓存'}
  busy={resource.busy}
  error={resource.error}
  onRetry={() => void load(false)}
  empty={resource.loaded && rows.length === 0}
  emptyText="当前筛选没有路演活动。"
  flush scroll fill
>
  {#snippet toolbar()}
    <Select options={typeOptions} value={type} width="156px" label="类型" onChange={(value) => { type = value; void load(); }} />
    <TextInput bind:value={status} width="112px" label="上市状态码" placeholder="013001" onEnter={() => void load()} />
    <TextInput bind:value={start} width="116px" label="开始" placeholder="YYYYMMDD" onEnter={() => void load()} />
    <TextInput bind:value={end} width="116px" label="结束" placeholder="YYYYMMDD" onEnter={() => void load()} />
    <Select options={PAGE_LIMITS} value={limit} width="110px" label="分页" onChange={(value) => { limit = value; void load(); }} />
    <TextInput bind:value={query} icon="search" width="248px" label="检索" placeholder="证券、标题、类型或摘要" onEnter={() => void load()} />
    <Button icon="search" onclick={() => void load()}>查询</Button>
    <div class="pager">
      <Button icon="chevron-left" disabled={resource.busy || offset === 0} onclick={previous} />
      <span>第 {count(offset / Number(limit) + 1)} 页</span>
      <Button icon="chevron-right" disabled={resource.busy || !doc?.counts.has_more} onclick={next} />
    </div>
  {/snippet}
  <DataTable {columns} {rows} numbered stickyFirst minWidth="1160px" rowKey={(row) => row.event_id}>
    {#snippet cell({ row, column })}
      {#if column.key === 'security'}
        {#if canOpenSecurity(row.security)}
          <button class="security-link" type="button" onclick={() => openSecurity(row.security)}>
            <span>{row.security.name || row.security.code}</span>
            <small>{row.security.security_id}</small>
          </button>
        {:else}
          <span>{row.security.name || row.security.security_id}</span>
        {/if}
      {:else if column.key === 'title'}
        {#if safeUrl(row.url)}
          <a class="roadshow-link" href={safeUrl(row.url) ?? undefined} target="_blank" rel="noreferrer">{row.title}</a>
        {:else}
          <span>{row.title}</span>
        {/if}
      {/if}
    {/snippet}
  </DataTable>
</Panel>

<p class="boundary-note">
  默认允许复用服务端缓存，未命中时读取固定上游；“刷新上游”才强制重取。外链只接受 http(s)，标题与摘要始终按纯文本渲染；本页不提供任意 TQLEX entry、key、URL 或请求体输入。
</p>

<style>
  .pager { display: inline-flex; align-items: center; gap: var(--sp-2); margin-left: auto; }
  .pager span { min-width: 54px; font-family: var(--font-num); font-size: var(--fs-micro); color: var(--fg-mute); text-align: center; }
  .security-link { display: inline-flex; min-width: 0; flex-direction: column; align-items: flex-start; color: var(--focus); line-height: 1.2; text-align: left; }
  .security-link:hover span, .roadshow-link:hover { text-decoration: underline; }
  .security-link small { margin-top: 1px; font-family: var(--font-num); font-size: 9px; color: var(--fg-mute); }
  .roadshow-link { color: var(--focus); }
  .boundary-note { margin: var(--sp-3) var(--sp-1) 0; color: var(--fg-mute); font-size: var(--fs-small); line-height: 1.6; }
</style>
