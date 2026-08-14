<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import HkShortVolumeChart from '../../charts/HkShortVolumeChart.svelte';
  import { compact, count, date, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import Spinner from '../../ui/Spinner.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { HkEventRecord, MarketHkEventsDocument, MarketHkShortHistoryDocument } from '../../types';

  type View = MarketHkEventsDocument['view'];

  const VIEWS = [
    { id: 'all', label: '全部' },
    { id: 'dividends', label: '分红派息' },
    { id: 'holdings', label: '权益披露' },
    { id: 'short-selling', label: '沽空统计' },
    { id: 'applications', label: '上市申请' }
  ];

  let view = $state<View>('all');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketHkEventsDocument>();
  const shortHistory = new Resource<MarketHkShortHistoryDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.rows ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);
  const selectedShortKey = $derived.by(() => {
    if (selected?.kind !== 'short-selling' || !selected.security) return '';
    const market = selected.security.market_id;
    return market === 31 || market === 48 ? `${market}:${selected.security.code}` : '';
  });
  let loadedShortKey = $state('');

  const stats = $derived.by<Stat[]>(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return [
      { label: '分红派息', value: count(summary.dividends) },
      { label: '权益披露', value: count(summary.holding_disclosures) },
      { label: '沽空记录', value: count(summary.short_selling) },
      { label: '上市申请', value: count(summary.listing_applications) },
      { label: '覆盖证券', value: count(summary.unique_securities) },
      { label: '数据范围', value: `${date(summary.earliest_date)} — ${date(summary.latest_date)}` }
    ];
  });

  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/hk-events?${queryString({
      view,
      q: query.trim(),
      limit: 10000,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function switchView(next: string) {
    view = next as View;
    load();
  }

  function loadShortHistory(refresh = false) {
    if (!selectedShortKey) return;
    const [market, code] = selectedShortKey.split(':');
    void shortHistory.load(`/api/v1/market/hk-short-history?${queryString({
      market,
      code,
      pages: 1,
      page_size: 800,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function securityName(row: HkEventRecord): string {
    return row.security?.name || row.security?.code || row.company || row.title;
  }

  function securitySub(row: HkEventRecord): string {
    return row.security?.security_id || `${row.board || '港交所'} · ${row.listing_type || '上市申请'}`;
  }

  function currencyAmount(value: number | null | undefined, currency = ''): string {
    const body = compact(value);
    return body === '—' ? body : `${body} ${currency}`.trim();
  }

  const allColumns: Column<HkEventRecord>[] = [
    { key: 'kind', label: '事件', width: '86px', value: (row) => row.kind_label, sortValue: (row) => row.kind },
    { key: 'security', label: '证券 / 公司', width: '150px', value: securityName, sub: securitySub },
    { key: 'date', label: '日期', width: '88px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'title', label: '事件摘要', wrap: true, value: (row) => row.plan || row.disclosure_reason || row.status || row.title },
    { key: 'metric', label: '关键数值', align: 'right', num: true, value: (row) => {
      if (row.kind === 'holding-disclosure') return `${compact(row.changed_shares, '股')} · ${percent(row.holding_after_pct)}`;
      if (row.kind === 'short-selling') return `${currencyAmount(row.short_amount_currency_units, row.currency)} · ${percent(row.short_turnover_pct)}`;
      if (row.kind === 'listing-application') return currencyAmount(row.last_year_revenue_currency_units, row.currency);
      return date(row.dates?.ex_dividend);
    }}
  ];

  const dividendColumns: Column<HkEventRecord>[] = [
    { key: 'security', label: '证券', width: '140px', value: securityName, sub: securitySub },
    { key: 'announcement', label: '公告日', width: '86px', num: true, value: (row) => date(row.dates?.announcement), sortValue: (row) => row.dates?.announcement || '' },
    { key: 'fiscal', label: '财政年度', width: '86px', num: true, value: (row) => date(row.dates?.fiscal_year_end) },
    { key: 'ex', label: '除净日', width: '86px', num: true, value: (row) => date(row.dates?.ex_dividend), sortValue: (row) => row.dates?.ex_dividend || '' },
    { key: 'payment', label: '派息日', width: '86px', num: true, value: (row) => date(row.dates?.payment), sortValue: (row) => row.dates?.payment || '' },
    { key: 'plan', label: '分配方案', wrap: true, value: (row) => text(row.plan) }
  ];

  const holdingColumns: Column<HkEventRecord>[] = [
    { key: 'security', label: '证券', width: '140px', value: securityName, sub: securitySub },
    { key: 'date', label: '披露日', width: '86px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'investor', label: '投资者', width: '190px', wrap: true, value: (row) => text(row.investor), sub: (row) => text(row.position) },
    { key: 'change', label: '变动股份', align: 'right', num: true, value: (row) => compact(row.changed_shares, '股'), tone: (row) => tone(row.changed_shares), sortValue: (row) => row.changed_shares ?? 0 },
    { key: 'after', label: '变动后持股', align: 'right', num: true, value: (row) => compact(row.holding_after_shares, '股'), sub: (row) => percent(row.holding_after_pct), sortValue: (row) => row.holding_after_shares ?? 0 },
    { key: 'reason', label: '披露原因', wrap: true, value: (row) => text(row.disclosure_reason) }
  ];

  const shortColumns: Column<HkEventRecord>[] = [
    { key: 'security', label: '证券', width: '140px', value: securityName, sub: securitySub },
    { key: 'date', label: '记录日', width: '86px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'shares', label: '沽空股数', align: 'right', num: true, value: (row) => compact(row.short_shares, '股'), sortValue: (row) => row.short_shares ?? 0 },
    { key: 'amount', label: '沽空金额', align: 'right', num: true, value: (row) => currencyAmount(row.short_amount_currency_units, row.currency), sortValue: (row) => row.short_amount_currency_units ?? 0 },
    { key: 'turnover', label: '成交金额', align: 'right', num: true, value: (row) => currencyAmount(row.turnover_currency_units, row.currency), sortValue: (row) => row.turnover_currency_units ?? 0 },
    { key: 'ratio', label: '沽空占比', align: 'right', num: true, value: (row) => percent(row.short_turnover_pct), sortValue: (row) => row.short_turnover_pct ?? 0 },
    { key: 'session', label: '统计时间', width: '88px', value: (row) => text(row.session) }
  ];

  const applicationColumns: Column<HkEventRecord>[] = [
    { key: 'company', label: '申请公司', width: '210px', wrap: true, value: (row) => text(row.company), sub: (row) => `${text(row.board)} · ${text(row.listing_type)}` },
    { key: 'filing', label: '申报日期', width: '86px', num: true, value: (row) => date(row.filing_date), sortValue: (row) => row.filing_date || '' },
    { key: 'status', label: '状态', width: '86px', value: (row) => text(row.status), sub: (row) => date(row.status_date) },
    { key: 'revenue', label: '上年营收', align: 'right', num: true, value: (row) => currencyAmount(row.last_year_revenue_currency_units, row.currency), sortValue: (row) => row.last_year_revenue_currency_units ?? 0 },
    { key: 'profit', label: '上年净利', align: 'right', num: true, value: (row) => currencyAmount(row.last_year_profit_currency_units, row.currency), tone: (row) => tone(row.last_year_profit_currency_units), sortValue: (row) => row.last_year_profit_currency_units ?? 0 },
    { key: 'sponsors', label: '保荐人', width: '260px', wrap: true, value: (row) => text(row.sponsors) }
  ];

  const columns = $derived.by<Column<HkEventRecord>[]>(() => {
    if (view === 'dividends') return dividendColumns;
    if (view === 'holdings') return holdingColumns;
    if (view === 'short-selling') return shortColumns;
    if (view === 'applications') return applicationColumns;
    return allColumns;
  });

  const selectedStats = $derived.by<Stat[]>(() => {
    if (!selected) return [];
    if (selected.kind === 'dividend') return [
      { label: '公告日', value: date(selected.dates?.announcement) },
      { label: '财政年度', value: date(selected.dates?.fiscal_year_end) },
      { label: '除净日', value: date(selected.dates?.ex_dividend) },
      { label: '派息日', value: date(selected.dates?.payment) }
    ];
    if (selected.kind === 'holding-disclosure') return [
      { label: '变动股份', value: compact(selected.changed_shares, '股'), tone: tone(selected.changed_shares) },
      { label: '变动后持股', value: compact(selected.holding_after_shares, '股') },
      { label: '持股比例', value: percent(selected.holding_after_pct) },
      { label: '好淡仓', value: text(selected.position) }
    ];
    if (selected.kind === 'short-selling') return [
      { label: '沽空股数', value: compact(selected.short_shares, '股') },
      { label: '沽空金额', value: currencyAmount(selected.short_amount_currency_units, selected.currency) },
      { label: '成交金额', value: currencyAmount(selected.turnover_currency_units, selected.currency) },
      { label: '沽空占比', value: percent(selected.short_turnover_pct) }
    ];
    return [
      { label: '申报日期', value: date(selected.filing_date) },
      { label: '状态日期', value: date(selected.status_date) },
      { label: '上年营收', value: currencyAmount(selected.last_year_revenue_currency_units, selected.currency) },
      { label: '上年净利', value: currencyAmount(selected.last_year_profit_currency_units, selected.currency), tone: tone(selected.last_year_profit_currency_units) }
    ];
  });

  onMount(() => load());

  $effect(() => {
    const key = selectedShortKey;
    if (!key) {
      loadedShortKey = '';
      shortHistory.reset();
    } else if (key !== loadedShortKey) {
      loadedShortKey = key;
      shortHistory.reset();
      loadShortHistory();
    }
  });
</script>

<PageHeader
  eyebrow="GGRL102—105 · HKEX EVENTS"
  title="港股事件库"
  description="通达信港股分红派息、权益披露、每日沽空统计和上市申请队列；数量与金额已从原始万/千单位统一换算。"
  {stats}
>
  {#snippet actions()}
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="港股事件类型" />
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="390px">
  {#snippet main()}
    <Panel
      title="港股公开事件"
      subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.sources.length)} 个原始资源` : 'GGRL102—105'}
      busy={resource.busy}
      error={resource.error}
      onRetry={() => load()}
      empty={resource.loaded && !resource.busy && rows.length === 0}
      emptyText="当前筛选下没有港股事件。"
      flush
      scroll
    >
      {#snippet toolbar()}
        <TextInput bind:value={query} icon="search" width="260px" label="检索事件" placeholder="证券、投资者、公司、状态或业务" onEnter={() => load()} />
      {/snippet}
      <DataTable
        {columns}
        {rows}
        rowKey={(row) => row.event_id}
        onRowClick={(row) => (selectedId = row.event_id)}
        isActive={(row) => row.event_id === selected?.event_id}
        stickyFirst
        numbered
        minWidth={view === 'applications' ? '1050px' : view === 'all' ? '900px' : '980px'}
      />
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      eyebrow={selected?.kind_label ?? 'EVENT DETAIL'}
      title={selected ? securityName(selected) : '事件详情'}
      subtitle={selected ? `${date(selected.date)} · ${selected.source_resource}` : '点击左侧记录展开'}
      empty={!selected && !resource.busy}
      emptyText="选择一条记录查看完整字段与原始口径。"
      scroll
    >
      {#if selected}
        <div class="badge-line"><Badge tone="focus">{selected.kind_label}</Badge></div>
        <StatGrid stats={selectedStats} inline />
        {#if selected.kind === 'dividend'}
          <h3>分配方案</h3><p>{text(selected.plan)}</p>
          <p class="note">登记期：{date(selected.dates?.register_start)} — {date(selected.dates?.register_end)}</p>
        {:else if selected.kind === 'holding-disclosure'}
          <h3>{text(selected.investor)}</h3><p>{text(selected.disclosure_reason)}</p>
        {:else if selected.kind === 'short-selling'}
          <p class="note">统计时点：{text(selected.session)}。原表数量和金额均以万为单位，页面展示值已乘以 10,000。</p>
          <div class="history-heading">
            <h3>历史沽空量</h3>
            <Button icon="refresh" busy={shortHistory.busy} onclick={() => loadShortHistory(true)}>刷新历史</Button>
          </div>
          {#if shortHistory.busy && !shortHistory.data}
            <p class="loading"><Spinner />正在读取 7727 港股日 K 辅助字段…</p>
          {:else if shortHistory.error}
            <p class="error">{shortHistory.error}</p>
          {:else if shortHistory.data}
            <div class="badge-line">
              <Badge tone={shortHistory.data.reconciliation.all_overlaps_exact ? 'up' : 'warn'}>
                GGRL104 对账 {count(shortHistory.data.reconciliation.exact_match_count)}/{count(shortHistory.data.reconciliation.overlap_day_count)}
              </Badge>
              <Badge tone="neutral">{count(shortHistory.data.summary.history_count)} 日</Badge>
            </div>
            <StatGrid inline stats={[
              { label: '最新沽空', value: compact(shortHistory.data.summary.latest_short_shares, '股') },
              { label: '占成交股数', value: percent(shortHistory.data.summary.latest_short_share_volume_pct) },
              { label: '最新日期', value: date(shortHistory.data.summary.latest_date) }
            ]} />
            <HkShortVolumeChart points={shortHistory.data.history} />
            <p class="note">柱线为沽空股数，MA5/MA20 为本地滚动均值。成交量按客户端港股 100 股/手换算；“占成交股数”不是按金额计算的沽空成交额占比。</p>
          {/if}
        {:else}
          <h3>保荐人</h3><p>{text(selected.sponsors)}</p>
          <h3>控股股东</h3><p>{text(selected.controlling_shareholders)}</p>
          <h3>主营业务</h3><p class="copy">{text(selected.business)}</p>
          <p class="note">上市申请原表财务值以千个币种单位记录，页面展示值已乘以 1,000；币种保留上游原值。</p>
        {/if}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .badge-line { margin-bottom: var(--sp-3); }
  .history-heading { display: flex; align-items: center; justify-content: space-between; gap: var(--sp-2); margin-top: var(--sp-4); }
  .history-heading h3 { margin: 0; }
  .loading { display: flex; align-items: center; gap: var(--sp-2); color: var(--fg-mute); }
  .error { color: var(--warn); }
  h3 { margin: var(--sp-4) 0 var(--sp-2); font-size: var(--fs-micro); font-weight: 600; color: var(--fg-dim); }
  p { font-size: var(--fs-xs); line-height: 1.65; }
  .copy { white-space: pre-wrap; }
  .note { margin-top: var(--sp-3); color: var(--fg-mute); font-size: 10px; }
</style>
