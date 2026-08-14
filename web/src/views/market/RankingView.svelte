<script lang="ts">
  /**
   * 全市场实时榜单。0x054B 由服务端完成排序，前端只做呈现与轮询，
   * 不再下载全市场证券后在浏览器里排。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import {
    count,
    delta,
    marketKey,
    marketLabel,
    money,
    percent,
    price,
    text,
    time,
    tone
  } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import type { RankingDocument, RankingRecord } from '../../types';

  interface SortSpec {
    id: string;
    label: string;
    hint: string;
    /** 该排序对应的记录字段。 */
    pick: (row: RankingRecord) => number | null;
    format: (value: number | null) => string;
    /** 指标本身有方向（涨速、涨幅），才做红绿着色。 */
    signed: boolean;
  }

  const SORTS: SortSpec[] = [
    { id: 'rise-speed', label: '涨速', hint: '分钟级涨跌速度', pick: (row) => row.rise_speed, format: (value) => delta(value), signed: true },
    { id: 'change-pct', label: '涨幅', hint: '相对昨收涨跌幅', pick: (row) => row.change_pct, format: (value) => delta(value), signed: true },
    { id: 'amount', label: '成交额', hint: '当日累计成交金额', pick: (row) => row.amount, format: (value) => money(value), signed: false },
    { id: 'seal-amount', label: '封单额', hint: '涨跌停封单金额', pick: (row) => row.seal_amount_yuan, format: (value) => money(value), signed: false },
    { id: 'opening-rush', label: '开盘抢筹', hint: '开盘相对昨收的抢筹强度', pick: (row) => row.opening_rush, format: (value) => delta(value), signed: true },
    { id: 'short-turnover', label: '短换手', hint: '短周期换手率', pick: (row) => row.short_turnover, format: (value) => percent(value), signed: false },
    { id: 'volume-rise-speed', label: '量涨速', hint: '成交量放大速度', pick: (row) => row.volume_rise_speed, format: (value) => count(value), signed: true },
    { id: 'two-minute-amount', label: '2 分钟金额', hint: '最近两分钟成交金额', pick: (row) => row.two_minute_amount, format: (value) => money(value), signed: false },
    { id: 'opening-amount', label: '开盘金额', hint: '开盘集合竞价成交金额', pick: (row) => row.open_amount_yuan, format: (value) => money(value), signed: false }
  ];

  // 0x054B 单次请求上限 80 条，再多要靠 start 翻页，这里只给协议允许的档位。
  const SIZES = [
    { id: '30', label: '30 条' },
    { id: '50', label: '50 条' },
    { id: '80', label: '80 条' }
  ];

  const REFRESH_MS = 5000;

  let sort = $state('rise-speed');
  let size = $state('50');

  const ranking = new Resource<RankingDocument>();

  const doc = $derived(ranking.data);
  const rows = $derived(doc?.records ?? []);
  const spec = $derived(SORTS.find((item) => item.id === sort) ?? SORTS[0]);
  /** 静默刷新失败时保留旧表，只在面板角上提示。 */
  const staleError = $derived(doc && ranking.error ? ranking.error : '');

  const stats = $derived.by(() => {
    if (!doc) return [];
    return [
      { label: '服务端节点', value: text(doc.server_name), note: text(doc.endpoint) },
      { label: '更新时间', value: time(doc.generated_at), note: `每 ${REFRESH_MS / 1000} 秒自动刷新` },
      { label: '返回条数', value: count(doc.received), note: `命令 ${text(doc.command)}` },
      { label: '排序指标', value: spec.label, note: doc.sort_type ? text(doc.sort_type) : text(doc.sort) },
      { label: '封板数量', value: count(rows.filter((row) => row.is_sealed).length), tone: 'up' as const },
      {
        label: '上涨 / 下跌',
        value: `${rows.filter((row) => (row.change_pct ?? 0) > 0).length} / ${rows.filter((row) => (row.change_pct ?? 0) < 0).length}`
      }
    ];
  });

  const columns = $derived.by(() => {
    const list: Column<RankingRecord>[] = [
      { key: 'name', label: '名称', width: '106px', value: (row) => text(row.name) },
      {
        key: 'code',
        label: '代码',
        width: '78px',
        num: true,
        value: (row) => row.code,
        sub: (row) => marketLabel(row.market_id)
      },
      {
        key: 'price',
        label: '现价',
        align: 'right',
        num: true,
        value: (row) => price(row.last_price),
        tone: (row) => tone(row.change_pct),
        sortValue: (row) => row.last_price ?? 0
      },
      {
        key: 'change',
        label: '涨跌幅',
        align: 'right',
        num: true,
        value: (row) => delta(row.change_pct),
        tone: (row) => tone(row.change_pct),
        sortValue: (row) => row.change_pct ?? 0
      }
    ];

    // 涨幅/成交额已有固定列，再单开一列指标就是重复。
    if (sort !== 'change-pct' && sort !== 'amount') {
      list.push({
        key: 'metric',
        label: spec.label,
        align: 'right',
        num: true,
        value: (row) => spec.format(spec.pick(row)),
        tone: (row) => (spec.signed ? tone(spec.pick(row)) : ''),
        sortValue: (row) => spec.pick(row) ?? 0
      });
    }

    list.push(
      {
        key: 'amount',
        label: '成交额',
        align: 'right',
        num: true,
        value: (row) => money(row.amount),
        sub: (row) => `${count(row.total_hand)} 手`,
        sortValue: (row) => row.amount ?? 0
      },
      {
        key: 'seal',
        label: '封单额',
        align: 'right',
        num: true,
        value: (row) => money(row.seal_amount_yuan),
        tone: (row) => (row.is_sealed ? tone(row.change_pct) : ''),
        sortValue: (row) => row.seal_amount_yuan ?? 0
      },
      { key: 'sealed', label: '封板', width: '52px', align: 'center', slot: true },
      {
        key: 'bid',
        label: '买一',
        align: 'right',
        num: true,
        value: (row) => (row.bid1_price > 0 ? price(row.bid1_price) : '—'),
        sub: (row) => (row.bid1_volume_hand > 0 ? `${count(row.bid1_volume_hand)} 手` : ''),
        tone: () => 'up'
      },
      {
        key: 'ask',
        label: '卖一',
        align: 'right',
        num: true,
        value: (row) => (row.ask1_price > 0 ? price(row.ask1_price) : '—'),
        sub: (row) => (row.ask1_volume_hand > 0 ? `${count(row.ask1_volume_hand)} 手` : ''),
        tone: () => 'down'
      }
    );

    return list;
  });

  function load(silent = false) {
    void ranking.load(
      `/api/v1/market/ranking?${queryString({
        category: 'a-shares',
        sort,
        count: size,
        ascending: 0
      })}`,
      { silent }
    );
  }

  function open(row: RankingRecord) {
    const market = marketKey(row.market_id);
    app.setStock({ market, code: row.code, name: row.name });
    router.go(stockPath(market, row.code));
  }

  onMount(() => {
    load();
    // 后台标签页不刷新，避免整夜空转拉行情；切回前台立即补一次。
    const tick = () => {
      if (!document.hidden) load(true);
    };
    const timer = window.setInterval(tick, REFRESH_MS);
    document.addEventListener('visibilitychange', tick);
    return () => {
      window.clearInterval(timer);
      document.removeEventListener('visibilitychange', tick);
    };
  });
