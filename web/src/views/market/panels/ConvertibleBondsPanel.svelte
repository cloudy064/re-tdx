<script lang="ts">
  import { queryString } from '../../../api';
  import { count, date, fixed, percent, text } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { ConvertibleBondDetailEvent, ConvertibleBondPricingRecord, ConvertibleBondRecord, ConvertibleBondSubscriptionRecord, MarketConvertibleBondsDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketConvertibleBondsDocument>();
  const pricingResource = new Resource<MarketConvertibleBondsDocument>();
  const subscriptionResource = new Resource<MarketConvertibleBondsDocument>();
  const bonds = $derived(resource.data?.bonds ?? []);
  const pricing = $derived(pricingResource.data?.pricing ?? []);
  const subscriptions = $derived(subscriptionResource.data?.subscriptions ?? []);
  const histories = $derived<ConvertibleBondDetailEvent[]>(resource.data?.details.flatMap((item) => [...item.sellback, ...item.redemption, ...item.revision]) ?? []);

  function load(refresh = false) {
    void resource.load(`/api/v1/market/convertible-bonds?${queryString({ market, code, include_details: 1, refresh: refresh ? 1 : 0 })}`);
    void pricingResource.load(`/api/v1/market/convertible-bonds?${queryString({ view: 'pricing', market, code, include_quotes: 1, refresh: refresh ? 1 : 0 })}`);
    void subscriptionResource.load(`/api/v1/market/convertible-bonds?${queryString({ view: 'subscriptions', market, code, refresh: refresh ? 1 : 0 })}`);
  }

  function pricingStats(row: ConvertibleBondPricingRecord): Stat[] {
    return [
      { label: '债券现价', value: fixed(row.quote.bond_last_price, 3), note: row.quote.bond_price_source === 'pre-close' ? '盘前昨收' : percent(row.quote.bond_change_pct) },
      { label: '正股现价', value: fixed(row.quote.underlying_last_price, 2), note: row.quote.underlying_price_source === 'pre-close' ? '盘前昨收' : percent(row.quote.underlying_change_pct) },
      { label: '转股价值', value: fixed(row.valuation.conversion_value, 3), note: `转股价 ${fixed(row.terms.conversion_price, 3)}` },
      { label: '转股溢价', value: percent(row.valuation.conversion_premium_pct), note: `全价 ${fixed(row.valuation.full_price, 3)}` },
      { label: '到期收益率', value: percent(row.valuation.maturity_yield_pct), note: `${count(row.valuation.cash_flow_count)} 笔剩余现金流` },
      { label: '纯债价值', value: fixed(row.valuation.pure_bond_value, 3), note: `溢价 ${percent(row.valuation.pure_bond_premium_pct)}` },
      { label: '双低值', value: fixed(row.valuation.double_low_score, 2), note: '净价 + 转股溢价率' },
      { label: '应计利息', value: fixed(row.valuation.accrued_interest, 4), note: '票息日程 · Actual/365' }
    ];
  }

  function stats(row: ConvertibleBondRecord): Stat[] {
    return [
      { label: '转债', value: row.bond.name, note: row.bond.security_id },
      { label: '类型', value: row.instrument_type === 'exchangeable-bond' ? '可交换债' : '可转债', note: row.exchangeable_supplemented ? '独立收益表已补齐条款' : '' },
      { label: '转股价', value: fixed(row.overview.conversion_price, 3), note: `${date(row.overview.conversion_start_date)} 起` },
      { label: '剩余余额', value: `${fixed(row.overview.remaining_balance_100m_yuan, 2)} 亿`, note: percent(row.overview.remaining_ratio_pct) },
      { label: '到期日', value: date(row.overview.maturity_date), note: `${fixed(row.overview.remaining_years, 2)} 年` },
      { label: '回售 / 赎回', value: `${text(row.sellback.status)} / ${text(row.redemption.status)}`, note: `${count(row.sellback.history_count)} / ${count(row.redemption.history_count)} 次` },
      { label: '调价历史', value: count(row.revision.history_count), note: text(row.revision.status) }
    ];
  }

  const columns: Column<ConvertibleBondDetailEvent>[] = [
    { key: 'date', label: '日期', num: true, value: (row) => date(row.date ?? row.start_date) },
    { key: 'kind', label: '类型', value: (row) => row.kind === 'sellback' ? '回售' : row.kind === 'redemption' ? '赎回' : '转股价调整' },
    { key: 'price', label: '价格', align: 'right', num: true, value: (row) => fixed(row.conversion_price ?? row.price, 3) },
    { key: 'reason', label: '原因', wrap: true, value: (row) => text(row.reason) }
  ];

  const subscriptionColumns: Column<ConvertibleBondSubscriptionRecord>[] = [
    { key: 'bond', label: '转债', value: (row) => row.bond.name || row.bond.code, sub: (row) => row.bond.security_id },
    { key: 'date', label: '申购日', num: true, value: (row) => date(row.subscription_date), sub: (row) => row.subscription_code },
    { key: 'size', label: '发行规模', align: 'right', num: true, value: (row) => `${fixed(row.issue_size_100m_yuan, 2)} 亿` },
    { key: 'lottery', label: '中签率', align: 'right', num: true, value: (row) => percent(row.lottery_rate_pct, 6), sub: (row) => date(row.lottery_date) },
    { key: 'value', label: '转股价值', align: 'right', num: true, value: (row) => fixed(row.conversion_value_yuan, 3), sub: (row) => `转股价 ${fixed(row.conversion_price_yuan, 3)}` },
    { key: 'premium', label: '转股溢价', align: 'right', num: true, value: (row) => percent(row.conversion_premium_pct), sub: (row) => row.listed ? date(row.listing_date) : '尚未上市' }
  ];

  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 关联可转债`} eyebrow="KZZ · BOND ↔ STOCK" subtitle={`${count(bonds.length)} 只关联转债`} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && bonds.length === 0} emptyText="该证券既不是当前可转债，也没有关联的可转债。">
  {#each bonds as bond (bond.bond.security_id)}<StatGrid stats={stats(bond)} columns={3} />{/each}
</Panel>

<Panel title="实时定价与收益" eyebrow="PUBLIC L1 + DISCLOSED CASH FLOWS" subtitle={`${count(pricing.length)} 只 · 本地可审计复算`} busy={pricingResource.busy} error={pricingResource.error} onRetry={() => load()} empty={pricingResource.loaded && !pricingResource.busy && pricing.length === 0} emptyText="该证券没有可关联的存续转债定价记录。">
  {#each pricing as row (row.bond.security_id)}<StatGrid stats={pricingStats(row)} columns={4} />{/each}
</Panel>

<Panel title="发行申购、中签与上市" eyebrow="KKZSS101 · CLIENT FORMULAS" subtitle={`${count(subscriptions.length)} 条 · 转股价值与溢价率可复算`} busy={subscriptionResource.busy} error={subscriptionResource.error} onRetry={() => load()} empty={subscriptionResource.loaded && !subscriptionResource.busy && subscriptions.length === 0} emptyText="该证券没有命中已发可转债申购表。" flush scroll>
  <DataTable columns={subscriptionColumns} rows={subscriptions} rowKey={(row) => row.event_id} />
</Panel>

<Panel title="回售、赎回与转股价历史" subtitle={`${count(histories.length)} 条`} busy={resource.busy} empty={!resource.busy && bonds.length > 0 && histories.length === 0} emptyText="关联转债没有返回动态历史。" flush scroll>
  <DataTable {columns} rows={histories} rowKey={(row, index) => `${row.kind}:${row.date ?? row.start_date}:${index}`} />
</Panel>
