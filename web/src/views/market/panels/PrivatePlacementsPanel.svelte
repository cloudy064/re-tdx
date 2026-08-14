<script lang="ts">
  /** 个股定向增发：同一接口按 market+code 过滤六张生命周期表。 */
  import { queryString } from '../../../api';
  import { compact, count, date, fixed, percent, text } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { FuturesIssuanceDocument, PrivatePlacementRecord, RightsOfferingRecord } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<FuturesIssuanceDocument>();
  const rightsResource = new Resource<FuturesIssuanceDocument>();
  const rows = $derived(resource.data?.private_placements ?? []);
  const rightsRows = $derived(rightsResource.data?.rights_offerings ?? []);
  let selectedId = $state('');
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0]);
  let selectedRightsId = $state('');
  const selectedRights = $derived(rightsRows.find((row) => row.event_id === selectedRightsId) ?? rightsRows[0]);

  function load(refresh = false) {
    void resource.load(`/api/v1/market/futures-issuance?${queryString({
      section: 'placements', market, code, limit: 500,
      refresh: refresh ? 1 : 0
    })}`);
    void rightsResource.load(`/api/v1/market/futures-issuance?${queryString({
      section: 'rights', market, code, limit: 500,
      refresh: refresh ? 1 : 0
    })}`);
  }

  const stats = $derived.by<Stat[]>(() => [
    { label: '增发记录', value: count(rows.length), note: `${count(resource.data?.placement_summary.unique_securities)} 只股票` },
    { label: '实际募资', value: compact(Number(resource.data?.placement_summary.actual_gross_10k_yuan ?? 0) * 10000, '元') },
    { label: '预计募资', value: compact(Number(resource.data?.placement_summary.expected_raise_10k_yuan ?? 0) * 10000, '元') },
    { label: '当前阶段', value: text(selected?.stage), note: selected?.lifecycle_label ?? '' },
    { label: '发行价格', value: fixed(selected?.issue_price, 2) },
    { label: '发行股数', value: compact(Number(selected?.issue_shares_10k ?? 0) * 10000, '股') }
  ]);

  const columns: Column<PrivatePlacementRecord>[] = [
    { key: 'stage', label: '阶段', value: (row) => row.stage, sub: (row) => row.lifecycle_label },
    { key: 'date', label: '关键日期', num: true, value: (row) => date(row.sort_date), sortValue: (row) => row.sort_date },
    { key: 'price', label: '发行价', align: 'right', num: true, value: (row) => fixed(row.issue_price, 2), sortValue: (row) => Number(row.issue_price ?? 0) },
    { key: 'raised', label: '募资', align: 'right', num: true, value: (row) => compact(Number(row.actual_gross_10k_yuan ?? row.expected_raise_10k_yuan ?? 0) * 10000, '元'), sortValue: (row) => Number(row.actual_gross_10k_yuan ?? row.expected_raise_10k_yuan ?? 0) },
    { key: 'shares', label: '发行股数', align: 'right', num: true, value: (row) => compact(Number(row.issue_shares_10k ?? 0) * 10000, '股'), sortValue: (row) => Number(row.issue_shares_10k ?? 0) },
    { key: 'lock', label: '锁定期表现', align: 'right', num: true, value: (row) => percent(row.lock_period_return_pct, 2, true), sortValue: (row) => Number(row.lock_period_return_pct ?? 0) }
  ];

  const rightsColumns: Column<RightsOfferingRecord>[] = [
    { key: 'stage', label: '进度', value: (row) => row.stage, sub: (row) => row.phase_label },
    { key: 'date', label: '更新日期', num: true, value: (row) => date(row.sort_date), sub: (row) => `公告 ${date(row.announcement_date)}` },
    { key: 'ratio', label: '每10股配售', align: 'right', num: true, value: (row) => fixed(row.rights_per_10_shares, 2), sub: (row) => row.rights_code },
    { key: 'price', label: '配股价', align: 'right', num: true, value: (row) => fixed(row.rights_price_yuan, 2) },
    { key: 'shares', label: '配售股数', align: 'right', num: true, value: (row) => compact(row.offered_shares, '股') },
    { key: 'raised', label: '募资', align: 'right', num: true, value: (row) => compact(row.raised_yuan, '元'), sub: (row) => row.amount_semantics === 'actual' ? '实际' : '计划' }
  ];

  $effect(() => {
    void market;
    void code;
    selectedId = '';
    selectedRightsId = '';
    load();
  });
