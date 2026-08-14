<script lang="ts">
  /** 本地只读参考档案：历史证券兼容名、基金映射与指数图事件。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    FundReferenceDocument,
    FundReferenceRecord,
    HistoricalSecuritiesDocument,
    HistoricalSecurityRecord,
    IndexEventBenchmark,
    IndexEventRecord,
    IndexEventsDocument
  } from '../../types';

  type Section = 'historical' | 'funds' | 'events';

  const SECTIONS = [
    { id: 'historical', label: '历史证券兼容名' },
    { id: 'funds', label: '基金与标的映射' },
    { id: 'events', label: '指数图事件' }
  ];
  const MARKETS = [
    { id: 'all', label: '全部市场' },
    { id: 'sz', label: '深市' },
    { id: 'sh', label: '沪市' },
    { id: 'bj', label: '北交所' }
  ];
  const FUND_MARKETS = MARKETS.slice(0, 3);
  const PRESENCE = [
    { id: 'all', label: '全部兼容记录' },
    { id: 'current', label: '仍在当前目录' },
    { id: 'absent', label: '已不在当前目录' }
  ];
  const HISTORICAL_SORT = [
    { id: 'code', label: '代码' },
    { id: 'name', label: '兼容名称' },
    { id: 'presence', label: '目录状态' }
  ];
  const FUND_VIEWS = [
    { id: 'all', label: '全部参考记录' },
    { id: 'snapshot', label: '基金份额与净值' },
    { id: 'etf-mapping', label: 'ETF 标的映射' },
    { id: 'lof-mapping', label: 'LOF 标的映射' }
  ];
  const BENCHMARKS = [
    { id: 'all', label: '全部基准' },
    { id: 'shanghai-composite', label: '上证指数' },
    { id: 'hang-seng', label: '恒生指数' },
    { id: 'nasdaq-composite', label: '纳斯达克' }
  ];
  const DATE_BASIS = [
    { id: 'occurrence', label: '实际发生日' },
    { id: 'chart', label: '图表交易日' }
  ];
  const EVENT_SORT = [
    { id: 'date', label: '日期' },
    { id: 'event-id', label: '事件编号' },
    { id: 'title', label: '标题' }
  ];
  const ORDERS = [
    { id: 'asc', label: '升序' },
    { id: 'desc', label: '降序' }
  ];
  const PAGE_LIMITS = [50, 100, 200, 500].map((value) => ({
    id: String(value), label: `${value} 条 / 页`
  }));
  const FUND_LIMITS = [200, 500, 1000, 5000].map((value) => ({
    id: String(value), label: `最多 ${value} 条`
  }));

  let section = $state<Section>('historical');
  let historicalMarket = $state('all');
  let historicalPresence = $state('all');
  let historicalSort = $state('code');
  let historicalOrder = $state('asc');
  let historicalQuery = $state('');
  let historicalLimit = $state('200');
  let historicalOffset = $state(0);

  let fundView = $state('all');
  let fundMarket = $state('all');
  let fundQuery = $state('');
  let fundAsOfDate = $state('');
  let fundLimit = $state('500');

  let benchmark = $state<IndexEventBenchmark>('all');
  let eventDateBasis = $state('occurrence');
  let eventSort = $state('date');
  let eventOrder = $state('desc');
  let eventQuery = $state('');
  let eventFrom = $state('');
  let eventTo = $state('');
  let eventLimit = $state('200');
  let eventOffset = $state(0);

  const historical = new Resource<HistoricalSecuritiesDocument>();
  const funds = new Resource<FundReferenceDocument>();
  const events = new Resource<IndexEventsDocument>();

  const stats = $derived.by<Stat[]>(() => {
    if (section === 'historical') {
      const doc = historical.data;
      return [
        { label: '兼容记录', value: count(doc?.match_count) },
        { label: '当前仍存在', value: count(doc?.summary.current_directory_present) },
        { label: '当前已缺席', value: count(doc?.summary.absent_from_current_directory) },
        { label: '历史 / 当前异名', value: count(doc?.summary.name_differs_from_current) }
      ];
    }
    if (section === 'funds') {
      const doc = funds.data;
      return [
        { label: '匹配记录', value: count(doc?.match_count) },
        { label: '基金快照', value: count(doc?.summary.fund_snapshots) },
        { label: 'ETF / LOF 映射', value: `${count(doc?.summary.etf_mappings)} / ${count(doc?.summary.lof_mappings)}` },
        { label: '生命周期未给定', value: count(doc?.summary.native_status_unset) }
      ];
    }
    const doc = events.data;
    return [
      { label: '指数事件', value: count(doc?.match_count) },
      { label: '上证 / 恒生 / 纳指', value: `${count(doc?.summary.shanghai_composite)} / ${count(doc?.summary.hang_seng)} / ${count(doc?.summary.nasdaq_composite)}` },
      { label: '休市日对齐', value: count(doc?.summary.chart_date_adjusted) },
      { label: '日期范围', value: `${date(doc?.summary.earliest_date)} — ${date(doc?.summary.latest_date)}` }
    ];
  });

  const historicalColumns: Column<HistoricalSecurityRecord>[] = [
    { key: 'security', label: '证券', width: '160px', slot: true },
    { key: 'compatibility', label: '兼容名称', width: '130px', value: (row) => row.compatibility_name },
    { key: 'current', label: '当前名称', width: '130px', value: (row) => row.current_name || '—' },
    { key: 'presence', label: '当前目录', width: '100px', value: (row) => row.current_directory_present ? '存在' : '缺席' },
    { key: 'different', label: '名称变化', width: '92px', value: (row) => row.name_differs_from_current ? '已变化' : '—' },
    { key: 'line', label: '源行', width: '72px', align: 'right', num: true, value: (row) => count(row.source_line) }
  ];

  const fundColumns: Column<FundReferenceRecord>[] = [
    { key: 'security', label: '基金', width: '166px', slot: true },
    { key: 'kind', label: '记录类型', width: '112px', value: (row) =>
      row.kind === 'fund-snapshot' ? '份额 / 净值' : row.kind === 'etf-mapping' ? 'ETF 映射' : 'LOF 映射' },
    { key: 'reference', label: '跟踪标的', width: '170px', wrap: true, value: (row) =>
      row.kind === 'fund-snapshot' ? '—' :
      row.reference_instrument ? `${row.reference_instrument.market.toUpperCase()} ${row.reference_instrument.code}` : '未给定' },
    { key: 'units', label: '基金份额', width: '112px', align: 'right', num: true, value: (row) =>
      row.kind === 'fund-snapshot' ? compact(row.fund_units) : '—' },
    { key: 'reference-value', label: '交易参考值', width: '100px', align: 'right', num: true, value: (row) =>
      row.kind === 'fund-snapshot' ? fixed(row.unit_reference_value, 4) : '—' },
    { key: 'nav', label: '已发布净值', width: '100px', align: 'right', num: true, value: (row) =>
      row.kind === 'fund-snapshot' ? fixed(row.unit_nav, 4) : '—' },
    { key: 'window', label: '原生窗口', width: '176px', value: (row) =>
      row.kind === 'fund-snapshot' ? date(row.as_of_date) :
      row.dates ? `${date(row.dates.window_start)} — ${date(row.dates.window_end)}` : '无日期窗口' },
    { key: 'phase', label: '生命周期', width: '98px', value: (row) =>
      row.kind === 'fund-snapshot' ? '快照' :
      row.lifecycle_phase === 'before-window' ? '窗口前' :
      row.lifecycle_phase === 'in-window' ? '窗口内' :
      row.lifecycle_phase === 'after-window' ? '窗口后' : '未给定' }
  ];

  const eventColumns: Column<IndexEventRecord>[] = [
    { key: 'event', label: '事件', width: '390px', wrap: true, slot: true },
    { key: 'benchmark', label: '基准', width: '106px', value: (row) =>
      row.benchmark === 'shanghai-composite' ? '上证指数' :
      row.benchmark === 'hang-seng' ? '恒生指数' : '纳斯达克' },
    { key: 'occurrence', label: '发生日', width: '96px', num: true, value: (row) => date(row.occurrence_date) },
    { key: 'chart', label: '图表交易日', width: '112px', num: true, value: (row) => date(row.chart_date) },
    { key: 'adjusted', label: '休市对齐', width: '86px', value: (row) => row.chart_date_adjusted ? '是' : '—' },
    { key: 'id', label: '事件编号', width: '92px', align: 'right', num: true, value: (row) => count(row.event_id) }
  ];

  function canOpenSecurity(
    security: HistoricalSecurityRecord['security'] | FundReferenceRecord['security'],
    current = true
  ): boolean {
    return current && (security.market === 'sz' || security.market === 'sh' ||
      security.market === 'bj') && /^\d{6}$/.test(security.code);
  }

  function openSecurity(
    security: HistoricalSecurityRecord['security'] | FundReferenceRecord['security']
  ) {
    if (!canOpenSecurity(security)) return;
    app.setStock({
      market: security.market,
      code: security.code,
      name: security.name || security.security_id
    });
    router.go(stockPath(security.market, security.code));
  }

  function safeDetailUrl(value: string): string | null {
    return /^https?:\/\//i.test(value) ? value : null;
  }

  function loadHistorical(reset = true) {
    if (reset) historicalOffset = 0;
    return historical.load(`/api/v1/market/historical-securities?${queryString({
      market: historicalMarket === 'all' ? '' : historicalMarket,
      q: historicalQuery.trim(),
      presence: historicalPresence,
      sort: historicalSort,
      order: historicalOrder,
      offset: historicalOffset,
      limit: Number(historicalLimit)
    })}`);
  }

  function loadFunds() {
    return funds.load(`/api/v1/market/fund-reference?${queryString({
      view: fundView,
      market: fundMarket === 'all' ? '' : fundMarket,
      q: fundQuery.trim(),
      as_of_date: fundAsOfDate.trim(),
      limit: Number(fundLimit)
    })}`);
  }

  function loadEvents(reset = true) {
    if (reset) eventOffset = 0;
    return events.load(`/api/v1/market/index-events?${queryString({
      benchmark,
      q: eventQuery.trim(),
      from: eventFrom.trim(),
      to: eventTo.trim(),
      date_basis: eventDateBasis,
      sort: eventSort,
      order: eventOrder,
      offset: eventOffset,
      limit: Number(eventLimit)
    })}`);
  }

  function switchSection(value: string) {
    section = value as Section;
    if (section === 'historical' && !historical.loaded) void loadHistorical();
    else if (section === 'funds' && !funds.loaded) void loadFunds();
    else if (section === 'events' && !events.loaded) void loadEvents();
  }

  function previousHistorical() {
    historicalOffset = Math.max(0, historicalOffset - Number(historicalLimit));
    void loadHistorical(false);
  }

  function nextHistorical() {
    if (historical.data?.next_offset == null) return;
    historicalOffset = historical.data.next_offset;
    void loadHistorical(false);
  }

  function previousEvents() {
    eventOffset = Math.max(0, eventOffset - Number(eventLimit));
    void loadEvents(false);
  }

  function nextEvents() {
    if (events.data?.next_offset == null) return;
    eventOffset = events.data.next_offset;
    void loadEvents(false);
  }

  onMount(() => void loadHistorical());
</script>

<PageHeader
  eyebrow="TDX LOCAL CACHE · ZERO NETWORK"
  title="本地参考档案"
  description="对账历史证券兼容名称、基金份额与 ETF/LOF 标的关系，并浏览原生指数图事件注记。"
  {stats}
>
  {#snippet actions()}
    <Select options={SECTIONS} value={section} width="174px" label="档案" onChange={switchSection} />
  {/snippet}
</PageHeader>

{#if section === 'historical'}
  <Panel
    title="历史证券兼容名称"
    subtitle={historical.data ? `${count(historical.data.returned)} / ${count(historical.data.match_count)} · 只有仍在当前目录的证券可打开` : 'pttab.dat · 本地只读'}
    busy={historical.busy}
    error={historical.error}
    onRetry={() => void loadHistorical(false)}
    empty={historical.loaded && (historical.data?.records.length ?? 0) === 0}
    emptyText="当前筛选没有兼容名称记录。"
    flush scroll fill
  >
    {#snippet toolbar()}
      <Select options={MARKETS} value={historicalMarket} width="120px" label="市场" onChange={(value) => { historicalMarket = value; void loadHistorical(); }} />
      <Select options={PRESENCE} value={historicalPresence} width="150px" label="目录状态" onChange={(value) => { historicalPresence = value; void loadHistorical(); }} />
      <Select options={HISTORICAL_SORT} value={historicalSort} width="120px" label="排序" onChange={(value) => { historicalSort = value; void loadHistorical(); }} />
      <Select options={ORDERS} value={historicalOrder} width="84px" label="顺序" onChange={(value) => { historicalOrder = value; void loadHistorical(); }} />
      <Select options={PAGE_LIMITS} value={historicalLimit} width="110px" label="分页" onChange={(value) => { historicalLimit = value; void loadHistorical(); }} />
      <TextInput bind:value={historicalQuery} icon="search" width="220px" label="检索" placeholder="代码 / 兼容名 / 当前名" onEnter={() => void loadHistorical()} />
      <Button icon="search" onclick={() => void loadHistorical()}>查询</Button>
      <div class="pager">
        <Button icon="chevron-left" disabled={historical.busy || historicalOffset === 0} onclick={previousHistorical} />
        <span>第 {count(historicalOffset / Number(historicalLimit) + 1)} 页</span>
        <Button icon="chevron-right" disabled={historical.busy || !historical.data?.has_more} onclick={nextHistorical} />
      </div>
    {/snippet}
    <DataTable columns={historicalColumns} rows={historical.data?.records ?? []} numbered stickyFirst minWidth="880px" rowKey={(row) => row.record_id}>
      {#snippet cell({ row, column })}
        {#if column.key === 'security'}
          {#if canOpenSecurity(row.security, row.current_directory_present)}
            <button class="security-link" type="button" onclick={() => openSecurity(row.security)}>
              <span>{row.current_name || row.compatibility_name}</span>
              <small>{row.security.security_id}</small>
            </button>
          {:else}
            <span class="security-static">
              <span>{row.compatibility_name}</span>
              <small>{row.security.security_id} · 历史兼容记录</small>
            </span>
          {/if}
        {/if}
      {/snippet}
    </DataTable>
  </Panel>
{:else if section === 'funds'}
  <Panel
    title="基金参考与标的映射"
    subtitle={funds.data ? `${count(funds.data.returned)} / ${count(funds.data.match_count)} · 空日期表示宿主未给定生命周期窗口` : 'specjjdata / specetfdata / speclofdata · 本地只读'}
    busy={funds.busy}
    error={funds.error}
    onRetry={() => void loadFunds()}
    empty={funds.loaded && (funds.data?.records.length ?? 0) === 0}
    emptyText="当前筛选没有基金参考记录。"
    flush scroll fill
  >
    {#snippet toolbar()}
      <Select options={FUND_VIEWS} value={fundView} width="156px" label="视图" onChange={(value) => { fundView = value; void loadFunds(); }} />
      <Select options={FUND_MARKETS} value={fundMarket} width="112px" label="市场" onChange={(value) => { fundMarket = value; void loadFunds(); }} />
      <TextInput bind:value={fundAsOfDate} width="128px" label="观察日" placeholder="YYYYMMDD" onEnter={() => void loadFunds()} />
      <Select options={FUND_LIMITS} value={fundLimit} width="116px" label="数量" onChange={(value) => { fundLimit = value; void loadFunds(); }} />
      <TextInput bind:value={fundQuery} icon="search" width="240px" label="检索" placeholder="基金 / 标的 / 目录 ID" onEnter={() => void loadFunds()} />
      <Button icon="search" onclick={() => void loadFunds()}>查询</Button>
    {/snippet}
    <DataTable columns={fundColumns} rows={funds.data?.records ?? []} numbered stickyFirst minWidth="1180px" rowKey={(row) => `${row.kind}:${row.security.security_id}:${row.source_file}`}>
      {#snippet cell({ row, column })}
        {#if column.key === 'security'}
          {#if canOpenSecurity(row.security, row.security.name_resolved)}
            <button class="security-link" type="button" onclick={() => openSecurity(row.security)}>
              <span>{row.security.name || row.security.code}</span>
              <small>{row.security.security_id}</small>
            </button>
          {:else}
            <span class="security-static">
              <span>{row.security.name || row.security.code}</span>
              <small>{row.security.security_id} · 当前目录未解析</small>
            </span>
          {/if}
        {/if}
      {/snippet}
    </DataTable>
  </Panel>
{:else}
  <Panel
    title="指数图重大事件"
    subtitle={events.data ? `${count(events.data.returned)} / ${count(events.data.match_count)} · 发生日与目标市场交易日分别保留` : 'speczsevent · 本地注记，不主动请求正文'}
    busy={events.busy}
    error={events.error}
    onRetry={() => void loadEvents(false)}
    empty={events.loaded && (events.data?.records.length ?? 0) === 0}
    emptyText="当前筛选没有指数图事件。"
    flush scroll fill
  >
    {#snippet toolbar()}
      <Select options={BENCHMARKS} value={benchmark} width="128px" label="基准" onChange={(value) => { benchmark = value as IndexEventBenchmark; void loadEvents(); }} />
      <Select options={DATE_BASIS} value={eventDateBasis} width="128px" label="日期口径" onChange={(value) => { eventDateBasis = value; void loadEvents(); }} />
      <TextInput bind:value={eventFrom} width="116px" label="开始" placeholder="YYYYMMDD" onEnter={() => void loadEvents()} />
      <TextInput bind:value={eventTo} width="116px" label="结束" placeholder="YYYYMMDD" onEnter={() => void loadEvents()} />
      <Select options={EVENT_SORT} value={eventSort} width="112px" label="排序" onChange={(value) => { eventSort = value; void loadEvents(); }} />
      <Select options={ORDERS} value={eventOrder} width="84px" label="顺序" onChange={(value) => { eventOrder = value; void loadEvents(); }} />
      <Select options={PAGE_LIMITS} value={eventLimit} width="110px" label="分页" onChange={(value) => { eventLimit = value; void loadEvents(); }} />
      <TextInput bind:value={eventQuery} icon="search" width="220px" label="检索" placeholder="标题 / 事件编号" onEnter={() => void loadEvents()} />
      <Button icon="search" onclick={() => void loadEvents()}>查询</Button>
      <div class="pager">
        <Button icon="chevron-left" disabled={events.busy || eventOffset === 0} onclick={previousEvents} />
        <span>第 {count(eventOffset / Number(eventLimit) + 1)} 页</span>
        <Button icon="chevron-right" disabled={events.busy || !events.data?.has_more} onclick={nextEvents} />
      </div>
    {/snippet}
    <DataTable columns={eventColumns} rows={events.data?.records ?? []} numbered stickyFirst minWidth="980px" rowKey={(row) => row.record_id}>
      {#snippet cell({ row, column })}
        {#if column.key === 'event'}
          {#if safeDetailUrl(row.detail_url)}
            <a class="event-link" href={safeDetailUrl(row.detail_url) ?? undefined} target="_blank" rel="noreferrer">
              {row.title}
            </a>
          {:else}
            <span>{row.title}</span>
          {/if}
        {/if}
      {/snippet}
    </DataTable>
  </Panel>
{/if}

<p class="boundary-note">
  三类数据均从服务启动时固定的 TDX 根只读加载，网络请求数为 0；页面不展示资源路径，也不提供文件、URL 或路径参数。
</p>

<style>
  .pager { display: inline-flex; align-items: center; gap: var(--sp-2); margin-left: auto; }
  .pager span { min-width: 54px; font-family: var(--font-num); font-size: var(--fs-micro); color: var(--fg-mute); text-align: center; }
  .security-link, .security-static { display: inline-flex; min-width: 0; flex-direction: column; align-items: flex-start; line-height: 1.2; text-align: left; }
  .security-link { color: var(--focus); }
  .security-link:hover span:first-child, .event-link:hover { text-decoration: underline; }
  .security-link small, .security-static small { margin-top: 1px; font-family: var(--font-num); font-size: 9px; color: var(--fg-mute); }
  .security-static { color: var(--fg-dim); }
  .event-link { color: var(--focus); }
  .boundary-note { margin: var(--sp-3) var(--sp-4) 0; color: var(--fg-mute); font-size: var(--fs-micro); }
  @media (max-width: 1100px) { .pager { margin-left: 0; } }
</style>
