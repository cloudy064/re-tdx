<script lang="ts">
  /** YYSG101：要约收购主表、当前进度与单票历史。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, money, percent, text } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type { MarketTenderOffersDocument, TenderOfferRecord } from '../../types';

  const STATUS = [
    { id: 'all', label: '全部进度' },
    { id: 'live', label: '准备 / 进行 / 暂停' },
    { id: 'preparing', label: '准备中' },
    { id: 'active', label: '进行中' },
    { id: 'paused', label: '暂停中' },
    { id: 'completed', label: '已完成' },
    { id: 'failed', label: '已失败' }
  ];

  let status = $state('all');
  let query = $state('');
  const resource = new Resource<MarketTenderOffersDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);

  const stats = $derived.by<Stat[]>(() => [
    { label: '要约事件', value: count(doc?.summary.rows), note: `${count(doc?.summary.unique_securities)} 只证券` },
    { label: '准备 / 进行 / 暂停', value: count(doc?.summary.live_rows), note: '按通达信最新转让进度' },
    { label: '完成 / 失败', value: `${count(doc?.summary.by_status.completed)} / ${count(doc?.summary.by_status.failed)}` },
    { label: '拟要约资金', value: money(doc?.summary.planned_funds_yuan), note: '当前筛选行合计' },
    { label: '拟 / 实际股数', value: `${compact(doc?.summary.planned_shares, '股')} / ${compact(doc?.summary.actual_shares, '股')}` },
    { label: '最新公告', value: date(doc?.summary.latest_announcement_date), note: doc?.availability === 'stale-cache' ? '缓存降级' : '在线 JSN' }
  ]);

  async function load(refresh = false) {
    await resource.load(`/api/v1/market/tender-offers?${queryString({
      status, q: query.trim(), sort: 'announcement-date', order: 'desc', limit: 1000,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function openSecurity(row: TenderOfferRecord) {
    router.go(stockPath(row.security.market, row.security.code, 'tender-offers'));
  }

  const columns: Column<TenderOfferRecord>[] = [
    { key: 'security', label: '证券', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'announcement', label: '最新公告', width: '92px', num: true, value: (row) => date(row.announcement_date), sortValue: (row) => row.announcement_date },
    { key: 'status', label: '进度', width: '82px', value: (row) => row.status },
    { key: 'acquirer', label: '收购人', width: '170px', wrap: true, value: (row) => text(row.acquirer) },
    { key: 'price', label: '要约价', width: '74px', align: 'right', num: true, value: (row) => fixed(row.offer_price, 2), sub: (row) => row.currency },
    { key: 'planned', label: '拟要约规模', width: '118px', align: 'right', num: true, value: (row) => compact(row.planned_shares, '股'), sub: (row) => percent(row.planned_total_pct), sortValue: (row) => row.planned_shares ?? 0 },
    { key: 'funds', label: '拟用资金', width: '112px', align: 'right', num: true, value: (row) => money(row.planned_funds_yuan), sortValue: (row) => row.planned_funds_yuan ?? 0 },
    { key: 'actual', label: '实际受让', width: '118px', align: 'right', num: true, value: (row) => compact(row.actual_shares, '股'), sub: (row) => row.actual_shares == null ? '尚无结果' : `${percent(row.actual_total_pct)} · 完成 ${percent(row.actual_to_planned_pct)}` },
    { key: 'window', label: '要约窗口', width: '176px', num: true, value: (row) => `${date(row.start_date)} — ${date(row.end_date)}` },
    { key: 'purpose', label: '要约目的', width: '300px', wrap: true, value: (row) => text(row.purpose) }
  ];

  onMount(() => void load());
</script>

<PageHeader eyebrow="19402 · YYSG101 · PUBLIC JSN" title="要约收购" description="把通达信要约收购表还原为可筛选事件：进度、价格、拟定与实际规模、期限、股份过户、退市标记和收购目的均保留原始口径。" {stats}>
  {#snippet actions()}
    <Button icon="refresh" busy={resource.busy} onclick={() => void load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Panel title="要约收购事件" subtitle="同一证券的多次历史要约逐条保留；点击任一行进入个股要约历史" busy={resource.busy} error={resource.error} onRetry={() => void load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选没有要约收购记录。" flush scroll fill>
  {#snippet toolbar()}
    <Select options={STATUS} bind:value={status} width="170px" label="进度" onChange={() => void load()} />
    <TextInput bind:value={query} icon="search" width="250px" label="检索" placeholder="证券、收购人、目的或状态" onEnter={() => void load()} />
    <Button icon="search" onclick={() => void load()}>查询</Button>
  {/snippet}
  <DataTable columns={columns} rows={rows} rowKey={(row) => row.offer_id} onRowClick={openSecurity} stickyFirst sortKey="announcement" minWidth="1450px" />
</Panel>
