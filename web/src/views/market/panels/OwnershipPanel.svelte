<script lang="ts">
  /**
   * 该股的增减持与股权质押（ZCJC + GQZY，单票模式）。
   *
   * 四张表来自同一份服务端主表核心，但视图参数不同：先串行取 changes 把
   * 15 张主表缓存热起来，其余三个视图再并发复用，避免各自重建一遍。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, fixed, money, percent, text } from '../../../lib/fmt';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import Segmented from '../../../ui/Segmented.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    InsiderChange,
    MarketOwnershipDocument,
    OwnershipChange,
    OwnershipPlan,
    PledgeHistory
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  type Section = 'changes' | 'plans' | 'insiders' | 'pledges';

  const SECTIONS = [
    { id: 'changes', label: '实际增减持', hint: '股东已完成的变动，ZCJC 动态键' },
    { id: 'plans', label: '拟增减持', hint: '计划规模与实施窗口' },
    { id: 'insiders', label: '董监高', hint: '内部人持股变动' },
    { id: 'pledges', label: '股权质押', hint: 'GQZY 完整质押历史' }
  ];

  let section = $state<Section>('changes');

  const changes = new Resource<MarketOwnershipDocument>();
  const plans = new Resource<MarketOwnershipDocument>();
  const insiders = new Resource<MarketOwnershipDocument>();
  const pledges = new Resource<MarketOwnershipDocument>();

  const busy = $derived(changes.busy || plans.busy || insiders.busy || pledges.busy);
  const error = $derived(changes.error || plans.error || insiders.error || pledges.error);
  const loaded = $derived(changes.loaded && plans.loaded && insiders.loaded && pledges.loaded);

  const changeRows = $derived<OwnershipChange[]>(changes.data?.change_history ?? []);
  const planRows = $derived<OwnershipPlan[]>(plans.data?.plans ?? []);
  const insiderRows = $derived<InsiderChange[]>(insiders.data?.insider_history ?? []);
  const pledgeRows = $derived<PledgeHistory[]>(pledges.data?.pledge_history ?? []);

  async function load(refresh = false) {
    const shared = { market, code, limit: 2000 };
    await changes.load(
      `/api/v1/market/ownership?${queryString({
        ...shared,
        view: 'changes',
        category: 'all',
        include_details: 1,
        detail_limit: 2000,
        refresh: refresh ? 1 : 0
      })}`
    );
    await Promise.all([
      plans.load(
        `/api/v1/market/ownership?${queryString({
          ...shared,
          view: 'plans',
          category: 'all',
          include_details: 0
        })}`
      ),
      insiders.load(
        `/api/v1/market/ownership?${queryString({
          ...shared,
          view: 'insiders',
          category: 'all',
          include_details: 1,
          detail_limit: 2000
        })}`
      ),
      pledges.load(
        `/api/v1/market/ownership?${queryString({
          ...shared,
          view: 'pledges',
          category: 'all',
          include_details: 1,
          detail_limit: 2000
        })}`
      )
    ]);
  }

  const latestChange = $derived(changeRows[0] ?? null);
  const latestPledge = $derived(pledgeRows[0] ?? null);

  const stats = $derived.by<Stat[]>(() => {
    if (!changes.data) return [];
    return [
      {
        label: '实际增减持',
        value: count(changeRows.length),
        note: latestChange
          ? `${date(latestChange.announcement_date)} · ${latestChange.direction_label}`
          : '暂无动态记录',
        tone: latestChange ? (latestChange.direction === 'increase' ? 'up' : 'down') : ''
      },
      { label: '拟增减持计划', value: count(planRows.length), note: '当前主表窗口' },
      {
        label: '董监高变动',
        value: count(insiderRows.length),
        note: insiderRows[0]?.position || '暂无内部人交易'
      },
      {
        label: '质押主表命中',
        value: count(pledges.data?.pledges.length ?? 0),
        note: '质押 · 预警 · 平仓 · 解押'
      },
      {
        label: '质押历史',
        value: count(pledgeRows.length),
        note: latestPledge ? `${date(latestPledge.pledge_date)} 起` : '暂无动态质押'
      },
      {
        label: '最新质押风险',
        value: text(latestPledge?.risk_status),
        note: latestPledge ? `距预警 ${percent(latestPledge.decline_to_warning_pct)}` : '',
        // 非「安全」按跌色示警，与平仓线同一语义
        tone:
          latestPledge?.risk_status && latestPledge.risk_status !== '安全' ? 'down' : ''
      }
    ];
  });

  // 变动股数、变动后持股在上游是「万股」，董监高表是「股」；
  // 服务端已按各表真实单位统一换算成股，这里直接输出，不再二次换算。
  const changeColumns: Column<OwnershipChange>[] = [
    {
      key: 'announce',
      label: '公告日',
      width: '84px',
      num: true,
      value: (row) => date(row.announcement_date),
      sortValue: (row) => row.announcement_date
    },
    {
      key: 'direction',
      label: '方向',
      width: '54px',
      value: (row) => row.direction_label,
      tone: (row) => (row.direction === 'increase' ? 'up' : 'down')
    },
    {
      key: 'shares',
      label: '变动股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.change_shares, '股'),
      tone: (row) => (row.direction === 'increase' ? 'up' : 'down'),
      sortValue: (row) => row.change_shares ?? 0
    },
    {
      key: 'price',
      label: '成交均价',
      align: 'right',
      num: true,
      value: (row) => fixed(row.average_price)
    },
    {
      key: 'after',
      label: '变动后持股',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_after_shares, '股'),
      sub: (row) => percent(row.holding_after_capital_pct),
      sortValue: (row) => row.holding_after_shares ?? 0
    },
    { key: 'actor', label: '变动人', wrap: true, value: (row) => text(row.actor) },
    {
      key: 'window',
      label: '变动区间',
      num: true,
      value: (row) => `${date(row.start_date)} — ${date(row.end_date)}`
    }
  ];

  const planColumns: Column<OwnershipPlan>[] = [
    {
      key: 'announce',
      label: '公告日',
      width: '84px',
      num: true,
      value: (row) => date(row.announcement_date),
      sortValue: (row) => row.announcement_date
    },
    {
      key: 'direction',
      label: '方向',
      width: '54px',
      value: (row) => row.direction_label,
      tone: (row) => (row.direction === 'increase' ? 'up' : 'down')
    },
    {
      key: 'scale',
      label: '计划规模',
      align: 'right',
      num: true,
      value: (row) => `${row.range} ${row.scale}`
    },
    {
      key: 'pct',
      label: '占股本',
      align: 'right',
      num: true,
      value: (row) => percent(row.capital_pct),
      sortValue: (row) => row.capital_pct ?? 0
    },
    {
      key: 'close',
      label: '公告收盘',
      align: 'right',
      num: true,
      value: (row) => fixed(row.announcement_close)
    },
    { key: 'actor', label: '计划人', wrap: true, value: (row) => text(row.actor) },
    { key: 'method', label: '方式', value: (row) => text(row.method) },
    {
      key: 'window',
      label: '实施窗口',
      num: true,
      value: (row) => `${date(row.start_date)} — ${date(row.end_date)}`
    }
  ];

  const insiderColumns: Column<InsiderChange>[] = [
    {
      key: 'date',
      label: '变动日',
      width: '84px',
      num: true,
      value: (row) => date(row.date),
      sortValue: (row) => row.date
    },
    {
      key: 'direction',
      label: '方向',
      width: '54px',
      value: (row) => row.direction_label,
      tone: (row) => (row.direction === 'increase' ? 'up' : 'down')
    },
    {
      key: 'shares',
      label: '变动股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.change_shares, '股'),
      tone: (row) => (row.direction === 'increase' ? 'up' : 'down'),
      sortValue: (row) => row.change_shares ?? 0
    },
    {
      key: 'amount',
      label: '变动金额',
      align: 'right',
      num: true,
      value: (row) => money(row.change_amount_yuan),
      tone: (row) => (row.direction === 'increase' ? 'up' : 'down'),
      sortValue: (row) => row.change_amount_yuan ?? 0
    },
    {
      key: 'price',
      label: '成交均价',
      align: 'right',
      num: true,
      value: (row) => fixed(row.average_price)
    },
    {
      key: 'after',
      label: '变动后持股',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_after_shares, '股'),
      sortValue: (row) => row.holding_after_shares ?? 0
    },
    {
      key: 'actor',
      label: '变动人',
      wrap: true,
      value: (row) => text(row.actor),
      sub: (row) => text(row.position)
    },
    {
      key: 'reason',
      label: '原因 / 关系',
      wrap: true,
      value: (row) => text(row.reason),
      sub: (row) => text(row.relationship)
    }
  ];

  const pledgeColumns: Column<PledgeHistory>[] = [
    {
      key: 'date',
      label: '质押日',
      width: '84px',
      num: true,
      value: (row) => date(row.pledge_date),
      sortValue: (row) => row.pledge_date
    },
    {
      key: 'holder',
      label: '股东',
      wrap: true,
      value: (row) => text(row.shareholder),
      sub: (row) => text(row.relationship)
    },
    { key: 'pledgee', label: '质押方', wrap: true, value: (row) => text(row.pledgee) },
    {
      key: 'shares',
      label: '质押股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.shares, '股'),
      sortValue: (row) => row.shares ?? 0
    },
    {
      key: 'holder-pct',
      label: '占所持',
      align: 'right',
      num: true,
      value: (row) => percent(row.single_holder_pct),
      sub: (row) => `累计 ${percent(row.cumulative_holder_pct)}`
    },
    {
      key: 'capital-pct',
      label: '占总股本',
      align: 'right',
      num: true,
      value: (row) => percent(row.single_capital_pct),
      sub: (row) => `累计 ${percent(row.cumulative_capital_pct)}`,
      sortValue: (row) => row.single_capital_pct ?? 0
    },
    {
      key: 'warn-line',
      label: '预警线',
      align: 'right',
      num: true,
      value: (row) => fixed(row.warning_line)
    },
    {
      key: 'liq-line',
      label: '平仓线',
      align: 'right',
      num: true,
      value: (row) => fixed(row.liquidation_line),
      tone: () => 'down'
    },
    {
      key: 'decline',
      label: '距预警跌幅',
      align: 'right',
      num: true,
      value: (row) => percent(row.decline_to_warning_pct),
      sortValue: (row) => row.decline_to_warning_pct ?? 0
    },
    {
      key: 'risk',
      label: '风险状态',
      width: '66px',
      value: (row) => text(row.risk_status),
      tone: (row) => (row.risk_status && row.risk_status !== '安全' ? 'down' : 'flat')
    },
    { key: 'maturity', label: '到期日', num: true, value: (row) => date(row.maturity_date) },
    { key: 'note', label: '说明', wrap: true, value: (row) => text(row.description) }
  ];

  const rowCount = $derived.by(() => {
    if (section === 'changes') return changeRows.length;
    if (section === 'plans') return planRows.length;
    if (section === 'insiders') return insiderRows.length;
    return pledgeRows.length;
  });

  const emptyText = $derived.by(() => {
    if (section === 'changes') return '该股在 ZCJC 动态资源里没有实际增减持记录';
    if (section === 'plans') return '该股没有命中拟增减持计划主表';
    if (section === 'insiders') return '该股没有可用的董监高持股变动';
    return '该股没有可用的股权质押历史';
  });

  const failures = $derived([
    ...(changes.data?.detail_errors ?? []),
    ...(insiders.data?.detail_errors ?? []),
    ...(pledges.data?.detail_errors ?? [])
  ]);

  $effect(() => {
    void market;
    void code;
    void load();
  });
</script>

<Panel
  title="增减持与股权质押"
  eyebrow="ZCJC + GQZY · SECURITY HISTORY"
  subtitle="主表命中与动态明细已按该股关联；万股字段由服务端按各表单位换算为股"
  {busy}
  {error}
  onRetry={() => void load()}
  empty={loaded && !busy && !changes.data}
  emptyText="该股不在当前股权变动主表窗口内"
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" {busy} onclick={() => void load(true)}>强制更新</Button>
  {/snippet}

  <StatGrid {stats} columns={6} />
</Panel>

<Panel
  title={SECTIONS.find((item) => item.id === section)?.label ?? ''}
  subtitle={`${count(rowCount)} 条`}
  empty={loaded && !busy && rowCount === 0}
  {emptyText}
  flush
  scroll
>
  {#snippet toolbar()}
    <Segmented
      options={SECTIONS}
      value={section}
      onChange={(next) => (section = next as Section)}
      ariaLabel="股权变动分区"
    />
  {/snippet}

  {#if section === 'changes'}
    <DataTable columns={changeColumns} rows={changeRows} sortKey="announce" />
  {:else if section === 'plans'}
    <DataTable columns={planColumns} rows={planRows} sortKey="announce" />
  {:else if section === 'insiders'}
    <DataTable columns={insiderColumns} rows={insiderRows} sortKey="date" />
  {:else}
    <DataTable columns={pledgeColumns} rows={pledgeRows} sortKey="date" />
  {/if}

  {#each failures as failure (failure.resource)}
    <p class="warn-line">{failure.resource} 暂无动态明细</p>
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
