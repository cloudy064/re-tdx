<script lang="ts">
  import { queryString } from '../../../api';
  import { count, date, money, percent } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { CompanyChangeRecord, MarketCompanyChangesDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketCompanyChangesDocument>();
  const rows = $derived(resource.data?.records ?? []);
  function load(refresh = false) {
    void resource.load(`/api/v1/market/company-changes?${queryString({ view: 'all', market, code, limit: 500, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  function relation(row: CompanyChangeRecord) {
    if (row.kind === 'security-renames' || row.kind === 'company-renames') return `${row.old_name || '—'} → ${row.new_name || name}`;
    if (row.kind.endsWith('index')) return `${row.direction || '调整'} · ${row.index_name || '—'}`;
    if (row.kind === 'controllers') return `${row.before_controller || '—'} → ${row.after_controller || '—'}`;
    if (row.kind === 'industries') return `${row.before_industry || '—'} → ${row.after_industry || '—'}`;
    return `${row.seller || '—'} → ${row.buyer || '—'}`;
  }
  function detail(row: CompanyChangeRecord) {
    if (row.kind === 'major-equity') return `${count(row.shares)} 股 · ${percent(row.total_share_pct)}`;
    if (row.kind === 'equity-transfers') return `${money(row.total_amount_yuan)} · ${percent(row.total_share_pct)}`;
    return row.status || row.cross_industry || row.business_change || '';
  }
  const columns: Column<CompanyChangeRecord>[] = [
    { key: 'kind', label: '类型', width: '150px', value: (row) => row.kind_label, sub: (row) => row.source_resource },
    { key: 'date', label: '日期', width: '110px', num: true, value: (row) => date(row.event_date) },
    { key: 'relation', label: '变更关系 / 指数', width: '430px', value: relation },
    { key: 'detail', label: '规模 / 状态', width: '250px', value: detail }
  ];
  $effect(() => { void market; void code; load(); });
</script>

<Panel title={`${name} · 公司与证券变更`} eyebrow="ZQBG · 9 PUBLIC SOURCES" subtitle={`${count(rows.length)} 条更名、指数、股权、实控人或行业记录`} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="该证券在当前九类变更资源中没有记录。" flush scroll>
  <DataTable {columns} {rows} rowKey={(row) => row.event_id} minWidth="940px" />
</Panel>
