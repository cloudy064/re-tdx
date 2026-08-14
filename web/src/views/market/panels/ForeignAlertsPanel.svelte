<script lang="ts">
  /**
   * 外资持股预警历史（WZMRYJ · OWNERSHIP STATUS HISTORY）。
   *
   * 状态文案按通达信原文展示，不自行推导监管阈值。逼近/暂停买入属于风险提示
   * 而不是涨跌方向，因此状态一律用 warn 徽标，只有持股变化才用涨跌色。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, percent, signedCompact, text, tone } from '../../../lib/fmt';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    ForeignAlertRecord,
    ForeignAlertStatusKey,
    MarketForeignAlertsDocument
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const alerts = new Resource<MarketForeignAlertsDocument>();
  const doc = $derived(alerts.data);
  const selected = $derived(doc?.selected_alert ?? null);
  const history = $derived<ForeignAlertRecord[]>(doc?.history ?? []);

  function load(refresh = false) {
    void alerts.load(
      `/api/v1/market/foreign-alerts?${queryString({
        market,
        code,
        include_details: 1,
        detail_limit: 2000,
        refresh: refresh ? 1 : 0
      })}`
    );
  }

  /** 预警状态是风险语义，不能借用涨跌色；未知状态退回中性。 */
  function statusTone(key: ForeignAlertStatusKey): 'warn' | 'neutral' {
    return key === 'unknown' ? 'neutral' : 'warn';
  }

  const stats = $derived.by<Stat[]>(() => {
    if (!doc || !selected) return [];
    return [
      { label: '状态日期', value: date(selected.date), note: '上游预警清单口径' },
      {
        label: '外资持股',
        value: compact(selected.foreign_holding_shares, '股'),
        note: '详情原始单位为股'
      },
      {
        label: '占总股本',
        value: percent(selected.foreign_holding_ratio_pct),
        note: '上游原始比例'
      },
      {
        label: '当日持股变化',
        value: signedCompact(selected.daily_change_shares, '股'),
        tone: tone(selected.daily_change_shares),
        note: percent(selected.daily_change_pct, 2, true)
      },
      {
        label: '历史记录',
        value: count(doc.counts.history),
        note: '按日期降序'
      },
      {
        label: '主表覆盖',
        value: `${count(doc.summary.securities)} 只`,
        note: `数据日 ${date(doc.summary.date)}`
      }
    ];
  });

  const columns: Column<ForeignAlertRecord>[] = [
    { key: 'date', label: '日期', width: '96px', num: true, value: (row) => date(row.date) },
    { key: 'status', label: '状态', width: '132px', slot: true },
    {
      key: 'shares',
      label: '持股数量',
      align: 'right',
      num: true,
      value: (row) => compact(row.foreign_holding_shares, '股'),
      sortValue: (row) => row.foreign_holding_shares ?? 0
    },
    {
      key: 'ratio',
      label: '占总股本',
      align: 'right',
      num: true,
      value: (row) => percent(row.foreign_holding_ratio_pct),
      sortValue: (row) => row.foreign_holding_ratio_pct ?? 0
    },
    {
      key: 'change',
      label: '当日变化',
      align: 'right',
      num: true,
      value: (row) => signedCompact(row.daily_change_shares, '股'),
      tone: (row) => tone(row.daily_change_shares)
    },
    {
      key: 'changePct',
      label: '变化比例',
      align: 'right',
      num: true,
      value: (row) => percent(row.daily_change_pct, 2, true),
      tone: (row) => tone(row.daily_change_pct)
    }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="外资持股预警状态"
  eyebrow="WZMRYJ · OWNERSHIP STATUS"
  subtitle="状态按通达信原文展示，不自行推导监管阈值"
  busy={alerts.busy}
  error={alerts.error}
  onRetry={() => load()}
  empty={alerts.loaded && !alerts.busy && !selected}
  emptyText="当前证券不在最新外资持股预警清单中"
  scroll
>
  {#snippet actions()}
    {#if selected}
      <Badge tone={statusTone(selected.status_key)}>{text(selected.status)}</Badge>
    {/if}
    <Button icon="refresh" busy={alerts.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  <StatGrid {stats} columns={3} />
</Panel>

<Panel
  title="状态历史"
  eyebrow="STATUS HISTORY"
  subtitle="主表「万股」已换算为股，详情原始单位即为股"
  empty={!alerts.busy && Boolean(selected) && history.length === 0}
  emptyText="当前证券在预警主表中，但动态详情暂未返回历史记录"
  flush
  scroll
>
  <DataTable {columns} rows={history} rowKey={(row, index) => `${row.date}-${index}`}>
    {#snippet cell({ row, column })}
      {#if column.key === 'status'}
        <Badge tone={statusTone(row.status_key)}>{text(row.status)}</Badge>
      {/if}
    {/snippet}
  </DataTable>

  {#each doc?.detail_errors ?? [] as failure, index (index)}
    <p class="fail">{failure.resource || '详情'}：{failure.message}</p>
  {/each}
</Panel>

<style>
  .fail {
    padding: var(--sp-2) var(--sp-4);
    font-size: var(--fs-micro);
    color: var(--warn);
    background: var(--warn-soft);
    border-top: 1px solid var(--line);
  }
</style>