</script>

<PageHeader
  eyebrow="0x054B · TCP 7709"
  title="全市场实时榜单"
  description="九种服务端排序口径，沪深京 A 股统一榜单；点击任意行进入个股工作台。"
  {stats}
>
  {#snippet actions()}
    {#if staleError}
      <Badge tone="warn">刷新失败</Badge>
    {:else if ranking.loaded}
      <Badge tone="up">实时</Badge>
    {/if}
    <Button icon="refresh" busy={ranking.busy} onclick={() => load()}>立即刷新</Button>
  {/snippet}
</PageHeader>

<Split>
  {#snippet main()}
    <Panel
      flush
      scroll
      title={`${spec.label}榜`}
      subtitle={staleError || spec.hint}
      busy={ranking.busy}
      error={doc ? '' : ranking.error}
      empty={ranking.loaded && rows.length === 0}
      emptyText="服务端没有返回记录，非交易时段该榜单可能为空。"
      onRetry={() => load()}
    >
      {#snippet toolbar()}
        <Segmented
          options={SORTS}
          value={sort}
          ariaLabel="榜单排序指标"
          onChange={(next) => {
            sort = next;
            load();
          }}
        />
        <Select
          value={size}
          options={SIZES}
          label="条数"
          width="118px"
          onChange={(next) => {
            size = next;
            load();
          }}
        />
      {/snippet}

      <DataTable
        {columns}
        {rows}
        numbered
        stickyFirst
        rowKey={(row) => row.security_id}
        onRowClick={open}
        isActive={(row) => marketKey(row.market_id) === app.stock.market && row.code === app.stock.code}
      >
        {#snippet cell({ row })}
          {#if row.is_sealed}
            <Badge tone={(row.change_pct ?? 0) < 0 ? 'down' : 'up'}>封</Badge>
          {:else}
            <span class="mute">—</span>
          {/if}
        {/snippet}
      </DataTable>
    </Panel>
  {/snippet}
</Split>
