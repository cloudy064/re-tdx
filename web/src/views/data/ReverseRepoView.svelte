<script lang="ts">
  /** 通达信 GZNHG：国债逆回购报价、交收日历和指定本金净收益。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type { MarketReverseRepoDocument, ReverseRepoRecord } from '../../types';

  const MARKETS = [
    { id: 'all', label: '沪深全部' },
    { id: 'sh', label: '上交所 GC' },
    { id: 'sz', label: '深交所 R' }
  ];
  const SORTS = [
    { id: 'net-rate', label: '净年化率' },
    { id: 'rate', label: '报价年化率' },
    { id: 'net-interest', label: '净收益' },
    { id: 'term', label: '期限' },
    { id: 'turnover', label: '成交额' }
  ];

  let market = $state('all');
  let sort = $state('net-rate');
  let principal = $state('100000');
  let query = $state('');
  const resource = new Resource<MarketReverseRepoDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);

  const stats = $derived.by<Stat[]>(() => [
    { label: '逆回购品种', value: count(doc?.summary.rows), note: `沪 ${count(doc?.summary.by_market?.sh)} · 深 ${count(doc?.summary.by_market?.sz)}` },
    { label: '行情覆盖', value: `${count(doc?.summary.quoted_rows)} / ${count(doc?.summary.rows)}`, note: doc?.availability === 'schedule-only' ? '仅交收日历' : '公开 L1 快照' },
    { label: '交收基准日', value: date(doc?.summary.schedule_date) },
    { label: '当前本金', value: compact(doc?.summary.principal_yuan, '元') },
    { label: '最高净年化', value: doc?.summary.best_net_rate ? `${fixed(doc.summary.best_net_rate.net_annualized_rate_pct, 3)}%` : '—', note: doc?.summary.best_net_rate?.security.name || doc?.summary.best_net_rate?.security.code },
    { label: '对应净收益', value: doc?.summary.best_net_rate ? `${fixed(doc.summary.best_net_rate.net_interest_yuan, 2)} 元` : '—' }
  ]);

  function normalizedPrincipal(): number {
    const parsed = Number(principal.replace(/,/g, '').trim());
    return Number.isFinite(parsed) ? Math.min(1_000_000_000, Math.max(1_000, Math.round(parsed))) : 100000;
  }

  async function load(refresh = false) {
    const amount = normalizedPrincipal();
    principal = String(amount);
    await resource.load(`/api/v1/market/reverse-repo?${queryString({
      view: 'rates', market, q: query.trim(), principal_yuan: amount,
      sort, order: sort === 'term' ? 'asc' : 'desc', limit: 100,
      quote_cache_ttl_seconds: 5, refresh: refresh ? 1 : 0
    })}`);
  }

  const columns: Column<ReverseRepoRecord>[] = [
    { key: 'security', label: '品种', width: '116px', value: (row) => row.security.name || row.security.code, sub: (row) => row.repo_id },
    { key: 'term', label: '期限', width: '64px', align: 'right', num: true, value: (row) => `${row.term_days} 天`, sortValue: (row) => row.term_days },
    { key: 'rate', label: '报价年化', width: '84px', align: 'right', num: true, value: (row) => row.annualized_rate_pct == null ? '—' : `${fixed(row.annualized_rate_pct, 3)}%`, sub: (row) => row.quote_price_source === 'pre-close' ? '盘前昨收' : '', sortValue: (row) => row.annualized_rate_pct ?? -9999 },
    { key: 'range', label: '日内区间', width: '122px', align: 'right', num: true, value: (row) => row.low_rate_pct == null || row.high_rate_pct == null ? '—' : `${fixed(row.low_rate_pct, 3)} — ${fixed(row.high_rate_pct, 3)}` },
    { key: 'interest-days', label: '计息天数', width: '78px', align: 'right', num: true, value: (row) => `${row.interest_days}${row.bonus_interest_days > 0 ? ` (+${row.bonus_interest_days})` : ''}`, sub: () => '实际 / 额外', sortValue: (row) => row.interest_days },
    { key: 'gross', label: '毛收益', width: '82px', align: 'right', num: true, value: (row) => row.gross_interest_yuan == null ? '—' : `${fixed(row.gross_interest_yuan, 2)} 元`, sortValue: (row) => row.gross_interest_yuan ?? -9999 },
    { key: 'fee', label: '手续费', width: '76px', align: 'right', num: true, value: (row) => `${fixed(row.fee_yuan, 2)} 元`, sub: (row) => `${fixed(row.fee_yuan_per_100k, 2)} / 10万` },
    { key: 'net', label: '净收益', width: '84px', align: 'right', num: true, value: (row) => row.net_interest_yuan == null ? '—' : `${fixed(row.net_interest_yuan, 2)} 元`, sortValue: (row) => row.net_interest_yuan ?? -9999 },
    { key: 'net-rate', label: '净年化', width: '82px', align: 'right', num: true, value: (row) => row.net_annualized_rate_pct == null ? '—' : `${fixed(row.net_annualized_rate_pct, 3)}%`, sortValue: (row) => row.net_annualized_rate_pct ?? -9999 },
    { key: 'available', label: '资金可用', width: '88px', num: true, value: (row) => date(row.funds_available_date), sortValue: (row) => row.funds_available_date },
    { key: 'withdrawable', label: '资金可取', width: '88px', num: true, value: (row) => date(row.funds_withdrawable_date), sortValue: (row) => row.funds_withdrawable_date },
    { key: 'turnover', label: '成交额', width: '108px', align: 'right', num: true, value: (row) => compact(row.turnover_amount_yuan, '元'), sortValue: (row) => row.turnover_amount_yuan ?? 0 }
  ];

  onMount(() => void load());
</script>

<PageHeader eyebrow="709/1721 + 0x054C · GZNHG100—102" title="国债逆回购收益" description="将当天交收日历与公开 L1 年化利率合并，按本金计算毛收益、手续费、净收益，以及资金可用和可取日期。" {stats}>
  {#snippet actions()}
    <Segmented options={MARKETS} value={market} onChange={(value) => { market = value; void load(); }} ariaLabel="逆回购市场" />
    <Button icon="refresh" busy={resource.busy} onclick={() => void load(true)}>刷新行情与日历</Button>
  {/snippet}
</PageHeader>

<Panel title="沪深逆回购比较" subtitle="报价是年化百分比；闭市后显示最近快照，不代表产生新成交" busy={resource.busy} error={resource.error} onRetry={() => void load()} empty={resource.loaded && rows.length === 0} emptyText="当前筛选没有逆回购品种。" flush scroll fill>
  {#snippet toolbar()}
    <TextInput bind:value={principal} width="150px" label="本金（元）" placeholder="100000" onEnter={() => void load()} />
    <Select options={SORTS} bind:value={sort} width="142px" label="排序" onChange={() => void load()} />
    <TextInput bind:value={query} icon="search" width="190px" label="检索" placeholder="GC001 / R-001" onEnter={() => void load()} />
    <Button onclick={() => void load()}>重新计算</Button>
  {/snippet}
  <DataTable columns={columns} rows={rows} rowKey={(row) => row.repo_id} stickyFirst sortKey={sort} sortDesc={sort !== 'term'} minWidth="1210px" />
</Panel>
