<script lang="ts">
  import { queryString } from '../../../api';
  import { compact, count, date, delta, money, percent } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { MarketShareholderSignalsDocument, ShareholderSignalRecord } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';
  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketShareholderSignalsDocument>();
  const rows = $derived(resource.data?.records ?? []);
  function load(refresh = false) {
    void resource.load(`/api/v1/market/shareholder-signals?${queryString({ market, code, limit: 10, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  function signal(row: ShareholderSignalRecord) {
    if (row.kind === 'notable-investors') return `${count(row.notable_investor_count)} 位牛散`;
    if (row.kind === 'institution-accumulation') return `机构 ${delta(row.institution_holding_growth_pct)}`;
    if (row.kind === 'small-cap-institution') return `小市值机构 ${delta(row.institution_holding_growth_pct)}`;
    return `${count(row.research_count_6m)} 次 / ${count(row.institutions_6m)} 家`;
  }
  function signalSub(row: ShareholderSignalRecord) {
    if (row.kind === 'notable-investors') return percent(row.holding_pct);
    if (row.kind === 'institution-accumulation') return `股东 ${delta(row.shareholder_count_change_pct)}`;
    if (row.kind === 'small-cap-institution') return `流通占比 ${percent(row.institution_float_holding_pct)}`;
    return `利润 ${delta(row.net_profit_growth_pct)}`;
  }
  function scale(row: ShareholderSignalRecord) {
    if (row.kind === 'notable-investors') return money(row.holding_value_yuan);
    if (row.kind === 'institution-accumulation') return compact(row.institution_holding_change_shares, '股');
    if (row.kind === 'small-cap-institution') return compact(row.institution_holding_change_shares, '股');
    return money(row.net_profit_yuan);
  }
  function holdingPct(row: ShareholderSignalRecord) {
    return percent(row.kind === 'small-cap-institution'
      ? row.institution_float_holding_pct : row.top10_float_holding_pct);
  }
  const columns: Column<ShareholderSignalRecord>[] = [
    { key: 'kind', label: '类型 / 报告期', width: '190px', value: (row) => row.kind_label, sub: (row) => date(row.report_period) },
    { key: 'signal', label: '核心信号', align: 'right', num: true, value: signal, sub: signalSub },
    { key: 'scale', label: '持股 / 利润规模', align: 'right', num: true, value: scale },
    { key: 'top10', label: '流通持股占比', align: 'right', num: true, value: holdingPct },
    { key: 'return', label: '半年表现', align: 'right', num: true, value: (row) => delta(row.return_6m_pct) },
    { key: 'research', label: '最近调研', align: 'right', num: true, value: (row) => date(row.latest_research_date) }
  ];
  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 牛散机构调研信号`} eyebrow="CWNSCG · JGXC · XSZGP · JGZD" subtitle="牛散持股、常规与小市值机构增持、股东收缩、成长与调研活跃" busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="该证券未命中当前股东与机构信号。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.record_id} minWidth="950px" />
</Panel>
