<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, money, percent, price } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { MarketShareholderSignalsDocument, NotableInvestorDirectoryRecord, NotableInvestorHoldingRecord, ShareholderSignalRecord, ShareholderSignalsSort } from '../../types';

  type View = MarketShareholderSignalsDocument['view'];
  const VIEWS = [
    { id: 'all', label: '全部' },
    { id: 'notable-investors', label: '牛散持股' },
    { id: 'institution-accumulation', label: '机构增持' },
    { id: 'small-cap-institution', label: '小市值机构' },
    { id: 'research-growth', label: '成长调研' },
    { id: 'investor-directory', label: '知名自然人 → 股票' }
  ];
  const SORTS = [
    { id: 'signal', label: '信号强度' }, { id: 'holding-value', label: '持股金额' },
    { id: 'institution-growth', label: '机构增幅' }, { id: 'holder-change', label: '股东变化' },
    { id: 'research-6m', label: '半年调研' }, { id: 'profit-growth', label: '利润增幅' }
  ];
  let view = $state<View>('all');
  let sort = $state<ShareholderSignalsSort>('signal');
  let order = $state<'asc' | 'desc'>('desc');
  let query = $state('');
  let selectedId = $state('');
  let selectedInvestorId = $state('');
  const resource = new Resource<MarketShareholderSignalsDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const directory = $derived(doc?.investor_directory ?? []);
  const holdings = $derived(doc?.investor_holdings ?? []);
  const investorMode = $derived(view === 'investor-directory');
  const selected = $derived(rows.find((row) => row.record_id === selectedId) ?? rows[0] ?? null);
  const selectedInvestor = $derived(doc?.selected_investor ?? directory.find((row) => row.investor_id === selectedInvestorId) ?? null);
  const stats = $derived.by<Stat[]>(() => {
    if (!doc) return [];
    if (investorMode) return [
      { label: '知名自然人', value: count(doc.summary.investor_directory ?? directory.length) },
      { label: '当前匹配', value: count(doc.summary.investor_directory_matched ?? directory.length) },
      { label: '所选持股', value: count(doc.summary.investor_holdings ?? holdings.length) },
      { label: '主从对账', value: doc.investor_reconciliation?.company_count_matches && doc.investor_reconciliation?.holding_shares_match && doc.investor_reconciliation?.holding_value_matches ? '一致' : selectedInvestor ? '待核对' : '选择一人' }
    ];
    return [
      { label: '信号股票', value: count(doc.summary.unique_securities) },
      { label: '牛散持股', value: count(doc.summary['notable-investors']) },
      { label: '机构增持收缩', value: count(doc.summary['institution-accumulation']) },
      { label: '小市值机构', value: count(doc.summary['small-cap-institution']) },
      { label: '成长调研', value: count(doc.summary['research-growth']) }
    ];
  });
  function load(refresh = false) {
    selectedId = '';
    if (!investorMode) selectedInvestorId = '';
    void resource.load(`/api/v1/market/shareholder-signals?${queryString({
      view, sort, order, q: investorMode ? '' : query.trim(), investor_q: investorMode ? query.trim() : '',
      investor_id: investorMode ? selectedInvestorId : '', limit: 5000, include_raw: 0,
      refresh: refresh ? 1 : 0
    })}`);
  }
  function switchView(next: string) { view = next as View; selectedInvestorId = ''; load(); }
  function switchSort(next: string) { sort = next as ShareholderSignalsSort; load(); }
  function toggleOrder() { order = order === 'desc' ? 'asc' : 'desc'; load(); }
  function name(row: ShareholderSignalRecord) { return row.security.name || row.security.code; }
  function primary(row: ShareholderSignalRecord) {
    if (row.kind === 'notable-investors') return `${count(row.notable_investor_count)} 位牛散`;
    if (row.kind === 'institution-accumulation') return `机构 ${delta(row.institution_holding_growth_pct)}`;
    if (row.kind === 'small-cap-institution') return `持股增加 ${delta(row.institution_holding_growth_pct)}`;
    return `半年 ${count(row.research_count_6m)} 次调研`;
  }
  function primarySub(row: ShareholderSignalRecord) {
    if (row.kind === 'notable-investors') return `持股 ${compact(row.holding_shares, '股')}`;
    if (row.kind === 'institution-accumulation') return `股东 ${delta(row.shareholder_count_change_pct)}`;
    if (row.kind === 'small-cap-institution') return `流通占比 ${percent(row.institution_float_holding_pct)}`;
    return `${count(row.institutions_6m)} 家机构`;
  }
  function capital(row: ShareholderSignalRecord) {
    if (row.kind === 'notable-investors') return money(row.holding_value_yuan);
    if (row.kind === 'institution-accumulation') return compact(row.institution_holding_change_shares, '股');
    if (row.kind === 'small-cap-institution') return compact(row.institution_holding_change_shares, '股');
    return money(row.net_profit_yuan);
  }
  function capitalSub(row: ShareholderSignalRecord) {
    if (row.kind === 'notable-investors') return percent(row.holding_pct);
    if (row.kind === 'institution-accumulation') return `前十流通 ${percent(row.top10_float_holding_pct)}`;
    if (row.kind === 'small-cap-institution') return `市值变动 ${money(row.institution_holding_value_change_yuan)}`;
    return `利润 ${delta(row.net_profit_growth_pct)}`;
  }
  function performance(row: ShareholderSignalRecord) {
    if (row.kind === 'notable-investors') return `PE ${fixed(row.pe_ttm)}`;
    if (row.kind === 'institution-accumulation') return `${count(row.shareholder_count)} 户`;
    if (row.kind === 'small-cap-institution') return `机构源值 ${fixed(row.institution_count_source, 2)}`;
    return `1月 ${delta(row.return_1m_pct)}`;
  }
  function performanceSub(row: ShareholderSignalRecord) {
    if (row.kind === 'notable-investors') return `期末 ${price(row.report_close)}`;
    if (row.kind === 'institution-accumulation') return `变化 ${compact(row.shareholder_count_change, '户')}`;
    if (row.kind === 'small-cap-institution') return `变动源值 ${fixed(row.institution_count_change_source, 2)}`;
    return `6月 ${delta(row.return_6m_pct)}`;
  }
  function openStock() {
    if (selected) router.go(stockPath(selected.security.market, selected.security.code, 'shareholder-signals'));
  }
  function selectInvestor(row: NotableInvestorDirectoryRecord) {
    selectedInvestorId = row.investor_id;
    load();
  }
  function openHolding(row: NotableInvestorHoldingRecord) {
    router.go(stockPath(row.security.market, row.security.code, 'shareholder-signals'));
  }
  const columns: Column<ShareholderSignalRecord>[] = [
    { key: 'security', label: '股票', width: '160px', value: name, sub: (row) => row.security.security_id },
    { key: 'kind', label: '信号 / 报告期', width: '170px', value: (row) => row.kind_label, sub: (row) => date(row.report_period) },
    { key: 'primary', label: '核心信号', align: 'right', num: true, value: primary, sub: primarySub },
    { key: 'capital', label: '持股 / 利润', align: 'right', num: true, value: capital, sub: capitalSub },
    { key: 'performance', label: '结构 / 表现', align: 'right', num: true, value: performance, sub: performanceSub },
    { key: 'research', label: '最近调研日', align: 'right', num: true, value: (row) => date(row.latest_research_date) }
  ];
  const investorColumns: Column<NotableInvestorDirectoryRecord>[] = [
    { key: 'name', label: '知名自然人', width: '170px', value: (row) => row.investor_name, sub: (row) => row.investor_id },
    { key: 'companies', label: '持股公司', align: 'right', num: true, value: (row) => count(row.holding_company_count), sub: () => '只' },
    { key: 'shares', label: '合计持股', align: 'right', num: true, value: (row) => compact(row.holding_shares, '股') },
    { key: 'value', label: '合计市值', align: 'right', num: true, value: (row) => money(row.holding_value_yuan) },
    { key: 'date', label: '数据截止', align: 'right', num: true, value: (row) => date(row.as_of_date) }
  ];
  const holdingColumns: Column<NotableInvestorHoldingRecord>[] = [
    { key: 'security', label: '持仓股票', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'shares', label: '本期持股', align: 'right', num: true, value: (row) => compact(row.holding_shares, '股'), sub: (row) => `变化 ${compact(row.holding_change_shares, '股')}` },
    { key: 'value', label: '本期市值', align: 'right', num: true, value: (row) => money(row.holding_value_yuan), sub: (row) => `变化 ${money(row.holding_value_change_yuan)}` },
    { key: 'pct', label: '持股比例', align: 'right', num: true, value: (row) => percent(row.holding_pct), sub: (row) => `变化 ${delta(row.holding_pct_change)}` },
    { key: 'rank', label: '名次', align: 'right', num: true, value: (row) => count(row.current_rank), sub: (row) => row.prior_rank == null ? '新进' : `上期 ${count(row.prior_rank)}` },
    { key: 'date', label: '最新日期', align: 'right', num: true, value: (row) => date(row.latest_date), sub: (row) => row.source_type }
  ];
  const detailStats = $derived.by<Stat[]>(() => {
    if (!selected) return [];
    if (selected.kind === 'notable-investors') return [
      { label: '牛散人数', value: count(selected.notable_investor_count) },
      { label: '合计持股', value: compact(selected.holding_shares, '股') },
      { label: '占总股本', value: percent(selected.holding_pct) },
      { label: '期末持股金额', value: money(selected.holding_value_yuan) }
    ];
    if (selected.kind === 'institution-accumulation') return [
      { label: '机构持股增幅', value: delta(selected.institution_holding_growth_pct) },
      { label: '机构持股变化', value: compact(selected.institution_holding_change_shares, '股') },
      { label: '股东户数变化', value: delta(selected.shareholder_count_change_pct) },
      { label: '前十流通占比', value: percent(selected.top10_float_holding_pct) }
    ];
    if (selected.kind === 'small-cap-institution') return [
      { label: '报告期流通市值', value: money(selected.report_float_market_cap_yuan) },
      { label: '机构持股增加', value: compact(selected.institution_holding_change_shares, '股') },
      { label: '机构持股增幅', value: delta(selected.institution_holding_growth_pct) },
      { label: '占报告期流通', value: percent(selected.institution_float_holding_pct) },
      { label: '机构持股市值', value: money(selected.institution_holding_value_yuan) },
      { label: '市值变动', value: money(selected.institution_holding_value_change_yuan) },
      { label: '机构数源值', value: fixed(selected.institution_count_source, 2) },
      { label: '机构数变动源值', value: fixed(selected.institution_count_change_source, 2) }
    ];
    return [
      { label: '净利润增幅', value: delta(selected.net_profit_growth_pct) },
      { label: '半年调研次数', value: count(selected.research_count_6m) },
      { label: '半年机构数', value: count(selected.institutions_6m) },
      { label: '半年涨幅', value: delta(selected.return_6m_pct) }
    ];
  });
  onMount(() => load());
