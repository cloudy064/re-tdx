<script lang="ts">
  /** 两融 / 北向逐日历史与四组分档模型；所有结果均为相关性研究，不是交易信号。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, fixed, percent, signedCompact, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    FlowFollowupBucket,
    FlowFollowupDocument,
    FlowFollowupHistoryDocument,
    FlowFollowupHistoryRecord,
    FlowFollowupModelDocument,
    FlowFollowupView
  } from '../../types';

  const VIEWS = [
    { id: 'margin', label: '两融逐日历史' },
    { id: 'northbound', label: '北向逐日历史' },
    { id: 'financing-model', label: '融资率分档' },
    { id: 'lending-model', label: '融券率分档' },
    { id: 'northbound-inflow-model', label: '北向净流入分档' },
    { id: 'northbound-purchase-model', label: '北向净买入分档' }
  ];
  const AVAILABILITY = [
    { id: 'all', label: '保留全部上游记录' },
    { id: 'available', label: '仅可用信号记录' }
  ];
  const LIMITS = [
    { id: '200', label: '最近 200 条' },
    { id: '1000', label: '最近 1,000 条' },
    { id: '5000', label: '最多 5,000 条' }
  ];

  function localDate(value: Date): string {
    const year = value.getFullYear();
    const month = String(value.getMonth() + 1).padStart(2, '0');
    const day = String(value.getDate()).padStart(2, '0');
    return `${year}-${month}-${day}`;
  }

  const today = localDate(new Date());
  let view = $state<FlowFollowupView>('margin');
  let start = $state('2017-01-01');
  let end = $state(today);
  let availability = $state<'all' | 'available'>('all');
  let limit = $state('200');
  let formError = $state('');

  const resource = new Resource<FlowFollowupDocument>();
  const doc = $derived(resource.data);
  const history = $derived(doc && isHistory(doc) ? doc : null);
  const model = $derived(doc && !isHistory(doc) ? doc : null);
  const warnings = $derived(doc?.warnings ?? []);
  const historyRows = $derived(history?.records ?? []);
  const bucketRows = $derived(model?.records ?? []);
  const staleError = $derived(doc && resource.error ? resource.error : '');

  function isHistory(value: FlowFollowupDocument): value is FlowFollowupHistoryDocument {
    return value.view === 'margin' || value.view === 'northbound';
  }

  function viewLabel(value: string): string {
    return VIEWS.find((item) => item.id === value)?.label ?? value;
  }

  const stats = $derived.by<Stat[]>(() => {
    if (history) {
      return [
        { label: '上游 / 规范化', value: `${count(history.counts.upstream_rows)} / ${count(history.counts.normalized)}` },
        { label: '可用信号', value: count(history.counts.signal_available), tone: 'up' },
        { label: '占位记录', value: count(history.counts.upstream_placeholder), tone: history.counts.upstream_placeholder ? 'down' : undefined },
        { label: '返回', value: count(history.counts.returned), note: history.counts.truncated ? '已截取最近记录' : '完整范围' },
        { label: '观测区间', value: `${date(history.summary.observed_start_date)} — ${date(history.summary.observed_end_date)}` },
        { label: '最后可用日', value: date(history.summary.last_signal_available_date) }
      ];
    }
    if (!model) return [];
    const signal = model.current_signal;
    return [
      { label: '分档', value: count(model.counts.normalized_buckets), note: `${count(model.counts.current_buckets)} 个当前档` },
      { label: '当前数据日', value: date(signal.date) },
      { label: '当前值', value: signal.rate_pct !== undefined ? percent(signal.rate_pct) : signedCompact(signal.flow_100m_cny === undefined ? signal.balance_100m_cny : signal.flow_100m_cny, '亿元') },
      { label: '当前分档', value: model.current_bucket?.bucket_label ?? '—' },
      { label: '当前信号', value: signal.signal_available ? '可用于统计' : '不可用', tone: signal.signal_available ? 'up' : 'down' },
      { label: '样本数', value: count(model.current_bucket?.observations) }
    ];
  });

  const marginColumns: Column<FlowFollowupHistoryRecord>[] = [
    { key: 'date', label: '日期', width: '92px', num: true, value: (row) => date(row.date) },
    { key: 'financing', label: '融资余额 / 融资率', align: 'right', num: true, value: (row) => signedCompact(row.financing_balance_100m_cny, '亿元'), sub: (row) => percent(row.financing_rate_pct) },
    { key: 'lending', label: '融券余额 / 融券率', align: 'right', num: true, value: (row) => signedCompact(row.securities_lending_balance_100m_cny, '亿元'), sub: (row) => percent(row.securities_lending_rate_pct) },
    { key: 'csi300', label: '沪深300', align: 'right', num: true, value: (row) => fixed(row.csi300_close, 2) },
    { key: 'd1', label: '后 1 日', align: 'right', num: true, value: (row) => percent(row.forward_returns_pct.days_1, 2, true), tone: (row) => tone(row.forward_returns_pct.days_1) },
    { key: 'd3', label: '后 3 日', align: 'right', num: true, value: (row) => percent(row.forward_returns_pct.days_3, 2, true), tone: (row) => tone(row.forward_returns_pct.days_3) },
    { key: 'd5', label: '后 5 日', align: 'right', num: true, value: (row) => percent(row.forward_returns_pct.days_5, 2, true), tone: (row) => tone(row.forward_returns_pct.days_5) },
    { key: 'd10', label: '后 10 日', align: 'right', num: true, value: (row) => percent(row.forward_returns_pct.days_10, 2, true), tone: (row) => tone(row.forward_returns_pct.days_10) },
    { key: 'status', label: '数据状态', width: '104px', align: 'center', slot: true }
  ];

  const northboundColumns: Column<FlowFollowupHistoryRecord>[] = [
    { key: 'date', label: '日期', width: '92px', num: true, value: (row) => date(row.date) },
    { key: 'inflow', label: '净流入', align: 'right', num: true, value: (row) => signedCompact(row.net_inflow_100m_cny, '亿元'), tone: (row) => tone(row.net_inflow_100m_cny), sub: (row) => row.signal_available ? '' : `上游 ${signedCompact(row.reported_net_inflow_100m_cny, '亿元')}` },
    { key: 'purchase', label: '净买入', align: 'right', num: true, value: (row) => signedCompact(row.net_purchase_100m_cny, '亿元'), tone: (row) => tone(row.net_purchase_100m_cny), sub: (row) => row.signal_available ? '' : `上游 ${signedCompact(row.reported_net_purchase_100m_cny, '亿元')}` },
    { key: 'csi300', label: '沪深300', align: 'right', num: true, value: (row) => fixed(row.csi300_close, 2) },
    { key: 'd1', label: '后 1 日', align: 'right', num: true, value: (row) => percent(row.forward_returns_pct.days_1, 2, true), tone: (row) => tone(row.forward_returns_pct.days_1) },
    { key: 'd3', label: '后 3 日', align: 'right', num: true, value: (row) => percent(row.forward_returns_pct.days_3, 2, true), tone: (row) => tone(row.forward_returns_pct.days_3) },
    { key: 'd5', label: '后 5 日', align: 'right', num: true, value: (row) => percent(row.forward_returns_pct.days_5, 2, true), tone: (row) => tone(row.forward_returns_pct.days_5) },
    { key: 'status', label: '数据状态', width: '104px', align: 'center', slot: true }
  ];

  const bucketColumns: Column<FlowFollowupBucket>[] = [
    { key: 'bucket', label: '信号分档', width: '132px', value: (row) => row.bucket_label, sub: (row) => row.bucket_unit === 'percentage-points' ? '百分点' : '亿元' },
    { key: 'observations', label: '样本数', align: 'right', num: true, value: (row) => count(row.observations) },
    { key: 'd1', label: '后 1 日均值 / 上涨概率', align: 'right', num: true, value: (row) => percent(row.csi300_forward_performance.days_1.mean_return_pct, 2, true), sub: (row) => percent(row.csi300_forward_performance.days_1.positive_ratio_pct), tone: (row) => tone(row.csi300_forward_performance.days_1.mean_return_pct) },
    { key: 'd3', label: '后 3 日均值 / 上涨概率', align: 'right', num: true, value: (row) => percent(row.csi300_forward_performance.days_3.mean_return_pct, 2, true), sub: (row) => percent(row.csi300_forward_performance.days_3.positive_ratio_pct), tone: (row) => tone(row.csi300_forward_performance.days_3.mean_return_pct) },
    { key: 'd5', label: '后 5 日均值 / 上涨概率', align: 'right', num: true, value: (row) => percent(row.csi300_forward_performance.days_5.mean_return_pct, 2, true), sub: (row) => percent(row.csi300_forward_performance.days_5.positive_ratio_pct), tone: (row) => tone(row.csi300_forward_performance.days_5.mean_return_pct) },
    { key: 'd10', label: '后 10 日均值 / 上涨概率', align: 'right', num: true, value: (row) => row.csi300_forward_performance.days_10 ? percent(row.csi300_forward_performance.days_10.mean_return_pct, 2, true) : '—', sub: (row) => row.csi300_forward_performance.days_10 ? percent(row.csi300_forward_performance.days_10.positive_ratio_pct) : '', tone: (row) => tone(row.csi300_forward_performance.days_10?.mean_return_pct) },
    { key: 'current', label: '当前档', width: '88px', align: 'center', slot: true }
  ];

  function load(refresh = false) {
    formError = '';
    if (!/^\d{4}-\d{2}-\d{2}$/.test(start) || !/^\d{4}-\d{2}-\d{2}$/.test(end)) {
      formError = '开始与结束日期必须使用 YYYY-MM-DD。';
      return;
    }
    if (start > end) {
      formError = '开始日期不能晚于结束日期。';
      return;
    }
    void resource.load(`/api/v1/market/flow-followup?${queryString({
      view,
      start,
      end,
      available_only: isHistoryView(view) && availability === 'available' ? 1 : 0,
      limit: Number(limit),
      refresh: refresh ? 1 : 0
    })}`);
  }

  function isHistoryView(value: FlowFollowupView): boolean {
    return value === 'margin' || value === 'northbound';
  }

  function switchView(value: string) {
    view = value as FlowFollowupView;
    resource.reset();
    load();
  }

  onMount(() => load());
</script>

<PageHeader
  eyebrow="RZRQ / BXZJ · TQLEX"
  title="资金信号后续表现"
  description="两融与北向资金的逐日历史和分档后续表现。所有结果只表达历史相关性，不是预测、投资建议或交易信号。"
  {stats}
>
  {#snippet actions()}
    {#if staleError}<Badge tone="warn">刷新失败</Badge>{:else if doc?.availability === 'stale-cache'}<Badge tone="warn">陈旧缓存</Badge>{:else if doc}<Badge tone="up">在线</Badge>{/if}
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新上游</Button>
  {/snippet}
</PageHeader>

{#if warnings.length > 0 || formError}
  <Panel title="数据边界" subtitle="上游占位段与最新披露状态会被保留，不静默改写">
    {#if formError}<p class="warning">{formError}</p>{/if}
    {#each warnings as warning}<p class="warning">{warning}</p>{/each}
  </Panel>
{/if}

<Panel
  flush
  scroll
  fill
  title={model?.view_title ?? viewLabel(view)}
  subtitle={staleError || (doc ? `${count(isHistory(doc) ? doc.counts.returned : doc.counts.normalized_buckets)} 条 · ${doc.cache.hit ? `缓存 ${count(doc.cache.age_seconds)} 秒` : '已读取上游'}` : '选择视图与日期范围')}
  busy={resource.busy}
  error={doc ? '' : resource.error}
  onRetry={() => load()}
  empty={resource.loaded && !resource.busy && (historyRows.length + bucketRows.length) === 0}
  emptyText="当前视图与日期范围没有可显示的记录。"
>
  {#snippet toolbar()}
    <Select options={VIEWS} value={view} width="180px" label="资金视图" onChange={switchView} />
    <TextInput bind:value={start} width="116px" label="开始日期" placeholder="开始 YYYY-MM-DD" onEnter={() => load()} />
    <TextInput bind:value={end} width="116px" label="结束日期" placeholder="结束 YYYY-MM-DD" onEnter={() => load()} />
    {#if isHistoryView(view)}
      <Select options={AVAILABILITY} value={availability} width="150px" label="可用性" onChange={(value) => { availability = value as 'all' | 'available'; load(); }} />
      <Select options={LIMITS} value={limit} width="120px" label="返回数量" onChange={(value) => { limit = value; load(); }} />
    {/if}
    <Button icon="search" onclick={() => load()}>查询</Button>
  {/snippet}

  {#if history}
    <DataTable
      columns={history.view === 'margin' ? marginColumns : northboundColumns}
      rows={historyRows}
      numbered
      stickyFirst
      minWidth="1080px"
      rowKey={(row) => row.date}
    >
      {#snippet cell({ row, column })}
        {#if column.key === 'status'}
          {#if row.signal_available}<Badge tone="up">可用</Badge>{:else}<Badge tone="warn">上游占位</Badge>{/if}
        {/if}
      {/snippet}
    </DataTable>
  {:else if model}
    <DataTable
      columns={bucketColumns}
      rows={bucketRows}
      numbered
      minWidth="1040px"
      rowKey={(row) => row.bucket_code}
    >
      {#snippet cell({ row, column })}
        {#if column.key === 'current'}
          {#if row.is_current_bucket && row.usable_for_current_signal === false}
            <Badge tone="warn">占位不可用</Badge>
          {:else if row.is_current_bucket}
            <Badge tone="up">当前档</Badge>
          {:else}
            <Badge tone="neutral">历史档</Badge>
          {/if}
        {/if}
      {/snippet}
    </DataTable>
  {/if}
</Panel>

<style>
  .warning { margin: var(--sp-1) 0; color: var(--warn); font-size: var(--fs-small); }
</style>