</script>

<Panel
  title={`${name} · 定向增发`}
  eyebrow="QXFA · PLACEMENTS"
  subtitle="方案推进、注册、实施、锁定和解锁分表保留，不把同票多期事件压成一条"
  busy={resource.busy}
  error={resource.error}
  onRetry={() => load()}
  empty={resource.loaded && !resource.busy && rows.length === 0}
  emptyText="该股不在当前定向增发生命周期表中"
  flush
  scroll
>
  {#snippet actions()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>强制更新</Button>{/snippet}
  {#if rows.length}
    <div class="pad"><StatGrid {stats} columns={3} /></div>
    <DataTable {columns} rows={rows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedId = row.event_id)} isActive={(row) => row.event_id === selected?.event_id} sortKey="date" minWidth="760px" maxHeight="310px" />
  {/if}
</Panel>

{#if selected}
  <Panel title="发行详情" eyebrow={selected.source_resource} subtitle={`${selected.stage} · ${selected.industry}`}>
    <div class="dates">
      董事会 {date(selected.dates.board_approved)} · 股东大会 {date(selected.dates.shareholders_approved)} ·
      注册/获准 {date(selected.dates.registered || selected.dates.regulator_approved)} ·
      实施 {date(selected.dates.implemented)} · 上市 {date(selected.dates.listed)} · 解锁 {date(selected.dates.unlock)}
    </div>
    {#if selected.change_notes}<p class="note">{selected.change_notes}</p>{/if}
    {#if selected.issue_details}<p class="copy">{selected.issue_details}</p>{/if}
    {#if selected.registration_announcement}
      <a href={selected.registration_announcement} target="_blank" rel="noreferrer">打开注册公告 PDF ↗</a>
    {/if}
  </Panel>
{/if}

<Panel title={`${name} · 配股募资`} eyebrow="QXFA107 / 108 / 109" subtitle="实施、审议中与异常进度分别保留" busy={rightsResource.busy} error={rightsResource.error} onRetry={() => load()} empty={rightsResource.loaded && !rightsResource.busy && rightsRows.length === 0} emptyText="该股不在当前配股生命周期表中" flush scroll>
  {#if rightsRows.length}
    <DataTable columns={rightsColumns} rows={rightsRows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedRightsId = row.event_id)} isActive={(row) => row.event_id === selectedRights?.event_id} sortKey="date" minWidth="760px" />
  {/if}
</Panel>

{#if selectedRights}
  <Panel title="配股详情" eyebrow={selectedRights.source_resource} subtitle={`${selectedRights.stage} · ${selectedRights.amount_semantics === 'actual' ? '实际口径' : '计划口径'}`}>
    <div class="dates">公告 {date(selectedRights.announcement_date)} · 股权登记 {date(selectedRights.registration_date)} · 缴款 {date(selectedRights.payment_start_date)} — {date(selectedRights.payment_end_date)} · 除权 {date(selectedRights.ex_rights_date)}</div>
    {#if selectedRights.major_shareholder_subscription}<p class="note">大股东认购：{selectedRights.major_shareholder_subscription}</p>{/if}
    {#if selectedRights.issue_details}<p class="copy">{selectedRights.issue_details}</p>{/if}
  </Panel>
{/if}

<style>
  .pad { padding: var(--sp-3); border-bottom: 1px solid var(--line); }
  .dates { color: var(--text-muted); font-size: var(--fs-xs); line-height: 1.7; }
  .copy, .note { white-space: pre-wrap; line-height: 1.72; font-size: var(--fs-sm); }
  .note { padding: var(--sp-3); border-radius: var(--radius-sm); background: var(--surface-raised); }
  a { color: var(--accent); font-size: var(--fs-sm); text-decoration: none; }
</style>
