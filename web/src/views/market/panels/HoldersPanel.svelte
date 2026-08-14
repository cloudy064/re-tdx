<script lang="ts">
  /**
   * 股东与机构（/api/v1/security/profile + GDJC → GDJCXQ 反查链）。
   *
   * 十大流通股东行里带 gdid 的股东可以反查它在全市场的持仓，这条跨股票链
   * 是本页的核心价值：先拿股东的全部持仓记录（服务端缓存、网页分页 50 条），
   * 再按需展开某一只股票的逐报告期变化，不批量展开全部股票。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, num, percent, text, tone } from '../../../lib/fmt';
  import { router, stockPath } from '../../../lib/router.svelte';
  import { app } from '../../../lib/store.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    HolderHistoryDocument,
    HolderHistoryRecord,
    HolderStockPeriod,
    InstitutionHolder,
    SecurityProfileDocument
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();

  /** 跨股票主链在服务端缓存，网页一次只取一页；沿用旧版 50 条口径。 */
  const PAGE_SIZE = 50;

  const profile = new Resource<SecurityProfileDocument>();
  const holder = new Resource<HolderHistoryDocument>();

  let picked = $state<InstitutionHolder | null>(null);
  let stockCode = $state('');

  const doc = $derived(profile.data);
  const chain = $derived(holder.data);
  const page = $derived(chain?.pagination ?? null);

  function load(refresh = false) {
    picked = null;
    stockCode = '';
    holder.reset();
    void profile.load(
      `/api/v1/security/profile?${queryString({ market, code, refresh: refresh ? 1 : 0 })}`
    );
  }

  /** 上游 zb1 / cczlt 是小数比例，展示前统一放大成百分数。 */
  function ratio(value: unknown): number | null {
    const parsed = num(value);
    return parsed === null ? null : parsed * 100;
  }

  /**
   * 服务端已规范化时直接用 holders[index]；老资源没有规范化字段，
   * 只能从 gdjc 这条 F10 链接里把 gdid / tdxid / gp 拆出来。
   */
  function normalize(raw: Record<string, unknown>, index: number): InstitutionHolder | null {
    const ready = doc?.holders?.[index];
    if (ready) return ready;
    const sourceUrl = String(raw.gdjc ?? '');
    if (!sourceUrl) return null;
    try {
      const url = new URL(sourceUrl);
      const holderId = url.searchParams.get('gdid') ?? '';
      const referenceCode = url.searchParams.get('gp') ?? code;
      return {
        rank: raw.gdpm as string | number,
        type: String(raw.gdlx ?? ''),
        name: String(raw.gdmc ?? ''),
        short_name: '',
        holding_shares: raw.cgsl as string | number,
        free_float_share: raw.zb1 as string | number,
        change_shares: raw.zjgs as string | number,
        change_label: String(raw.zl ?? ''),
        holder_id: holderId,
        variant_id: url.searchParams.get('tdxid') ?? '',
        reference_code: referenceCode,
        source_url: sourceUrl,
        queryable: Boolean(holderId && /^\d{6}$/.test(referenceCode)),
        raw
      };
    } catch {
      return null;
    }
  }

  interface TopHolderRow {
    index: number;
    raw: Record<string, unknown>;
    ref: InstitutionHolder | null;
  }

  const topRows = $derived<TopHolderRow[]>(
    (doc?.top_float_holders ?? []).map((raw, index) => ({ index, raw, ref: normalize(raw, index) }))
  );

  const institutionRows = $derived<Array<Record<string, unknown>>>(doc?.institution_history ?? []);

  const topColumns: Column<TopHolderRow>[] = [
    {
      key: 'rank',
      label: '排名',
      width: '48px',
      align: 'right',
      num: true,
      value: (row) => text(row.raw.gdpm)
    },
    {
      key: 'holder',
      label: '股东',
      wrap: true,
      value: (row) => text(row.raw.gdmc),
      sub: (row) => (row.ref?.holder_id ? `gdid ${row.ref.holder_id}` : '')
    },
    { key: 'type', label: '类型', width: '96px', value: (row) => text(row.raw.gdlx) },
    {
      key: 'shares',
      label: '持股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.raw.cgsl, '股'),
      sortValue: (row) => num(row.raw.cgsl) ?? 0
    },
    {
      key: 'pct',
      label: '占流通',
      align: 'right',
      num: true,
      value: (row) => percent(ratio(row.raw.zb1)),
      sortValue: (row) => ratio(row.raw.zb1) ?? 0
    },
    {
      key: 'change',
      label: '变化',
      align: 'right',
      num: true,
      value: (row) => text(row.raw.zl),
      sub: (row) => compact(row.raw.zjgs, '股'),
      tone: (row) => tone(num(row.raw.zjgs))
    },
    { key: 'lookup', label: '反查', width: '76px', align: 'center', slot: true }
  ];

  const institutionColumns: Column<Record<string, unknown>>[] = [
    { key: 'period', label: '报告期', width: '88px', num: true, value: (row) => date(row.bgq) },
    { key: 'category', label: '机构类别', wrap: true, value: (row) => text(row.lb) },
    {
      key: 'firms',
      label: '家数',
      align: 'right',
      num: true,
      value: (row) => count(row.jgs),
      sortValue: (row) => num(row.jgs) ?? 0
    },
    {
      key: 'shares',
      label: '持股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.ccgs, '股'),
      sortValue: (row) => num(row.ccgs) ?? 0
    },
    {
      key: 'pct',
      label: '占流通',
      align: 'right',
      num: true,
      value: (row) => percent(ratio(row.cczlt)),
      sortValue: (row) => ratio(row.cczlt) ?? 0
    },
    {
      key: 'value',
      label: '持仓市值',
      align: 'right',
      num: true,
      value: (row) => compact(row.ccsz, '元'),
      sortValue: (row) => num(row.ccsz) ?? 0
    },
    {
      key: 'delta',
      label: '增减',
      align: 'right',
      num: true,
      value: (row) => compact(row.zjgs, '股'),
      tone: (row) => tone(num(row.zjgs))
    }
  ];

  const crossColumns: Column<HolderHistoryRecord>[] = [
    {
      key: 'period',
      label: '报告期',
      width: '88px',
      num: true,
      value: (row) => date(row.report_date)
    },
    {
      key: 'stock',
      label: '股票',
      value: (row) => text(row.name),
      sub: (row) => `${(row.market || '').toUpperCase()}${row.code}`
    },
    {
      key: 'shares',
      label: '持股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_shares, '股'),
      sortValue: (row) => num(row.holding_shares) ?? 0
    },
    {
      key: 'pct',
      label: '占比',
      align: 'right',
      num: true,
      value: (row) => percent(row.share_percent),
      sortValue: (row) => num(row.share_percent) ?? 0
    },
    {
      key: 'change',
      label: '变化',
      align: 'right',
      num: true,
      value: (row) => text(row.change_label),
      sub: (row) => compact(row.change_shares, '股'),
      tone: (row) => tone(num(row.change_shares))
    },
    {
      key: 'state',
      label: '状态',
      width: '72px',
      value: (row) => (row.currently_held ? '当前持有' : '历史'),
      tone: (row) => (row.currently_held ? 'up' : 'flat')
    },
    { key: 'goto', label: '工作台', width: '72px', align: 'center', slot: true }
  ];

  const periodColumns: Column<HolderStockPeriod>[] = [
    {
      key: 'period',
      label: '报告期',
      width: '88px',
      num: true,
      value: (row) => date(row.report_date)
    },
    {
      key: 'shares',
      label: '持股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_shares, '股')
    },
    {
      key: 'pct',
      label: '占比',
      align: 'right',
      num: true,
      value: (row) => percent(row.share_percent)
    },
    { key: 'nature', label: '股份性质', value: (row) => text(row.share_nature) },
    { key: 'label', label: '变化', value: (row) => text(row.change_label) },
    {
      key: 'delta',
      label: '增减股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.change_shares, '股'),
      tone: (row) => tone(num(row.change_shares))
    }
  ];

  const chainStats = $derived.by<Stat[]>(() => {
    if (!chain) return [];
    return [
      { label: '跨股票历史', value: count(chain.counts.records), note: '完整结果服务端缓存' },
      { label: '关联股票', value: count(chain.counts.unique_stocks), note: '市场 + 代码去重' },
      { label: '当前持有记录', value: count(chain.counts.currently_held_records), note: '上游 bh=1' },
      {
        label: '主链缓存',
        value: `${chain.cache.history_age_seconds}s`,
        note: chain.cache.history_refreshed ? '本次刷新' : `TTL ${chain.cache.history_ttl_seconds}s`
      }
    ];
  });

  const stockName = $derived(
    chain?.records.find((item) => item.code === stockCode)?.name ?? (stockCode === code ? name : '')
  );

  function loadHolder(target: InstitutionHolder | null, offset = 0, stock = '', refresh = false) {
    if (!target?.queryable) return;
    picked = target;
    stockCode = stock || target.reference_code || code;
    void holder.load(
      `/api/v1/market/holder?${queryString({
        holder_id: target.holder_id,
        variant_id: target.variant_id,
        holder_name: target.name,
        reference_code: target.reference_code || code,
        stock_code: stockCode,
        offset,
        limit: PAGE_SIZE,
        refresh: refresh ? 1 : 0
      })}`
    );
  }

  function gotoStock(row: HolderHistoryRecord) {
    app.setStock({ market: row.market, code: row.code, name: row.name });
    router.go(stockPath(row.market, row.code));
  }

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="十大流通股东"
  eyebrow="7709 · CGFXMX"
  subtitle={doc?.sources.length ? doc.sources.map((item) => item.resource).join(' · ') : ''}
  busy={profile.busy}
  error={profile.error}
  onRetry={() => load()}
  empty={profile.loaded && !profile.busy && topRows.length === 0}
  emptyText="当前证券暂无十大流通股东动态资源"
  flush
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={profile.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  <DataTable
    columns={topColumns}
    rows={topRows}
    rowKey={(row) => row.index}
    isActive={(row) => Boolean(row.ref?.holder_id) && row.ref?.holder_id === picked?.holder_id}
    maxHeight="320px"
  >
    {#snippet cell({ row })}
      <Button
        variant="ghost"
        disabled={!row.ref?.queryable}
        busy={holder.busy && picked?.holder_id === row.ref?.holder_id}
        onclick={() => loadHolder(row.ref)}
      >{row.ref?.queryable ? '反查' : '无入口'}</Button>
    {/snippet}
  </DataTable>

  {#each doc?.errors ?? [] as failure (failure.resource)}
    <p class="warn-line">{failure.resource}：{failure.message}</p>
  {/each}
</Panel>

<Panel
  title="机构持仓分类"
  eyebrow="INSTITUTION HISTORY"
  subtitle={`${count(institutionRows.length)} 条「报告期 × 机构类别」记录`}
  empty={profile.loaded && !profile.busy && institutionRows.length === 0}
  emptyText="当前证券暂无机构持仓历史"
  flush
  scroll
>
  <DataTable
    columns={institutionColumns}
    rows={institutionRows}
    rowKey={(_row, index) => index}
    maxHeight="320px"
  />
</Panel>

<Panel
  title={picked ? picked.name : '股东跨股票反查'}
  eyebrow="GDJC → GDJCXQ"
  subtitle={page
    ? `第 ${count(page.offset + 1)}—${count(page.offset + page.returned)} 条 / 共 ${count(page.total)}，每页 ${PAGE_SIZE}；点击股票行展开逐报告期`
    : '在上方十大流通股东表点「反查」，读取该股东在全市场的持仓'}
  busy={holder.busy}
  error={holder.error}
  onRetry={() => loadHolder(picked, page?.offset ?? 0, stockCode)}
  empty={!holder.busy && (chain?.records.length ?? 0) === 0}
  emptyText={picked ? '该股东没有可用的跨股票持仓记录' : '尚未选择股东'}
  flush
  scroll
>
  {#snippet actions()}
    <Button
      icon="chevron-left"
      disabled={!page?.has_previous || holder.busy}
      onclick={() => loadHolder(picked, Math.max(0, (page?.offset ?? 0) - PAGE_SIZE), stockCode)}
    >上一页</Button>
    <Button
      icon="chevron-right"
      disabled={!page?.has_next || holder.busy}
      onclick={() => loadHolder(picked, (page?.offset ?? 0) + PAGE_SIZE, stockCode)}
    >下一页</Button>
    <Button
      icon="refresh"
      disabled={!picked}
      busy={holder.busy}
      onclick={() => loadHolder(picked, page?.offset ?? 0, stockCode, true)}
    >强制更新</Button>
  {/snippet}

  {#if chain}
    <div class="pad"><StatGrid stats={chainStats} columns={4} /></div>
    <DataTable
      columns={crossColumns}
      rows={chain.records}
      rowKey={(row) => row.row_id}
      onRowClick={(row) => loadHolder(picked, page?.offset ?? 0, row.code)}
      isActive={(row) => row.code === stockCode}
      maxHeight="360px"
    >
      {#snippet cell({ row })}
        <Button
          variant="ghost"
          icon="external"
          title="在个股工作台打开"
          onclick={() => gotoStock(row)}
        />
      {/snippet}
    </DataTable>
  {/if}
</Panel>

<Panel
  title="选中股票的逐报告期持仓"
  eyebrow="REPORT PERIODS"
  subtitle={stockCode ? `${stockCode} ${stockName}` : '点击上表任意股票行展开'}
  empty={!holder.busy && (chain?.stock_detail.records.length ?? 0) === 0}
  emptyText={picked ? '当前股东在所选股票没有报告期明细' : '尚未选择股东'}
  flush
  scroll
>
  <DataTable
    columns={periodColumns}
    rows={chain?.stock_detail.records ?? []}
    rowKey={(_row, index) => index}
    maxHeight="300px"
  />
</Panel>

<style>
  .pad {
    padding: var(--sp-3) var(--sp-4);
    border-bottom: 1px solid var(--line);
  }

  .warn-line {
    margin: var(--sp-2) var(--sp-4);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }
</style>
