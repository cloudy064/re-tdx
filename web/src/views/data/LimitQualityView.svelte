<script lang="ts">
  /** 四路原生数据闭环：服务端封板榜、五档、统计资源与集合竞价。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, marketKey, percent, price, text, time, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import type { LimitQualityDocument, LimitQualityRecord } from '../../types';

  const REFRESH_MS = 5000;
  const quality = new Resource<LimitQualityDocument>();
  const doc = $derived(quality.data);
  const rows = $derived(doc?.records ?? []);
  const staleError = $derived(doc && quality.error ? quality.error : '');

  const stats = $derived.by(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return [
      { label: '行情日', value: date(doc?.trade_date), note: `更新 ${time(doc?.generated_at)}` },
      { label: '封板确认', value: `${count(summary.depth_sealed)} / ${count(summary.records)}`, tone: 'up' as const },
      { label: '当前封单合计', value: compact(summary.total_current_seal_amount_yuan, '元') },
      { label: '统计关联', value: `${count(summary.stats_linked)} / ${count(summary.records)}`, note: `缓存 ${count(doc?.statistics_cache_age_seconds)}s` },
      { label: '可比历史封单', value: count(summary.history_comparable), note: '仅昨日正封单可比' },
      { label: '竞价复算', value: count(summary.auction_linked), note: '榜单前列逐点序列' }
    ];
  });

  function direction(row: LimitQualityRecord): string {
    const raw = row.quality.auction?.last_sample_unmatched_direction_raw;
    return raw === null || raw === undefined ? '—' : raw > 0 ? '买方未匹配' : raw < 0 ? '卖方未匹配' : '平衡';
  }

  const columns: Column<LimitQualityRecord>[] = [
    {
      key: 'security', label: '股票', width: '126px',
      value: (row) => text(row.name || row.code), sub: (row) => row.security_id
    },
    {
      key: 'change', label: '涨幅', align: 'right', num: true,
      value: (row) => delta(row.change_pct), tone: (row) => tone(row.change_pct),
      sortValue: (row) => row.change_pct ?? 0
    },
    {
      key: 'seal', label: '当前封单', align: 'right', num: true,
      value: (row) => compact(row.quality.current_seal_amount_yuan, '元'),
      sub: (row) => row.quality.depth_status === 'sealed' ? '五档确认' : '盘口已变化',
      sortValue: (row) => row.quality.current_seal_amount_yuan ?? 0
    },
    {
      key: 'seal-free', label: '封流比', align: 'right', num: true,
      value: (row) => percent(row.quality.seal_free_float_pct),
      sub: (row) => compact(row.quality.free_float_market_value_yuan, '元'),
      sortValue: (row) => row.quality.seal_free_float_pct ?? 0
    },
    {
      key: 'history', label: '封昨比', align: 'right', num: true,
      value: (row) => row.quality.seal_to_previous === null ? '—' : `${fixed(row.quality.seal_to_previous, 2)}×`,
      sub: (row) => row.quality.stats_alignment === 'previous-resource-day' ? `统计日 ${date(row.quality.stats_date)}` : row.quality.stats_alignment,
      tone: (row) => tone(row.quality.seal_change_pct),
      sortValue: (row) => row.quality.seal_to_previous ?? -1
    },
    {
      key: 'decay', label: '封单衰减', align: 'right', num: true,
      value: (row) => percent(row.quality.seal_decay_pct, 2, true),
      tone: (row) => tone(row.quality.seal_decay_pct === null ? null : -row.quality.seal_decay_pct),
      sortValue: (row) => row.quality.seal_decay_pct ?? Number.MAX_SAFE_INTEGER
    },
    {
      key: 'rush', label: '开盘抢筹', align: 'right', num: true,
      value: (row) => delta(row.opening_rush),
      sub: (row) => row.quality.auction ? `复算误差 ${fixed(row.quality.auction.opening_rush_error_pct_point, 2)}pp` : '未取逐点',
      tone: (row) => tone(row.opening_rush), sortValue: (row) => row.opening_rush ?? 0
    },
    {
      key: 'auction', label: '竞价未匹配', align: 'right', num: true,
      value: (row) => compact(row.quality.auction?.last_sample_unmatched_volume_hand, '手'),
      sub: direction
    },
    { key: 'status', label: '状态', width: '62px', align: 'center', slot: true }
  ];

  function load(silent = false, refresh = false) {
    void quality.load(`/api/v1/market/limit-quality?${queryString({
      limit: 200,
      auction_limit: 20,
      cache_ttl_seconds: 5,
      refresh: refresh ? 1 : 0
    })}`, { silent });
  }

  function open(row: LimitQualityRecord) {
    const market = marketKey(row.market_id);
    app.setStock({ market, code: row.code, name: row.name });
    router.go(stockPath(market, row.code));
  }

  onMount(() => {
    load();
    const tick = () => { if (!document.hidden) load(true); };
    const timer = window.setInterval(tick, REFRESH_MS);
    document.addEventListener('visibilitychange', tick);
    return () => {
      window.clearInterval(timer);
      document.removeEventListener('visibilitychange', tick);
    };
  });
</script>

<PageHeader
  eyebrow="0x054B + 0x0547 + 0x06B9 + 0x056A"
  title="涨停与竞价质量"
  description="封板榜、实时五档、在线统计和集合竞价四路交叉验证；封单增强为红、衰减为绿。"
  {stats}
>
  {#snippet actions()}
    {#if staleError}<Badge tone="warn">刷新失败</Badge>{:else if doc}<Badge tone="up">实时</Badge>{/if}
    <Button icon="refresh" busy={quality.busy} onclick={() => load(false, true)}>强制刷新</Button>
  {/snippet}
</PageHeader>

<Panel
  flush scroll fill
  title="当前封板候选"
  subtitle={staleError || '按服务端封单额降序；五档变化会立即标记，不继续冒充封板'}
  busy={quality.busy}
  error={doc ? '' : quality.error}
  empty={quality.loaded && rows.length === 0}
  emptyText="当前服务端没有返回封板候选，非交易时段可能为空。"
  onRetry={() => load()}
>
  <DataTable {columns} {rows} rowKey={(row) => row.security_id} onRowClick={open} stickyFirst numbered>
    {#snippet cell({ row, column })}
      {#if column.key === 'status'}
        <Badge tone={row.quality.depth_status === 'sealed' ? 'up' : 'warn'}>
          {row.quality.depth_status === 'sealed' ? '封板' : '变化'}
        </Badge>
      {/if}
    {/snippet}
  </DataTable>
</Panel>
