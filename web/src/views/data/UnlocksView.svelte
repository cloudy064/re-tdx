<script lang="ts">
  /**
   * 限售解禁。主表是解禁日历（后端已把同日同票的多个锁定批次合并成一个事件），
   * 右栏按事件展开锁定批次与逐股东明细——股东明细是动态资源，未实施事件常常尚未发布。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { Resource } from '../../lib/resource.svelte';
  import { compact, count, date, fixed, money, percent, text, tone } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    MarketUnlocksDocument,
    UnlockEvent,
    UnlockLot,
    UnlockMonthlyPressure,
    UnlockMonthlySummary,
    UnlockSummary,
    UnlockShareholder
  } from '../../types';

  const PROGRESS = [
    { id: '', label: '全部状态' },
    { id: '实施', label: '实施' },
    { id: '未实施', label: '未实施' }
  ];

  let query = $state('');
  let view = $state<'calendar' | 'recent-large' | 'monthly-pressure'>('calendar');
  let progress = $state('');
  let reason = $state('');
  let selectedId = $state('');

  const catalog = new Resource<MarketUnlocksDocument>();
  const detail = new Resource<MarketUnlocksDocument>();

  const doc = $derived(catalog.data);
  const eventSummary = $derived(view === 'monthly-pressure' ? null : doc?.summary as UnlockSummary | undefined);
  const monthlySummary = $derived(view === 'monthly-pressure' ? doc?.summary as UnlockMonthlySummary | undefined : null);
  const months = $derived(doc?.months ?? []);
  const event = $derived(view === 'recent-large'
    ? doc?.events.find((item) => item.detail_id === selectedId) ?? null
    : detail.data?.selected_event ?? null);
  const holders = $derived(
    detail.data?.details.find((item) => item.detail_id === selectedId) ?? null
  );

  // 原因清单取全量目录口径，否则筛选后选项会自我收敛
  const reasons = $derived([
    { id: '', label: '全部原因' },
    ...((view === 'monthly-pressure' ? [] : (doc?.catalog_summary as UnlockSummary | undefined)?.reason_counts) ?? []).map((item) => ({
      id: item.label,
      label: `${item.label} · ${item.count}`
    }))
  ]);

  const stats = $derived.by<Stat[]>(() => {
    if (monthlySummary) return [
      { label: '覆盖月份', value: count(monthlySummary.months), note: `${monthLabel(monthlySummary.first_month)} 至 ${monthLabel(monthlySummary.last_month)}` },
      { label: '累计解禁股数', value: compact(monthlySummary.total_unlock_shares, '股'), note: '未来月度计划汇总' },
      { label: '累计参考市值', value: money(monthlySummary.total_unlock_market_value_yuan), note: '客户端静态参考价口径' },
      { label: '峰值月份', value: monthLabel(monthlySummary.peak_month), note: money(monthlySummary.peak_unlock_market_value_yuan) },
      { label: '公式复核', value: `${monthlySummary.formula_checked - monthlySummary.formula_mismatches}/${monthlySummary.formula_checked}` },
      { label: '来源', value: 'DXFJJ', note: '月度市场压力，不是单票事件' }
    ];
    if (!eventSummary) return [];
    return [
      { label: '解禁事件', value: count(eventSummary.events), note: `${eventSummary.unique_securities} 只股票` },
      { label: '解禁股数', value: compact(eventSummary.total_unlock_shares, '股'), note: '同日同票批次已合并' },
      { label: view === 'calendar' ? '参考市值' : '覆盖证券', value: view === 'calendar' ? money(eventSummary.total_unlock_market_value) : count(eventSummary.unique_securities), note: view === 'calendar' ? '股数 × 解禁前收盘' : '近期大比例滚动窗口' },
      { label: '已实施 / 未实施', value: `${eventSummary.implemented_events} / ${eventSummary.pending_events}` },
      { label: view === 'calendar' ? '日历区间' : '窗口区间', value: `${date(eventSummary.first_date)}` , note: `至 ${date(eventSummary.last_date)}` },
      { label: '原始行数', value: count(eventSummary.raw_rows), note: `${doc?.counts.catalog_events ?? 0} 个目录事件` }
    ];
  });

  function load(refresh = false) {
    void catalog.load(
      `/api/v1/market/unlocks?${queryString({
        view,
        q: query.trim(),
        progress: view === 'calendar' ? progress : '',
        reason,
        limit: 1000,
        refresh: refresh ? 1 : 0
      })}`
    );
    detail.reset();
    selectedId = '';
  }

  function openEvent(row: UnlockEvent) {
    selectedId = row.detail_id;
    if (view === 'recent-large') {
      detail.reset();
      return;
    }
    void detail.load(
      `/api/v1/market/unlocks?${queryString({
        detail_id: row.detail_id,
        include_details: 1,
        detail_limit: 2000
      })}`
    );
  }

  function monthLabel(value: string) {
    return value.length === 6 ? `${value.slice(0, 4)}-${value.slice(4)}` : value || '—';
  }

  function gotoWorkbench() {
    if (!event) return;
    app.setStock({
      market: event.security.market,
      code: event.security.code,
      name: event.security.name
    });
    router.go(stockPath(event.security.market, event.security.code));
  }

  /** 单个股东占本次事件的比例，接口只给绝对股数 */
  function holderShare(row: UnlockShareholder): string {
    const total = event?.unlock_shares ?? 0;
    return total && row.unlock_shares !== null
      ? percent((row.unlock_shares / total) * 100)
      : '—';
  }

  const eventColumns = $derived.by<Column<UnlockEvent>[]>(() => [
    { key: 'date', label: '解禁日', width: '84px', num: true, value: (row) => date(row.date) },
    {
      key: 'security',
      label: '证券',
      width: '128px',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => row.security.security_id
    },
    {
      key: 'progress',
      label: '状态',
      width: '58px',
      value: (row) => text(row.progress),
      tone: (row) => (row.progress === '实施' || row.progress === '已解禁' ? 'up' : '')
    },
    { key: 'reason', label: '解禁原因', wrap: true, value: (row) => text(row.reason) },
    {
      key: 'shares',
      label: '解禁股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.unlock_shares, '股'),
      sortValue: (row) => row.unlock_shares ?? 0
    },
    ...(view === 'recent-large' ? [{
      key: 'ratio', label: '占总股本', align: 'right' as const, num: true,
      value: (row: UnlockEvent) => percent(row.unlock_to_total_pct),
      sortValue: (row: UnlockEvent) => row.unlock_to_total_pct ?? 0
    }, {
      key: 'total', label: '总股本', align: 'right' as const, num: true,
      value: (row: UnlockEvent) => compact(row.total_shares, '股'),
      sortValue: (row: UnlockEvent) => row.total_shares ?? 0
    }] : [{
      key: 'value', label: '参考市值', align: 'right' as const, num: true,
      value: (row: UnlockEvent) => money(row.unlock_market_value),
      sortValue: (row: UnlockEvent) => row.unlock_market_value ?? 0
    }, {
      key: 'close', label: '解禁前收盘', align: 'right' as const, num: true,
      value: (row: UnlockEvent) => fixed(row.pre_unlock_close)
    }, {
      key: 'lots', label: '批次', align: 'right' as const, width: '54px', num: true,
      value: (row: UnlockEvent) => count(row.lot_count),
      sortValue: (row: UnlockEvent) => row.lot_count ?? 0
    }])
  ]);

  const lotColumns: Column<UnlockLot>[] = [
    { key: 'months', label: '锁定期', align: 'right', num: true, value: (row) => `${fixed(row.lock_months, 0)} 个月` },
    { key: 'shares', label: '解禁数量', align: 'right', num: true, value: (row) => compact(row.unlock_shares, '股') },
    { key: 'issue', label: '发行价', align: 'right', num: true, value: (row) => fixed(row.issue_price) },
    {
      key: 'lock-return',
      label: '锁定期收益',
      align: 'right',
      num: true,
      value: (row) => percent(row.lock_return_pct),
      tone: (row) => tone(row.lock_return_pct)
    },
    {
      key: 'pre',
      label: '前一月',
      align: 'right',
      num: true,
      value: (row) => percent(row.pre_month_return_pct),
      tone: (row) => tone(row.pre_month_return_pct)
    },
    {
      key: 'post',
      label: '后一月',
      align: 'right',
      num: true,
      value: (row) => percent(row.post_month_return_pct),
      tone: (row) => tone(row.post_month_return_pct)
    }
  ];

  const holderColumns: Column<UnlockShareholder>[] = [
    { key: 'holder', label: '股东 / 激励对象', wrap: true, value: (row) => text(row.shareholder) },
    {
      key: 'shares',
      label: '解禁数量',
      align: 'right',
      num: true,
      value: (row) => compact(row.unlock_shares, '股'),
      sortValue: (row) => row.unlock_shares ?? 0
    },
    { key: 'share', label: '事件占比', align: 'right', num: true, value: holderShare },
    { key: 'value', label: '参考市值', align: 'right', num: true, value: (row) => money(row.unlock_market_value) },
    { key: 'progress', label: '状态', width: '54px', value: (row) => text(row.progress) }
  ];

  const monthlyColumns: Column<UnlockMonthlyPressure>[] = [
    { key: 'month', label: '解禁月份', width: '100px', num: true, value: (row) => monthLabel(row.month) },
    { key: 'shares', label: '解禁股数', align: 'right', num: true, value: (row) => compact(row.unlock_shares, '股'), sortValue: (row) => row.unlock_shares ?? 0 },
    { key: 'value', label: '参考市值', align: 'right', num: true, value: (row) => money(row.unlock_market_value_yuan), sortValue: (row) => row.unlock_market_value_yuan ?? 0 },
    { key: 'stocks', label: '股票数', align: 'right', num: true, value: (row) => count(row.security_count), sortValue: (row) => row.security_count ?? 0 },
    { key: 'lots', label: '解禁批次', align: 'right', num: true, value: (row) => count(row.lot_count), sortValue: (row) => row.lot_count ?? 0 },
    { key: 'total-ratio', label: '占A股总市值', align: 'right', num: true, value: (row) => percent(row.unlock_to_total_market_cap_pct), sortValue: (row) => row.unlock_to_total_market_cap_pct ?? 0 },
    { key: 'float-ratio', label: '占A股流通市值', align: 'right', num: true, value: (row) => percent(row.unlock_to_float_market_cap_pct), sortValue: (row) => row.unlock_to_float_market_cap_pct ?? 0 }
  ];

  const eventStats = $derived.by<Stat[]>(() => {
    if (!event) return [];
    return [
      { label: '解禁日期', value: date(event.date), note: text(event.progress) },
      { label: '解禁数量', value: compact(event.unlock_shares, '股'), note: view === 'calendar' ? `${event.lot_count} 个锁定批次` : '近期大额历史' },
      { label: view === 'calendar' ? '参考市值' : '占总股本', value: view === 'calendar' ? money(event.unlock_market_value) : percent(event.unlock_to_total_pct) },
      { label: view === 'calendar' ? '解禁前收盘' : '总股本', value: view === 'calendar' ? fixed(event.pre_unlock_close) : compact(event.total_shares, '股') }
    ];
  });

  onMount(() => load());
