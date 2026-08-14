<script lang="ts">
  /**
   * 在线交易统计（0x06B9 · ZHB.ZIP）。
   * 服务端只给绝对值，封流比 / 封昨比 / 封单衰减这几个判断封板质量的比值
   * 在这里派生，避免每次人工心算。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, fixed, percent, tone } from '../../../lib/fmt';
  import Button from '../../../ui/Button.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { StatsDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const stats = new Resource<StatsDocument>();
  const record = $derived(stats.data?.records[0] ?? null);
  const stat = $derived(record?.stat ?? null);
  const stat2 = $derived(record?.stat2 ?? null);
  const exact = $derived(stats.data?.valuation ?? null);

  function load(refresh = false) {
    void stats.load(
      `/api/v1/market/stats?${queryString({ market, code, valuation: 1, refresh: refresh ? 1 : 0 })}`
    );
  }

  const valuation = $derived.by<Stat[]>(() => {
    if (!stat) return [];
    return [
      { label: '统计日', value: date(stat.stats_date) },
      {
        label: '动态 PE',
        value: fixed(exact?.metrics.pe_dynamic.value),
        note: '实时价 / 报告期年化 EPS'
      },
      {
        label: '静态 PE',
        value: fixed(exact?.metrics.pe_static.value ?? stat.pe_static),
        note: 'tdxstat 第 10 字段'
      },
      {
        label: 'PE(TTM)',
        value: fixed(exact?.metrics.pe_ttm.value ?? stat.pe_ttm),
        note: 'tdxstat 第 4 字段'
      },
      {
        label: 'PB(MRQ)',
        value: fixed(exact?.metrics.pb_mrq.value),
        note: '实时价 / 最新每股净资产'
      },
      {
        label: '估值使用价',
        value: fixed(exact?.inputs.price),
        note: exact?.inputs.price_field === 'pre_close_price' ? '昨收回退' : '最新价'
      },
      {
        label: '财务更新日',
        value: date(exact?.inputs.finance_updated_date),
        note: `${fixed(exact?.inputs.report_months, 0)} 个月报告期`
      },
      { label: '60 日 Beta', value: fixed(stat.beta_60d) },
      { label: '自由流通股本', value: compact(stat.free_float_shares, '股') },
      { label: '年内涨停天数', value: count(stat.year_limit_up_days) },
      {
        label: '统计窗口涨停',
        value: `${count(stat.limit_up_count_in_stat_days)} / ${count(stat.limit_stat_days)}`
      },
      { label: '连板天数', value: count(stat.limit_up_streak_days) }
    ];
  });

  /** 封流比：封单额 / 自由流通市值的近似——用自由流通股本乘以成交均价不可得，
      因此只给封单额与成交额的比值（封成比），并把口径写在标签上。 */
  const sealing = $derived.by<Stat[]>(() => {
    if (!stat2) return [];
    const seal = stat2.seal_amount_yuan ?? 0;
    const amount = stat2.amount_yuan ?? 0;
    const prevSeal = stat2.prev_seal_amount_yuan ?? 0;
    const sealToAmount = amount > 0 ? (seal / amount) * 100 : null;
    const sealToPrev = prevSeal > 0 ? (seal / prevSeal) * 100 : null;
    // 衰减为正表示封单变弱、负数表示增强，与涨停质量聚合接口保持同一口径。
    const decay = prevSeal > 0 ? (1 - seal / prevSeal) * 100 : null;
    return [
      { label: '当日成交额', value: compact(stat2.amount_yuan, '元') },
      { label: '当日封单额', value: compact(stat2.seal_amount_yuan, '元') },
      { label: '昨日封单额', value: compact(stat2.prev_seal_amount_yuan, '元') },
      { label: '前日封单额', value: compact(stat2.prev2_seal_amount_yuan, '元') },
      { label: '封成比（封单/成交额）', value: percent(sealToAmount) },
      { label: '封昨比（封单/昨封单）', value: percent(sealToPrev) },
      { label: '封单衰减', value: percent(decay, 2, true), tone: tone(decay === null ? null : -decay) },
      { label: '开盘量', value: compact(stat2.open_volume_hand, '手') },
      { label: '开盘金额', value: compact(stat2.open_amount_yuan, '元') },
      { label: '昨开盘金额', value: compact(stat2.prev_open_amount_yuan, '元') }
    ];
  });

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="估值与涨停统计"
  eyebrow="0x06B9 · ZHB.ZIP · tdxstat"
  subtitle={stats.data
    ? `统计日 ${date(stats.data.stats_date)} · 估值 ${exact?.available_metric_count ?? 0}/4 · 缓存 ${stats.data.cache_age_seconds}s / TTL ${stats.data.cache_ttl_seconds}s`
    : ''}
  busy={stats.busy}
  error={stats.error}
  onRetry={() => load()}
  empty={stats.loaded && !stats.busy && !stat}
  emptyText="统计包中没有该标的的 tdxstat 记录"
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={stats.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  <StatGrid stats={valuation} columns={4} />
</Panel>

<Panel
  title="封单与竞价"
  eyebrow="tdxstat2"
  subtitle="比值口径已在标签中标注，均由服务端绝对值派生"
  empty={!stat2 && !stats.busy}
  emptyText="统计包中没有该标的的 tdxstat2 记录"
  scroll
>
  <StatGrid stats={sealing} columns={5} />
</Panel>
