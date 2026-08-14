<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { CalendarRecord, CalendarSecurity, MarketCalendarDocument } from '../../types';

  const VIEWS = [
    { id: 'macro', label: '宏观数据' }, { id: 'meetings', label: '热点会议' },
    { id: 'company', label: '公司事件' }, { id: 'major-events', label: '重大提醒' },
    { id: 'rights-issues', label: '配股日历' },
    { id: 'listings', label: '新股日历' }, { id: 'ipo-guidance', label: '上市辅导' }, { id: 'ipo-review', label: 'IPO审核' }, { id: 'ipo-subscriptions', label: '新股申购' },
    { id: 'ipo-subscription-details', label: '北证申购详情' }, { id: 'us-ipo', label: '美股IPO' }, { id: 'recent-ipos', label: '次新表现' },
    { id: 'ipo-announcements', label: '发行公告' }, { id: 'board-news', label: '板块资讯' },
    { id: 'futures', label: '期货日历' }
  ];
  let view = $state('macro');
  let query = $state('');
  let from = $state('');
  let to = $state('');
  let selected = $state<CalendarRecord | null>(null);
  const calendar = new Resource<MarketCalendarDocument>();
  const detail = new Resource<MarketCalendarDocument>();
  const doc = $derived(calendar.data);
  const rows = $derived(doc?.rows ?? []);
  const members = $derived(detail.data?.meeting_members ?? []);
  const selectedMembers = $derived((selected?.related_securities ?? []).map((security) => ({ event_id: selected?.event_id ?? '', security })));

  const stats = $derived.by(() => doc ? [
    { label: '重大提醒', value: count(doc.summary.major_events ?? 0) },
    { label: '公司事件', value: count(doc.summary.company_events ?? 0) },
    { label: '配股节点', value: count(doc.summary.rights_issue_events ?? 0) },
    { label: '上市辅导', value: count(doc.summary.ipo_guidance ?? 0), note: `关联证券 ${count(doc.summary.ipo_guidance_linked_securities ?? 0)}` },
    { label: 'IPO审核', value: count(doc.summary.ipo_reviews ?? 0), note: `关联证券 ${count(doc.summary.ipo_review_linked_securities ?? 0)}` },
    { label: '新股申购', value: count(doc.summary.ipo_subscriptions ?? 0) },
    { label: '北证申购详情', value: count(doc.summary.ipo_subscription_details ?? 0) },
    { label: '美股IPO', value: count((doc.summary.us_ipo_applications ?? 0) + (doc.summary.us_ipo_calendar ?? 0) + (doc.summary.us_ipo_listed ?? 0) + (doc.summary.us_ipo_pending ?? 0)), note: `排期→上市 ${count(doc.summary.us_ipo_calendar_listed_code_overlap ?? 0)} 只` },
    { label: '次新表现', value: count(doc.summary.recent_ipos ?? 0) },
    { label: '发行公告', value: count(doc.summary.ipo_announcements ?? 0) },
    { label: '板块资讯', value: count(doc.summary.board_news ?? 0) },
    { label: '期货日历', value: count(doc.summary.futures_calendar ?? 0) },
    { label: '当前返回', value: count(rows.length) },
    { label: '数据源', value: `${count(doc.sources.length)} 张`, note: doc.sources[0]?.endpoint }
  ] : []);

  function load(refresh = false) {
    void calendar.load(`/api/v1/market/calendar?${queryString({ view, q: query.trim(), from: from.trim(), to: to.trim(), limit: 5000, refresh: refresh ? 1 : 0 })}`);
    selected = null;
    detail.reset();
  }
  function open(row: CalendarRecord) {
    selected = row;
    if (row.kind === 'meeting' && row.event_id) {
      void detail.load(`/api/v1/market/calendar?${queryString({ view: 'meetings', event: row.event_id, limit: 100 })}`);
    } else detail.reset();
  }
  function openSecurity(security: CalendarSecurity) { router.go(stockPath(security.market, security.code)); }
  function titleSub(row: CalendarRecord) {
    if (row.security) return `${row.security.name} · ${row.security.security_id}${row.exchange ? ` · ${row.exchange}` : ''}`;
    return row.industry || row.region || '';
  }
  function issueValue(row: CalendarRecord) {
    if (row.offer_price_usd != null) return `$${fixed(row.offer_price_usd, 3)}`;
    if (row.price_text) return `$${row.price_text}`;
    return fixed(row.issue_price_yuan ?? row.issue_price, 3);
  }
  function issueSub(row: CalendarRecord) {
    if (row.issue_amount_usd != null) return compact(row.issue_amount_usd, '美元');
    if (row.raised_yuan != null) return compact(row.raised_yuan, '元');
    return row.subscription_code ? `申购 ${row.subscription_code}` : '';
  }
  function contentValue(row: CalendarRecord) {
    if (row.kind === 'ipo-subscription-detail') return `${row.pricing_method || '发行'} · 网上 ${compact(row.online_issue_shares, '股')} · 中签 ${percent(row.winning_rate_pct)}`;
    if (row.kind.startsWith('us-ipo-')) return `${row.bookrunner || row.exchange || '美股发行'} · ${compact(row.issue_shares, '股')}`;
    return text(row.content_excerpt || row.content);
  }

  const columns: Column<CalendarRecord>[] = [
    { key: 'date', label: '日期 / 时间', width: '122px', num: true, value: (row) => row.date.includes(' ') ? `${date(row.date)} ${row.date.split(' ')[1]}` : date(row.date), sortValue: (row) => row.date },
    { key: 'title', label: '事件', wrap: true, value: (row) => row.title, sub: titleSub },
    { key: 'type', label: '分类', width: '108px', value: (row) => text(row.event_type || row.kind) },
    { key: 'importance', label: '重要度', width: '62px', value: (row) => text(row.importance) },
    { key: 'previous', label: '前值', align: 'right', num: true, value: (row) => fixed(row.previous, 2) },
    { key: 'consensus', label: '预期', align: 'right', num: true, value: (row) => fixed(row.consensus, 2) },
    { key: 'actual', label: '公布', align: 'right', num: true, value: (row) => fixed(row.actual, 2), tone: (row) => tone((row.actual ?? 0) - (row.consensus ?? row.previous ?? 0)) },
    { key: 'issue', label: '发行 / 上市价', align: 'right', num: true, value: issueValue, sub: issueSub },
    { key: 'return', label: '上市涨幅', align: 'right', num: true, value: (row) => percent(row.listing_return_pct), sub: (row) => row.change_5d_pct == null ? '' : `5日 ${percent(row.change_5d_pct)} · 10日 ${percent(row.change_10d_pct)}`, tone: (row) => tone(row.listing_return_pct) },
    { key: 'content', label: '内容', wrap: true, value: contentValue }
  ];
  const memberColumns: Column<{ event_id: string; security: CalendarSecurity }>[] = [
    { key: 'security', label: '关联股票', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id }
  ];
  onMount(() => load());
