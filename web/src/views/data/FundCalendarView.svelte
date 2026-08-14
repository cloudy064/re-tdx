<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { FundCalendarRecord, MarketFundCalendarDocument } from '../../types';

  let query = $state('');
  let eventType = $state('');
  let category = $state('');
  let order = $state<'asc' | 'desc'>('asc');
  let selectedId = $state('');
  const resource = new Resource<MarketFundCalendarDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived.by<Stat[]>(() => doc ? [
    { label: '基金事件', value: count(doc.match_count) },
    { label: '涉及基金', value: count(doc.summary.unique_funds) },
    { label: '事件类型', value: count(doc.summary.event_type_count) },
    { label: '基金类别', value: count(doc.summary.category_count) },
    { label: '日期范围', value: `${date(doc.summary.earliest_date)} 至 ${date(doc.summary.latest_date)}` }
  ] : []);
  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/fund-calendar?${queryString({
      q: query.trim(), event_type: eventType.trim(), category: category.trim(),
      order, limit: 5000, include_raw: 0, refresh: refresh ? 1 : 0
    })}`);
  }
  function toggleOrder() { order = order === 'asc' ? 'desc' : 'asc'; load(); }
  function name(row: FundCalendarRecord) { return row.security.name || row.security.code; }
  const columns: Column<FundCalendarRecord>[] = [
    { key: 'date', label: '日期', width: '115px', num: true, value: (row) => date(row.event_date) },
    { key: 'fund', label: '基金', width: '240px', value: name, sub: (row) => row.security.security_id },
    { key: 'event', label: '事件', width: '180px', value: (row) => row.event_type, sub: (row) => row.fund_category },
    { key: 'content', label: '事件内容', width: '500px', value: (row) => row.content }
  ];
  const detailStats = $derived.by<Stat[]>(() => selected ? [
    { label: '日期', value: date(selected.event_date) },
    { label: '事件', value: selected.event_type },
    { label: '类别', value: selected.fund_category },
    { label: '基金代码', value: selected.security.security_id }
  ] : []);
  onMount(() => load());
</script>

<PageHeader eyebrow="JJRL · FUND EVENTS" title="基金事件日历" description="覆盖场内与场外基金的开放、分红等事件日期、内容和基金类别，可按基金、事件或类别检索。" {stats}>
  {#snippet actions()}
    <Button onclick={toggleOrder}>{order === 'asc' ? '日期升序 ↑' : '日期降序 ↓'}</Button>
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>
<Split asideWidth="390px">
  {#snippet main()}
    <Panel title="事件日历" subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.summary.unique_funds)} 只基金` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前条件下没有基金事件。" flush scroll>
      {#snippet toolbar()}
        <TextInput bind:value={query} icon="search" width="210px" label="检索基金或内容" placeholder="代码、名称、内容" onEnter={() => load()} />
        <TextInput bind:value={eventType} width="170px" label="事件类型" placeholder="如 基金分红" onEnter={() => load()} />
        <TextInput bind:value={category} width="150px" label="基金类别" placeholder="如 FOF" onEnter={() => load()} />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedId = row.event_id)} isActive={(row) => row.event_id === selected?.event_id} stickyFirst numbered minWidth="1080px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.fund_category ?? 'FUND CALENDAR'} title={selected ? name(selected) : '事件详情'} subtitle={selected ? `${selected.security.security_id} · ${date(selected.event_date)}` : '选择左侧事件'} empty={!selected && !resource.busy}>
      {#if selected}
        <StatGrid stats={detailStats} inline />
        <p>{selected.content}</p>
      {/if}
    </Panel>
  {/snippet}
</Split>
<style>
  p { margin-top: var(--sp-3); font-size: var(--fs-sm); line-height: 1.8; color: var(--fg-mute); white-space: pre-wrap; overflow-wrap: anywhere; }
</style>
