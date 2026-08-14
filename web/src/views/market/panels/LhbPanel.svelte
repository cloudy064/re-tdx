<script lang="ts">
  /**
   * 龙虎榜事件与营业部席位（LHBFX · 709 / 1721）。
   *
   * 同一股票同日可能因多个上榜原因产生多个事件，金额不能合并，
   * 所以这里按 event_id 逐条列出，再展开选中事件的席位明细（主从结构）。
   */
  import { onDestroy, onMount } from 'svelte';
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import {
    DASH,
    compact,
    count,
    date,
    num,
    percent,
    signedCompact,
    text,
    tone
  } from '../../../lib/fmt';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { LhbDetailRow, LhbDocument, LhbEvent } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const lhb = new Resource<LhbDocument>();
  const doc = $derived(lhb.data);
  const events = $derived<LhbEvent[]>(doc?.events ?? []);

  let picked = $state('');

  // 刷新后旧 event_id 可能已经不存在，回落到首个事件而不是留空
  const selected = $derived<LhbEvent | null>(
    events.find((item) => item.event_id === picked) ?? events[0] ?? null
  );
  const details = $derived<LhbDetailRow[]>(selected?.details ?? []);

  let timer: ReturnType<typeof setInterval> | undefined;

  function load(silent = false, refresh = false) {
    void lhb.load(
      `/api/v1/market/lhb?${queryString({
        market,
        code,
        include_details: 1,
        limit: 10,
        refresh: refresh ? 1 : 0
      })}`,
      { silent }
    );
  }

  const summary = $derived.by<Stat[]>(() => {
    if (!doc) return [];
    const s = doc.summary;
    return [
      { label: '上榜事件', value: count(s.event_count), note: `${count(s.unique_dates)} 个交易日` },
      { label: '分析视图命中', value: count(s.view_memberships), note: '8 类视图聚合' },
      { label: '席位明细行', value: count(s.detail_rows), note: '含买卖席位与合计行' },
      { label: '最新上榜日', value: date(s.latest_date), note: `主表缓存 ${doc.cache.master_age_seconds}s` },
      { label: '事件买入合计', value: compact(s.event_reported_buy_amount_sum, '元'), tone: 'up' },
      { label: '事件卖出合计', value: compact(s.event_reported_sell_amount_sum, '元'), tone: 'down' },
      {
        label: '事件净额合计',
        value: signedCompact(s.event_reported_net_buy_sum, '元'),
        tone: tone(s.event_reported_net_buy_sum)
      }
    ];
  });

  const eventColumns: Column<LhbEvent>[] = [
    { key: 'date', label: '上榜日', width: '84px', num: true, value: (row) => date(row.date) },
    {
      key: 'reason',
      label: '上榜原因 / 分析视图',
      wrap: true,
      value: (row) => row.event_types.join('；') || '龙虎榜事件',
      sub: (row) => row.views.join(' · ')
    },
    {
      key: 'net',
      label: '净买入',
      align: 'right',
      num: true,
      value: (row) => signedCompact(num(row.metrics.net_buy), '元'),
      tone: (row) => tone(num(row.metrics.net_buy)),
      sortValue: (row) => num(row.metrics.net_buy) ?? 0
    },
    {
      key: 'buy',
      label: '买入额',
      align: 'right',
      num: true,
      value: (row) => compact(num(row.metrics.buy_amount), '元'),
      sub: (row) => percent(num(row.metrics.buy_share_pct)),
      tone: () => 'up'
    },
    {
      key: 'sell',
      label: '卖出额',
      align: 'right',
      num: true,
      value: (row) => compact(num(row.metrics.sell_amount), '元'),
      sub: (row) => percent(num(row.metrics.sell_share_pct)),
      tone: () => 'down'
    },
    {
      key: 'institution',
      label: '机构买 / 卖',
      align: 'right',
      num: true,
      value: (row) =>
        `${count(num(row.metrics.institution_buy_count) ?? 0)} / ${count(num(row.metrics.institution_sell_count) ?? 0)}`
    },
    {
      key: 'rows',
      label: '席位行',
      align: 'right',
      num: true,
      value: (row) => count(row.detail_row_count ?? row.details.length)
    }
  ];

  function sideLabel(row: LhbDetailRow): string {
    const side = row.side === 'B' ? '买' : row.side === 'S' ? '卖' : text(row.side);
    const rank = String(row.rank) === '0' ? '合计' : text(row.rank);
    return `${side} ${rank}`;
  }

  const detailColumns: Column<LhbDetailRow>[] = [
    {
      key: 'side',
      label: '方向 / 排名',
      width: '78px',
      value: sideLabel,
      tone: (row) => (row.side === 'B' ? 'up' : row.side === 'S' ? 'down' : 'flat')
    },
    {
      key: 'broker',
      label: '营业部 / 合计',
      wrap: true,
      value: (row) => text(row.broker),
      sub: (row) => (row.tag ? String(row.tag) : '')
    },
    {
      key: 'buy',
      label: '买入额',
      align: 'right',
      num: true,
      value: (row) => compact(num(row.buy_amount), '元'),
      tone: () => 'up'
    },
    {
      key: 'sell',
      label: '卖出额',
      align: 'right',
      num: true,
      value: (row) => compact(num(row.sell_amount), '元'),
      tone: () => 'down'
    },
    {
      key: 'net',
      label: '净买入',
      align: 'right',
      num: true,
      value: (row) => signedCompact(num(row.net_buy), '元'),
      tone: (row) => tone(num(row.net_buy))
    },
    {
      key: 'share',
      label: '占成交比',
      align: 'right',
      num: true,
      value: (row) => percent(num(row.share_percent))
    },
    {
      key: 'rate',
      label: '1 / 3 / 5 日成功率',
      align: 'right',
      num: true,
      // 合计行没有席位胜率，留破折号避免误读成 0
      value: (row) =>
        row.row_type === 'total'
          ? DASH
          : `${text(row.buy_success_rate_1d)} / ${text(row.buy_success_rate_3d)} / ${text(row.buy_success_rate_5d)}`
    }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });

  onMount(() => {
    // 主表 60 秒一轮；页面不可见时跳过，避免后台空转
    timer = setInterval(() => {
      if (!document.hidden) load(true);
    }, 60000);
  });

  onDestroy(() => clearInterval(timer));
