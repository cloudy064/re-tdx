<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { EmployeeCompanyRecord, EmployeeSharePlanRecord, ExecutiveRecord, MarketEmployeesDocument } from '../../types';

  const VIEWS = [
    { id: 'catalog', label: '员工与高管' },
    { id: 'share-plans', label: '员工持股计划' }
  ];

  const SORTS = [
    { id: 'executive-compensation', label: '高管薪酬' }, { id: 'employees', label: '员工人数' },
    { id: 'employee-change', label: '员工变动' }, { id: 'compensation', label: '职工薪酬' },
    { id: 'rd-expense', label: '研发费用' }, { id: 'profit-per-employee', label: '人均净利润' }
  ];
  let sort = $state('executive-compensation');
  let view = $state<'catalog' | 'share-plans'>('catalog');
  let query = $state('');
  let selected = $state<EmployeeCompanyRecord | null>(null);
  let selectedPlanId = $state('');
  const catalog = new Resource<MarketEmployeesDocument>();
  const detail = new Resource<MarketEmployeesDocument>();
  const doc = $derived(catalog.data);
  const rows = $derived(doc?.companies ?? []);
  const plans = $derived(doc?.share_plans ?? []);
  const selectedPlan = $derived(plans.find((row) => row.event_id === selectedPlanId) ?? plans[0]);
  const company = $derived(detail.data?.selected ?? selected);
  const executives = $derived(detail.data?.executives ?? []);

  const stats = $derived.by(() => !doc ? [] : view === 'catalog' ? [
      { label: '覆盖公司', value: count(doc.summary.companies) },
      { label: '员工总数', value: compact(doc.summary.employees, '人') },
      { label: '职工薪酬', value: compact(doc.summary.employee_compensation_yuan, '元') },
      { label: '研发费用', value: compact(doc.summary.rd_expense_yuan, '元') },
      { label: '员工增加公司', value: count(doc.summary.employee_growing_companies) },
      { label: '员工减少公司', value: count(doc.summary.employee_shrinking_companies) }
    ] : [
      { label: '持股计划', value: count(doc.share_plan_summary.plans) },
      { label: '覆盖公司', value: count(doc.share_plan_summary.unique_securities) },
      { label: '购买中', value: count(doc.share_plan_summary.active_plans) },
      { label: '购买完成', value: count(doc.share_plan_summary.completed_plans) },
      { label: '累计购买', value: compact(doc.share_plan_summary.purchase_shares, '股') },
      { label: '参考金额', value: compact(doc.share_plan_summary.purchase_amount_yuan, '元') }
    ]);

  function load(refresh = false) {
    void catalog.load(`/api/v1/market/employees?${queryString({ view, sort, q: query.trim(), limit: 5000, refresh: refresh ? 1 : 0 })}`);
    selected = null;
    selectedPlanId = '';
    detail.reset();
  }
  function select(row: EmployeeCompanyRecord) {
    selected = row;
    void detail.load(`/api/v1/market/employees?${queryString({ view: 'all', market: row.security.market, code: row.security.code })}`);
  }
  function openWorkbench() { if (company) router.go(stockPath(company.security.market, company.security.code, 'employees')); }
  function openPlanWorkbench() { if (selectedPlan) router.go(stockPath(selectedPlan.security.market, selectedPlan.security.code, 'employees')); }
  function switchView(next: string) { view = next as typeof view; load(); }

  const columns: Column<EmployeeCompanyRecord>[] = [
    { key: 'security', label: '公司', width: '132px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'employees', label: '员工', align: 'right', num: true, value: (row) => count(row.employees), sub: (row) => `${row.employee_change && row.employee_change > 0 ? '+' : ''}${count(row.employee_change)} · ${percent(row.employee_change_pct)}`, tone: (row) => tone(row.employee_change), sortValue: (row) => row.employees ?? 0 },
    { key: 'employee-comp', label: '职工薪酬', align: 'right', num: true, value: (row) => compact(row.employee_compensation_yuan, '元'), sortValue: (row) => row.employee_compensation_yuan ?? 0 },
    { key: 'per-capita', label: '人均薪酬', align: 'right', num: true, value: (row) => compact(row.compensation_per_employee_yuan, '元'), sub: (row) => `人均净利 ${compact(row.profit_per_employee_yuan, '元')}`, sortValue: (row) => row.compensation_per_employee_yuan ?? 0 },
    { key: 'executive', label: '高管薪酬', align: 'right', num: true, value: (row) => compact(row.executive_compensation_yuan, '元'), sub: (row) => `占职工薪酬 ${percent(row.executive_compensation_employee_pct)}`, sortValue: (row) => row.executive_compensation_yuan ?? 0 },
    { key: 'rd', label: '研发费用', align: 'right', num: true, value: (row) => compact(row.rd_expense_yuan, '元'), sub: (row) => `营收占比 ${percent(row.rd_revenue_pct)}`, sortValue: (row) => row.rd_expense_yuan ?? 0 },
    { key: 'education', label: '学历结构', align: 'right', num: true, value: (row) => `硕士+ ${count(row.masters_or_above)}`, sub: (row) => `本科+ ${count(row.bachelors_or_above)}` }
  ];
  const executiveColumns: Column<ExecutiveRecord>[] = [
    { key: 'name', label: '姓名', value: (row) => row.name, sub: (row) => row.gender_age_education },
    { key: 'position', label: '职务', wrap: true, value: (row) => row.position },
    { key: 'pay', label: '年薪', align: 'right', num: true, value: (row) => compact(row.annual_compensation_yuan, '元'), sub: (row) => date(row.cutoff_date), sortValue: (row) => row.annual_compensation_yuan ?? 0 }
  ];
  const planColumns: Column<EmployeeSharePlanRecord>[] = [
    { key: 'security', label: '公司', width: '132px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'status', label: '状态', value: (row) => row.status, sub: (row) => row.industry, sortValue: (row) => row.status },
    { key: 'date', label: '实施起始', num: true, value: (row) => date(row.dates.implementation_start), sortValue: (row) => row.sort_date },
    { key: 'shares', label: '购买股数', align: 'right', num: true, value: (row) => compact(row.purchase_shares, '股'), sortValue: (row) => row.purchase_shares ?? 0 },
    { key: 'price', label: '购买均价', align: 'right', num: true, value: (row) => fixed(row.purchase_average_price, 2), sortValue: (row) => row.purchase_average_price ?? 0 },
    { key: 'amount', label: '参考金额', align: 'right', num: true, value: (row) => compact(row.purchase_amount_yuan, '元'), sortValue: (row) => row.purchase_amount_yuan ?? 0 },
    { key: 'ratio', label: '占股本', align: 'right', num: true, value: (row) => percent(row.share_capital_pct), sortValue: (row) => row.share_capital_pct ?? 0 }
  ];
  onMount(() => load());
</script>

<PageHeader eyebrow="YGXC · GGXC · QXFA501" title="员工、高管与持股计划" description="全市场员工薪酬和研发画像、高管名单，以及员工持股计划的购买、存续与锁定期限。" {stats}>
  {#snippet actions()}
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="员工数据类型" />
    <Button icon="refresh" busy={catalog.busy} onclick={() => load(true)}>刷新主表</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="390px">
  {#snippet main()}
    <Panel title={view === 'catalog' ? '公司人员结构排行' : '员工持股计划'} subtitle={`${count(view === 'catalog' ? rows.length : plans.length)} 条`} busy={catalog.busy} error={catalog.error} onRetry={() => load()} empty={catalog.loaded && !catalog.busy && (view === 'catalog' ? rows.length === 0 : plans.length === 0)} flush scroll>
      {#snippet toolbar()}
        {#if view === 'catalog'}<Select options={SORTS} value={sort} width="170px" label="排序" onChange={(next) => { sort = next; load(); }} />{/if}
        <TextInput bind:value={query} icon="search" width="220px" label="检索" placeholder="名称 / 代码 / 行业 / 状态" onEnter={() => load()} />
      {/snippet}
      {#if view === 'catalog'}
        <DataTable {columns} {rows} rowKey={(row) => row.security.security_id} onRowClick={select} isActive={(row) => row.security.security_id === selected?.security.security_id} stickyFirst numbered />
      {:else}
        <DataTable columns={planColumns} rows={plans} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedPlanId = row.event_id)} isActive={(row) => row.event_id === selectedPlan?.event_id} stickyFirst numbered sortKey="date" />
      {/if}
    </Panel>
  {/snippet}
  {#snippet aside()}
    {#if view === 'catalog'}<div class="aside">
      <Panel title={company?.security.name ?? '公司人员画像'} eyebrow="COMPANY" subtitle={company?.security.security_id ?? '点击左侧公司'} busy={detail.busy} error={detail.error} empty={!company && !detail.busy} emptyText="选择公司查看员工与研发指标。">
        {#if company}
          <button class="jump" type="button" onclick={openWorkbench}>在个股工作台打开</button>
          <StatGrid inline stats={[
            { label: '员工人数', value: count(company.employees), note: `较上期 ${count(company.employee_change)} · ${percent(company.employee_change_pct)}`, tone: tone(company.employee_change) },
            { label: '职工薪酬', value: compact(company.employee_compensation_yuan, '元') },
            { label: '高管薪酬', value: compact(company.executive_compensation_yuan, '元'), note: `占职工 ${percent(company.executive_compensation_employee_pct)}` },
            { label: '人均薪酬', value: compact(company.compensation_per_employee_yuan, '元') },
            { label: '人均净利润', value: compact(company.profit_per_employee_yuan, '元'), tone: tone(company.profit_per_employee_yuan) },
            { label: '研发费用', value: compact(company.rd_expense_yuan, '元'), note: percent(company.rd_revenue_pct) }
          ]} />
        {/if}
      </Panel>
      <Panel title="高管名单与年薪" subtitle={`${count(executives.length)} 人`} busy={detail.busy} empty={Boolean(company) && !detail.busy && executives.length === 0} emptyText="动态明细没有返回高管记录。" flush scroll>
        <DataTable columns={executiveColumns} rows={executives} rowKey={(row, index) => `${row.name}:${row.position}:${index}`} sortKey="pay" />
      </Panel>
    </div>{:else}
      <Panel title={selectedPlan?.security.name ?? '员工持股计划'} eyebrow="QXFA501" subtitle={selectedPlan ? `${selectedPlan.security.security_id} · ${selectedPlan.status}` : '点击左侧计划'} busy={catalog.busy} empty={!selectedPlan && !catalog.busy} emptyText="当前筛选下没有员工持股计划">
        {#if selectedPlan}
          <button class="jump" type="button" onclick={openPlanWorkbench}>在个股工作台打开</button>
          <StatGrid inline stats={[
            { label: '购买股数', value: compact(selectedPlan.purchase_shares, '股') },
            { label: '购买均价', value: fixed(selectedPlan.purchase_average_price, 2) },
            { label: '参考金额', value: compact(selectedPlan.purchase_amount_yuan, '元') },
            { label: '占总股本', value: percent(selectedPlan.share_capital_pct) },
            { label: '实施期', value: `${date(selectedPlan.dates.implementation_start)} — ${date(selectedPlan.dates.implementation_end)}` },
            { label: '锁定期', value: `${date(selectedPlan.dates.lock_start)} — ${date(selectedPlan.dates.lock_end)}` }
          ]} />
          {#if selectedPlan.details}<p class="plan-copy">{selectedPlan.details}</p>{/if}
          <p class="source-note">来源：{selectedPlan.source_resource}</p>
        {/if}
      </Panel>
    {/if}
  {/snippet}
</Split>

<style>
  .aside { display: flex; min-height: 0; flex-direction: column; gap: var(--sp-2); }
  .jump { width: 100%; height: 24px; margin-bottom: var(--sp-2); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  .plan-copy { white-space: pre-wrap; line-height: 1.72; font-size: var(--fs-sm); }
  .source-note { color: var(--text-muted); font-size: var(--fs-xs); }
</style>
