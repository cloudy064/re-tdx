<script lang="ts">
  /**
   * 该股的股份回购（QXFA104 / GGHG，单票模式）。
   *
   * A 股同一只票的多期方案不合并，逐期保留；港股走另一条链路——主表市场号是
   * 31 / 48，动态键 `gghg/{市场号}{代码}.jsn` 必须按上游原值拼，所以 market
   * 原样透传，不能先归一成 'hk'（那会被固定成 31，丢掉 48）。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, fixed, money, num, percent, text, tone } from '../../../lib/fmt';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    HongKongRepurchase,
    MarketRepurchasesDocument,
    RepurchasePlan
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  /** 服务端把这些别名归一到 0/1/2；其余两位市场号一律按境外处理 */
  const A_SHARE_MARKETS = new Set(['sz', 'sh', 'bj', '0', '1', '2', '44']);
  const hongKong = $derived(!A_SHARE_MARKETS.has(market.trim().toLowerCase()));

  const repurchases = new Resource<MarketRepurchasesDocument>();
  const doc = $derived(repurchases.data);
  const plans = $derived<RepurchasePlan[]>(doc?.plans ?? []);
  const history = $derived<HongKongRepurchase[]>(doc?.hong_kong_history ?? []);

  function load(refresh = false) {
    const query: Record<string, string | number> = hongKong
      ? {
          view: 'hong-kong',
          market,
          code,
          include_details: 1,
          detail_limit: 2000,
          limit: 1000,
          refresh: refresh ? 1 : 0
        }
      : { view: 'plans', market, code, limit: 1000, refresh: refresh ? 1 : 0 };
    void repurchases.load(`/api/v1/market/repurchases?${queryString(query)}`);
  }

  // 港股逐笔没有服务端汇总，按币种原值就地累计
  const hkTotals = $derived.by(() => {
    let shares = 0;
    let amount = 0;
    for (const row of history) {
      shares += num(row.shares) ?? 0;
      amount += num(row.amount) ?? 0;
    }
    return { shares, amount, currency: history[0]?.currency ?? '' };
  });

  const stats = $derived.by<Stat[]>(() => {
    if (!doc) return [];
    if (hongKong) {
      return [
        { label: '逐笔回购', value: count(history.length), note: '按交易日展开' },
        { label: '最近回购日', value: date(history[0]?.date), note: text(history[0]?.method) },
        { label: '累计回购数量', value: compact(hkTotals.shares, '股') },
        { label: '累计回购金额', value: compact(hkTotals.amount, hkTotals.currency) },
        {
          label: '最近均价',
          value: fixed(history[0]?.average_price, 3),
          note: `收盘 ${fixed(history[0]?.close, 3)}`
        },
        {
          label: '最近溢价率',
          value: percent(history[0]?.premium_pct, 2, true),
          tone: tone(history[0]?.premium_pct)
        }
      ];
    }
    const summary = doc.matched_plan_summary;
    return [
      {
        label: '历史方案',
        value: count(summary.plans),
        note: doc.selected_security?.security_id ?? ''
      },
      {
        label: '已完成 / 进行中',
        value: `${summary.completed_plans} / ${summary.active_plans}`,
        note: '按上游完成字段'
      },
      { label: '计划金额上限', value: money(summary.planned_amount_upper_yuan), note: '全部方案累计' },
      { label: '实际回购金额', value: money(summary.actual_amount_yuan) },
      { label: '实际回购股数', value: compact(summary.actual_shares, '股') },
      { label: '金额完成率', value: percent(summary.amount_completion_pct) }
    ];
  });

  const planColumns: Column<RepurchasePlan>[] = [
    {
      key: 'board',
      label: '董事会通过',
      width: '90px',
      num: true,
      value: (row) => date(row.board_approval_date),
      sortValue: (row) => row.board_approval_date
    },
    {
      key: 'status',
      label: '进度',
      width: '62px',
      value: (row) => (row.completed ? '已完成' : text(row.status || '进行中')),
      tone: (row) => (row.completed ? 'up' : 'flat')
    },
    {
      key: 'planned',
      label: '计划上限',
      align: 'right',
      num: true,
      value: (row) => money(row.planned_amount_upper_yuan),
      sub: (row) => compact(row.planned_shares, '股'),
      sortValue: (row) => row.planned_amount_upper_yuan ?? 0
    },
    {
      key: 'actual',
      label: '实际金额',
      align: 'right',
      num: true,
      value: (row) => money(row.actual_amount_yuan),
      sub: (row) => compact(row.actual_shares, '股'),
      // 回购是买入方向，一律用涨色
      tone: () => 'up',
      sortValue: (row) => row.actual_amount_yuan ?? 0
    },
    {
      key: 'completion',
      label: '金额完成率',
      align: 'right',
      num: true,
      value: (row) => percent(row.amount_completion_pct),
      sortValue: (row) => row.amount_completion_pct ?? 0
    },
    {
      key: 'pct',
      label: '实际占股本',
      align: 'right',
      num: true,
      value: (row) => percent(row.actual_capital_pct)
    },
    {
      key: 'price',
      label: '价格上限',
      align: 'right',
      num: true,
      value: (row) => fixed(row.planned_price_upper),
      sub: (row) => `实际 ${fixed(row.actual_price_low)} — ${fixed(row.actual_price_high)}`
    },
    { key: 'purpose', label: '回购用途', wrap: true, value: (row) => text(row.purpose) },
    {
      key: 'window',
      label: '实施窗口',
      num: true,
      value: (row) => `${date(row.start_date)} — ${date(row.end_date)}`,
      sub: (row) => `截止 ${date(row.cutoff_date)}`
    }
  ];

  const hongKongColumns: Column<HongKongRepurchase>[] = [
    {
      key: 'date',
      label: '交易日',
      width: '84px',
      num: true,
      value: (row) => date(row.date),
      sortValue: (row) => row.date
    },
    { key: 'price', label: '回购均价', align: 'right', num: true, value: (row) => fixed(row.average_price, 3) },
    { key: 'close', label: '收盘', align: 'right', num: true, value: (row) => fixed(row.close, 3) },
    {
      key: 'premium',
      label: '溢价率',
      align: 'right',
      num: true,
      value: (row) => percent(row.premium_pct, 2, true),
      tone: (row) => tone(row.premium_pct),
      sortValue: (row) => row.premium_pct ?? 0
    },
    {
      key: 'shares',
      label: '回购数量',
      align: 'right',
      num: true,
      value: (row) => compact(row.shares, '股'),
      tone: () => 'up',
      sortValue: (row) => row.shares ?? 0
    },
    {
      key: 'amount',
      label: '回购金额',
      align: 'right',
      num: true,
      value: (row) => compact(row.amount, row.currency || ''),
      tone: () => 'up',
      sortValue: (row) => row.amount ?? 0
    },
    {
      key: 'range',
      label: '最高 / 最低',
      align: 'right',
      num: true,
      value: (row) => `${fixed(row.high, 3)} / ${fixed(row.low, 3)}`
    },
    { key: 'method', label: '回购方式', wrap: true, value: (row) => text(row.method) }
  ];

  const rowCount = $derived(hongKong ? history.length : plans.length);

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="股份回购方案"
  eyebrow={hongKong ? 'GGHG · HK SECURITY HISTORY' : 'QXFA104 · SECURITY PLAN HISTORY'}
  subtitle={hongKong
    ? '港股按上游市场号（31 / 48）拼动态键，逐交易日展开回购明细'
    : '同一只票的多期方案逐期保留，金额与股数已由服务端统一换算为元和股'}
  busy={repurchases.busy}
  error={repurchases.error}
  onRetry={() => load()}
  empty={repurchases.loaded && !repurchases.busy && !doc}
  emptyText="该股没有命中回购主表"
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={repurchases.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  <StatGrid {stats} columns={6} />
</Panel>

<Panel
  title={hongKong ? '逐笔回购明细' : '历期回购方案'}
  subtitle={`${count(rowCount)} 条`}
  empty={repurchases.loaded && !repurchases.busy && rowCount === 0}
  emptyText={hongKong
    ? '该港股当前没有可展开的逐笔回购记录'
    : '该股没有命中通达信 A 股回购方案主表'}
  flush
  scroll
>
  {#if hongKong}
    <DataTable columns={hongKongColumns} rows={history} sortKey="date" />
  {:else}
    <DataTable columns={planColumns} rows={plans} sortKey="board" />
  {/if}

  {#each doc?.detail_errors ?? [] as failure (failure.resource)}
    <p class="warn-line">{failure.resource} 暂无动态数据</p>
  {/each}
</Panel>

<style>
  .warn-line {
    margin: var(--sp-2) var(--sp-4);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }
</style>
