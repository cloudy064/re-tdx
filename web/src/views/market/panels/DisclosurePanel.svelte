<script lang="ts">
  /** 财报披露日历的单票只读入口；归档写入不在浏览器工作台暴露。 */
  import { untrack } from 'svelte';
  import { queryString } from '../../../api';
  import { compact, count, date, percent, text } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import Segmented from '../../../ui/Segmented.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { DisclosureRecord, MarketDisclosureDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  type View = 'all' | 'schedule' | 'express' | 'recent' | 'announcement';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketDisclosureDocument>();
  let view = $state<View>('all');
  const mainland = $derived(['sz', 'sh', 'bj', '0', '1', '2', '44'].includes(
    market.trim().toLowerCase()
  ));
  const options = $derived([
    { id: 'all', label: '全部' },
    { id: 'schedule', label: '预约日' },
    { id: 'express', label: '业绩快报' },
    { id: 'recent', label: '近期披露' },
    ...(mainland ? [{ id: 'announcement', label: '正式报告' }] : [])
  ]);
  const rows = $derived(resource.data?.rows ?? []);
  const summary = $derived(resource.data?.summary);

  function load(refresh = false) {
    const announcement = view === 'announcement';
    void resource.load(`/api/v1/market/disclosures?${queryString({
      view,
      market,
      code,
      backfill_announcements: announcement ? 1 : 0,
      refresh: refresh ? 1 : 0,
      limit: 200
    })}`);
  }

  function switchView(next: string) {
    view = next as View;
    resource.reset();
    load();
  }

  function kindLabel(kind: string): string {
    if (kind === 'schedule') return '预约披露';
    if (kind === 'express') return '业绩快报';
    if (kind === 'recent') return '近期披露';
    if (kind === 'announcement-report') return '正式报告';
    return text(kind);
  }

  function statusLabel(status: string): string {
    if (status === 'scheduled') return '待披露';
    if (status === 'rescheduled') return '已变更';
    if (status === 'disclosed') return '已披露';
    return text(status);
  }

  function primaryDate(row: DisclosureRecord): string {
    return date(row.actual_disclosure_date || row.scheduled_disclosure_date);
  }

  function metric(row: DisclosureRecord): string {
    if (row.net_profit_yuan !== null && row.net_profit_yuan !== undefined)
      return compact(row.net_profit_yuan, '元');
    if (row.net_profit !== null && row.net_profit !== undefined)
      return compact(row.net_profit, row.currency || '');
    return text(row.selected_announcement?.title);
  }

  function metricNote(row: DisclosureRecord): string {
    if (row.net_profit_yoy_pct !== null && row.net_profit_yoy_pct !== undefined)
      return `同比 ${percent(row.net_profit_yoy_pct, 2, true)}`;
    if (row.change_dates?.length)
      return `原预约 ${row.change_dates.map(date).join(' → ')}`;
    return row.industry || row.report_type || '';
  }

  const columns: Column<DisclosureRecord>[] = [
    {
      key: 'date', label: '披露 / 预约日', width: '112px', num: true,
      value: primaryDate,
      sub: (row) => row.available_from ? `可用 ${date(row.available_from)}` : '',
      sortValue: (row) => row.actual_disclosure_date || row.scheduled_disclosure_date || ''
    },
    {
      key: 'period', label: '报告期', width: '106px', num: true,
      value: (row) => date(row.report_period),
      sub: (row) => text(row.report_type),
      sortValue: (row) => row.report_period
    },
    {
      key: 'kind', label: '资料类型', width: '104px',
      value: (row) => kindLabel(row.kind), sub: (row) => statusLabel(row.status)
    },
    {
      key: 'metric', label: '核心数据 / 公告', wrap: true,
      value: metric, sub: metricNote
    },
    {
      key: 'schedule', label: '首次预约', width: '112px', num: true,
      value: (row) => date(row.first_scheduled_date),
      sub: (row) => row.change_status || ''
    }
  ];

  const stats = $derived.by<Stat[]>(() => [
    { label: '当前返回', value: count(rows.length) },
    { label: '预约', value: count(summary?.by_kind.schedule) },
    { label: '快报', value: count(summary?.by_kind.express) },
    { label: '近期披露', value: count(summary?.by_kind.recent) },
    { label: '已变更', value: count(summary?.by_status.rescheduled) },
    { label: '已披露', value: count(summary?.by_status.disclosed) }
  ]);

  $effect(() => {
    void market; void code;
    untrack(() => load());
  });
</script>

<div class="stack">
  <Panel
    title={`${name} · 财报披露日历`}
    eyebrow="DISCLOSURE · READ ONLY"
    subtitle={view === 'announcement'
      ? '按交易所正式定期报告公告回补最近一年；摘要不会被当作完整报告。'
      : '预约、变更、实际披露日与快报分开呈现；实际披露日前不视为财务数据可用。'}
    busy={resource.busy}
    error={resource.error}
    onRetry={() => load()}
    empty={resource.loaded && !resource.busy && rows.length === 0}
    emptyText="当前公开披露窗口没有该证券的匹配记录。"
  >
    {#snippet toolbar()}
      <Segmented {options} value={view} onChange={switchView} ariaLabel="披露资料类型" />
      <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新上游</Button>
    {/snippet}
    {#if resource.data}<StatGrid {stats} columns={6} />{/if}
  </Panel>

  {#if rows.length}
    <Panel title="披露记录" subtitle={`${count(rows.length)} 条 · ${text(resource.data?.history_limit)}`} flush scroll fill>
      <DataTable {columns} {rows} rowKey={(row, index) => `${row.kind}:${row.report_period}:${row.actual_disclosure_date}:${index}`} sortKey="date" minWidth="760px" />
    </Panel>
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
