<script lang="ts">
  /** HYLHB：三周期全席位活跃龙虎榜，点击证券展开逐日异动。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, percent, text, tone } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { ActiveLhbRanking, MarketActiveLhbDocument } from '../../types';

  type Period = '5d' | 'month' | 'half-year';
  const PERIODS = [
    { id: '5d', label: '近5日', hint: 'hylhb13801' },
    { id: 'month', label: '近一月', hint: 'hylhb13901' },
    { id: 'half-year', label: '近半年', hint: 'hylhb14001' }
  ];
  const DIRECTIONS = [
    { id: 'all', label: '全部方向' },
    { id: 'net-buy', label: '累计净买' },
    { id: 'net-sell', label: '累计净卖' }
  ];
  let period = $state<Period>('5d');
  let direction = $state('all');
  let query = $state('');
  const ranking = new Resource<MarketActiveLhbDocument>();
  const detail = new Resource<MarketActiveLhbDocument>();
  const doc = $derived(ranking.data);
  const selected = $derived(detail.data?.selected_ranking ?? null);

  const stats = $derived.by<Stat[]>(() => [
    { label: '活跃证券', value: count(doc?.summary.rows), note: `${count(doc?.summary.names_resolved)} 个名称已解析` },
    { label: '累计上榜', value: count(doc?.summary.event_count_sum), note: date(doc?.summary.statistics_date) },
    { label: '累计买入', value: compact(doc?.summary.buy_amount_yuan_sum, '元'), tone: 'up' },
    { label: '累计卖出', value: compact(doc?.summary.sell_amount_yuan_sum, '元'), tone: 'down' },
    { label: '累计净额', value: compact(doc?.summary.net_buy_amount_yuan_sum, '元'), tone: tone(doc?.summary.net_buy_amount_yuan_sum), note: `${count(doc?.summary.net_buy_rows)} 净买 / ${count(doc?.summary.net_sell_rows)} 净卖` }
  ]);

  function load(refresh = false) {
    void ranking.load(`/api/v1/market/active-lhb?${queryString({ period, direction, q: query.trim(), sort: 'events', order: 'desc', limit: 500, include_details: 0, refresh: refresh ? 1 : 0 })}`);
    detail.reset();
  }
  function switchPeriod(value: string) { period = value as Period; load(); }
  function open(row: ActiveLhbRanking) {
    void detail.load(`/api/v1/market/active-lhb?${queryString({ period, market: row.security.market, code: row.security.code, include_details: 1, detail_limit: 500 })}`);
  }
  const columns: Column<ActiveLhbRanking>[] = [
    { key: 'security', label: '证券', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'events', label: '上榜次数', width: '76px', align: 'right', num: true, value: (row) => count(row.event_count), sortValue: (row) => row.event_count },
    { key: 'net', label: '累计净额', width: '105px', align: 'right', num: true, value: (row) => compact(row.net_buy_amount_yuan, '元'), tone: (row) => tone(row.net_buy_amount_yuan), sortValue: (row) => row.net_buy_amount_yuan },
    { key: 'buy', label: '累计买入', width: '105px', align: 'right', num: true, value: (row) => compact(row.buy_amount_yuan, '元'), tone: () => 'up', sortValue: (row) => row.buy_amount_yuan },
    { key: 'sell', label: '累计卖出', width: '105px', align: 'right', num: true, value: (row) => compact(row.sell_amount_yuan, '元'), tone: () => 'down', sortValue: (row) => row.sell_amount_yuan },
    { key: 'ratio', label: '买卖比', width: '72px', align: 'right', num: true, value: (row) => fixed(row.buy_sell_ratio, 2), sortValue: (row) => row.buy_sell_ratio ?? 0 },
    { key: 'return', label: '区间涨幅', width: '82px', align: 'right', num: true, value: (row) => percent(row.period_return_pct), tone: (row) => tone(row.period_return_pct), sortValue: (row) => row.period_return_pct ?? -9999 }
  ];
  onMount(load);
</script>

<PageHeader eyebrow="HYLHB · 709/1721" title="活跃龙虎榜" description="按近5日、近一月、近半年聚合全部龙虎榜上榜次数与累计买卖额；点击证券查看每次异动类型。它不是行业榜，也不是仅机构席位口径。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={ranking.busy} onclick={() => load(true)}>刷新三周期主表</Button>{/snippet}
</PageHeader>

<Split asideWidth="410px">
  {#snippet main()}
    <Panel title="活跃证券排行" subtitle={`${doc?.period_label ?? ''} · 每周期固定返回前 50 名`} busy={ranking.busy} error={ranking.error} onRetry={load} empty={ranking.loaded && (doc?.rankings.length ?? 0) === 0} emptyText="当前筛选没有活跃龙虎榜证券。" flush scroll fill>
      {#snippet toolbar()}
        <Segmented options={PERIODS} value={period} onChange={switchPeriod} ariaLabel="活跃龙虎周期" />
        <Select options={DIRECTIONS} bind:value={direction} width="132px" label="方向" onChange={() => load()} />
        <TextInput bind:value={query} icon="search" width="205px" label="检索" placeholder="股票名称或代码" onEnter={load} />
      {/snippet}
      <DataTable numbered columns={columns} rows={doc?.rankings ?? []} rowKey={(row) => row.ranking_id} onRowClick={open} isActive={(row) => row.ranking_id === selected?.ranking_id} stickyFirst sortKey="events" minWidth="790px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel title={selected ? selected.security.name || selected.security.code : '历史异动'} eyebrow="HYLHB DETAIL" subtitle={selected ? `${selected.period_label} · ${count(detail.data?.events.length)} 条异动原因` : '从左侧选择证券'} busy={detail.busy} error={detail.error} empty={!selected} emptyText="点击左侧证券读取该周期逐日异动。" scroll fill>
      {#if selected}
        <button class="jump" type="button" onclick={() => router.go(stockPath(selected!.security.market, selected!.security.code, 'lhb'))}>在个股龙虎榜页打开</button>
        <StatGrid inline stats={[
          { label: '上榜次数', value: count(selected.event_count) },
          { label: '累计净额', value: compact(selected.net_buy_amount_yuan, '元'), tone: tone(selected.net_buy_amount_yuan) },
          { label: '区间涨幅', value: percent(selected.period_return_pct), tone: tone(selected.period_return_pct) }
        ]} />
        <ul class="events">
          {#each detail.data?.events ?? [] as event, index (index)}
            <li><div><time>{date(event.event_date)}</time><span class={tone(event.change_pct)}>{percent(event.change_pct, 2, true)}</span></div><strong>{text(event.event_type)}</strong><p>换手 {percent(event.turnover_rate_pct)} · 买 {compact(event.buy_amount_yuan, '元')} · 卖 {compact(event.sell_amount_yuan, '元')}</p></li>
          {/each}
        </ul>
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .jump { width: 100%; height: 22px; margin-bottom: var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  .events { display: flex; flex-direction: column; gap: var(--sp-2); margin: var(--sp-3) 0 0; padding: 0; list-style: none; }
  .events li { padding: var(--sp-2) var(--sp-3); background: var(--bg-raised); border: 1px solid var(--line); border-radius: var(--radius); }
  .events div { display: flex; justify-content: space-between; }
  time, p { color: var(--fg-mute); }
  strong, p, time, span { font-size: var(--fs-micro); }
  p { margin: 2px 0 0; }
</style>