</script>

<PageHeader eyebrow="CWNSCG · NSCG/&lt;投资人&gt; · JGXC · XSZGP · JGZD" title="牛散、机构与调研信号" description="既可从股票反查牛散、常规机构与小市值专业机构信号，也可从 1,755 位知名自然人目录展开其当前持仓股票。" {stats}>
  {#snippet actions()}
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="信号类型" />
    {#if !investorMode}<Button onclick={toggleOrder}>{order === 'desc' ? '降序 ↓' : '升序 ↑'}</Button>{/if}
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="390px">
  {#snippet main()}
    {#if investorMode}
      <Panel title="知名自然人目录" subtitle={doc ? `${count(directory.length)} 人 · 点击展开持仓` : '主表 NSCG，明细按投资人动态读取'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && directory.length === 0} emptyText="当前条件下没有知名自然人。" flush scroll>
        {#snippet toolbar()}
          <TextInput bind:value={query} icon="search" width="260px" label="检索投资人" placeholder="姓名或投资人 ID" onEnter={() => { selectedInvestorId = ''; load(); }} />
        {/snippet}
        <DataTable columns={investorColumns} rows={directory} rowKey={(row) => row.investor_id} onRowClick={selectInvestor} isActive={(row) => row.investor_id === selectedInvestor?.investor_id} stickyFirst numbered minWidth="760px" />
      </Panel>
    {:else}
      <Panel title="股东与机构信号" subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.summary.unique_securities)} 只股票` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="当前条件下没有信号。" flush scroll>
        {#snippet toolbar()}
          <TextInput bind:value={query} icon="search" width="220px" label="检索股票" placeholder="代码或名称" onEnter={() => load()} />
          <Segmented options={SORTS} value={sort} onChange={switchSort} ariaLabel="信号排序" />
        {/snippet}
        <DataTable {columns} {rows} rowKey={(row) => row.record_id} onRowClick={(row) => (selectedId = row.record_id)} isActive={(row) => row.record_id === selected?.record_id} stickyFirst numbered minWidth="1000px" />
      </Panel>
    {/if}
  {/snippet}
  {#snippet aside()}
    {#if investorMode}
      <Panel eyebrow={selectedInvestor?.investor_id ?? 'NSCG DYNAMIC DETAIL'} title={selectedInvestor?.investor_name ?? '投资人持仓'} subtitle={selectedInvestor ? `${date(selectedInvestor.as_of_date)} · 主表 ${count(selectedInvestor.holding_company_count)} 只` : '选择左侧投资人'} busy={resource.busy} empty={!selectedInvestor && !resource.busy} emptyText="选择一位知名自然人，按其动态 ID 加载持仓股票。" flush scroll>
        {#if selectedInvestor}
          <div class="investor-summary">
            <strong>{money(selectedInvestor.holding_value_yuan)}</strong>
            <span>{compact(selectedInvestor.holding_shares, '股')} · 明细 {count(holdings.length)} 只</span>
            {#if doc?.investor_reconciliation}
              <span class:ok={doc.investor_reconciliation.company_count_matches && doc.investor_reconciliation.holding_shares_match && doc.investor_reconciliation.holding_value_matches}>主表 / 明细：{doc.investor_reconciliation.company_count_matches && doc.investor_reconciliation.holding_shares_match && doc.investor_reconciliation.holding_value_matches ? '三项一致' : '存在差异'}</span>
            {/if}
          </div>
          <DataTable columns={holdingColumns} rows={holdings} rowKey={(row) => row.record_id} onRowClick={openHolding} numbered minWidth="720px" />
        {/if}
      </Panel>
    {:else}
      <Panel eyebrow={selected?.kind_label ?? 'SHAREHOLDER SIGNAL'} title={selected ? name(selected) : '信号详情'} subtitle={selected ? `${selected.security.security_id} · ${date(selected.report_period)}` : '选择左侧股票'} empty={!selected && !resource.busy}>
        {#if selected}
          <button type="button" onclick={openStock}>在个股工作台打开</button>
          <StatGrid stats={detailStats} inline />
          <p>{primary(selected)}；{primarySub(selected)}。源数据来自 {selected.source_resource}。</p>
        {/if}
      </Panel>
    {/if}
  {/snippet}
</Split>

<style>
  button { min-width: 72px; height: 28px; padding: 0 var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  p { margin-top: var(--sp-3); font-size: var(--fs-xs); line-height: 1.7; color: var(--fg-mute); overflow-wrap: anywhere; }
  .investor-summary { display: grid; gap: var(--sp-1); padding: var(--sp-4); border-bottom: 1px solid var(--line); }
  .investor-summary strong { font-family: var(--font-num); font-size: var(--fs-lg); }
  .investor-summary span { color: var(--fg-mute); font-size: var(--fs-xs); }
  .investor-summary span.ok { color: var(--positive); }
</style>
