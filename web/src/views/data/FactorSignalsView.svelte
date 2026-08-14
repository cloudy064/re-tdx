<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, fixed, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';

  type Mode = 'catalog' | 'dashboard' | 'technical';
  type FactorKind = 'standard' | 'pattern';

  interface Security {
    market: string;
    code: string;
    name: string;
    security_id: string;
  }

  interface FactorRecord {
    factor_id: string;
    name: string;
    factor_type?: string;
    factor_kind?: string;
    family?: string;
    description?: string;
    direction?: string;
    live_performance?: { today_return_pct?: number | null; five_day_return_pct?: number | null };
    backtest?: {
      window_days?: number;
      forward_returns_pct?: Record<string, number | null>;
      maximum_drawdown_pct?: number | null;
      sharpe_ratio?: number | null;
    };
  }

  interface SecurityRecord {
    security?: Security;
    price?: number | null;
    change_pct?: number | null;
    ten_day_change_pct?: number | null;
    safety_score?: number | null;
    signal_count?: number | null;
    selected_factors_text?: string;
    selected_factor?: string;
    selection_time?: string;
    since_selection_pct?: number | null;
    [key: string]: unknown;
  }

  interface ResearchDocument<T> {
    schema: string;
    availability: string;
    view: string;
    counts: Record<string, number | boolean>;
    records: T[];
  }

  const MODES = [
    { id: 'catalog', label: '因子与成分股' },
    { id: 'dashboard', label: '因子看板' },
    { id: 'technical', label: '技术选股' }
  ];
  const FACTOR_KINDS = [
    { id: 'standard', label: '普通因子' },
    { id: 'pattern', label: 'K线形态' }
  ];
  const DIRECTIONS = [
    { id: 'all', label: '全部方向' },
    { id: 'up', label: '向上' },
    { id: 'down', label: '向下' }
  ];
  const SIGNAL_VIEWS = [
    { id: 'nine-turn', label: '神奇九转' },
    { id: 'rps-stock', label: '个股 RPS' },
    { id: 'rps-block', label: '板块 RPS' },
    { id: 'new-high', label: '阶段新高' },
    { id: 'new-low', label: '阶段新低' },
    { id: 'breakout', label: '横盘突破' },
    { id: 'strong-start', label: '强势启动' },
    { id: 'trend-up', label: '上升趋势' },
    { id: 'trend-down', label: '下降趋势' },
    { id: 'event-driven', label: '事件驱动' },
    { id: 'model-new-high', label: '模型创新高' },
    { id: 'two-day-event', label: '前两日事件驱动' },
    { id: 'liquidity-space', label: '流动空间' },
    { id: 'limit-break', label: '涨停开板' },
    { id: 'low-turnover-chase', label: '低换手追涨' },
    { id: 'high-turnover-chase', label: '高换手追涨' },
    { id: 'high-liquidity-enhance', label: '高流动性增强' },
    { id: 'weak-limit-reversal', label: '烂板转强' },
    { id: 'failed-limit-reversal', label: '炸板转强' },
    { id: 'auction-bottom-reversal', label: '竞价止跌' },
    { id: 'upper-shadow-engulf', label: '预吞上影' },
    { id: 'auction-volume-spike', label: '竞价爆量' },
    { id: 'limit-up-gap', label: '涨停高开' },
    { id: 'five-minute-volume-surge', label: '5分钟陡增' },
    { id: 'accumulation-surge', label: '积突信号' },
    { id: 'new-high-low-120', label: '120日新高低' },
    { id: 'ma120-cross', label: '120日均线上下' },
    { id: 'ma-break-reclaim', label: '均线破立' },
    { id: 'pullback-rally', label: '缩量踩放量拉' },
    { id: 'ma-support-pressure', label: '均线托压' },
    { id: 'intraday-opportunity', label: '个股分时机会' },
    { id: 't0-opportunity', label: 'T+0机会' }
  ];

  let mode = $state<Mode>('catalog');
  let factorKind = $state<FactorKind>('standard');
  let signalView = $state('nine-turn');
  let direction = $state('all');
  let query = $state('');
  let selectedFactorId = $state('');

  const results = new Resource<ResearchDocument<FactorRecord | SecurityRecord>>();
  const members = new Resource<ResearchDocument<SecurityRecord>>();
  const factorRows = $derived((results.data?.records ?? []) as FactorRecord[]);
  const securityRows = $derived((results.data?.records ?? []) as SecurityRecord[]);
  const memberRows = $derived(members.data?.records ?? []);
  const selectedFactor = $derived(
    factorRows.find((row) => row.factor_id === selectedFactorId) ?? factorRows[0] ?? null
  );

  const stats = $derived.by<Stat[]>(() => {
    if (mode === 'catalog') return [
      { label: factorKind === 'standard' ? '普通因子' : '形态因子', value: count(results.data?.counts.matched_rows ?? factorRows.length) },
      { label: '当前成分股', value: count(members.data?.counts.matched_rows ?? memberRows.length) },
      { label: '因子今日表现', value: percent(selectedFactor?.live_performance?.today_return_pct), tone: tone(selectedFactor?.live_performance?.today_return_pct) },
      { label: '因子5日表现', value: percent(selectedFactor?.live_performance?.five_day_return_pct), tone: tone(selectedFactor?.live_performance?.five_day_return_pct) }
    ];
    if (mode === 'dashboard') return [
      { label: '看板股票', value: count(results.data?.counts.matched_rows ?? securityRows.length) },
      { label: '当前返回', value: count(results.data?.counts.returned) },
      { label: '最高安全分', value: count(Math.max(0, ...securityRows.map((row) => Number(row.safety_score) || 0))) },
      { label: '来源状态', value: results.data?.availability === 'live' ? '在线' : text(results.data?.availability) }
    ];
    return [
      { label: '上游结果', value: count(results.data?.counts.upstream_rows) },
      { label: '客户端筛选后', value: count(results.data?.counts.after_client_filter) },
      { label: '当前返回', value: count(results.data?.counts.returned) },
      { label: '来源状态', value: results.data?.availability === 'live' ? '在线' : text(results.data?.availability) }
    ];
  });

  function asObject(value: unknown): Record<string, unknown> {
    return value && typeof value === 'object' && !Array.isArray(value)
      ? value as Record<string, unknown>
      : {};
  }

  function firstValue(row: SecurityRecord, keys: string[]): unknown {
    for (const key of keys) {
      const value = row[key];
      if (value !== null && value !== undefined && value !== '') return value;
    }
    return null;
  }

  function signalLabel(row: SecurityRecord): string {
    const signal = asObject(row.signal);
    return text(firstValue(row, ['strategy_title', 'strategy_name', 'direction', 'selected_factor']) ?? signal.label);
  }

  function signalTime(row: SecurityRecord): string {
    return date(firstValue(row, ['signal_date', 'break_date', 'chosen_date', 'selection_time', 'selection_date']));
  }

  function signalReturn(row: SecurityRecord): unknown {
    return firstValue(row, [
      'since_signal_pct', 'five_day_growth_pct', 'change_pct', 'breakout_growth_pct',
      'since_selection_pct', 'growth_since_chosen_pct', 'period_return_pct'
    ]);
  }

  function signalDetail(row: SecurityRecord): string {
    const signal = asObject(row.signal);
    const parts = [
      signal.label,
      row.selected_factors_text,
      firstValue(row, ['industry', 'exit_reason', 'client_rule'])
    ].filter((value) => value !== null && value !== undefined && value !== '');
    return parts.length ? parts.map(String).join(' · ') : '—';
  }

  function openSecurity(security?: Security) {
    if (!security) return;
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code));
  }

  async function loadMembers(factor?: FactorRecord | null, refresh = false) {
    if (!factor) {
      members.reset();
      return;
    }
    selectedFactorId = factor.factor_id;
    const view = factorKind === 'standard' ? 'members' : 'pattern-members';
    await members.load(`/api/v1/market/factors?${queryString({
      view, factor_id: factor.factor_id, all_pages: 1, limit: 5000,
      refresh: refresh ? 1 : 0
    })}`);
  }

  async function load(refresh = false) {
    if (mode === 'catalog') {
      members.reset();
      const view = factorKind === 'standard' ? 'catalog' : 'patterns';
      const result = await results.load(`/api/v1/market/factors?${queryString({
        view, q: query.trim(), all_pages: 1, limit: 1000, refresh: refresh ? 1 : 0
      })}`);
      const first = result?.records[0] as FactorRecord | undefined;
      if (first) await loadMembers(first, refresh);
      return;
    }
    if (mode === 'dashboard') {
      await results.load(`/api/v1/market/factors?${queryString({
        view: 'dashboard', q: query.trim(), all_pages: 1, limit: 2000,
        refresh: refresh ? 1 : 0
      })}`);
      return;
    }
    await results.load(`/api/v1/market/technical-signals?${queryString({
      view: signalView,
      direction: signalView === 'nine-turn' ? direction : 'all',
      limit: 2000,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function switchMode(next: string) {
    mode = next as Mode;
    query = '';
    selectedFactorId = '';
    results.reset();
    members.reset();
    void load();
  }

  const factorColumns: Column<FactorRecord>[] = [
    { key: 'name', label: '因子', width: '170px', value: (row) => row.name, sub: (row) => row.family || row.factor_id },
    { key: 'type', label: '类型', width: '100px', value: (row) => text(row.factor_type) },
    { key: 'today', label: '今日', width: '80px', align: 'right', num: true, value: (row) => percent(row.live_performance?.today_return_pct), tone: (row) => tone(row.live_performance?.today_return_pct), sortValue: (row) => row.live_performance?.today_return_pct ?? -999 },
    { key: 'five', label: '5日', width: '80px', align: 'right', num: true, value: (row) => percent(row.live_performance?.five_day_return_pct), tone: (row) => tone(row.live_performance?.five_day_return_pct), sortValue: (row) => row.live_performance?.five_day_return_pct ?? -999 },
    { key: 'sharpe', label: '夏普', width: '72px', align: 'right', num: true, value: (row) => fixed(row.backtest?.sharpe_ratio, 2), sortValue: (row) => row.backtest?.sharpe_ratio ?? -999 },
    { key: 'description', label: '因子说明', value: (row) => text(row.description), wrap: true }
  ];

  const memberColumns: Column<SecurityRecord>[] = [
    { key: 'security', label: '股票', width: '145px', value: (row) => row.security?.name || row.security?.code || '—', sub: (row) => row.security?.security_id || '' },
    { key: 'price', label: '价格', width: '85px', align: 'right', num: true, value: (row) => fixed(row.price, 2) },
    { key: 'change', label: '涨跌幅', width: '88px', align: 'right', num: true, value: (row) => percent(row.change_pct), tone: (row) => tone(row.change_pct), sortValue: (row) => row.change_pct ?? -999 },
    { key: 'ten', label: '10日表现', width: '92px', align: 'right', num: true, value: (row) => percent(row.ten_day_change_pct), tone: (row) => tone(row.ten_day_change_pct), sortValue: (row) => row.ten_day_change_pct ?? -999 }
  ];

  const dashboardColumns: Column<SecurityRecord>[] = [
    { key: 'security', label: '股票', width: '145px', value: (row) => row.security?.name || row.security?.code || '—', sub: (row) => row.security?.security_id || '' },
    { key: 'signals', label: '命中数', width: '76px', align: 'right', num: true, value: (row) => count(row.signal_count), sortValue: (row) => row.signal_count ?? 0 },
    { key: 'safety', label: '安全分', width: '76px', align: 'right', num: true, value: (row) => count(row.safety_score), sortValue: (row) => row.safety_score ?? 0 },
    { key: 'change', label: '涨跌幅', width: '88px', align: 'right', num: true, value: (row) => percent(row.change_pct), tone: (row) => tone(row.change_pct), sortValue: (row) => row.change_pct ?? -999 },
    { key: 'ten', label: '近10日', width: '88px', align: 'right', num: true, value: (row) => percent(row.ten_day_change_pct), tone: (row) => tone(row.ten_day_change_pct), sortValue: (row) => row.ten_day_change_pct ?? -999 },
    { key: 'factors', label: '当前命中因子', value: (row) => text(row.selected_factors_text), wrap: true }
  ];

  const technicalColumns: Column<SecurityRecord>[] = [
    { key: 'security', label: '股票 / 板块', width: '155px', value: (row) => row.security?.name || row.security?.code || text(asObject(row.block).name), sub: (row) => row.security?.security_id || text(asObject(row.block).block_id) },
    { key: 'signal', label: '信号', width: '145px', value: signalLabel },
    { key: 'time', label: '信号日期 / 时间', width: '120px', value: signalTime },
    { key: 'return', label: '信号后表现', width: '100px', align: 'right', num: true, value: (row) => percent(signalReturn(row)), tone: (row) => tone(signalReturn(row)), sortValue: (row) => Number(signalReturn(row)) || -999 },
    { key: 'safety', label: '安全分', width: '78px', align: 'right', num: true, value: (row) => count(firstValue(row, ['safety_score', 'reported_safety'])), sortValue: (row) => Number(firstValue(row, ['safety_score', 'reported_safety'])) || 0 },
    { key: 'detail', label: '信号依据', value: signalDetail, wrap: true }
  ];

  onMount(() => void load());
</script>

<PageHeader eyebrow="TQLEX / PBRPC · 200626—200662" title="因子与技术信号" description="把普通因子、K线形态、因子成分股和三十二类通达信技术选股放在同一研究工作台中。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={results.busy || members.busy} onclick={() => void load(true)}>刷新源数据</Button>{/snippet}
</PageHeader>

<div class="mode-bar"><Segmented options={MODES} value={mode} onChange={switchMode} ariaLabel="研究模式" /></div>

{#if mode === 'catalog'}
  <Split asideWidth="55%">
    {#snippet main()}
      <Panel title="因子目录" subtitle={results.data ? `${count(factorRows.length)} 项 · 点击查看成分股` : '33 个普通因子 / 57 个K线形态'} busy={results.busy} error={results.error} onRetry={() => void load()} empty={results.loaded && factorRows.length === 0} emptyText="当前检索没有匹配因子。" flush scroll>
        {#snippet toolbar()}
          <Select options={FACTOR_KINDS} value={factorKind} width="145px" label="目录" onChange={(next) => { factorKind = next as FactorKind; selectedFactorId = ''; void load(); }} />
          <TextInput bind:value={query} icon="search" width="230px" label="检索" placeholder="因子名称、类型或说明" onEnter={() => void load()} />
        {/snippet}
        <DataTable columns={factorColumns} rows={factorRows} stickyFirst rowKey={(row) => row.factor_id} onRowClick={(row) => void loadMembers(row)} isActive={(row) => row.factor_id === selectedFactor?.factor_id} minWidth="850px" sortKey="today" />
      </Panel>
    {/snippet}
    {#snippet aside()}
      <Panel title={selectedFactor ? `${selectedFactor.name} · 成分股` : '因子成分股'} subtitle={selectedFactor?.description || '从左侧选择因子'} busy={members.busy} error={members.error} onRetry={() => void loadMembers(selectedFactor)} empty={members.loaded && memberRows.length === 0} emptyText="该因子当前没有返回成分股。" flush scroll>
        <DataTable columns={memberColumns} rows={memberRows} stickyFirst numbered rowKey={(row, index) => row.security?.security_id ?? index} onRowClick={(row) => openSecurity(row.security)} sortKey="change" />
      </Panel>
    {/snippet}
  </Split>
{:else}
  <Panel title={mode === 'dashboard' ? '个股因子看板' : SIGNAL_VIEWS.find((item) => item.id === signalView)?.label ?? '技术选股'} subtitle={results.data ? `${count(results.data.counts.returned)} 条 · 点击股票进入个股工作台` : ''} busy={results.busy} error={results.error} onRetry={() => void load()} empty={results.loaded && securityRows.length === 0} emptyText="当前视图没有返回记录。" flush scroll fill>
    {#snippet toolbar()}
      {#if mode === 'dashboard'}
        <TextInput bind:value={query} icon="search" width="260px" label="检索" placeholder="股票、因子名称" onEnter={() => void load()} />
      {:else}
        <Select options={SIGNAL_VIEWS} value={signalView} width="200px" label="技术选股" onChange={(next) => { signalView = next; direction = 'all'; void load(); }} />
        {#if signalView === 'nine-turn'}
          <Select options={DIRECTIONS} value={direction} width="130px" label="方向" onChange={(next) => { direction = next; void load(); }} />
        {/if}
      {/if}
    {/snippet}
    {#if mode === 'dashboard'}
      <DataTable columns={dashboardColumns} rows={securityRows} stickyFirst numbered rowKey={(row, index) => row.security?.security_id ?? index} onRowClick={(row) => openSecurity(row.security)} sortKey="signals" />
    {:else}
      <DataTable columns={technicalColumns} rows={securityRows} stickyFirst numbered rowKey={(row, index) => `${row.security?.security_id ?? 'block'}:${index}`} onRowClick={(row) => openSecurity(row.security)} sortKey="return" minWidth="930px" />
    {/if}
  </Panel>
{/if}

<style>
  .mode-bar { display: flex; flex: none; align-items: center; }
</style>