</script>

<Panel
  title="龙虎榜事件"
  eyebrow="LHBFX · 709 / 1721"
  subtitle="同股同日的多个上榜原因分列为独立事件，金额不合并"
  busy={lhb.busy}
  error={lhb.error}
  onRetry={() => load()}
  empty={lhb.loaded && !lhb.busy && events.length === 0}
  emptyText="当前 8 张龙虎榜主视图中没有这只股票"
  flush
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={lhb.busy} onclick={() => load(false, true)}>强制更新</Button>
  {/snippet}

  {#if doc}
    <div class="pad"><StatGrid stats={summary} columns={4} /></div>
  {/if}
  <DataTable
    columns={eventColumns}
    rows={events}
    rowKey={(row) => row.event_id}
    onRowClick={(row) => (picked = row.event_id)}
    isActive={(row) => row.event_id === selected?.event_id}
    maxHeight="240px"
  />
</Panel>

<Panel
  title="选中事件的席位明细"
  eyebrow="EVENT SEATS"
  subtitle={selected
    ? `${date(selected.date)} · 事件 ${selected.event_id} · ${selected.event_types.join('；') || '龙虎榜事件'}`
    : '在上表选择一个事件'}
  empty={!lhb.busy && details.length === 0}
  emptyText={selected ? '当前事件未返回营业部明细' : '尚未选中事件'}
  flush
  scroll
>
  {#snippet actions()}
    {#if selected?.key_conflict}
      <Badge tone="warn">市场键冲突 · 以详情为准</Badge>
    {/if}
  {/snippet}

  <DataTable columns={detailColumns} rows={details} rowKey={(row, index) => index} />
  {#if details.length}
    <p class="note">
      事件 ID、上榜原因与分析视图分别保留，同日多事件不合并金额。主表约 60 秒更新，事件席位缓存 5 分钟。
    </p>
  {/if}
</Panel>

<style>
  .pad {
    padding: var(--sp-3) var(--sp-4);
    border-bottom: 1px solid var(--line);
  }

  .note {
    padding: var(--sp-2) var(--sp-4);
    font-size: var(--fs-micro);
    color: var(--fg-mute);
    border-top: 1px solid var(--line);
  }
</style>
