<script lang="ts">
  /**
   * 该股的大宗交易与意向申报（DZJY3 + DZJY13，单票模式）。
   *
   * 成交历史与意向申报是两份独立的动态资源，键都是「市场号 + 代码」；
   * 北交所按规范市场号 2 处理，不使用主表里的原始 44——归一由服务端完成，
   * 这里原样透传 market 即可。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, fixed, money, percent, text, tone } from '../../../lib/fmt';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    BlockTradeHistoryRow,
    BlockTradeIntentionRow,
    MarketBlockTradesDocument
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const blocks = new Resource<MarketBlockTradesDocument>();
  const doc = $derived(blocks.data);
  const history = $derived<BlockTradeHistoryRow[]>(doc?.security_history ?? []);
  const intentions = $derived<BlockTradeIntentionRow[]>(doc?.security_intentions ?? []);
  const latest = $derived(doc?.trades[0] ?? null);

  function load(refresh = false) {
    void blocks.load(
      `/api/v1/market/block-trades?${queryString({
        view: 'trades',
        market,
        code,
        include_details: 1,
        detail_limit: 1000,
        refresh: refresh ? 1 : 0
      })}`
    );
  }

  const stats = $derived.by<Stat[]>(() => {
    if (!doc) return [];
    return [
      {
        label: '主表上榜',
        value: count(doc.trades.length),
        note: latest ? date(latest.date) : '当前窗口未上榜'
      },
      { label: '逐笔成交历史', value: count(history.length), note: '保留同日多笔' },
      { label: '意向申报历史', value: count(intentions.length), note: '买入与卖出意向' },
      {
        label: '最新折溢价',
        value: percent(latest?.premium_pct, 2, true),
        note: latest ? money(latest.amount_yuan) : '暂无成交',
        tone: tone(latest?.premium_pct)
      },
      {
        label: '最新成交价',
        value: fixed(latest?.price),
        note: `收盘 ${fixed(latest?.close)}`
      },
      {
        label: '7 / 30 / 90 日上榜',
        value: `${latest?.frequency.days_7 ?? 0} / ${latest?.frequency.days_30 ?? 0} / ${latest?.frequency.days_90 ?? 0}`,
        note: text(latest?.security_type)
      }
    ];
  });

  const historyColumns: Column<BlockTradeHistoryRow>[] = [
    {
      key: 'date',
      label: '日期',
      width: '84px',
      num: true,
      value: (row) => date(row.date),
      sortValue: (row) => row.date
    },
    {
      key: 'price',
      label: '成交价',
      align: 'right',
      num: true,
      value: (row) => fixed(row.price),
      sortValue: (row) => row.price ?? 0
    },
    {
      key: 'amount',
      label: '成交额',
      align: 'right',
      num: true,
      value: (row) => money(row.amount_yuan),
      sortValue: (row) => row.amount_yuan ?? 0
    },
    {
      key: 'buyer',
      label: '买方营业部',
      wrap: true,
      value: (row) => text(row.buyer),
      tone: () => 'up'
    },
    {
      key: 'seller',
      label: '卖方营业部',
      wrap: true,
      value: (row) => text(row.seller),
      tone: () => 'down'
    }
  ];

  const intentionColumns: Column<BlockTradeIntentionRow>[] = [
    {
      key: 'date',
      label: '日期',
      width: '84px',
      num: true,
      value: (row) => date(row.date),
      sortValue: (row) => row.date
    },
    {
      key: 'direction',
      label: '方向',
      width: '54px',
      value: (row) => text(row.direction),
      tone: (row) =>
        row.direction.includes('买') ? 'up' : row.direction.includes('卖') ? 'down' : 'flat'
    },
    {
      key: 'price',
      label: '申报价',
      align: 'right',
      num: true,
      value: (row) => fixed(row.declaration_price)
    },
    { key: 'close', label: '收盘', align: 'right', num: true, value: (row) => fixed(row.close) },
    {
      key: 'premium',
      label: '折溢价',
      align: 'right',
      num: true,
      value: (row) => percent(row.premium_pct, 2, true),
      // 折价为负要跌色，溢价为正涨色
      tone: (row) => tone(row.premium_pct),
      sortValue: (row) => row.premium_pct ?? 0
    },
    {
      key: 'quantity',
      label: '数量',
      align: 'right',
      num: true,
      value: (row) => compact(row.quantity_shares, '股'),
      sortValue: (row) => row.quantity_shares ?? 0
    },
    {
      key: 'amount',
      label: '申报金额',
      align: 'right',
      num: true,
      value: (row) => money(row.amount_yuan),
      sortValue: (row) => row.amount_yuan ?? 0
    }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="大宗交易与意向申报"
  eyebrow="DZJY3 + DZJY13 · SECURITY HISTORY"
  subtitle="成交历史来自 DZJY3，意向申报来自 DZJY13；北交所动态键按规范市场号 2 生成"
  busy={blocks.busy}
  error={blocks.error}
  onRetry={() => load()}
  empty={blocks.loaded && !blocks.busy && !doc}
  emptyText="该股不在当前大宗交易主表窗口内"
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={blocks.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  <StatGrid {stats} columns={6} />
</Panel>

<Panel
  title="逐笔大宗成交"
  subtitle={`${count(history.length)} 条`}
  empty={blocks.loaded && !blocks.busy && history.length === 0}
  emptyText="当前动态资源没有该股的逐笔大宗成交"
  flush
  scroll
>
  <DataTable columns={historyColumns} rows={history} sortKey="date" />
</Panel>

<Panel
  title="意向申报"
  subtitle={`${count(intentions.length)} 条`}
  empty={blocks.loaded && !blocks.busy && intentions.length === 0}
  emptyText="当前动态资源没有该股的意向申报历史"
  flush
  scroll
>
  <DataTable columns={intentionColumns} rows={intentions} sortKey="date" />

  {#each doc?.detail_errors ?? [] as failure (failure.resource)}
    <p class="warn-line">{failure.resource} 暂无对应的动态明细</p>
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
