<script lang="ts">
  /** 当前证券的 ZDTFX 盘后复盘；只读、非实时，raw 审计字段不渲染。 */
  import { queryString } from '../../../api';
  import { count, date, delta, percent, text, time } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid from '../../../ui/StatGrid.svelte';
  import type {
    LimitReviewAnnualRecord,
    LimitReviewCurrentRecord,
    LimitReviewSecurityHistoryRecord,
    MarketLimitReviewDocument
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketLimitReviewDocument>();
  const doc = $derived(resource.data);
  const current = $derived((doc?.records.filter((row) => row.category !== 'annual') ?? []) as LimitReviewCurrentRecord[]);
  const annual = $derived((doc?.records.filter((row) => row.category === 'annual') ?? []) as LimitReviewAnnualRecord[]);
  const history = $derived(doc?.history ?? []);

  function categoryLabel(value: string): string {
    return value === 'limit-up' ? '涨停'
      : value === 'limit-down' ? '跌停'
      : value === 'surge' ? '涨超 10%'
      : value;
  }

  function load(refresh = false) {
    void resource.load(`/api/v1/market/limit-review?${queryString({
      view: 'security', market, code, offset: 0, limit: 200,
      refresh: refresh ? 1 : 0
    })}`);
  }

  const currentColumns: Column<LimitReviewCurrentRecord>[] = [
    { key: 'category', label: '类型', width: '78px', align: 'center', slot: true },
    { key: 'date', label: '日期', width: '92px', num: true, value: (row) => date(row.date) },
    { key: 'reason', label: '原因', width: '360px', wrap: true, value: (row) => text(row.reason) },
    { key: 'shape', label: '板型 / 类别', width: '132px', value: (row) => text([row.limit_type, row.board_shape].filter(Boolean).join(' · ')) },
    { key: 'time', label: '首次 → 末次 / 触发', width: '142px', num: true, value: (row) => row.trigger_time ? time(row.trigger_time) : `${time(row.first_time)} → ${time(row.last_time)}` },
    { key: 'streak', label: '连板 / 打开', width: '92px', align: 'right', num: true, value: (row) => `${count(row.streak_days)} / ${count(row.break_count)}` },
    { key: 'gene', label: '近一年涨停 / 次日均值', width: '142px', align: 'right', num: true, value: (row) => row.limit_gene ? `${count(row.limit_gene.past_year_limit_count)} / ${delta(row.limit_gene.next_day_mean_return_pct)}` : '—' }
  ];

  const annualColumns: Column<LimitReviewAnnualRecord>[] = [
    { key: 'date', label: '最新日期', width: '92px', num: true, value: (row) => date(row.latest_date), sub: (row) => row.newly_listed ? `次新 ${row.newly_listed}` : '' },
    { key: 'up-count', label: '涨停 收盘 / 盘中 / 合计', width: '160px', align: 'right', num: true, value: (row) => `${count(row.limit_up.close_count)} / ${count(row.limit_up.intraday_count)} / ${count(row.limit_up.total_count)}` },
    { key: 'up-turnover', label: '涨停均换手', width: '90px', align: 'right', num: true, value: (row) => percent(row.limit_up.average_turnover_pct) },
    { key: 'up-next', label: '次日高开 / 高收', width: '120px', align: 'right', num: true, value: (row) => `${percent(row.limit_up.next_day_gap_up_rate_pct)} / ${percent(row.limit_up.next_day_higher_close_rate_pct)}` },
    { key: 'down-count', label: '跌停 收盘 / 盘中 / 合计', width: '160px', align: 'right', num: true, value: (row) => `${count(row.limit_down.close_count)} / ${count(row.limit_down.intraday_count)} / ${count(row.limit_down.total_count)}` },
    { key: 'down-turnover', label: '跌停均换手', width: '90px', align: 'right', num: true, value: (row) => percent(row.limit_down.average_turnover_pct) },
    { key: 'down-next', label: '次日低开 / 低收', width: '120px', align: 'right', num: true, value: (row) => `${percent(row.limit_down.next_day_gap_down_rate_pct)} / ${percent(row.limit_down.next_day_lower_close_rate_pct)}` }
  ];

  const historyColumns: Column<LimitReviewSecurityHistoryRecord>[] = [
    { key: 'date', label: '日期', width: '92px', num: true, value: (row) => date(row.date) },
    { key: 'type', label: '涨跌停类型', width: '110px', value: (row) => text(row.limit_type) },
    { key: 'streak', label: '连板天数', width: '78px', align: 'right', num: true, value: (row) => count(row.streak_days) },
    { key: 'reason', label: '原因', width: '300px', wrap: true, value: (row) => text(row.reason) },
    { key: 'explanation', label: '复盘解释', width: '480px', wrap: true, value: (row) => text(row.explanation) }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<div class="stack">
  <Panel
    eyebrow="ZDTFX · SECURITY · READ ONLY"
    title={`${name} · 涨跌停复盘`}
    subtitle="盘后逐步更新的非实时资料；未命中是正常关系，实时封单仍以右侧盘口和“涨停质量”为准。"
    busy={resource.busy}
    error={doc ? '' : resource.error}
    onRetry={load}
    empty={resource.loaded && Boolean(doc) && current.length === 0 && annual.length === 0 && history.length === 0}
    emptyText="当前证券没有进入已下载的涨跌停复盘资源。"
  >
    {#snippet actions()}
      <Badge tone="warn">盘后 · 非实时</Badge>
      {#if resource.error && doc}<Badge tone="warn">刷新失败</Badge>{/if}
      <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新复盘</Button>
    {/snippet}
    {#if doc}
      <StatGrid inline stats={[
        { label: '当前 / 年度命中', value: `${count(current.length)} / ${count(annual.length)}`, note: `${count(doc.counts.source_rows)} 条源记录` },
        { label: '原因历史', value: count(history.length), note: doc.counts.missing_sources ? '动态历史资源缺失' : '单票动态资源' },
        { label: '来源', value: `${count(doc.upstream_health.live_sources)} live / ${count(doc.upstream_health.stale_sources)} stale` },
        { label: '缓存', value: doc.availability === 'stale-cache' ? '陈旧' : '已读取', note: `TTL ${count(doc.cache.ttl_seconds)} 秒` }
      ]} />
      <p class="boundary">仅展示归一化只读字段；响应中的 <code>raw</code> 审计数据默认不渲染，也不会自动轮询。</p>
    {/if}
  </Panel>

  {#if current.length > 0}
    <Panel title="当前复盘命中" subtitle="涨停、跌停或涨超 10% 的盘后原因" flush scroll>
      <DataTable columns={currentColumns} rows={current} minWidth="1080px" rowKey={(row) => `${row.category}:${row.date}`}>
        {#snippet cell({ row, column })}
          {#if column.key === 'category'}
            <Badge tone={row.direction === 'down' ? 'down' : 'up'}>{categoryLabel(row.category)}</Badge>
          {/if}
        {/snippet}
      </DataTable>
    </Panel>
  {/if}

  {#if annual.length > 0}
    <Panel title="年度涨跌停行为" subtitle="收盘/盘中次数与次日行为统计" flush scroll>
      <DataTable columns={annualColumns} rows={annual} minWidth="980px" rowKey={(row) => row.security.security_id} />
    </Panel>
  {/if}

  <Panel
    title="历次涨跌停原因"
    subtitle={history.length ? `${count(history.length)} 条动态历史 · 保持上游顺序` : '单票动态资源可能尚未生成'}
    empty={resource.loaded && history.length === 0}
    emptyText="当前证券没有可用的历次涨跌停原因。"
    flush
    scroll
    fill
  >
    <DataTable columns={historyColumns} rows={history} minWidth="1060px" rowKey={(row, index) => `${row.date}:${row.limit_type}:${index}`} />
  </Panel>
</div>

<style>
  .stack { display: flex; min-height: 0; flex: 1; flex-direction: column; gap: var(--sp-2); overflow: hidden; }
  .boundary { margin-top: var(--sp-3); font-size: var(--fs-micro); color: var(--fg-mute); }
  .boundary code { font-family: var(--font-num); color: var(--fg); }
</style>