</script>

<PageHeader eyebrow="CJRL · GSRL · XGRL · SBXG · MGRL · MGXG · QHRL" title="财经、公司与发行日历" description="宏观与公司事件、境内新股发行、北交所申购明细、美股 IPO 阶段及期货交易所提示统一时间轴。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={calendar.busy} onclick={() => load(true)}>刷新二十八张日历</Button>{/snippet}
</PageHeader>

<Split asideWidth="330px">
  {#snippet main()}
    <Panel title={VIEWS.find((item) => item.id === view)?.label ?? '日历'} subtitle={`${count(rows.length)} 条`} busy={calendar.busy} error={calendar.error} onRetry={() => load()} empty={calendar.loaded && !calendar.busy && rows.length === 0} flush scroll>
      {#snippet toolbar()}
        <Segmented options={VIEWS} value={view} ariaLabel="日历类型" onChange={(next) => { view = next; load(); }} />
        <TextInput bind:value={from} width="110px" label="起始" placeholder="20260801" onEnter={() => load()} />
        <TextInput bind:value={to} width="110px" label="截止" placeholder="20260831" onEnter={() => load()} />
        <TextInput bind:value={query} icon="search" width="220px" label="检索" placeholder="事件、行业、股票" onEnter={() => load()} />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row, index) => `${row.kind}:${row.date}:${row.event_id}:${row.security?.security_id}:${index}`} onRowClick={open} isActive={(row) => row === selected} stickyFirst />
    </Panel>
  {/snippet}
  {#snippet aside()}
    {#if selected?.kind === 'meeting'}
      <Panel title={selected.title} eyebrow="CJRL/<EVENT>.JSN" subtitle={`事件 ${selected.event_id} · ${count(members.length)} 只`} busy={detail.busy} error={detail.error} empty={!detail.busy && members.length === 0} emptyText="这场会议暂未关联股票。" flush scroll>
        <DataTable columns={memberColumns} rows={members} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} numbered />
      </Panel>
    {:else}
      <Panel title={selected?.title ?? '事件详情'} eyebrow={selected?.event_type || 'TDX CALENDAR'} subtitle={selected ? `${date(selected.date)} · ${selected.source_resource ?? '客户端日历'}` : '点击左侧记录查看'} empty={!selected} emptyText="选择一条记录，查看正文、来源和关联证券。" flush scroll>
        {#if selected}
          <div class="calendar-detail">
            {#if selected.content}<p>{selected.content}</p>{/if}
            {#if selected.source_url}<a href={selected.source_url} target="_blank" rel="noreferrer">打开原始公告 / 资讯来源 ↗</a>{/if}
          </div>
          <DataTable columns={memberColumns} rows={selectedMembers} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} numbered />
        {/if}
      </Panel>
    {/if}
  {/snippet}
</Split>

<style>
  .calendar-detail { padding: var(--sp-5); border-bottom: 1px solid var(--line); }
  .calendar-detail p { margin: 0 0 var(--sp-4); white-space: pre-wrap; line-height: 1.7; color: var(--fg-dim); }
  .calendar-detail a { color: var(--focus); text-decoration: none; }
</style>
