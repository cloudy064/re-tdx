<script lang="ts">
  import { queryString } from '../../../api';
  import { compact, count, date, fixed, percent, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { EmployeeSharePlanRecord, ExecutiveRecord, MarketEmployeesDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';
  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<MarketEmployeesDocument>();
  const company = $derived(resource.data?.selected ?? null);
  const executives = $derived(resource.data?.executives ?? []);
  const sharePlans = $derived(resource.data?.share_plans ?? []);
  function load(refresh = false) { void resource.load(`/api/v1/market/employees?${queryString({ view: 'all', market, code, refresh: refresh ? 1 : 0 })}`); }
  const stats = $derived.by<Stat[]>(() => company ? [
    { label: '员工人数', value: count(company.employees), note: `${count(company.employee_change)} · ${percent(company.employee_change_pct)}`, tone: tone(company.employee_change) },
    { label: '职工薪酬', value: compact(company.employee_compensation_yuan, '元') },
    { label: '人均薪酬', value: compact(company.compensation_per_employee_yuan, '元') },
    { label: '人均净利润', value: compact(company.profit_per_employee_yuan, '元'), tone: tone(company.profit_per_employee_yuan) },
    { label: '研发费用', value: compact(company.rd_expense_yuan, '元'), note: percent(company.rd_revenue_pct) },
    { label: '高管薪酬', value: compact(company.executive_compensation_yuan, '元'), note: percent(company.executive_compensation_employee_pct) }
  ] : []);
  const columns: Column<ExecutiveRecord>[] = [
    { key: 'name', label: '姓名', value: (row) => row.name, sub: (row) => row.gender_age_education },
    { key: 'position', label: '职务', wrap: true, value: (row) => row.position },
    { key: 'pay', label: '年薪', align: 'right', num: true, value: (row) => compact(row.annual_compensation_yuan, '元'), sub: (row) => date(row.cutoff_date), sortValue: (row) => row.annual_compensation_yuan ?? 0 }
  ];
  const planColumns: Column<EmployeeSharePlanRecord>[] = [
    { key: 'status', label: '状态', value: (row) => row.status, sub: (row) => row.industry },
    { key: 'start', label: '实施期', num: true, value: (row) => date(row.dates.implementation_start), sub: (row) => `至 ${date(row.dates.implementation_end)}`, sortValue: (row) => row.sort_date },
    { key: 'shares', label: '购买股数', align: 'right', num: true, value: (row) => compact(row.purchase_shares, '股'), sub: (row) => `${percent(row.share_capital_pct)} 股本`, sortValue: (row) => row.purchase_shares ?? 0 },
    { key: 'price', label: '均价 / 金额', align: 'right', num: true, value: (row) => fixed(row.purchase_average_price, 2), sub: (row) => compact(row.purchase_amount_yuan, '元'), sortValue: (row) => row.purchase_amount_yuan ?? 0 }
  ];
  $effect(() => { void market; void code; load(); });
</script>
<Panel title={`${name} · 员工与研发`} eyebrow="YGXC" busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && !company} emptyText="该证券不在员工薪酬主表中。"><StatGrid {stats} columns={3} /></Panel>
<Panel title="高管名单与年薪" eyebrow="GGXC" subtitle={`${count(executives.length)} 人`} busy={resource.busy} empty={!resource.busy && Boolean(company) && executives.length === 0} emptyText="动态明细没有返回高管记录。" flush scroll><DataTable {columns} rows={executives} rowKey={(row, index) => `${row.name}:${row.position}:${index}`} sortKey="pay" /></Panel>
<Panel title="员工持股计划" eyebrow="QXFA501" subtitle={`${count(sharePlans.length)} 期`} busy={resource.busy} empty={!resource.busy && sharePlans.length === 0} emptyText="该证券没有公开员工持股计划。" flush scroll><DataTable columns={planColumns} rows={sharePlans} rowKey={(row) => row.event_id} sortKey="start" /></Panel>
