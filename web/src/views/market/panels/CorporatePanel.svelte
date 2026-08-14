<script lang="ts">
  /** 7709 原生财务、股本变迁和特殊涨跌停覆盖层。 */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, fixed, price } from '../../../lib/fmt';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    CapitalChangeRecord,
    CapitalDocument,
    FinanceDocument,
    SpecialLimitsDocument
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const finance = new Resource<FinanceDocument>();
  const capital = new Resource<CapitalDocument>();
  const limits = new Resource<SpecialLimitsDocument>();
  const financeRow = $derived(finance.data?.records[0] ?? null);
  const events = $derived(capital.data?.blocks[0]?.records ?? []);
  const specialLimit = $derived(limits.data?.records[0] ?? null);

  function load() {
    const query = queryString({ market, code });
    void finance.load(`/api/v1/market/finance?${query}`);
    void capital.load(`/api/v1/market/capital?${query}`);
    void limits.load(`/api/v1/market/limits?${query}`);
  }

  const overview = $derived.by<Stat[]>(() => {
    if (!financeRow) return [];
    return [
      { label: '财务更新日', value: date(financeRow.updated_date) },
      { label: '上市日', value: date(financeRow.listing_date) },
      { label: '流通股本', value: compact(financeRow.shares.circulating, '股') },
      { label: '总股本', value: compact(financeRow.shares.total, '股') },
      { label: '每股收益', value: fixed(financeRow.per_share.eps, 3) },
      { label: '每股净资产', value: fixed(financeRow.per_share.net_assets, 3) },
      { label: '股东人数', value: count(financeRow.shareholder_count) },
      { label: '总资产', value: compact(financeRow.balance_sheet.total_assets_yuan, '元') },
      { label: '净资产', value: compact(financeRow.balance_sheet.net_assets_yuan, '元') },
      { label: '营业收入', value: compact(financeRow.income_statement.revenue_yuan, '元') },
      { label: '营业利润', value: compact(financeRow.income_statement.operating_profit_yuan, '元') },
      { label: '净利润', value: compact(financeRow.income_statement.net_profit_yuan, '元') },
      { label: '经营现金流', value: compact(financeRow.cash_flow.operating_yuan, '元') },
      { label: '总负债（流动）', value: compact(financeRow.balance_sheet.current_liabilities_yuan, '元') }
    ];
  });

  function eventDetail(row: CapitalChangeRecord): string {
    const d = row.details;
    if (row.category === 1) {
      return `每10股派 ${fixed(d.dividend_per_10_shares_yuan, 4)} · 送转 ${fixed(d.bonus_transfer_per_10_shares, 4)} · 配 ${fixed(d.rights_per_10_shares, 4)} @ ${price(d.rights_price_yuan)}`;
    }
    if ('after_circulating_shares' in d) {
      return `流通 ${compact(d.before_circulating_shares, '股')} → ${compact(d.after_circulating_shares, '股')} · 总股本 ${compact(d.before_total_shares, '股')} → ${compact(d.after_total_shares, '股')}`;
    }
    if ('shrink_ratio' in d) return `缩股比例 ${fixed(d.shrink_ratio, 4)}`;
    return Object.entries(d)
      .map(([key, value]) => `${key}=${fixed(value, 4)}`)
      .join(' · ');
  }

  const eventColumns: Column<CapitalChangeRecord>[] = [
    { key: 'date', label: '日期', width: '92px', value: (row) => date(row.date), sortValue: (row) => row.date_raw },
    { key: 'category', label: '事件', width: '110px', value: (row) => row.category_name },
    { key: 'detail', label: '事件数据', value: eventDetail, wrap: true }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="财务基础信息"
  eyebrow="0x0010 · TCP 7709"
  subtitle={finance.data ? `${finance.data.server_name} · 固定 143 字节记录` : ''}
  busy={finance.busy}
  error={finance.error}
  onRetry={load}
  empty={finance.loaded && !finance.busy && !financeRow}
  emptyText="该证券没有返回财务基础记录"
  scroll
>
  <StatGrid stats={overview} columns={4} />
</Panel>

<Panel
  title="股本变迁与除权事件"
  eyebrow="0x000F · GBBQ"
  subtitle={capital.data ? `${count(capital.data.event_count)} 条 · 可用于历史股本与复权因子` : ''}
  busy={capital.busy}
  error={capital.error}
  onRetry={load}
  empty={capital.loaded && !capital.busy && events.length === 0}
  emptyText="该证券没有股本变迁记录"
  scroll
>
  <DataTable columns={eventColumns} rows={events} rowKey={(row) => `${row.date_raw}-${row.category}`} sortKey="date" />
</Panel>

<Panel
  title="特殊涨跌停覆盖"
  eyebrow="0x0452 · 全表本地索引"
  subtitle={limits.data?.cache_source ? `缓存 ${limits.data.cache_source}` : ''}
  busy={limits.busy}
  error={limits.error}
  onRetry={load}
  empty={limits.loaded && !limits.busy && !specialLimit}
  emptyText="该证券不在特殊限制表中，涨跌停价按普通交易规则计算"
>
  {#if specialLimit}
    <StatGrid
      columns={3}
      stats={[
        { label: '涨停价', value: price(specialLimit.limit_up_price) },
        { label: '跌停价', value: price(specialLimit.limit_down_price) },
        { label: '表内位置', value: count(specialLimit.index) }
      ]}
    />
  {/if}
</Panel>
