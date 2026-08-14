<script lang="ts">
  /** 官方 tdxfin / tdxgp 专业数据的单票入口；只读取现有 HTTP 能力。 */
  import { untrack } from 'svelte';
  import { getJson, queryString } from '../../../api';
  import { count, date } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import Segmented from '../../../ui/Segmented.svelte';
  import TextInput from '../../../ui/TextInput.svelte';
  import type {
    ProfessionalFinancePeriod,
    ProfessionalSecurityDocument,
    ProfessionalTradingField,
    ProfessionalTradingHistoryPoint
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  type Mode = 'finance-series' | 'stock';
  interface FinanceDisplayRow extends ProfessionalFinancePeriod {
    values: Record<number, number | null>;
  }

  const FINANCE_DEFAULT = '183,184,230,232,271,308';
  const STOCK_DEFAULT = '1,3,6,15,16,21,27,28,44';
  const FINANCE_LABELS: Record<number, string> = {
    183: '营业收入同比',
    184: '净利润同比',
    230: '营业收入',
    232: '归母净利润'
  };
  const MODES = [
    { id: 'finance-series', label: '季度财务' },
    { id: 'stock', label: '个股交易序列' }
  ];

  const { market, code, name }: PanelProps = $props();
  const resource = new Resource<ProfessionalSecurityDocument>();
  let mode = $state<Mode>('finance-series');
  let fieldInput = $state(FINANCE_DEFAULT);
  let activeFieldIds = $state<number[]>([183, 184, 230, 232, 271, 308]);

  const finance = $derived(
    resource.data?.schema === 'tdx-professional-finance-series-v1'
      ? resource.data : null
  );
  const trading = $derived(
    resource.data?.schema === 'tdx-professional-trading-v1'
      ? resource.data : null
  );
  const financeRows = $derived.by((): FinanceDisplayRow[] =>
    (finance?.periods ?? []).map((period) => ({
      ...period,
      values: Object.fromEntries(period.fields.map((field) => [field.id, field.value]))
    }))
  );
  const visibleCount = $derived(
    finance ? financeRows.length : trading ? trading.fields.length : 0
  );
  const emptyText = $derived(
    finance?.package_errors.length
      ? `最近官方季度包未返回该证券；另有 ${count(finance.package_errors.length)} 个包读取诊断。`
      : '当前官方专业数据中没有该证券的匹配记录。'
  );

  function parseFields(): number[] {
    const maximum = mode === 'stock' ? 44 : 584;
    const minimum = mode === 'stock' ? 1 : 0;
    const values = fieldInput.split(',')
      .map((value) => value.trim())
      .filter(Boolean)
      .map((value) => Number(value));
    if (!values.length || values.some((value) =>
      !Number.isInteger(value) || value < minimum || value > maximum))
      throw new Error(`字段 ID 必须是 ${minimum}..${maximum} 的逗号分隔整数`);
    return [...new Set(values)];
  }

  function load(refresh = false) {
    void resource.loadWith(async () => {
      const fields = parseFields();
      activeFieldIds = fields;
      return getJson<ProfessionalSecurityDocument>(
        `/api/v1/market/professional?${queryString({
          kind: mode,
          market,
          code,
          fields: fields.join(','),
          limit: mode === 'finance-series' ? 8 : 500,
          history: mode === 'stock' ? 1 : '',
          refresh: refresh ? 1 : 0
        })}`
      );
    });
  }

  function switchMode(next: string) {
    mode = next as Mode;
    fieldInput = mode === 'finance-series' ? FINANCE_DEFAULT : STOCK_DEFAULT;
    resource.reset();
    load();
  }

  function financeLabel(id: number): string {
    return FINANCE_LABELS[id] ? `FN${id} · ${FINANCE_LABELS[id]}` : `FN${id}`;
  }

  const financeColumns = $derived.by((): Column<FinanceDisplayRow>[] => [
    {
      key: 'period', label: '报告期', width: '110px', num: true,
      value: (row) => date(row.report_date), sortValue: (row) => row.report_date_raw
    },
    ...activeFieldIds.map((id): Column<FinanceDisplayRow> => ({
      key: `fn-${id}`, label: financeLabel(id), align: 'right', num: true,
      value: (row) => count(row.values[id]),
      sortValue: (row) => row.values[id] ?? Number.NEGATIVE_INFINITY
    }))
  ]);

  const tradingColumns: Column<ProfessionalTradingField>[] = [
    {
      key: 'field', label: '字段', width: '190px',
      value: (row) => row.name ?? `GPJYVALUE(${row.id})`, sub: (row) => `ID ${row.id}`,
      sortValue: (row) => row.id
    },
    { key: 'count', label: '记录数', width: '80px', align: 'right', num: true, value: (row) => count(row.count), sortValue: (row) => row.count },
    { key: 'value1', label: '最新值 1', align: 'right', num: true, value: (row) => count(row.latest_value1), sortValue: (row) => row.latest_value1 ?? Number.NEGATIVE_INFINITY },
    { key: 'value2', label: '最新值 2', align: 'right', num: true, value: (row) => count(row.latest_value2), sortValue: (row) => row.latest_value2 ?? Number.NEGATIVE_INFINITY },
    { key: 'range', label: '首期 / 末期', width: '180px', num: true, value: (row) => date(row.last_date), sub: (row) => date(row.first_date) }
  ];

  const historyColumns: Column<ProfessionalTradingHistoryPoint>[] = [
    { key: 'date', label: '日期', width: '110px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date_raw },
    { key: 'field', label: '字段 ID', width: '90px', num: true, value: (row) => String(row.id), sortValue: (row) => row.id },
    { key: 'value1', label: '值 1', align: 'right', num: true, value: (row) => count(row.value1), sortValue: (row) => row.value1 ?? Number.NEGATIVE_INFINITY },
    { key: 'value2', label: '值 2', align: 'right', num: true, value: (row) => count(row.value2), sortValue: (row) => row.value2 ?? Number.NEGATIVE_INFINITY }
  ];

  $effect(() => {
    void market; void code;
    untrack(() => load());
  });
</script>

<div class="stack">
  <Panel
    title={`${name} · 官方专业序列`}
    eyebrow="TDXFIN / TDXGP · READ ONLY"
    subtitle={mode === 'finance-series'
      ? '最近八个实际包含该证券的报告期；报告期不是公告日，不能直接当作无前视回测时间。'
      : '官方个股专业交易字段及历史值；字段 ID 与 GPJYVALUE 保持一致。'}
    busy={resource.busy}
    error={resource.error}
    onRetry={() => load()}
    empty={resource.loaded && !resource.busy && visibleCount === 0}
    {emptyText}
  >
    {#snippet toolbar()}
      <Segmented options={MODES} value={mode} onChange={switchMode} ariaLabel="专业数据类型" />
      <TextInput bind:value={fieldInput} width="270px" label="字段 ID" placeholder="逗号分隔字段 ID" onEnter={() => load()} />
      <Button icon="search" busy={resource.busy} onclick={() => load()}>读取</Button>
      <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新上游</Button>
    {/snippet}
  </Panel>

  {#if financeRows.length}
    <Panel
      title="季度财务字段"
      subtitle={`${count(finance?.period_count)} 期 · 尝试 ${count(finance?.attempted_packages)} 个官方包 · ${count(finance?.package_errors.length)} 个包诊断`}
      flush scroll fill
    >
      <DataTable columns={financeColumns} rows={financeRows} rowKey={(row) => row.report_date_raw} sortKey="period" sortDesc={false} stickyFirst minWidth={`${Math.max(760, 130 + activeFieldIds.length * 150)}px`} />
    </Panel>
  {:else if trading}
    <Panel title="专业字段概览" subtitle={`${count(trading.observed_id_count)} 个原始字段 · ${count(trading.record_count)} 条记录`} flush scroll>
      <DataTable columns={tradingColumns} rows={trading.fields} rowKey={(row) => row.id} sortKey="field" sortDesc={false} />
    </Panel>
    {#if trading.history.length}
      <Panel title="字段历史" subtitle={trading.history_truncated ? '仅显示最后 500 条' : `${count(trading.history.length)} 条`} flush scroll fill>
        <DataTable columns={historyColumns} rows={trading.history} rowKey={(row, index) => `${row.date_raw}-${row.id}-${index}`} sortKey="date" />
      </Panel>
    {/if}
  {/if}
</div>

<style>
  .stack {
    display: flex;
    min-height: 0;
    flex: 1;
    flex-direction: column;
    gap: var(--sp-2);
    overflow: auto;
  }
</style>
