<script lang="ts">
  /** 本地港股公司行动与财务缓存，只读且不暴露宿主路径。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    HkActionKind,
    HkActionRecord,
    HkFinanceRecord,
    MarketHkActionsDocument,
    MarketHkFinanceDocument
  } from '../../types';

  type Section = 'actions' | 'finance';

  const SECTIONS = [
    { id: 'actions', label: '历史公司行动' },
    { id: 'finance', label: '本地财务快照' }
  ];
  const ACTION_KINDS = [
    { id: 'all', label: '全部行动' },
    { id: 'dividend', label: '派息 / 红利' },
    { id: 'bonus', label: '送股 / 红股' },
    { id: 'rights', label: '供股 / 配股' },
    { id: 'split', label: '拆股' },
    { id: 'consolidation', label: '合股' },
    { id: 'mixed', label: '复合行动' },
    { id: 'adjustment', label: '其他价格调整' },
    { id: 'other', label: '其他' }
  ];
  const FINANCE_SORTS = [
    { id: 'code', label: '证券代码' },
    { id: 'report_date', label: '报告日期' },
    { id: 'listing_date', label: '上市日期' },
    { id: 'classification', label: '分类码' }
  ];
  const ORDERS = [
    { id: 'desc', label: '降序' },
    { id: 'asc', label: '升序' }
  ];
  const PAGE_LIMITS = [50, 100, 200, 500].map((value) => ({
    id: String(value), label: `${value} 条 / 页`
  }));

  let section = $state<Section>('actions');

  let actionCode = $state('');
  let actionQuery = $state('');
  let actionKind = $state<HkActionKind>('all');
  let actionFrom = $state('');
  let actionTo = $state('');
  let actionOrder = $state('desc');
  let actionLimit = $state('100');
  let actionOffset = $state(0);

  let financeCode = $state('');
  let financeQuery = $state('');
  let financeClassification = $state('');
  let financeFrom = $state('');
  let financeTo = $state('');
  let financeSort = $state('report_date');
  let financeOrder = $state('desc');
  let financeLimit = $state('100');
  let financeOffset = $state(0);

  const actions = new Resource<MarketHkActionsDocument>();
  const finance = new Resource<MarketHkFinanceDocument>();

  const stats = $derived.by<Stat[]>(() => {
    if (section === 'actions') {
      const doc = actions.data;
      return [
        { label: '匹配行动', value: count(doc?.match_count) },
        { label: '覆盖证券', value: count(doc?.summary.securities) },
        { label: '派息标记', value: count(doc?.summary.by_flag.dividend) },
        { label: '日期范围', value: `${date(doc?.summary.earliest_date)} — ${date(doc?.summary.latest_date)}` }
      ];
    }
    const doc = finance.data;
    return [
      { label: '匹配快照', value: count(doc?.match_count) },
      { label: '源记录', value: count(doc?.summary.source_records) },
      { label: '分类码', value: count(Object.keys(doc?.summary.by_classification ?? {}).length) },
      { label: '报告范围', value: `${date(doc?.summary.earliest_report_date)} — ${date(doc?.summary.latest_report_date)}` }
    ];
  });

  function actionKindLabel(kind: HkActionRecord['kind']): string {
    return ACTION_KINDS.find((option) => option.id === kind)?.label ?? kind;
  }

  function tenThousand(value: number | null, unit = ''): string {
    return value == null ? '—' : compact(value * 10000, unit);
  }

  function loadActions(reset = true) {
    if (reset) actionOffset = 0;
    return actions.load(`/api/v1/market/hk-actions?${queryString({
      code: actionCode.trim(),
      q: actionQuery.trim(),
      kind: actionKind,
      from: actionFrom.trim(),
      to: actionTo.trim(),
      order: actionOrder,
      offset: actionOffset,
      limit: Number(actionLimit)
    })}`);
  }

  function loadFinance(reset = true) {
    if (reset) financeOffset = 0;
    return finance.load(`/api/v1/market/hk-finance?${queryString({
      code: financeCode.trim(),
      q: financeQuery.trim(),
      classification: financeClassification.trim(),
      from: financeFrom.trim(),
      to: financeTo.trim(),
      sort: financeSort,
      order: financeOrder,
      offset: financeOffset,
      limit: Number(financeLimit)
    })}`);
  }

  function switchSection(value: string) {
    section = value as Section;
    if (section === 'actions' && !actions.loaded) void loadActions();
    if (section === 'finance' && !finance.loaded) void loadFinance();
  }

  function previousActions() {
    actionOffset = Math.max(0, actionOffset - Number(actionLimit));
    void loadActions(false);
  }

  function nextActions() {
    if (!actions.data || actionOffset + actions.data.returned >= actions.data.match_count) return;
    actionOffset += Number(actionLimit);
    void loadActions(false);
  }

  function previousFinance() {
    financeOffset = Math.max(0, financeOffset - Number(financeLimit));
    void loadFinance(false);
  }

  function nextFinance() {
    if (!finance.data || financeOffset + finance.data.returned >= finance.data.match_count) return;
    financeOffset += Number(financeLimit);
    void loadFinance(false);
  }

  const actionColumns: Column<HkActionRecord>[] = [
    { key: 'security', label: '证券', width: '112px', value: (row) => row.security.security_id },
    { key: 'date', label: '行动日', width: '100px', num: true, value: (row) => date(row.date) },
    { key: 'kind', label: '主要类型', width: '116px', value: (row) => actionKindLabel(row.kind), sub: (row) => row.flags.join(' · ') || '原生调整' },
    { key: 'description', label: '原始说明', width: '340px', wrap: true, value: (row) => text(row.description) },
    { key: 'share', label: '股份倍率', width: '100px', align: 'right', num: true, value: (row) => fixed(row.factors.event_share_multiplier, 6) },
    { key: 'price', label: '价格加项', width: '100px', align: 'right', num: true, value: (row) => fixed(row.factors.event_additive_adjustment, 6) },
    { key: 'basis', label: '基准变化', width: '110px', value: (row) => {
      const changed = [];
      if (row.factors.changes_share_basis) changed.push('股份');
      if (row.factors.changes_price_basis) changed.push('价格');
      return changed.join(' + ') || '无';
    }}
  ];

  const financeColumns: Column<HkFinanceRecord>[] = [
    { key: 'security', label: '证券', width: '104px', value: (row) => `HK${row.code}` },
    { key: 'report', label: '报告日', width: '100px', num: true, value: (row) => date(row.report_date), sub: (row) => `上市 ${date(row.listing_date)}` },
    { key: 'classification', label: '分类码', width: '96px', value: (row) => text(row.classification_code) },
    { key: 'revenue', label: '营业收入', width: '120px', align: 'right', num: true, value: (row) => tenThousand(row.finance.income_statement.revenue_10k), sub: () => '报告币种' },
    { key: 'profit', label: '净利润', width: '120px', align: 'right', num: true, value: (row) => tenThousand(row.finance.income_statement.net_profit_10k), sub: () => '报告币种' },
    { key: 'assets', label: '总资产 / 净资产', width: '150px', align: 'right', num: true, value: (row) => tenThousand(row.finance.balance_sheet.total_assets_10k), sub: (row) => tenThousand(row.finance.balance_sheet.net_assets_10k) },
    { key: 'shares', label: '总股本 / H股', width: '142px', align: 'right', num: true, value: (row) => tenThousand(row.finance.shares.total_10k, '股'), sub: (row) => tenThousand(row.finance.shares.h_10k, '股') },
    { key: 'per-share', label: '每股收益 / 股息', width: '132px', align: 'right', num: true, value: (row) => fixed(row.finance.per_share.earnings, 4), sub: (row) => fixed(row.finance.per_share.dividend, 4) },
    { key: 'pe', label: 'PE TTM / 静态', width: '118px', align: 'right', num: true, value: (row) => fixed(row.finance.valuation.pe_ttm, 2), sub: (row) => fixed(row.finance.valuation.pe_static, 2) },
    { key: 'currency', label: '币种折算', width: '104px', value: (row) => row.finance.currency_adjustment.conversion_required == null ? '未知' : row.finance.currency_adjustment.conversion_required ? '需要折算' : '无需折算', sub: (row) => `原生码 ${text(row.finance.currency_adjustment.native_code)}` }
  ];

  onMount(() => void loadActions());
</script>

<PageHeader
  eyebrow="TDX HK LOCAL CACHE · ZERO NETWORK"
  title="港股本地档案"
  description="解密客户端本地港股公司行动与财务缓存；保留原生累计复权因子和报告币种口径，不把历史快照包装成实时数据。"
  {stats}
>
  {#snippet actions()}
    <Select options={SECTIONS} value={section} width="158px" label="档案" onChange={switchSection} />
  {/snippet}
</PageHeader>

{#if section === 'actions'}
  <Panel
    title="港股历史公司行动"
    subtitle={actions.data ? `${count(actions.data.returned)} / ${count(actions.data.match_count)} · ${count(actions.data.summary.securities)} 只证券` : 'hkqxinfo2.dat + hkqxinfo.dat · 本地加密缓存'}
    busy={actions.busy}
    error={actions.error}
    onRetry={() => void loadActions(false)}
    empty={actions.loaded && (actions.data?.rows.length ?? 0) === 0}
    emptyText="当前筛选没有港股公司行动。"
    flush scroll fill
  >
    {#snippet toolbar()}
      <TextInput bind:value={actionCode} width="104px" label="港股代码" placeholder="00001" onEnter={() => void loadActions()} />
      <Select options={ACTION_KINDS} value={actionKind} width="136px" label="类型" onChange={(value) => { actionKind = value as HkActionKind; void loadActions(); }} />
      <TextInput bind:value={actionFrom} width="116px" label="开始" placeholder="YYYYMMDD" onEnter={() => void loadActions()} />
      <TextInput bind:value={actionTo} width="116px" label="结束" placeholder="YYYYMMDD" onEnter={() => void loadActions()} />
      <Select options={ORDERS} value={actionOrder} width="84px" label="顺序" onChange={(value) => { actionOrder = value; void loadActions(); }} />
      <Select options={PAGE_LIMITS} value={actionLimit} width="110px" label="分页" onChange={(value) => { actionLimit = value; void loadActions(); }} />
      <TextInput bind:value={actionQuery} icon="search" width="210px" label="检索" placeholder="代码 / 原始说明" onEnter={() => void loadActions()} />
      <Button icon="search" onclick={() => void loadActions()}>查询</Button>
      <div class="pager">
        <Button icon="chevron-left" disabled={actions.busy || actionOffset === 0} onclick={previousActions} />
        <span>第 {count(actionOffset / Number(actionLimit) + 1)} 页</span>
        <Button icon="chevron-right" disabled={actions.busy || !actions.data || actionOffset + actions.data.returned >= actions.data.match_count} onclick={nextActions} />
      </div>
    {/snippet}
    <DataTable columns={actionColumns} rows={actions.data?.rows ?? []} numbered stickyFirst minWidth="1080px" rowKey={(row) => `${row.security.security_id}:${row.date}:${row.source_file}:${row.description}`} />
  </Panel>
{:else}
  <Panel
    title="港股本地财务快照"
    subtitle={finance.data ? `${count(finance.data.returned)} / ${count(finance.data.match_count)} · 报告币种未自动换算` : 'hkcwdata.dat · 当前本地财务缓存'}
    busy={finance.busy}
    error={finance.error}
    onRetry={() => void loadFinance(false)}
    empty={finance.loaded && (finance.data?.rows.length ?? 0) === 0}
    emptyText="当前筛选没有港股财务快照。"
    flush scroll fill
  >
    {#snippet toolbar()}
      <TextInput bind:value={financeCode} width="104px" label="港股代码" placeholder="00001" onEnter={() => void loadFinance()} />
      <TextInput bind:value={financeClassification} width="108px" label="分类码" placeholder="111001" onEnter={() => void loadFinance()} />
      <TextInput bind:value={financeFrom} width="116px" label="报告起始" placeholder="YYYYMMDD" onEnter={() => void loadFinance()} />
      <TextInput bind:value={financeTo} width="116px" label="报告截止" placeholder="YYYYMMDD" onEnter={() => void loadFinance()} />
      <Select options={FINANCE_SORTS} value={financeSort} width="118px" label="排序" onChange={(value) => { financeSort = value; void loadFinance(); }} />
      <Select options={ORDERS} value={financeOrder} width="84px" label="顺序" onChange={(value) => { financeOrder = value; void loadFinance(); }} />
      <Select options={PAGE_LIMITS} value={financeLimit} width="110px" label="分页" onChange={(value) => { financeLimit = value; void loadFinance(); }} />
      <TextInput bind:value={financeQuery} icon="search" width="190px" label="原生字段检索" placeholder="代码 / 日期 / 数值" onEnter={() => void loadFinance()} />
      <Button icon="search" onclick={() => void loadFinance()}>查询</Button>
      <div class="pager">
        <Button icon="chevron-left" disabled={finance.busy || financeOffset === 0} onclick={previousFinance} />
        <span>第 {count(financeOffset / Number(financeLimit) + 1)} 页</span>
        <Button icon="chevron-right" disabled={finance.busy || !finance.data || financeOffset + finance.data.returned >= finance.data.match_count} onclick={nextFinance} />
      </div>
    {/snippet}
    <DataTable columns={financeColumns} rows={finance.data?.rows ?? []} numbered stickyFirst minWidth="1320px" rowKey={(row) => `${row.code}:${row.report_date ?? ''}:${row.classification_code ?? ''}`} />
  </Panel>
{/if}

<p class="boundary-note">
  两类档案都只读取服务启动时固定的 TDX 根，网络请求数为 0；页面不显示资源路径，也不接受文件、路径或 URL。公司行动是历史复权依据，财务字段沿用报告币种，原生币种折算码尚不臆造枚举名称。
</p>

<style>
  .pager { display: inline-flex; align-items: center; gap: var(--sp-2); margin-left: auto; }
  .pager span { min-width: 54px; font-family: var(--font-num); font-size: var(--fs-micro); color: var(--fg-mute); text-align: center; }
  .boundary-note { margin: var(--sp-3) var(--sp-1) 0; color: var(--fg-mute); font-size: var(--fs-small); line-height: 1.6; }
</style>
