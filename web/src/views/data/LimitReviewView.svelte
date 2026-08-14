<script lang="ts">
  /** ZDTFX：盘后逐步更新的涨跌停复盘，不与实时封板质量混用。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, percent, text, time, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
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
    LimitReviewAnnualRecord,
    LimitReviewCategory,
    LimitReviewCurrentRecord,
    LimitReviewDailyRecord,
    LimitReviewMarketHistoryRecord,
    LimitReviewSecurity,
    LimitReviewView,
    MarketLimitReviewDocument
  } from '../../types';

  type PageView = Exclude<LimitReviewView, 'security'>;

  const VIEWS = [
    { id: 'current', label: '当日复盘' },
    { id: 'history', label: '市场历史' },
    { id: 'daily', label: '指定日期' },
    { id: 'annual', label: '年度行为' }
  ];
  const CURRENT_CATEGORIES = [
    { id: 'all', label: '全部类型' },
    { id: 'limit-up', label: '涨停' },
    { id: 'limit-down', label: '跌停' },
    { id: 'surge', label: '涨超 10%' }
  ];
  const DAILY_CATEGORIES = CURRENT_CATEGORIES.slice(0, 3);
  const MARKETS = [
    { id: 'all', label: '全部市场' },
    { id: 'sz', label: '深市' },
    { id: 'sh', label: '沪市' },
    { id: 'bj', label: '北交所' }
  ];
  const LIMITS = [50, 100, 200, 500, 1000].map((value) => ({
    id: String(value), label: `${value} 条 / 页`
  }));
  const SAFE_MARKETS = new Set(['sz', 'sh', 'bj']);

  function compactToday(): string {
    const now = new Date();
    return `${now.getFullYear()}${String(now.getMonth() + 1).padStart(2, '0')}${String(now.getDate()).padStart(2, '0')}`;
  }

  let view = $state<PageView>('current');
  let category = $state<LimitReviewCategory>('all');
  let market = $state('all');
  let code = $state('');
  let selectedDate = $state(compactToday());
  let query = $state('');
  let limit = $state('200');
  let offset = $state(0);
  let formError = $state('');
  let loadedKey = '';

  const resource = new Resource<MarketLimitReviewDocument>();
  const doc = $derived(resource.data);
  const currentRows = $derived((doc?.view === 'current' ? doc.records : []) as LimitReviewCurrentRecord[]);
  const historyRows = $derived((doc?.view === 'history' ? doc.records : []) as LimitReviewMarketHistoryRecord[]);
  const dailyRows = $derived((doc?.view === 'daily' ? doc.records : []) as LimitReviewDailyRecord[]);
  const annualRows = $derived((doc?.view === 'annual' ? doc.records : []) as LimitReviewAnnualRecord[]);
  const rows = $derived(doc?.records ?? []);
  const pageStart = $derived(rows.length ? offset + 1 : 0);
  const pageEnd = $derived(offset + rows.length);
  const hasNext = $derived(Boolean(
    doc
      && offset + doc.counts.returned < doc.counts.matched
      && offset + Number(limit) <= 1_000_000
  ));
  const categoryOptions = $derived(view === 'daily' ? DAILY_CATEGORIES : CURRENT_CATEGORIES);

  function viewLabel(value: PageView): string {
    return VIEWS.find((item) => item.id === value)?.label ?? value;
  }

  function categoryLabel(value: string): string {
    return value === 'limit-up' ? '涨停'
      : value === 'limit-down' ? '跌停'
      : value === 'surge' ? '涨超 10%'
      : value;
  }

  function validate(): boolean {
    if (view === 'daily' && !/^\d{8}$/.test(selectedDate.trim())) {
      formError = '指定日期视图要求 YYYYMMDD 八位日期。';
      return false;
    }
    if (view !== 'history' && code.trim()) {
      if (market === 'all') {
        formError = '按代码筛选时必须同时选择深市、沪市或北交所。';
        return false;
      }
      if (!/^\d{6}$/.test(code.trim())) {
        formError = '证券代码必须是六位数字。';
        return false;
      }
    }
    formError = '';
    return true;
  }

  function path(refresh: boolean): string {
    return `/api/v1/market/limit-review?${queryString({
      view,
      category: view === 'current' || view === 'daily' ? category : 'all',
      market: view === 'history' || market === 'all' ? '' : market,
      code: view === 'history' ? '' : code.trim(),
      date: view === 'daily' ? selectedDate.trim() : '',
      q: query.trim(),
      offset,
      limit: Number(limit),
      refresh: refresh ? 1 : 0
    })}`;
  }

  async function load(refresh = false) {
    if (!validate()) {
      resource.reset();
      loadedKey = '';
      return;
    }
    const stableKey = path(false);
    if (stableKey !== loadedKey) resource.reset();
    const result = await resource.load(path(refresh));
    if (result) loadedKey = stableKey;
  }

  function queryFirstPage() {
    offset = 0;
    void load();
  }

  function selectView(next: string) {
    view = next as PageView;
    category = 'all';
    offset = 0;
    formError = '';
    if (view === 'history') {
      market = 'all';
      code = '';
    }
    resource.reset();
    loadedKey = '';
    void load();
  }

  function previousPage() {
    offset = Math.max(0, offset - Number(limit));
    void load();
  }

  function nextPage() {
    if (!hasNext) return;
    offset += Number(limit);
    void load();
  }

  function canOpenSecurity(security: LimitReviewSecurity): boolean {
    return SAFE_MARKETS.has(security.market) && /^\d{6}$/.test(security.code);
  }

  function openSecurity(security: LimitReviewSecurity) {
    if (!canOpenSecurity(security)) return;
    app.setStock({
      market: security.market,
      code: security.code,
      name: security.name || security.security_id
    });
    router.go(stockPath(security.market, security.code, 'limit-review'));
  }

  function geneText(row: LimitReviewCurrentRecord): string {
    const gene = row.limit_gene;
    if (!gene) return '—';
    return `年涨停 ${count(gene.past_year_limit_count)} · 次日均值 ${delta(gene.next_day_mean_return_pct)}`;
  }

  function geneSub(row: LimitReviewCurrentRecord): string {
    const gene = row.limit_gene;
    if (!gene) return '';
    return `次日红盘率 ${fixed(gene.next_day_red_rate)} · 首板封板率 ${fixed(gene.first_board_seal_rate)}`;
  }

  function streakDistribution(row: LimitReviewMarketHistoryRecord): string {
    const parts = Object.entries(row.limit_up.streak_distribution)
      .filter(([, value]) => value !== null)
      .map(([streak, value]) => `${streak}板 ${count(value)}`);
    return parts.length ? parts.join(' · ') : '—';
  }

  const currentColumns: Column<LimitReviewCurrentRecord>[] = [
    { key: 'security', label: '股票', width: '138px', slot: true },
    { key: 'category', label: '类型', width: '78px', align: 'center', slot: true },
    { key: 'date', label: '复盘日期', width: '92px', num: true, value: (row) => date(row.date) },
    { key: 'reason', label: '原因', width: '300px', wrap: true, value: (row) => text(row.reason) },
    { key: 'shape', label: '板型 / 类别', width: '130px', value: (row) => text([row.limit_type, row.board_shape].filter(Boolean).join(' · ')) },
    { key: 'time', label: '首次 → 末次 / 触发', width: '140px', num: true, value: (row) => row.trigger_time ? time(row.trigger_time) : `${time(row.first_time)} → ${time(row.last_time)}` },
    { key: 'streak', label: '连板 / 打开', width: '92px', align: 'right', num: true, value: (row) => `${count(row.streak_days)} / ${count(row.break_count)}` },
    { key: 'gene', label: '涨停基因', width: '270px', value: geneText, sub: geneSub }
  ];

  const historyColumns: Column<LimitReviewMarketHistoryRecord>[] = [
    { key: 'date', label: '日期', width: '92px', num: true, value: (row) => date(row.date) },
    { key: 'index', label: '上证涨幅', width: '84px', align: 'right', num: true, value: (row) => delta(row.shanghai_index_change_pct), tone: (row) => tone(row.shanghai_index_change_pct) },
    { key: 'turnover', label: '市场成交额', width: '112px', align: 'right', num: true, value: (row) => compact(row.market_turnover_yuan, '元') },
    { key: 'up', label: '涨停 / 封板 / 炸板', width: '126px', align: 'right', num: true, value: (row) => `${count(row.limit_up.all_count)} / ${count(row.limit_up.closed_count)} / ${count(row.limit_up.broken_count)}` },
    { key: 'seal-rate', label: '封板率', width: '78px', align: 'right', num: true, value: (row) => percent(row.limit_up.seal_rate_pct) },
    { key: 'streak', label: '连板 / 最高板', width: '100px', align: 'right', num: true, value: (row) => `${count(row.limit_up.streak_count)} / ${count(row.limit_up.max_streak)}` },
    { key: 'one-price', label: '一字板', width: '68px', align: 'right', num: true, value: (row) => count(row.limit_up.one_price_count) },
    { key: 'seal', label: '封单合计 / 最大', width: '190px', align: 'right', num: true, value: (row) => `${compact(row.limit_up.total_seal_amount_yuan, '元')} / ${compact(row.limit_up.max_seal_amount_yuan, '元')}` },
    { key: 'down', label: '跌停 / 封死 / 盘中', width: '126px', align: 'right', num: true, value: (row) => `${count(row.limit_down.all_count)} / ${count(row.limit_down.closed_count)} / ${count(row.limit_down.intraday_count)}` },
    { key: 'ratio', label: '涨跌停比 / 开板比', width: '130px', align: 'right', num: true, value: (row) => `${fixed(row.up_down_count_ratio)} / ${fixed(row.up_down_break_ratio)}` },
    { key: 'distribution', label: '1—12 板分布', width: '430px', wrap: true, value: streakDistribution }
  ];

  const dailyColumns: Column<LimitReviewDailyRecord>[] = [
    { key: 'security', label: '股票', width: '138px', slot: true },
    { key: 'category', label: '方向', width: '68px', align: 'center', slot: true },
    { key: 'date', label: '日期', width: '92px', num: true, value: (row) => date(row.date) },
    { key: 'change', label: '当日涨幅', width: '86px', align: 'right', num: true, value: (row) => delta(row.daily_change_pct), tone: (row) => tone(row.daily_change_pct) },
    { key: 'limit-category', label: '类别', width: '110px', value: (row) => text(row.limit_category) },
    { key: 'reason', label: '原因', width: '360px', wrap: true, value: (row) => text(row.reason) },
    { key: 'time', label: '首次 → 末次', width: '120px', num: true, value: (row) => `${time(row.first_time)} → ${time(row.last_time)}` },
    { key: 'streak', label: '连板 / 打开', width: '92px', align: 'right', num: true, value: (row) => `${count(row.streak_days)} / ${count(row.break_count)}` }
  ];

  const annualColumns: Column<LimitReviewAnnualRecord>[] = [
    { key: 'security', label: '股票', width: '138px', slot: true },
    { key: 'date', label: '最新日期', width: '92px', num: true, value: (row) => date(row.latest_date), sub: (row) => row.newly_listed ? `次新 ${row.newly_listed}` : '' },
    { key: 'up-count', label: '涨停 收盘 / 盘中 / 合计', width: '158px', align: 'right', num: true, value: (row) => `${count(row.limit_up.close_count)} / ${count(row.limit_up.intraday_count)} / ${count(row.limit_up.total_count)}` },
    { key: 'up-turnover', label: '涨停均换手', width: '88px', align: 'right', num: true, value: (row) => percent(row.limit_up.average_turnover_pct) },
    { key: 'up-next', label: '次日高开 / 高收', width: '116px', align: 'right', num: true, value: (row) => `${percent(row.limit_up.next_day_gap_up_rate_pct)} / ${percent(row.limit_up.next_day_higher_close_rate_pct)}` },
    { key: 'down-count', label: '跌停 收盘 / 盘中 / 合计', width: '158px', align: 'right', num: true, value: (row) => `${count(row.limit_down.close_count)} / ${count(row.limit_down.intraday_count)} / ${count(row.limit_down.total_count)}` },
    { key: 'down-turnover', label: '跌停均换手', width: '88px', align: 'right', num: true, value: (row) => percent(row.limit_down.average_turnover_pct) },
    { key: 'down-next', label: '次日低开 / 低收', width: '116px', align: 'right', num: true, value: (row) => `${percent(row.limit_down.next_day_gap_down_rate_pct)} / ${percent(row.limit_down.next_day_lower_close_rate_pct)}` }
  ];

  const stats = $derived.by<Stat[]>(() => {
    if (!doc) return [];
    return [
      { label: '视图', value: viewLabel(doc.view as PageView), note: '盘后 / 逐步更新' },
      { label: '源记录', value: count(doc.counts.source_rows), note: `${count(doc.sources.length)} 项资源` },
      { label: '筛选命中', value: count(doc.counts.matched), note: `当前页 ${count(doc.counts.returned)} 条` },
      { label: '来源状态', value: doc.availability === 'stale-cache' ? '陈旧缓存' : doc.availability === 'empty' ? '无记录' : '已读取', note: `${count(doc.upstream_health.live_sources)} live / ${count(doc.upstream_health.stale_sources)} stale` },
      { label: '缺失动态资源', value: count(doc.counts.missing_sources), note: '指定日缺失可正常为空' },
      { label: '生成时间', value: time(doc.generated_at), note: `缓存 TTL ${count(doc.cache.ttl_seconds)} 秒` }
    ];
  });

  onMount(() => void load());
</script>

<PageHeader
  eyebrow="ZDTFX · POST-MARKET / NON-REALTIME"
  title="涨跌停复盘"
  description="通达信盘后、逐步更新的复盘资料；不是实时盘口。实时五档、封单与竞价质量请使用“涨停质量”。"
  {stats}
>
  {#snippet actions()}
    <Badge tone="warn">盘后 / 逐步更新 · 非实时</Badge>
    {#if resource.error && doc}<Badge tone="warn">刷新失败，保留同一查询旧数据</Badge>{/if}
    {#if doc?.availability === 'stale-cache'}<Badge tone="warn">陈旧缓存</Badge>{/if}
    <Button icon="refresh" busy={resource.busy} onclick={() => void load(true)}>刷新盘后资源</Button>
  {/snippet}
</PageHeader>

<div class="boundary" role="note">
  <strong>数据边界</strong>
  <span>仅展示后端归一化字段，API 中用于审计的 <code>raw</code> 默认不渲染；本页不会自动轮询，也不把复盘表冒充实时涨跌停。</span>
</div>

<Panel
  title={`${viewLabel(view)} · 归一化记录`}
  subtitle={doc
    ? `${count(pageStart)}—${count(pageEnd)} / ${count(doc.counts.matched)} · 保持上游/服务端顺序${resource.error ? ` · 刷新失败：${resource.error}` : ''}`
    : '按服务端 offset / limit 分页，不在浏览器内二次排序'}
  busy={resource.busy}
  error={doc ? '' : formError || resource.error}
  onRetry={() => void load()}
  empty={resource.loaded && Boolean(doc) && rows.length === 0}
  emptyText={view === 'daily' ? '该日期没有已下载的涨跌停成员，盘后资源也可能尚未生成。' : '当前筛选没有复盘记录。'}
  flush
  scroll
  fill
>
  {#snippet toolbar()}
    <Segmented options={VIEWS} value={view} onChange={selectView} ariaLabel="复盘视图" />
    {#if view === 'current' || view === 'daily'}
      <Select options={categoryOptions} value={category} width="128px" label="类型" onChange={(value) => { category = value as LimitReviewCategory; queryFirstPage(); }} />
    {/if}
    {#if view === 'daily'}
      <TextInput bind:value={selectedDate} width="112px" label="指定日期" placeholder="YYYYMMDD" onEnter={queryFirstPage} />
    {/if}
    {#if view !== 'history'}
      <Select options={MARKETS} value={market} width="126px" label="市场" onChange={(value) => { market = value; if (value === 'all') code = ''; queryFirstPage(); }} />
      <TextInput bind:value={code} width="102px" label="证券代码" placeholder="六位代码" onEnter={queryFirstPage} />
    {/if}
    <TextInput bind:value={query} icon="search" width="210px" label="检索" placeholder={view === 'history' ? '日期 / 市场统计' : '名称 / 代码 / 原因'} onEnter={queryFirstPage} />
    <Select options={LIMITS} value={limit} width="112px" label="分页" onChange={(value) => { limit = value; queryFirstPage(); }} />
    <Button icon="search" variant="primary" onclick={queryFirstPage}>查询</Button>
    <div class="pager">
      <Button icon="chevron-left" disabled={resource.busy || offset === 0} title="上一页" onclick={previousPage} />
      <span>第 {count(Math.floor(offset / Number(limit)) + 1)} 页</span>
      <Button icon="chevron-right" disabled={resource.busy || !hasNext} title="下一页" onclick={nextPage} />
    </div>
  {/snippet}

  {#if view === 'current'}
    <DataTable columns={currentColumns} rows={currentRows} stickyFirst minWidth="1240px" rowKey={(row) => `${row.security.security_id}:${row.category}:${row.date}`}>
      {#snippet cell({ row, column })}
        {#if column.key === 'security'}
          {#if canOpenSecurity(row.security)}
            <button class="security-link" type="button" title="在个股工作台打开复盘" onclick={() => openSecurity(row.security)}>
              <span>{row.security.name || row.security.code}</span><small>{row.security.security_id}</small>
            </button>
          {:else}<span class="security-static">{row.security.name || row.security.security_id}</span>{/if}
        {:else if column.key === 'category'}
          <Badge tone={row.direction === 'down' ? 'down' : 'up'}>{categoryLabel(row.category)}</Badge>
        {/if}
      {/snippet}
    </DataTable>
  {:else if view === 'history'}
    <DataTable columns={historyColumns} rows={historyRows} stickyFirst minWidth="1540px" rowKey={(row) => row.date} />
  {:else if view === 'daily'}
    <DataTable columns={dailyColumns} rows={dailyRows} stickyFirst minWidth="1080px" rowKey={(row) => `${row.security.security_id}:${row.category}:${row.date}`}>
      {#snippet cell({ row, column })}
        {#if column.key === 'security'}
          {#if canOpenSecurity(row.security)}
            <button class="security-link" type="button" title="在个股工作台打开复盘" onclick={() => openSecurity(row.security)}>
              <span>{row.security.name || row.security.code}</span><small>{row.security.security_id}</small>
            </button>
          {:else}<span class="security-static">{row.security.name || row.security.security_id}</span>{/if}
        {:else if column.key === 'category'}
          <Badge tone={row.direction === 'down' ? 'down' : 'up'}>{categoryLabel(row.category)}</Badge>
        {/if}
      {/snippet}
    </DataTable>
  {:else}
    <DataTable columns={annualColumns} rows={annualRows} stickyFirst minWidth="1120px" rowKey={(row) => row.security.security_id}>
      {#snippet cell({ row, column })}
        {#if column.key === 'security'}
          {#if canOpenSecurity(row.security)}
            <button class="security-link" type="button" title="在个股工作台打开复盘" onclick={() => openSecurity(row.security)}>
              <span>{row.security.name || row.security.code}</span><small>{row.security.security_id}</small>
            </button>
          {:else}<span class="security-static">{row.security.name || row.security.security_id}</span>{/if}
        {/if}
      {/snippet}
    </DataTable>
  {/if}
</Panel>

<style>
  .boundary { display: flex; flex: none; flex-wrap: wrap; align-items: center; gap: var(--sp-2); padding: var(--sp-2) var(--sp-3); font-size: var(--fs-micro); color: var(--fg-dim); background: var(--warn-soft); border: 1px solid var(--line); border-radius: var(--radius); }
  .boundary strong { color: var(--warn); }
  .boundary code { font-family: var(--font-num); color: var(--fg); }
  .pager { display: inline-flex; align-items: center; gap: var(--sp-2); margin-left: auto; }
  .pager span { min-width: 54px; font-family: var(--font-num); font-size: var(--fs-micro); color: var(--fg-mute); text-align: center; }
  .security-link, .security-static { display: inline-flex; min-width: 0; flex-direction: column; align-items: flex-start; line-height: 1.2; text-align: left; }
  .security-link { color: var(--focus); }
  .security-link:hover span:first-child { text-decoration: underline; }
  .security-link small { margin-top: 1px; font-family: var(--font-num); font-size: 9px; color: var(--fg-mute); }
  .security-static { color: var(--fg-dim); }
  @media (max-width: 1100px) { .pager { margin-left: 0; } }
</style>
