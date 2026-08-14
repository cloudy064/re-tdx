<script lang="ts">
  /** BKLD 四分支：行业、概念、地区和风格板块的多周期异动轮动。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, fixed, percent, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, sectorPath } from '../../lib/router.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    BlockRotationCategory,
    BlockRotationCategorySummary,
    BlockRotationPeriod,
    BlockRotationPeriodMetrics,
    BlockRotationRecord,
    BlockRotationSignal,
    MarketBlockRotationDocument
  } from '../../types';

  const CATEGORIES = [
    { id: 'all', label: '全部' },
    { id: 'industry', label: '行业' },
    { id: 'concept', label: '概念' },
    { id: 'region', label: '地区' },
    { id: 'style', label: '风格' }
  ];
  const PERIODS = [
    { id: '1w', label: '近一周' },
    { id: '1m', label: '近一月' },
    { id: '3m', label: '近三月' },
    { id: '1y', label: '近一年' }
  ];
  const SIGNALS = [
    { id: 'all', label: '全部方向' },
    { id: 'up-dominant', label: '上涨主导' },
    { id: 'down-dominant', label: '下跌主导' },
    { id: 'balanced', label: '多空平衡' },
    { id: 'inactive', label: '周期内无异动' }
  ];
  const SORTS = [
    { id: 'last-date', label: '最近异动日' },
    { id: 'anomalies', label: '异动次数' },
    { id: 'return', label: '区间涨幅' },
    { id: 'imbalance', label: '方向差' },
    { id: 'cycle-gap', label: '周期缺口' },
    { id: 'average-cycle', label: '平均周期' },
    { id: 'days-since', label: '距今时间' },
    { id: 'up-anomalies', label: '上涨异动' },
    { id: 'down-anomalies', label: '下跌异动' }
  ];
  const ORDERS = [
    { id: 'desc', label: '降序' },
    { id: 'asc', label: '升序' }
  ];

  let category = $state<BlockRotationCategory>('all');
  let period = $state<BlockRotationPeriod>('1w');
  let signal = $state<BlockRotationSignal>('all');
  let sort = $state('last-date');
  let order = $state<'asc' | 'desc'>('desc');
  let query = $state('');

  const resource = new Resource<MarketBlockRotationDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const categoryRows = $derived(doc?.summary.by_category ?? []);
  const staleError = $derived(doc && resource.error ? resource.error : '');

  function categoryLabel(value: string): string {
    return CATEGORIES.find((item) => item.id === value)?.label ?? value;
  }

  function periodLabel(value: string): string {
    return PERIODS.find((item) => item.id === value)?.label ?? value;
  }

  function signalLabel(value: string): string {
    return SIGNALS.find((item) => item.id === value)?.label ?? value;
  }

  function selectedMetrics(row: BlockRotationRecord): BlockRotationPeriodMetrics {
    return row.periods[row.selected_period];
  }

  function canOpenMembers(row: BlockRotationRecord): boolean {
    return row.block.members_available &&
      row.block.local_family !== null &&
      ['industry', 'concept', 'style', 'index'].includes(row.block.local_family);
  }

  function openMembers(row: BlockRotationRecord) {
    if (!canOpenMembers(row) || !row.block.local_family) return;
    router.go(sectorPath(row.block.local_family, row.block.code));
  }

  const stats = $derived.by<Stat[]>(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return [
      { label: '板块', value: count(summary.blocks), note: `${count(summary.names_resolved)} 个名称已解析` },
      { label: `${periodLabel(summary.period)}异动`, value: count(summary.anomaly_count), note: `涨 ${count(summary.up_anomaly_count)} / 跌 ${count(summary.down_anomaly_count)}` },
      { label: '上涨主导', value: count(summary.up_dominant_blocks), tone: 'up' },
      { label: '下跌主导', value: count(summary.down_dominant_blocks), tone: 'down' },
      { label: '正 / 负收益', value: `${count(summary.positive_return_blocks)} / ${count(summary.negative_return_blocks)}` },
      { label: '最近异动', value: date(summary.latest_anomaly_date), note: doc?.availability === 'stale-cache' ? '陈旧缓存' : '在线源' }
    ];
  });

  const summaryColumns: Column<BlockRotationCategorySummary>[] = [
    { key: 'category', label: '分类', width: '90px', value: (row) => categoryLabel(row.category) },
    { key: 'blocks', label: '板块数', align: 'right', num: true, value: (row) => count(row.blocks) },
    { key: 'anomalies', label: '异动', align: 'right', num: true, value: (row) => count(row.anomaly_count) },
    { key: 'up-down', label: '涨 / 跌异动', align: 'right', num: true, value: (row) => `${count(row.up_anomaly_count)} / ${count(row.down_anomaly_count)}` },
    { key: 'dominant', label: '涨主导 / 跌主导', align: 'right', num: true, value: (row) => `${count(row.up_dominant_blocks)} / ${count(row.down_dominant_blocks)}` },
    { key: 'balanced', label: '平衡', align: 'right', num: true, value: (row) => count(row.balanced_blocks) },
    { key: 'inactive', label: '无异动', align: 'right', num: true, value: (row) => count(row.inactive_blocks) },
    { key: 'resolved', label: '名称解析', align: 'right', num: true, value: (row) => `${count(row.names_resolved)} / ${count(row.blocks)}` }
  ];

  const columns: Column<BlockRotationRecord>[] = [
    {
      key: 'block', label: '板块', width: '150px',
      value: (row) => row.block.name || row.block.code,
      sub: (row) => `${categoryLabel(row.block.category)} · ${row.block.code}`
    },
    {
      key: 'last-date', label: '最近异动', width: '96px', num: true,
      value: (row) => date(row.last_anomaly_date),
      sub: (row) => row.days_since_last_anomaly === null ? '距今天数未知' : `${count(row.days_since_last_anomaly)} 天前`
    },
    {
      key: 'anomalies', label: '区间异动', align: 'right', num: true,
      value: (row) => count(selectedMetrics(row).anomaly_count),
      sub: (row) => `涨 ${count(selectedMetrics(row).up_anomaly_count)} / 跌 ${count(selectedMetrics(row).down_anomaly_count)}`
    },
    {
      key: 'imbalance', label: '方向差', align: 'right', num: true,
      value: (row) => fixed(selectedMetrics(row).direction_imbalance, 0),
      tone: (row) => tone(selectedMetrics(row).direction_imbalance)
    },
    {
      key: 'return', label: '区间涨幅', align: 'right', num: true,
      value: (row) => percent(selectedMetrics(row).return_pct, 2, true),
      tone: (row) => tone(selectedMetrics(row).return_pct)
    },
    {
      key: 'cycle', label: '平均周期', align: 'right', num: true,
      value: (row) => row.average_cycle_days === null ? '—' : `${fixed(row.average_cycle_days, 2)} 天`,
      sub: (row) => row.cycle_gap_days === null ? '' : `周期缺口 ${fixed(row.cycle_gap_days, 2)} 天`
    },
    { key: 'signal', label: '轮动方向', width: '90px', align: 'center', slot: true },
    { key: 'members', label: '本地成分', width: '100px', align: 'center', slot: true }
  ];

  function load(refresh = false) {
    void resource.load(`/api/v1/market/block-rotation?${queryString({
      category,
      period,
      signal,
      sort,
      order,
      q: query.trim(),
      limit: 1000,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function switchCategory(value: string) {
    category = value as BlockRotationCategory;
    load();
  }

  function switchPeriod(value: string) {
    period = value as BlockRotationPeriod;
    load();
  }

  function switchSignal(value: string) {
    signal = value as BlockRotationSignal;
    load();
  }

  onMount(() => load());
</script>

<PageHeader
  eyebrow="BKLD101—104 · TQLEX 709/1721"
  title="板块轮动"
  description="通达信行业、概念、地区与风格板块的上次异动、平均周期、方向次数和区间涨幅；不补造当前行情列。"
  {stats}
>
  {#snippet actions()}
    {#if staleError}<Badge tone="warn">刷新失败</Badge>{:else if doc?.availability === 'stale-cache'}<Badge tone="warn">陈旧缓存</Badge>{:else if doc}<Badge tone="up">在线</Badge>{/if}
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

{#if categoryRows.length > 0}
  <Panel
    flush
    title={`${periodLabel(doc?.summary.period ?? period)}分类汇总`}
    subtitle="点击分类可直接筛选；地区板块没有本地成分时会保持不可跳转"
  >
    <DataTable
      columns={summaryColumns}
      rows={categoryRows}
      rowKey={(row) => row.category}
      onRowClick={(row) => switchCategory(row.category)}
      isActive={(row) => row.category === category}
    />
  </Panel>
{/if}

<Panel
  flush
  scroll
  fill
  title={`${categoryLabel(category)}板块 · ${periodLabel(period)}`}
  subtitle={staleError || (doc
    ? `${count(doc.counts.matched)} 个命中 · 返回 ${count(doc.counts.returned)} 个 · ${doc.cache.refreshed ? '已刷新上游' : `缓存 ${count(doc.cache.age_seconds)} 秒`}`
    : '服务端按所选周期排序')}
  busy={resource.busy}
  error={doc ? '' : resource.error}
  onRetry={() => load()}
  empty={resource.loaded && !resource.busy && rows.length === 0}
  emptyText="当前分类、周期、方向和检索条件下没有板块。"
>
  {#snippet toolbar()}
    <Segmented options={CATEGORIES} value={category} onChange={switchCategory} ariaLabel="板块分类" />
    <Segmented options={PERIODS} value={period} onChange={switchPeriod} ariaLabel="轮动周期" />
    <Select options={SIGNALS} value={signal} width="126px" label="方向" onChange={switchSignal} />
    <Select options={SORTS} value={sort} width="126px" label="服务端排序" onChange={(value) => { sort = value; load(); }} />
    <Select options={ORDERS} value={order} width="84px" label="顺序" onChange={(value) => { order = value as 'asc' | 'desc'; load(); }} />
    <TextInput bind:value={query} icon="search" width="220px" label="检索" placeholder="板块代码或名称" onEnter={() => load()} />
    <Button icon="search" onclick={() => load()}>查询</Button>
  {/snippet}

  <DataTable
    {columns}
    {rows}
    numbered
    stickyFirst
    minWidth="980px"
    rowKey={(row) => `${row.block.category}:${row.block.code}`}
  >
    {#snippet cell({ row, column })}
      {#if column.key === 'signal'}
        <Badge tone={row.selected_signal === 'up-dominant' ? 'up' : row.selected_signal === 'down-dominant' ? 'down' : row.selected_signal === 'inactive' ? 'neutral' : 'focus'}>
          {signalLabel(row.selected_signal)}
        </Badge>
      {:else if column.key === 'members'}
        <Button
          variant="ghost"
          icon="sectors"
          disabled={!canOpenMembers(row)}
          title={canOpenMembers(row) ? `查看 ${row.block.name} 的本地成分` : '该板块没有已验证的本地成分映射'}
          onclick={() => openMembers(row)}
        >
          {canOpenMembers(row) ? count(row.block.member_count) : '不可用'}
        </Button>
      {/if}
    {/snippet}
  </DataTable>
</Panel>
