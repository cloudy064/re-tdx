<script lang="ts">
  /** 7727 expansion-market instrument directory, quote, timeline and trades. */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, fixed, percent, price, tone } from '../../lib/fmt';
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
    ExpansionInstrument,
    ExpansionQuoteDocument,
    ExpansionTimelineDocument,
    ExpansionTimelinePoint,
    ExpansionTrade,
    ExpansionTradesDocument,
    ExpansionInstrumentsDocument
  } from '../../types';

  interface DepthRow {
    level: number;
    bidPrice: number;
    bidVolume: number;
    askPrice: number;
    askVolume: number;
  }

  const DIRECTORY_COUNTS = [20, 100, 500, 1000].map((value) => ({
    id: String(value), label: `抓取 ${value} 条`
  }));
  const TRADE_PAGE_SIZES = [50, 100, 200, 500, 1000, 1800].map((value) => ({
    id: String(value), label: `每页 ${value} 条`
  }));
  const MARKET_ALIASES: Record<string, number> = {
    qz: 28, qd: 29, qs: 30, cz: 47, qg: 66
  };

  let market = $state('29');
  let code = $state('A2609');
  let selectedName = $state('豆一 2609');
  let namedSecurity = $state('29:A2609');
  let tradingDate = $state('');
  let tradeStart = $state('0');
  let tradePageSize = $state('200');
  let selectionError = $state('');

  let directoryStart = $state('0');
  let directoryCount = $state('100');
  let directoryMarket = $state('');
  let directoryQuery = $state('');
  let directoryError = $state('');

  const directory = new Resource<ExpansionInstrumentsDocument>();
  const quote = new Resource<ExpansionQuoteDocument>();
  const timeline = new Resource<ExpansionTimelineDocument>();
  const trades = new Resource<ExpansionTradesDocument>();

  const instruments = $derived(directory.data?.instruments ?? []);
  const timelineRows = $derived(timeline.data?.points ?? []);
  const tradeRows = $derived(trades.data?.trades ?? []);
  const depthRows = $derived.by<DepthRow[]>(() => {
    const document = quote.data;
    if (!document) return [];
    return Array.from({ length: 5 }, (_, index) => ({
      level: index + 1,
      bidPrice: document.bids[index]?.price ?? 0,
      bidVolume: document.bids[index]?.volume ?? 0,
      askPrice: document.asks[index]?.price ?? 0,
      askVolume: document.asks[index]?.volume ?? 0
    }));
  });

  function validatedMarket(value: string, allowEmpty = false): string {
    const normalized = value.trim().toLowerCase();
    if (allowEmpty && normalized === '') return '';
    if (normalized in MARKET_ALIASES) return String(MARKET_ALIASES[normalized]);
    if (!/^\d{1,3}$/.test(normalized))
      throw new Error('市场必须是 qz/qd/qs/cz/qg 或 3..255 的数字 ID。');
    const numeric = Number(normalized);
    if (numeric < 3 || numeric > 255)
      throw new Error('扩展市场 ID 必须在 3..255。');
    return String(numeric);
  }

  function validatedCode(value: string): string {
    const normalized = value.trim().toUpperCase();
    if (!/^[A-Z0-9](?:[A-Z0-9 ]{0,7}[A-Z0-9])?$/.test(normalized))
      throw new Error('代码必须是 1..9 位 ASCII 字母/数字；空格只能出现在内部。');
    return normalized;
  }

  function validatedDate(value: string): string {
    const normalized = value.trim();
    if (normalized !== '' && !/^\d{8}$/.test(normalized))
      throw new Error('交易日必须是 YYYYMMDD，留空表示当前会话。');
    return normalized;
  }

  function integerInRange(value: string, label: string, minimum: number, maximum: number): number {
    if (!/^\d+$/.test(value.trim())) throw new Error(`${label}必须是整数。`);
    const numeric = Number(value);
    if (!Number.isSafeInteger(numeric) || numeric < minimum || numeric > maximum)
      throw new Error(`${label}必须在 ${minimum}..${maximum}。`);
    return numeric;
  }

  async function loadSelected(resetTrades = true) {
    try {
      selectionError = '';
      const nextMarket = validatedMarket(market);
      const nextCode = validatedCode(code);
      const nextDate = validatedDate(tradingDate);
      const nextSecurity = `${nextMarket}:${nextCode}`;
      if (nextSecurity !== namedSecurity) {
        selectedName = nextCode;
        namedSecurity = nextSecurity;
      }
      if (resetTrades) tradeStart = '0';
      const nextStart = integerInRange(tradeStart, '逐笔起点', 0, 2147483647);
      const pageSize = integerInRange(tradePageSize, '逐笔页长', 1, 1800);
      const switching = quote.data?.market !== nextMarket || quote.data?.code !== nextCode;
      market = nextMarket;
      code = nextCode;
      tradingDate = nextDate;
      if (switching) {
        quote.reset();
        timeline.reset();
        trades.reset();
      }
      await Promise.all([
        quote.load(`/api/v1/market/snapshot?${queryString({ market, code })}`),
        timeline.load(`/api/v1/market/expansion-timeline?${queryString({
          market, code, date: tradingDate
        })}`),
        trades.load(`/api/v1/market/expansion-trades?${queryString({
          market, code, date: tradingDate, start: nextStart, page_size: pageSize, pages: 1
        })}`)
      ]);
    } catch (error) {
      selectionError = error instanceof Error ? error.message : String(error);
    }
  }

  async function loadDirectory(startOverride?: number) {
    try {
      directoryError = '';
      const start = startOverride ?? integerInRange(directoryStart, '目录起点', 0, 1000000);
      const requested = integerInRange(directoryCount, '目录抓取数', 1, 200000);
      const marketFilter = validatedMarket(directoryMarket, true);
      directoryStart = String(start);
      directory.reset();
      await directory.load(`/api/v1/market/instruments?${queryString({
        start,
        count: requested,
        market: marketFilter,
        query: directoryQuery.trim()
      })}`);
    } catch (error) {
      directoryError = error instanceof Error ? error.message : String(error);
    }
  }

  function selectInstrument(row: ExpansionInstrument) {
    market = String(row.market_id);
    code = row.code;
    selectedName = row.name || row.description || row.security;
    namedSecurity = `${row.market_id}:${row.code}`;
    void loadSelected(true);
  }

  function previousDirectoryPage() {
    const size = Number(directory.data?.fetched || directoryCount);
    void loadDirectory(Math.max(0, Number(directoryStart) - size));
  }

  function nextDirectoryPage() {
    if (directory.data) void loadDirectory(directory.data.next_start);
  }

  function previousTradePage() {
    const size = Number(tradePageSize);
    tradeStart = String(Math.max(0, Number(tradeStart) - size));
    void loadSelected(false);
  }

  function nextTradePage() {
    if (!trades.data) return;
    tradeStart = String(trades.data.next_start);
    void loadSelected(false);
  }

  const stats = $derived.by<Stat[]>(() => {
    const snapshot = quote.data;
    if (!snapshot) return [];
    return [
      { label: '最新价', value: price(snapshot.price), note: `${fixed(snapshot.change, 2)} · ${percent(snapshot.change_percent, 2, true)}`, tone: tone(snapshot.change) },
      { label: '今高 / 今低', value: `${price(snapshot.high)} / ${price(snapshot.low)}` },
      { label: '成交量', value: compact(snapshot.volume), note: snapshot.volume_unit },
      { label: '持仓量', value: compact(snapshot.open_interest) },
      { label: '分时点', value: count(timeline.data?.wire_count), note: timeline.data?.mode === 'historical' ? '历史会话' : '当前会话' },
      { label: '逐笔', value: count(trades.data?.downloaded), note: `起点 ${count(trades.data?.start)}` }
    ];
  });

  const instrumentColumns: Column<ExpansionInstrument>[] = [
    { key: 'security', label: '合约', width: '150px', value: (row) => row.name || row.code, sub: (row) => row.security },
    { key: 'description', label: '说明', width: '150px', value: (row) => row.description || '—' },
    { key: 'market', label: '市场 ID', width: '75px', align: 'right', num: true, value: (row) => count(row.market_id) },
    { key: 'category', label: '类别 raw', width: '80px', align: 'right', num: true, value: (row) => count(row.category) },
    { key: 'multiplier', label: '合约乘数', width: '90px', align: 'right', num: true, value: (row) => count(row.contract_multiplier) },
    { key: 'action', label: '行情', width: '75px', align: 'center', value: () => '查看' }
  ];

  const depthColumns: Column<DepthRow>[] = [
    { key: 'bid-volume', label: '买量', align: 'right', num: true, value: (row) => compact(row.bidVolume) },
    { key: 'bid-price', label: '买价', align: 'right', num: true, value: (row) => price(row.bidPrice) },
    { key: 'level', label: '档位', width: '55px', align: 'center', value: (row) => String(row.level) },
    { key: 'ask-price', label: '卖价', align: 'right', num: true, value: (row) => price(row.askPrice) },
    { key: 'ask-volume', label: '卖量', align: 'right', num: true, value: (row) => compact(row.askVolume) }
  ];

  const timelineColumns: Column<ExpansionTimelinePoint>[] = [
    { key: 'time', label: '时间', width: '75px', value: (row) => row.time ?? '无效时间', sub: (row) => row.session_day_offset ? `会话偏移 ${row.session_day_offset}` : '' },
    { key: 'price', label: '价格', align: 'right', num: true, value: (row) => price(row.price) },
    { key: 'average', label: '均价', align: 'right', num: true, value: (row) => price(row.average_price) },
    { key: 'volume', label: '成交量', align: 'right', num: true, value: (row) => compact(row.volume) },
    { key: 'open-interest', label: '持仓量', align: 'right', num: true, value: (row) => compact(row.open_interest) }
  ];

  const tradeColumns: Column<ExpansionTrade>[] = [
    { key: 'time', label: '时间', width: '82px', value: (row) => row.time, sub: (row) => `#${count(row.absolute_index)}` },
    { key: 'price', label: '价格', align: 'right', num: true, value: (row) => price(row.price) },
    { key: 'volume', label: '成交量', align: 'right', num: true, value: (row) => compact(row.volume) },
    { key: 'oi-change', label: '持仓变化', align: 'right', num: true, value: (row) => fixed(row.open_interest_change, 0), tone: (row) => tone(row.open_interest_change) },
    { key: 'nature', label: '开平性质', width: '85px', value: (row) => row.nature || '未知', sub: (row) => `${row.side || 'unknown'} · raw ${row.nature_raw}` },
    { key: 'direction', label: '方向 raw', width: '75px', align: 'right', num: true, value: (row) => fixed(row.direction, 0), tone: (row) => tone(row.direction) }
  ];

  onMount(() => {
    void loadDirectory();
    void loadSelected(true);
  });