</script>

<PageHeader
  eyebrow="DBLJJ + JQGZ + DXFJJ"
  title="限售解禁"
  description="解禁日历可展开批次和真实股东；近期大比例窗口展示滚动单票信号；月度压力汇总未来解禁股数、市值和股票数量。"
  {stats}
>
  {#snippet actions()}
    <Button variant={view === 'calendar' ? 'primary' : 'default'} onclick={() => { view = 'calendar'; progress = ''; reason = ''; load(); }}>未来日历</Button>
    <Button variant={view === 'recent-large' ? 'primary' : 'default'} onclick={() => { view = 'recent-large'; progress = ''; reason = ''; load(); }}>近期大比例</Button>
    <Button variant={view === 'monthly-pressure' ? 'primary' : 'default'} onclick={() => { view = 'monthly-pressure'; progress = ''; reason = ''; load(); }}>月度压力</Button>
    <Button icon="refresh" busy={catalog.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

{#if view === 'monthly-pressure'}
  <Panel title="未来月度解禁压力" subtitle={doc ? `${count(doc.counts.months ?? months.length)} 个月 · 金额和股数已从“亿”换算` : '本地 JSN 优先'} busy={catalog.busy} error={catalog.error} onRetry={() => load()} empty={catalog.loaded && months.length === 0} emptyText="当前没有月度解禁压力数据。" flush scroll>
    {#snippet toolbar()}<TextInput bind:value={query} icon="search" width="240px" label="月份" placeholder="例如 202707" onEnter={() => load()} />{/snippet}
    <DataTable columns={monthlyColumns} rows={months} rowKey={(row) => row.month} sortKey="month" minWidth="900px" />
  </Panel>
{:else}
<Split asideWidth="400px">
  {#snippet main()}
    <Panel
      flush
      scroll
      busy={catalog.busy}
      error={catalog.error}
      onRetry={() => load()}
      title={view === 'calendar' ? '解禁日历' : '近期大比例解禁'}
      subtitle={doc ? `${count(doc.counts.returned_events)} / ${count(doc.counts.catalog_events)} 个事件` : ''}
      empty={catalog.loaded && !catalog.busy && (doc?.events.length ?? 0) === 0}
      emptyText="当前筛选条件下没有解禁事件"
    >
      {#snippet toolbar()}
        <TextInput
          bind:value={query}
          icon="search"
          width="240px"
          label="检索解禁日历"
          placeholder="股票、代码、日期或原因"
          onEnter={() => load()}
        />
        {#if view === 'calendar'}
          <Select
            value={progress}
            options={PROGRESS}
            label="状态"
            width="140px"
            onChange={(next) => {
              progress = next;
              load();
            }}
          />
        {/if}
        <Select
          value={reason}
          options={reasons}
          label="解禁原因"
          width="230px"
          onChange={(next) => {
            reason = next;
            load();
          }}
        />
      {/snippet}

      <DataTable
        columns={eventColumns}
        rows={doc?.events ?? []}
        rowKey={(row) => `${row.detail_id}:${row.security.security_id}`}
        onRowClick={openEvent}
        isActive={(row) => row.detail_id === selectedId}
      />
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="EVENT DETAIL"
      title={event ? event.security.name || event.security.code : '事件详情'}
      subtitle={event ? `${event.security.security_id} · ${text(event.reason)}` : '点击左侧任意事件展开'}
      busy={detail.busy}
      error={detail.error}
      empty={!detail.loaded && !detail.busy}
      emptyText={view === 'calendar' ? '点击左侧任意事件，读取该事件的锁定批次与逐股东明细。' : '点击左侧任意历史事件查看解禁规模和股本占比。'}
    >
      {#if event}
        <button class="jump" type="button" onclick={gotoWorkbench}>
          在个股工作台打开 {event.security.name || event.security.code}
        </button>

        <StatGrid stats={eventStats} inline />

        {#if view === 'calendar'}
          <h3>锁定批次</h3>
          <DataTable columns={lotColumns} rows={event.lots} />

          <h3>
            具体解禁股东
            <small>
              {holders ? `${holders.returned_shareholders} / ${holders.shareholder_count}` : '尚未发布'}
            </small>
          </h3>
          {#if holders?.shareholders.length}
            <p class="check">
              明细合计 {compact(holders.detail_unlock_shares, '股')} · 与主表差额
              {compact(holders.share_difference, '股')}
            </p>
            <DataTable columns={holderColumns} rows={holders.shareholders} />
          {:else}
            <p class="note">
              主表已给出解禁计划，但动态股东明细尚未发布。未实施事件出现这种情况是正常的。
            </p>
          {/if}
        {:else}
          <p class="note">该视图来自 JQGZ 的近期大比例解禁滚动窗口，可能同时包含已发生和临近日期；它没有 DBLJJ 的逐股东动态键，因此不会尝试伪造股东明细。</p>
        {/if}
      {/if}

      {#each detail.data?.detail_errors ?? [] as failure (failure.detail_id + failure.resource)}
        <p class="warn-line">{failure.resource} 当前没有可展开的股东明细</p>
      {/each}
    </Panel>
  {/snippet}
</Split>
{/if}

<style>
  .jump {
    display: block;
    width: 100%;
    height: 22px;
    margin-bottom: var(--sp-3);
    font-size: var(--fs-micro);
    color: var(--focus);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  .jump:hover {
    background: var(--bg-hover);
  }

  h3 {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: var(--sp-3);
    padding: var(--sp-4) 0 var(--sp-2);
    font-size: var(--fs-title);
    font-weight: 600;
  }

  h3 small {
    font-size: var(--fs-micro);
    font-weight: 400;
    color: var(--fg-mute);
  }

  .check {
    padding-bottom: var(--sp-2);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
    color: var(--fg-dim);
  }

  .note {
    padding: var(--sp-3);
    font-size: var(--fs-micro);
    line-height: var(--lh-body);
    color: var(--fg-mute);
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .warn-line {
    margin-top: var(--sp-2);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }
</style>
