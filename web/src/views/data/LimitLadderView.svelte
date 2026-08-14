<script lang="ts">
  /** LBTT101：行业与概念板块的连板天梯及晋级率交叉验证。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, percent } from '../../lib/fmt';
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
    LimitLadderActivity,
    LimitLadderCategory,
    LimitLadderRecord,
    LimitLadderSort,
    LimitLadderSummary,
    MarketLimitLadderDocument
  } from '../../types';

  const CATEGORIES = [
    { id: 'all', label: '全部' },
    { id: 'industry', label: '行业' },
    { id: 'concept', label: '概念' }
  ];
  const ACTIVITIES = [
    { id: 'all', label: '全部状态' },
    { id: 'sealed', label: '有封板' },
    { id: 'broken', label: '有炸板' },
    { id: 'consecutive', label: '有连板' },
    { id: 'advanced', label: '晋级率为正' },
    { id: 'inactive', label: '封板与炸板均无' }
  ];
  const SORTS = [
    { id: 'total-height', label: '连板总高度' },
    { id: 'max-height', label: '最高连板' },
    { id: 'advancement-rate', label: '晋级率' },
    { id: 'consecutive', label: '连板家数' },
    { id: 'sealed', label: '封板家数' },
    { id: 'broken', label: '炸板家数' },
    { id: 'prior-limit', label: '昨日涨停' },
    { id: 'date', label: '统计日期' }
  ];
  const ORDERS = [
    { id: 'desc', label: '降序' },
    { id: 'asc', label: '升序' }
  ];
  const PAGE_SIZE = 200;

  let category = $state<LimitLadderCategory>('all');
  let activity = $state<LimitLadderActivity>('all');
  let sort = $state<LimitLadderSort>('total-height');
  let order = $state<'asc' | 'desc'>('desc');
  let query = $state('');
  let offset = $state(0);

  const resource = new Resource<MarketLimitLadderDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const categoryRows = $derived(doc?.summary.by_category ?? []);
  const staleError = $derived(doc && resource.error ? resource.error : '');
  const canPrevious = $derived(offset > 0 && !resource.busy);
  const canNext = $derived(Boolean(doc && offset + doc.counts.returned < doc.counts.matched) && !resource.busy);

  function categoryLabel(value: string): string {
    return CATEGORIES.find((item) => item.id === value)?.label ?? value;
  }

  function canOpenMembers(row: LimitLadderRecord): boolean {
    return row.block.members_available &&
      row.block.block_id !== null &&
      row.block.local_family !== null &&
      ['industry', 'research-industry', 'concept', 'style', 'index'].includes(row.block.local_family) &&
      /^\d{6}$/.test(row.block.code);
  }

  function openMembers(row: LimitLadderRecord) {
    if (!canOpenMembers(row) || !row.block.local_family) return;
    router.go(sectorPath(row.block.local_family, row.block.code));
  }

  const stats = $derived.by<Stat[]>(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return [
      { label: '板块', value: count(summary.blocks), note: `${count(summary.names_resolved)} 个名称已解析` },
      { label: '封板 / 炸板', value: `${count(summary.sealed_limit_up_count)} / ${count(summary.broken_board_count)}` },
      { label: '连板板块', value: count(summary.consecutive_blocks), tone: 'up' },
      { label: '最高 / 总高度', value: `${count(summary.max_streak_height)} / ${count(summary.sum_streak_heights)}` },
      { label: '加权晋级率', value: percent(summary.weighted_advancement_rate_pct) },
      { label: '统计日期', value: date(summary.latest_date), note: doc?.availability === 'stale-cache' ? '陈旧缓存' : '在线源' }
    ];
  });

  const summaryColumns: Column<LimitLadderSummary>[] = [
    { key: 'category', label: '分类', width: '90px', value: (row) => categoryLabel(row.category) },
    { key: 'blocks', label: '板块', align: 'right', num: true, value: (row) => count(row.blocks) },
    { key: 'sealed', label: '封板板块 / 家数', align: 'right', num: true, value: (row) => `${count(row.sealed_blocks)} / ${count(row.sealed_limit_up_count)}` },
    { key: 'broken', label: '炸板板块 / 家数', align: 'right', num: true, value: (row) => `${count(row.broken_blocks)} / ${count(row.broken_board_count)}` },
    { key: 'consecutive', label: '连板板块 / 家数', align: 'right', num: true, value: (row) => `${count(row.consecutive_blocks)} / ${count(row.consecutive_limit_up_count)}` },
    { key: 'height', label: '最高 / 总高度', align: 'right', num: true, value: (row) => `${count(row.max_streak_height)} / ${count(row.sum_streak_heights)}` },
    { key: 'rate', label: '加权晋级率', align: 'right', num: true, value: (row) => percent(row.weighted_advancement_rate_pct) },
    { key: 'inactive', label: '静默板块', align: 'right', num: true, value: (row) => count(row.inactive_blocks) }
  ];

  const columns: Column<LimitLadderRecord>[] = [
    {
      key: 'block', label: '板块', width: '160px',
      value: (row) => row.block.name || row.block.code,
      sub: (row) => `${categoryLabel(row.block.category)} · ${row.block.code}`
    },
    { key: 'date', label: '统计日', width: '92px', num: true, value: (row) => date(row.date) },
    { key: 'sealed', label: '封板', align: 'right', num: true, value: (row) => count(row.sealed_limit_up_count) },
    { key: 'broken', label: '炸板', align: 'right', num: true, value: (row) => count(row.broken_board_count) },
    { key: 'prior', label: '昨日涨停', align: 'right', num: true, value: (row) => count(row.prior_limit_up_count) },
    { key: 'consecutive', label: '连板家数', align: 'right', num: true, value: (row) => count(row.consecutive_limit_up_count) },
    { key: 'height', label: '最高 / 总高度', align: 'right', num: true, value: (row) => `${count(row.max_streak_height)} / ${count(row.sum_streak_heights)}` },
    {
      key: 'rate', label: '晋级率', align: 'right', num: true,
      value: (row) => percent(row.advancement_rate_pct),
      sub: (row) => row.calculated_advancement_rate_pct === null ? '' : `复算 ${percent(row.calculated_advancement_rate_pct)}`
    },
    { key: 'formula', label: '公式校验', width: '88px', align: 'center', slot: true },
    { key: 'members', label: '本地成分', width: '100px', align: 'center', slot: true }
  ];

  function load(refresh = false) {
    void resource.load(`/api/v1/market/limit-ladder?${queryString({
      category,
      activity,
      sort,
      order,
      q: query.trim(),
      offset,
      limit: PAGE_SIZE,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function runQuery() {
    offset = 0;
    resource.reset();
    load();
  }

  function goPage(nextOffset: number) {
    offset = Math.max(0, nextOffset);
    resource.reset();
    load();
  }

  function switchCategory(value: string) {
    category = value as LimitLadderCategory;
    runQuery();
  }

  onMount(() => load());
</script>

<PageHeader
  eyebrow="LBTT101 · TQLEX JSN"
  title="连板天梯"
  description="研究行业与概念板块的封板、炸板、连板高度及晋级率；晋级率同时展示上游值和本地复算，不补造实时行情列。"
  {stats}
>
  {#snippet actions()}
    {#if staleError}<Badge tone="warn">刷新失败</Badge>{:else if doc?.availability === 'stale-cache'}<Badge tone="warn">陈旧缓存</Badge>{:else if doc}<Badge tone="up">在线</Badge>{/if}
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

{#if categoryRows.length > 0}
  <Panel flush title="分类汇总" subtitle="点击行业或概念可直接筛选；统计基于当前完整分类，不受分页影响">
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
  title={`${categoryLabel(category)}板块连板天梯`}
  subtitle={staleError || (doc
    ? `${count(doc.counts.matched)} 个命中 · 第 ${count(offset + 1)}—${count(offset + doc.counts.returned)} 条 · ${doc.cache.refreshed ? '已刷新上游' : `缓存 ${count(doc.cache.age_seconds)} 秒`}`
    : '服务端排序与分页')}
  busy={resource.busy}
  error={doc ? '' : resource.error}
  onRetry={() => load()}
  empty={resource.loaded && !resource.busy && rows.length === 0}
  emptyText="当前分类、状态和检索条件下没有板块。"
>
  {#snippet toolbar()}
    <Segmented options={CATEGORIES} value={category} onChange={switchCategory} ariaLabel="板块分类" />
    <Select options={ACTIVITIES} value={activity} width="132px" label="活跃状态" onChange={(value) => { activity = value as LimitLadderActivity; runQuery(); }} />
    <Select options={SORTS} value={sort} width="126px" label="服务端排序" onChange={(value) => { sort = value as LimitLadderSort; runQuery(); }} />
    <Select options={ORDERS} value={order} width="84px" label="顺序" onChange={(value) => { order = value as 'asc' | 'desc'; runQuery(); }} />
    <TextInput bind:value={query} icon="search" width="210px" label="检索" placeholder="板块代码或名称" onEnter={runQuery} />
    <Button icon="search" onclick={runQuery}>查询</Button>
    <Button variant="ghost" disabled={!canPrevious} onclick={() => goPage(offset - PAGE_SIZE)}>上一页</Button>
    <Button variant="ghost" disabled={!canNext} onclick={() => goPage(offset + PAGE_SIZE)}>下一页</Button>
  {/snippet}

  <DataTable
    {columns}
    {rows}
    numbered
    stickyFirst
    minWidth="1050px"
    rowKey={(row) => `${row.block.category}:${row.block.code}`}
  >
    {#snippet cell({ row, column })}
      {#if column.key === 'formula'}
        {#if row.advancement_rate_formula_matches === true}
          <Badge tone="up">一致</Badge>
        {:else if row.advancement_rate_formula_matches === false}
          <Badge tone="warn">不一致</Badge>
        {:else}
          <Badge tone="neutral">不可复算</Badge>
        {/if}
      {:else if column.key === 'members'}
        <Button
          variant="ghost"
          icon="sectors"
          disabled={!canOpenMembers(row)}
          title={canOpenMembers(row) ? `查看 ${row.block.name} 的本地成分` : '该板块没有可验证的本地成分映射'}
          onclick={() => openMembers(row)}
        >
          {canOpenMembers(row) ? count(row.block.member_count) : '不可用'}
        </Button>
      {/if}
    {/snippet}
  </DataTable>
</Panel>
