<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, fixed, money, percent, price, text } from '../../lib/fmt';
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
  import type {
    MarketSpecialAttentionDocument,
    SpecialAttentionRecord
  } from '../../types';

  type View = MarketSpecialAttentionDocument['view'];
  const VIEWS = [
    { id: 'all', label: '全部' },
    { id: 'investigations', label: '立案调查' },
    { id: 'st-risk', label: '*ST 风险' },
    { id: 'star-cap-removal', label: '摘星摘帽' },
    { id: 'goodwill-risk', label: '商誉风险' },
    { id: 'equity-dispersion', label: '股权分散' }
  ];

  let view = $state<View>('investigations');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketSpecialAttentionDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);

  const stats = $derived.by<Stat[]>(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return [
      { label: '立案调查', value: count(summary.investigations) },
      { label: '仍在调查', value: count(summary.active_investigations), tone: summary.active_investigations ? 'down' : 'flat' },
      { label: '*ST 风险', value: count(summary.st_risk) },
      { label: '商誉风险', value: count(summary.goodwill_risk) },
      { label: '股权分散', value: count(summary.equity_dispersion) },
      { label: '覆盖证券', value: count(summary.unique_securities) }
    ];
  });

  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/special-attention?${queryString({
      view, q: query.trim(), limit: 5000, refresh: refresh ? 1 : 0
    })}`);
  }

  function switchView(next: string) {
    view = next as View;
    load();
  }

  function securityName(row: SpecialAttentionRecord): string {
    return row.security.name || row.security.code;
  }

  function gotoWorkbench() {
    if (selected)
      router.go(stockPath(selected.security.market, selected.security.code, 'special-attention'));
  }

  const genericColumns: Column<SpecialAttentionRecord>[] = [
    { key: 'kind', label: '关注类型', width: '126px', value: (row) => row.kind_label },
    { key: 'security', label: '证券', width: '156px', value: securityName, sub: (row) => row.security.security_id },
    { key: 'date', label: '业务日期', width: '100px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'summary', label: '核心说明', width: '420px', value: (row) => text(row.reason || row.risk_reason || row.status_or_forecast || row.largest_shareholder || row.industry) },
    { key: 'source', label: '原始资源', width: '210px', value: (row) => row.source_resource }
  ];

  const investigationColumns: Column<SpecialAttentionRecord>[] = [
    { key: 'security', label: '被调查证券', width: '156px', value: securityName, sub: (row) => row.security.security_id },
    { key: 'date', label: '立案日期', width: '100px', num: true, value: (row) => date(row.filing_date), sortValue: (row) => row.filing_date ?? '' },
    { key: 'reason', label: '原因', width: '280px', value: (row) => text(row.reason) },
    { key: 'progress', label: '案件进展', width: '260px', value: (row) => text(row.progress), sub: (row) => row.active ? '调查中' : date(row.penalty_date) },
    { key: 'close', label: '立案日收盘', align: 'right', num: true, value: (row) => price(row.filing_close) },
    { key: 'count', label: '十年次数', align: 'right', num: true, value: (row) => count(row.occurrence_count_10y) }
  ];

  const stColumns: Column<SpecialAttentionRecord>[] = [
    { key: 'security', label: '风险证券', width: '156px', value: securityName, sub: (row) => row.security.security_id },
    { key: 'period', label: '报告期', width: '100px', num: true, value: (row) => date(row.report_period) },
    { key: 'type', label: '风险类型', width: '140px', value: (row) => text(row.risk_type) },
    { key: 'reason', label: '不利原因', width: '340px', value: (row) => text(row.risk_reason) },
    { key: 'holders', label: '股东户数', align: 'right', num: true, value: (row) => `${fixed(row.shareholder_households_10k, 2)} 万户` },
    { key: 'profit', label: '净利润', align: 'right', num: true, value: (row) => money(row.net_profit_yuan) },
    { key: 'assets', label: '净资产', align: 'right', num: true, value: (row) => money(row.net_assets_yuan) }
  ];

  const removalColumns: Column<SpecialAttentionRecord>[] = [
    { key: 'security', label: '证券', width: '156px', value: securityName, sub: (row) => row.security.security_id },
    { key: 'status', label: '状态 / 预期', width: '180px', value: (row) => text(row.status_or_forecast) },
    { key: 'date', label: '实施日期', width: '100px', num: true, value: (row) => date(row.implementation_date) },
    { key: 'profit', label: '本期净利润', align: 'right', num: true, value: (row) => money(row.current_net_profit_yuan), sub: (row) => `上期 ${money(row.prior_net_profit_yuan)}` },
    { key: 'pb', label: '市净率', align: 'right', num: true, value: (row) => fixed(row.price_to_book_ratio, 3) },
    { key: 'explanation', label: '客户端分析', width: '380px', value: (row) => text(row.explanation_excerpt) }
  ];

  const goodwillColumns: Column<SpecialAttentionRecord>[] = [
    { key: 'security', label: '商誉风险证券', width: '156px', value: securityName, sub: (row) => `${row.security.security_id} · ${text(row.industry)}` },
    { key: 'period', label: '报告期', width: '100px', num: true, value: (row) => date(row.report_period) },
    { key: 'goodwill', label: '本期商誉', align: 'right', num: true, value: (row) => money(row.goodwill_current_yuan), sub: (row) => `上期 ${money(row.goodwill_prior_yuan)}` },
    { key: 'change', label: '商誉变动', align: 'right', num: true, value: (row) => money(row.goodwill_change_yuan), sub: (row) => percent(row.goodwill_change_pct) },
    { key: 'profit', label: '商誉 / 净利润', align: 'right', num: true, value: (row) => percent(row.goodwill_to_net_profit_pct) },
    { key: 'assets', label: '商誉 / 总资产', align: 'right', num: true, value: (row) => percent(row.goodwill_to_total_assets_pct) },
    { key: 'growth', label: '净利润增速', align: 'right', num: true, value: (row) => percent(row.net_profit_growth_pct) }
  ];

  const dispersionColumns: Column<SpecialAttentionRecord>[] = [
    { key: 'security', label: '股权分散证券', width: '156px', value: securityName, sub: (row) => row.security.security_id },
    { key: 'holder', label: '第一大股东', width: '280px', value: (row) => text(row.largest_shareholder) },
    { key: 'pct', label: '持股比例', align: 'right', num: true, value: (row) => percent(row.largest_shareholder_pct), sortValue: (row) => row.largest_shareholder_pct ?? Infinity },
    { key: 'industry', label: '行业', width: '160px', value: (row) => text(row.industry) },
    { key: 'region', label: '地区', width: '100px', value: (row) => text(row.region) },
    { key: 'date', label: '截止日期', width: '100px', num: true, value: (row) => date(row.cutoff_date) }
  ];

  const columns = $derived.by<Column<SpecialAttentionRecord>[]>(() => {
    if (view === 'investigations') return investigationColumns;
    if (view === 'st-risk') return stColumns;
    if (view === 'star-cap-removal') return removalColumns;
    if (view === 'goodwill-risk') return goodwillColumns;
    if (view === 'equity-dispersion') return dispersionColumns;
    return genericColumns;
  });

  const selectedStats = $derived.by<Stat[]>(() => {
    if (!selected) return [];
    if (selected.kind === 'investigations') return [
      { label: '立案日期', value: date(selected.filing_date) },
      { label: '立案日收盘', value: price(selected.filing_close) },
      { label: '十年次数', value: count(selected.occurrence_count_10y) },
      { label: '状态', value: selected.active ? '调查中' : text(selected.progress), tone: selected.active ? 'down' : 'flat' }
    ];
    if (selected.kind === 'goodwill-risk') return [
      { label: '本期商誉', value: money(selected.goodwill_current_yuan) },
      { label: '商誉变动', value: percent(selected.goodwill_change_pct) },
      { label: '商誉 / 净利润', value: percent(selected.goodwill_to_net_profit_pct) },
      { label: '商誉 / 总资产', value: percent(selected.goodwill_to_total_assets_pct) }
    ];
    return [
      { label: '业务日期', value: date(selected.date) },
      { label: '报告期', value: date(selected.report_period) },
      { label: '行业', value: text(selected.industry) },
      { label: '原始字段', value: count(Object.keys(selected.raw).length) }
    ];
  });

  onMount(() => load());
</script>

<PageHeader
  eyebrow="TBGZ · 5 ACTIVE SOURCES"
  title="特别关注"
  description="还原客户端当前有数据的股权分散、*ST 风险、摘星摘帽、立案调查和商誉风险五张表；保留原始字段，不把历史快照伪装成实时行情。"
  {stats}
>
  {#snippet actions()}
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="特别关注视图" />
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="410px">
  {#snippet main()}
    <Panel title="特别关注主表" subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.sources.length)} 个原始资源` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="当前筛选下没有命中记录。" flush scroll>
      {#snippet toolbar()}
        <TextInput bind:value={query} icon="search" width="260px" label="检索特别关注" placeholder="代码、名称、原因或行业" onEnter={() => load()} />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedId = row.event_id)} isActive={(row) => row.event_id === selected?.event_id} stickyFirst numbered minWidth="1080px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.kind_label ?? 'SPECIAL ATTENTION'} title={selected ? securityName(selected) : '特别关注详情'} subtitle={selected ? selected.source_resource : '点击左侧记录展开'} empty={!selected && !resource.busy} emptyText="选择一条记录查看客户端说明和口径。" scroll>
      {#if selected}
        <button class="jump" type="button" onclick={gotoWorkbench}>在证券工作台打开</button>
        <StatGrid stats={selectedStats} inline />
        {#if selected.kind === 'investigations'}
          <h3>{text(selected.reason)}</h3>
          <p class="copy">{text(selected.case_detail)}</p>
          <p>案件进展：{text(selected.progress)}；处罚披露日：{date(selected.penalty_date)}</p>
        {:else if selected.kind === 'star-cap-removal'}
          <h3>{text(selected.status_or_forecast)}</h3>
          <p class="copy">{text(selected.explanation)}</p>
        {:else if selected.kind === 'st-risk'}
          <h3>{text(selected.risk_type)}</h3>
          <p class="copy">{text(selected.risk_reason)}</p>
          <p>营收 {money(selected.revenue_yuan)}；扣非净利润 {money(selected.deducted_net_profit_yuan)}。</p>
        {:else if selected.kind === 'equity-dispersion'}
          <h3>第一大股东</h3>
          <p>{text(selected.largest_shareholder)}，持股 {percent(selected.largest_shareholder_pct)}；{text(selected.region)} · {text(selected.industry)}。</p>
        {:else}
          <h3>利润与营收</h3>
          <p>净利润 {money(selected.net_profit_current_yuan)}，同比 {percent(selected.net_profit_growth_pct)}；营收 {money(selected.revenue_current_yuan)}，同比 {percent(selected.revenue_growth_pct)}。</p>
        {/if}
        <p class="note">当前资源是客户端业务快照。所有展示换算均保留原始行供审计，未用当前价格反推历史收益。</p>
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .jump { width: 100%; height: 24px; margin-bottom: var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  h3 { margin: var(--sp-4) 0 var(--sp-2); font-size: var(--fs-micro); color: var(--fg-dim); }
  p { font-size: var(--fs-xs); line-height: 1.68; }
  .copy { white-space: pre-wrap; }
  .note { margin-top: var(--sp-3); color: var(--fg-mute); font-size: 10px; }
</style>