</script>

<PageHeader
  eyebrow="TDX 7727 · 0x23F0/23F5/23FA/240B/240C/23FC/2406"
  title={`${selectedName || code} · 扩展市场行情`}
  description="统一查看期货、商品指数等扩展市场的合约目录、五档快照、当前或历史分时与逐笔；市场 ID 与开平性质均按原生证据展示。"
  {stats}
>
  {#snippet actions()}
    {#if quote.data}<Badge tone="up">公开行情</Badge>{/if}
    <Button icon="refresh" busy={quote.busy || timeline.busy || trades.busy} onclick={() => loadSelected(false)}>刷新当前合约</Button>
  {/snippet}
</PageHeader>

<Panel
  title="行情选择"
  subtitle="常用别名：qz=28 郑商、qd=29 大商、qs=30 上期、cz=47 中金、qg=66 广期；也可直接输入 3..255 市场 ID。"
  error={selectionError}
>
  {#snippet toolbar()}
    <TextInput bind:value={market} width="105px" label="市场" placeholder="市场 ID / 别名" onEnter={() => loadSelected(true)} />
    <TextInput bind:value={code} width="125px" label="代码" placeholder="合约代码" onEnter={() => loadSelected(true)} />
    <TextInput bind:value={tradingDate} width="125px" label="交易日" placeholder="YYYYMMDD / 当前" onEnter={() => loadSelected(true)} />
    <Select value={tradePageSize} options={TRADE_PAGE_SIZES} width="120px" label="逐笔页长" onChange={(value) => { tradePageSize = value; }} />
    <Button variant="primary" onclick={() => loadSelected(true)}>读取行情</Button>
  {/snippet}
  <p class="method-note">日期留空走当前会话；指定 YYYYMMDD 时走历史分时与逐笔。页面不会提供任意服务器、路径或原始请求入口。</p>
</Panel>

<div class="market-grid">
  <Panel
    title="行情快照与五档"
    subtitle={quote.data ? `${quote.data.market_id}:${quote.data.code} · ${quote.data.reference_price_semantics}` : '选择合约后读取 150B 扩展行情记录。'}
    busy={quote.busy}
    error={quote.error}
    onRetry={() => loadSelected(false)}
    empty={quote.loaded && !quote.data}
    emptyText="当前合约没有可用行情快照。"
    flush
  >
    {#if quote.data}
      <div class="quote-strip">
        <div><span>开盘</span><strong>{price(quote.data.open)}</strong></div>
        <div><span>昨结 / 昨收</span><strong>{price(quote.data.pre_settlement)}</strong></div>
        <div><span>内 / 外盘</span><strong>{compact(quote.data.inside_volume)} / {compact(quote.data.outside_volume)}</strong></div>
        <div><span>最新成交</span><strong>{compact(quote.data.last_volume)}</strong></div>
      </div>
      <DataTable columns={depthColumns} rows={depthRows} rowKey={(row) => row.level} minWidth="470px" />
    {/if}
  </Panel>

  <Panel
    title={timeline.data?.mode === 'historical' ? `历史分时 · ${timeline.data.date}` : '当前分时'}
    subtitle={timeline.data ? `${count(timeline.data.wire_count)} 点 · ${timeline.data.transport} · 无效时间 ${count(timeline.data.invalid_time_count)}` : '价格、均价、成交量与持仓量。'}
    busy={timeline.busy}
    error={timeline.error}
    onRetry={() => loadSelected(false)}
    empty={timeline.loaded && timelineRows.length === 0}
    emptyText="该会话没有分时记录。"
    flush
    scroll
  >
    <DataTable columns={timelineColumns} rows={timelineRows} rowKey={(row) => row.index} maxHeight="430px" minWidth="520px" />
  </Panel>
</div>

<Panel
  title={trades.data?.mode === 'historical' ? `历史逐笔 · ${trades.data.date}` : '当前逐笔'}
  subtitle={trades.data ? `${count(trades.data.downloaded)} 条 · ${trades.data.ordering_note ?? '按服务端顺序返回'} · ${Object.entries(trades.data.nature_summary).map(([name, value]) => `${name} ${value}`).join(' / ') || '无性质记录'}` : '逐笔价格、成交量、持仓变化与开平性质。'}
  busy={trades.busy}
  error={trades.error}
  onRetry={() => loadSelected(false)}
  empty={trades.loaded && tradeRows.length === 0}
  emptyText="该范围没有逐笔记录。"
  flush
  scroll
>
  {#snippet toolbar()}
    <Button disabled={Number(tradeStart) <= 0 || trades.busy} onclick={previousTradePage}>较新一页</Button>
    <Badge>起点 {count(trades.data?.start ?? tradeStart)}</Badge>
    <Button disabled={!trades.data?.has_more || trades.busy} onclick={nextTradePage}>更早一页</Button>
  {/snippet}
  <DataTable columns={tradeColumns} rows={tradeRows} rowKey={(row) => row.absolute_index} numbered maxHeight="430px" minWidth="650px" />
</Panel>

<Panel
  title="扩展市场合约目录"
  subtitle={directory.data ? `全目录报告 ${count(directory.data.total)} 条；本页抓取 ${count(directory.data.fetched)} 条、筛后返回 ${count(directory.data.returned)} 条。market/query 仅过滤当前抓取窗口。` : '分页读取 7727 合约目录；选择任一记录即可切换上方行情。'}
  busy={directory.busy}
  error={directoryError || directory.error}
  onRetry={() => loadDirectory()}
  empty={directory.loaded && instruments.length === 0}
  emptyText="当前抓取窗口没有匹配合约；可清空页内市场/关键词过滤或翻页。"
  flush
  scroll
>
  {#snippet toolbar()}
    <TextInput bind:value={directoryStart} width="95px" label="目录起点" placeholder="起点" onEnter={() => loadDirectory()} />
    <Select value={directoryCount} options={DIRECTORY_COUNTS} width="115px" label="抓取数" onChange={(value) => { directoryCount = value; }} />
    <TextInput bind:value={directoryMarket} width="105px" label="页内市场" placeholder="页内市场" onEnter={() => loadDirectory()} />
    <TextInput bind:value={directoryQuery} icon="search" width="165px" label="页内检索" placeholder="页内代码 / 名称" onEnter={() => loadDirectory()} />
    <Button variant="primary" onclick={() => loadDirectory()}>读取目录</Button>
    <Button disabled={Number(directoryStart) <= 0 || directory.busy} onclick={previousDirectoryPage}>上一窗口</Button>
    <Button disabled={!directory.data?.has_more || directory.busy} onclick={nextDirectoryPage}>下一窗口</Button>
  {/snippet}
  <DataTable
    columns={instrumentColumns}
    rows={instruments}
    rowKey={(row) => row.security}
    onRowClick={selectInstrument}
    isActive={(row) => String(row.market_id) === quote.data?.market && row.code === quote.data?.code}
    numbered
    stickyFirst
    maxHeight="430px"
    minWidth="720px"
  />
</Panel>

<style>
  :global(.panel + .panel),
  .market-grid { margin-top: var(--sp-3); }

  .market-grid {
    display: grid;
    grid-template-columns: minmax(390px, 0.85fr) minmax(480px, 1.15fr);
    gap: var(--sp-3);
  }

  .quote-strip {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: var(--sp-2);
    padding: var(--sp-3);
  }

  .quote-strip > div {
    display: flex;
    min-width: 0;
    flex-direction: column;
    gap: 2px;
    padding: var(--sp-2);
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .quote-strip span,
  .method-note { color: var(--fg-mute); font-size: var(--fs-micro); }
  .quote-strip strong { font-size: var(--fs-small); font-variant-numeric: tabular-nums; }
  .method-note { margin: 0; }

  @media (max-width: 1050px) {
    .market-grid { grid-template-columns: 1fr; }
  }
</style>
