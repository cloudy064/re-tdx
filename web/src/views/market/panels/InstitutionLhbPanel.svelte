<script lang="ts">
  /**
   * 机构席位龙虎统计（JGLHTJ · 四个滚动窗口）。
   *
   * 主表是「机构席位」在滚动周期内的累计，明细是「异动日全榜」的买卖总额，
   * 两者口径不同不能相加，所以拆成两个面板并在标题与列名上写清楚。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, fixed, percent, signedCompact, text, tone } from '../../../lib/fmt';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import Segmented, { type SegmentOption } from '../../../ui/Segmented.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { InstitutionLhbEvent, MarketInstitutionLhbDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  type Period = 'week' | 'month' | 'quarter' | 'year';

  const { market, code }: PanelProps = $props();

  const PERIODS: SegmentOption[] = [
    { id: 'week', label: '一周' },
    { id: 'month', label: '一月' },
    { id: 'quarter', label: '三月' },
    { id: 'year', label: '一年' }
  ];

  let period = $state<Period>('month');

  const institution = new Resource<MarketInstitutionLhbDocument>();
  const doc = $derived(institution.data);
  const ranking = $derived(doc?.selected_ranking ?? null);
  const events = $derived<InstitutionLhbEvent[]>(doc?.events ?? []);

  function load(refresh = false) {
    void institution.load(
      `/api/v1/market/institution-lhb?${queryString({
        period,
        market,
        code,
        include_details: 1,
        detail_limit: 1000,
        refresh: refresh ? 1 : 0
      })}`
    );
  }

  const stats = $derived.by<Stat[]>(() => {
    if (!doc || !ranking) return [];
    return [
      {
        label: '滚动周期',
        value: doc.period_label,
        note: `${date(ranking.earliest_trade_date)} — ${date(ranking.latest_trade_date)}`
      },
      {
        label: '机构参与次数',
        value: count(ranking.institution_participations),
        note: '主表机构席位统计'
      },
      {
        label: '机构席位买入',
        value: compact(ranking.institution_buy_amount_yuan, '元'),
        tone: 'up',
        note: '周期累计'
      },
      {
        label: '机构席位卖出',
        value: compact(ranking.institution_sell_amount_yuan, '元'),
        tone: 'down',
        note: '周期累计'
      },
      {
        label: '机构席位净额',
        value: signedCompact(ranking.net_institution_amount_yuan, '元'),
        tone: tone(ranking.net_institution_amount_yuan),
        note: `买卖比 ${fixed(ranking.buy_sell_ratio)}`
      },
      {
        label: '本周期覆盖',
        value: `${count(doc.summary.securities)} 只`,
        note: `净买 ${count(doc.summary.net_buy_securities)} · 净卖 ${count(doc.summary.net_sell_securities)}`
      }
    ];
  });

  const columns: Column<InstitutionLhbEvent>[] = [
    {
      key: 'date',
      label: '异动日',
      width: '96px',
      num: true,
      value: (row) => date(row.event_date),
      // 明细可能落在主表统计区间之外，标出来免得当成周期内数据
      sub: (row) => (row.outside_master_date_range ? '超出主表区间' : ''),
      sortValue: (row) => row.event_date ?? ''
    },
    { key: 'type', label: '异动类型', wrap: true, value: (row) => text(row.event_type) },
    {
      key: 'change',
      label: '当日涨幅',
      align: 'right',
      num: true,
      value: (row) => percent(row.event_change_pct, 2, true),
      tone: (row) => tone(row.event_change_pct)
    },
    {
      key: 'buy',
      label: '异动日榜单买入总额',
      align: 'right',
      num: true,
      value: (row) => compact(row.event_total_buy_amount_yuan, '元'),
      tone: () => 'up'
    },
    {
      key: 'sell',
      label: '异动日榜单卖出总额',
      align: 'right',
      num: true,
      value: (row) => compact(row.event_total_sell_amount_yuan, '元'),
      tone: () => 'down'
    },
    {
      key: 'net',
      label: '异动日榜单净额',
      align: 'right',
      num: true,
      value: (row) => signedCompact(row.event_net_buy_amount_yuan, '元'),
      tone: (row) => tone(row.event_net_buy_amount_yuan),
      sortValue: (row) => row.event_net_buy_amount_yuan ?? 0
    }
  ];

  $effect(() => {
    void market;
    void code;
    void period;
    load();
  });
</script>

<Panel
  title="机构席位滚动统计"
  eyebrow="JGLHTJ · FOUR ROLLING WINDOWS"
  subtitle="本表金额只统计机构专用席位，不含普通营业部"
  busy={institution.busy}
  error={institution.error}
  onRetry={() => load()}
  empty={institution.loaded && !institution.busy && !ranking}
  emptyText="当前证券在所选周期内没有机构席位龙虎记录"
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={institution.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  {#snippet toolbar()}
    <Segmented
      options={PERIODS}
      value={period}
      onChange={(next) => (period = next as Period)}
      ariaLabel="机构龙虎滚动周期"
    />
    {#if ranking}
      <Badge
        tone={ranking.direction === 'net-buy'
          ? 'up'
          : ranking.direction === 'net-sell'
            ? 'down'
            : 'neutral'}
      >
        {ranking.direction === 'net-buy'
          ? '机构净买'
          : ranking.direction === 'net-sell'
            ? '机构净卖'
            : '机构持平'}
      </Badge>
    {/if}
  {/snippet}

  <StatGrid {stats} columns={3} />
</Panel>

<Panel
  title="异动日榜单总额"
  eyebrow="EVENT-DAY WHOLE-BOARD"
  subtitle="下列金额是异动日龙虎榜全部席位合计（全榜口径），与上表机构席位口径不同，不可相加"
  empty={!institution.busy && Boolean(ranking) && events.length === 0}
  emptyText="该周期主表命中，但动态详情暂未返回异动记录"
  flush
  scroll
>
  <DataTable
    {columns}
    rows={events}
    rowKey={(row, index) => `${row.event_date}-${index}`}
    sortKey="date"
  />
  {#if events.length}
    <p class="note">
      上表是机构席位在滚动周期内的累计，本表是异动日全榜买卖总额，口径不同。具体营业部买卖席位请看「龙虎榜」页签。
    </p>
  {/if}
</Panel>

<style>
  .note {
    padding: var(--sp-2) var(--sp-4);
    font-size: var(--fs-micro);
    color: var(--fg-mute);
    border-top: 1px solid var(--line);
  }
</style>
