<script lang="ts">
  /**
   * 该股的限售解禁（DBLJJ，单票模式）。
   *
   * 主表已把同日同票的多个锁定批次合并成一个事件，所以「批次」与「逐股东」
   * 是两层展开：批次来自主表合并前的原始行，股东明细是动态资源，
   * 未实施事件常常只有计划、还没有发布名单。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, fixed, money, percent, text, tone } from '../../../lib/fmt';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    MarketUnlocksDocument,
    UnlockEvent,
    UnlockLot,
    UnlockSummary,
    UnlockShareholder
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const unlocks = new Resource<MarketUnlocksDocument>();
  const recent = new Resource<MarketUnlocksDocument>();
  const doc = $derived(unlocks.data);
  const summary = $derived(doc?.summary as UnlockSummary | undefined);
  const events = $derived<UnlockEvent[]>(doc?.events ?? []);
  const recentEvents = $derived<UnlockEvent[]>(recent.data?.events ?? []);

  let selectedId = $state('');

  // 选中项落空时回落到最新事件，切换标的后不必额外重置
  const event = $derived(
    events.find((item) => item.detail_id === selectedId) ?? events[0] ?? null
  );
  const detail = $derived(
    doc?.details.find((item) => item.detail_id === event?.detail_id) ?? null
  );

  function load(refresh = false) {
    void unlocks.load(
      `/api/v1/market/unlocks?${queryString({
        market,
        code,
        include_details: 1,
        detail_limit: 2000,
        refresh: refresh ? 1 : 0
      })}`
    );
    void recent.load(
      `/api/v1/market/unlocks?${queryString({
        view: 'recent-large', market, code, limit: 100, refresh: refresh ? 1 : 0
      })}`
    );
  }

  const stats = $derived.by<Stat[]>(() => {
    if (!doc) return [];
    return [
      {
        label: '解禁事件',
        value: count(doc.counts.returned_events),
        note: summary?.first_date ? `${date(summary.first_date)} 起` : '窗口内无事件'
      },
      {
        label: '解禁股数',
        value: compact(summary?.total_unlock_shares, '股'),
        note: '同日批次已合并'
      },
      {
        label: '参考市值',
        value: money(summary?.total_unlock_market_value),
        note: '解禁前收盘价口径'
      },
      {
        label: '已实施 / 未实施',
        value: `${summary?.implemented_events ?? 0} / ${summary?.pending_events ?? 0}`
      },
      {
        label: '股东明细',
        value: count(doc.counts.returned_shareholders),
        note: `${doc.counts.detail_errors} 个事件待发布`
      },
      {
        label: '最近解禁日',
        value: date(summary?.last_date),
        note: text(events[0]?.reason)
      }
    ];
  });

  const eventColumns: Column<UnlockEvent>[] = [
    {
      key: 'date',
      label: '解禁日',
      width: '84px',
      num: true,
      value: (row) => date(row.date),
      sortValue: (row) => row.date
    },
    {
      key: 'progress',
      label: '状态',
      width: '58px',
      value: (row) => text(row.progress),
      tone: (row) => (row.progress === '实施' ? 'up' : 'flat')
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
    {
      key: 'value',
      label: '参考市值',
      align: 'right',
      num: true,
      value: (row) => money(row.unlock_market_value),
      sortValue: (row) => row.unlock_market_value ?? 0
    },
    {
      key: 'close',
      label: '解禁前收盘',
      align: 'right',
      num: true,
      value: (row) => fixed(row.pre_unlock_close)
    },
    {
      key: 'lots',
      label: '批次',
      align: 'right',
      width: '54px',
      num: true,
      value: (row) => count(row.lot_count),
      sortValue: (row) => row.lot_count ?? 0
    }
  ];

  const recentColumns: Column<UnlockEvent>[] = [
    { key: 'date', label: '解禁日', width: '84px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'reason', label: '解禁原因', wrap: true, value: (row) => text(row.reason) },
    { key: 'shares', label: '解禁股数', align: 'right', num: true, value: (row) => compact(row.unlock_shares, '股'), sortValue: (row) => row.unlock_shares ?? 0 },
    { key: 'ratio', label: '占总股本', align: 'right', num: true, value: (row) => percent(row.unlock_to_total_pct), sortValue: (row) => row.unlock_to_total_pct ?? 0 },
    { key: 'total', label: '总股本', align: 'right', num: true, value: (row) => compact(row.total_shares, '股'), sortValue: (row) => row.total_shares ?? 0 }
  ];

  const lotColumns: Column<UnlockLot>[] = [
    {
      key: 'months',
      label: '锁定期',
      align: 'right',
      num: true,
      value: (row) => `${fixed(row.lock_months, 0)} 个月`
    },
    {
      key: 'shares',
      label: '解禁数量',
      align: 'right',
      num: true,
      value: (row) => compact(row.unlock_shares, '股')
    },
    { key: 'issue', label: '发行价', align: 'right', num: true, value: (row) => fixed(row.issue_price) },
    {
      key: 'lock-return',
      label: '锁定期收益',
      align: 'right',
      num: true,
      value: (row) => percent(row.lock_return_pct, 2, true),
      tone: (row) => tone(row.lock_return_pct)
    },
    {
      key: 'pre',
      label: '解禁前一月',
      align: 'right',
      num: true,
      value: (row) => percent(row.pre_month_return_pct, 2, true),
      tone: (row) => tone(row.pre_month_return_pct)
    },
    {
      key: 'post',
      label: '解禁后一月',
      align: 'right',
      num: true,
      value: (row) => percent(row.post_month_return_pct, 2, true),
      tone: (row) => tone(row.post_month_return_pct)
    }
  ];

  /** 单个股东占本次事件的比例，接口只给绝对股数 */
  function holderShare(row: UnlockShareholder): string {
    const total = event?.unlock_shares ?? 0;
    return total && row.unlock_shares !== null ? percent((row.unlock_shares / total) * 100) : '—';
  }

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
    {
      key: 'value',
      label: '参考市值',
      align: 'right',
      num: true,
      value: (row) => money(row.unlock_market_value)
    },
    { key: 'progress', label: '状态', width: '54px', value: (row) => text(row.progress) },
    { key: 'reason', label: '解禁原因', wrap: true, value: (row) => text(row.reason) }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="限售解禁"
  eyebrow="DBLJJ · EVENT → SHAREHOLDERS"
  subtitle="隐藏键严格使用「解禁日期 + 股票代码」，同日同票的锁定批次已合并为一个事件"
  busy={unlocks.busy}
  error={unlocks.error}
  onRetry={() => load()}
  empty={unlocks.loaded && !unlocks.busy && !doc}
  emptyText="该股不在当前解禁主表窗口内"
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={unlocks.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  <StatGrid {stats} columns={6} />
</Panel>

<Panel
  title="近期大比例解禁"
  eyebrow="JQGZ · RECENT HIGH-RATIO UNLOCKS"
  subtitle={`${count(recentEvents.length)} 条滚动窗口记录 · 占总股本比例来自客户端原表`}
  busy={recent.busy}
  error={recent.error}
  onRetry={() => load()}
  empty={recent.loaded && !recent.busy && recentEvents.length === 0}
  emptyText="该股未进入近期大比例解禁名单"
  flush
  scroll
>
  <DataTable columns={recentColumns} rows={recentEvents} rowKey={(row) => row.detail_id} />
</Panel>

<Panel
  title="解禁事件"
  subtitle={`${count(events.length)} 个事件 · 点击任意行展开批次与股东`}
  empty={unlocks.loaded && !unlocks.busy && events.length === 0}
  emptyText="该股在近期主表窗口内没有限售解禁事件"
  flush
  scroll
>
  <DataTable
    columns={eventColumns}
    rows={events}
    rowKey={(row) => row.detail_id}
    sortKey="date"
    maxHeight="200px"
    onRowClick={(row) => (selectedId = row.detail_id)}
    isActive={(row) => row.detail_id === event?.detail_id}
  />
</Panel>

<Panel
  title={event ? `${date(event.date)} · ${text(event.reason)}` : '事件明细'}
  eyebrow="LOCK LOTS + SHAREHOLDERS"
  subtitle={event
    ? `${text(event.progress)} · ${compact(event.unlock_shares, '股')} · 参考市值 ${money(event.unlock_market_value)} · ${event.lot_count} 个锁定批次`
    : ''}
  empty={unlocks.loaded && !unlocks.busy && !event}
  emptyText="没有可展开的解禁事件"
  flush
  scroll
>
  {#if event}
    <h3>锁定批次</h3>
    <DataTable columns={lotColumns} rows={event.lots} />

    <h3>
      具体解禁股东
      <small>
        {detail ? `${detail.returned_shareholders} / ${detail.shareholder_count}` : '尚未发布'}
      </small>
    </h3>
    {#if detail?.shareholders.length}
      <p class="check">
        明细合计 {compact(detail.detail_unlock_shares, '股')} · 与主表差额
        {compact(detail.share_difference, '股')}
      </p>
      <DataTable columns={holderColumns} rows={detail.shareholders} maxHeight="320px" />
    {:else}
      <p class="note">主表已给出解禁计划，动态股东明细尚未发布；未实施事件出现这种情况是正常的。</p>
    {/if}
  {/if}

  {#each doc?.detail_errors ?? [] as failure (failure.detail_id + failure.resource)}
    <p class="warn-line">{failure.resource} 当前没有可展开的股东明细</p>
  {/each}
</Panel>

<style>
  h3 {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: var(--sp-3);
    padding: var(--sp-3) var(--sp-4) var(--sp-2);
    font-size: var(--fs-title);
    font-weight: 600;
    border-top: 1px solid var(--line);
  }

  h3:first-child {
    border-top: 0;
  }

  h3 small {
    font-size: var(--fs-micro);
    font-weight: 400;
    color: var(--fg-mute);
  }

  .check {
    padding: 0 var(--sp-4) var(--sp-2);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
    color: var(--fg-dim);
  }

  .note {
    margin: 0 var(--sp-4) var(--sp-4);
    padding: var(--sp-3);
    font-size: var(--fs-micro);
    line-height: var(--lh-body);
    color: var(--fg-mute);
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .warn-line {
    margin: var(--sp-2) var(--sp-4);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }
</style>
