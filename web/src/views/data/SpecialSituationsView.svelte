<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, delta, fixed, money, price, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { MarketSpecialSituationsDocument, SpecialSituationRecord } from '../../types';

  type View = MarketSpecialSituationsDocument['view'];
  const VIEWS = [
    { id: 'all', label: '全部' },
    { id: 'corporate-actions', label: '并购重组' },
    { id: 'neeq-transfers', label: '三板转板' },
    { id: 'neeq-regulation', label: '三板监管' },
    { id: 'mergers', label: '吸收合并' },
    { id: 'b-to-h', label: 'B转H' },
    { id: 'market-cap-risk', label: '市值预警' }
  ];

  let view = $state<View>('all');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketSpecialSituationsDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);

  const stats = $derived.by<Stat[]>(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return [
      { label: '吸收合并', value: count(summary.mergers) },
      { label: 'B转H', value: count(summary.b_to_h) },
      { label: '有效转换事件', value: count(summary.active_merger_events) },
      { label: '市值预警', value: count(summary.market_cap_warnings), note: `${count(summary.market_cap_unique_securities)} 只证券` },
      { label: '重大重组', value: count(summary.major_restructuring_plans + summary.major_restructuring_reviews + summary.major_restructuring_completed), note: '预案 / 审核 / 实施' },
      { label: '普通并购', value: count(summary.ordinary_merger_plans), note: '客户端普通并购预案' },
      { label: '拟转 / 已转', value: `${count(summary.neeq_transfer_plans)} / ${count(summary.neeq_transfer_completed)}`, note: '新三板转板链路' },
      { label: '三板监管', value: count(summary.neeq_regulation_events) }
    ];
  });

  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/special-situations?${queryString({
      view,
      q: query.trim(),
      include_quotes: ['mergers', 'b-to-h', 'market-cap-risk'].includes(view) ? 1 : 0,
      limit: 5000,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function switchView(next: string) {
    view = next as View;
    load();
  }

  function securityName(row: SpecialSituationRecord): string {
    return row.primary_security.name || row.primary_security.code;
  }

  function relatedName(row: SpecialSituationRecord): string {
    return row.related_security?.name || row.related_security?.code || '—';
  }

  function gotoWorkbench() {
    if (selected) router.go(stockPath(
      selected.primary_security.market,
      selected.primary_security.code,
      'special-situations'
    ));
  }

  function kindTone(row: SpecialSituationRecord): 'warn' | 'focus' | 'neutral' {
    if (row.kind === 'market-cap-risk' || row.kind === 'neeq-regulation') return 'warn';
    return row.active ? 'focus' : 'neutral';
  }

  function relation(row: SpecialSituationRecord): string {
    if (row.kind === 'market-cap-risk') return text(row.sample_index);
    if (row.kind.startsWith('major-') || row.kind === 'ordinary-merger-plan') return text(row.acquiring_party);
    if (row.kind === 'neeq-transfer-plan') return text(row.target_board);
    if (row.kind === 'neeq-transfer-completed') return `${text(row.listing_venue_before)} → ${text(row.listing_venue_after)}`;
    if (row.kind === 'neeq-regulation') return text(row.regulation_reason);
    return relatedName(row);
  }

  const allColumns: Column<SpecialSituationRecord>[] = [
    { key: 'kind', label: '事项', width: '104px', slot: true, sortValue: (row) => row.kind },
    { key: 'primary', label: '主证券', width: '138px', value: securityName, sub: (row) => row.primary_security.security_id },
    { key: 'related', label: '关联对象 / 交易方', width: '210px', wrap: true, value: relation, sub: (row) => row.related_security?.security_id || text(row.transaction_type || row.trigger_type || row.industry) },
    { key: 'status', label: '状态 / 触发', width: '104px', value: (row) => text(row.status) },
    { key: 'metric', label: '关键数据', align: 'right', num: true, value: (row) => row.transaction_amount_yuan !== undefined ? money(row.transaction_amount_yuan) : row.net_profit_yuan !== undefined ? money(row.net_profit_yuan) : row.kind === 'market-cap-risk' ? delta(row.high_to_current_change_pct) : delta(row.cash_option_premium_pct), sub: (row) => text(row.adviser || row.regulation_measure || row.industry), tone: (row) => tone(row.kind === 'market-cap-risk' ? row.high_to_current_change_pct : row.cash_option_premium_pct) },
    { key: 'date', label: '统计日', width: '86px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date }
  ];

  const mergerColumns: Column<SpecialSituationRecord>[] = [
    { key: 'primary', label: '吸收方', width: '140px', value: securityName, sub: (row) => row.primary_security.security_id },
    { key: 'related', label: '被吸收方', width: '140px', value: relatedName, sub: (row) => row.related_security?.security_id || '' },
    { key: 'status', label: '进度', width: '104px', value: (row) => text(row.status) },
    { key: 'primary-price', label: '吸收方现价 / 换股价', align: 'right', num: true, value: (row) => `${price(row.primary_quote?.last_price)} / ${fixed(row.absorber_exchange_price, 2)}`, sub: (row) => delta(row.absorber_exchange_premium_pct), tone: (row) => tone(row.absorber_exchange_premium_pct) },
    { key: 'cash', label: '被吸收方现价 / 现金权', align: 'right', num: true, value: (row) => `${price(row.related_quote?.last_price)} / ${fixed(row.absorbed_cash_option_price, 2)}`, sub: (row) => delta(row.cash_option_premium_pct), tone: (row) => tone(row.cash_option_premium_pct) },
    { key: 'exchange', label: '被吸收方换股价', align: 'right', num: true, value: (row) => fixed(row.absorbed_exchange_price, 2), sub: (row) => delta(row.absorbed_exchange_premium_pct), tone: (row) => tone(row.absorbed_exchange_premium_pct) }
  ];

  const bToHColumns: Column<SpecialSituationRecord>[] = [
    { key: 'primary', label: 'B股', width: '140px', value: securityName, sub: (row) => row.primary_security.security_id },
    { key: 'related', label: '关联A股', width: '140px', value: relatedName, sub: (row) => row.related_security?.security_id || '' },
    { key: 'status', label: '进度', width: '100px', value: (row) => text(row.status) },
    { key: 'current', label: 'B股现价', align: 'right', num: true, value: (row) => price(row.primary_quote?.last_price), sub: (row) => text(row.currency), tone: (row) => tone(row.primary_quote?.change_pct) },
    { key: 'cash', label: '现金选择权', align: 'right', num: true, value: (row) => fixed(row.cash_option_price, 3), sub: (row) => text(row.currency) },
    { key: 'premium', label: '选择权溢价', align: 'right', num: true, value: (row) => delta(row.cash_option_premium_pct), tone: (row) => tone(row.cash_option_premium_pct), sortValue: (row) => row.cash_option_premium_pct ?? 0 }
  ];

  const riskColumns: Column<SpecialSituationRecord>[] = [
    { key: 'primary', label: '证券', width: '140px', value: securityName, sub: (row) => row.primary_security.security_id },
    { key: 'index', label: '样本指数', width: '170px', wrap: true, value: (row) => text(row.sample_index) },
    { key: 'trigger', label: '入选类型', width: '96px', value: (row) => text(row.trigger_type), sub: (row) => `${count(row.breach_count)} 项阈值` },
    { key: '20d', label: '近20日涨幅', align: 'right', num: true, value: (row) => delta(row.twenty_day_change_pct), tone: (row) => tone(row.twenty_day_change_pct), sortValue: (row) => row.twenty_day_change_pct ?? 0 },
    { key: 'high', label: '一年最高 / 最新', align: 'right', num: true, value: (row) => `${price(row.one_year_high_price)} / ${price(row.primary_quote?.last_price)}`, sub: (row) => `实时 ${delta(row.live_high_to_current_change_pct)}` },
    { key: 'decline', label: '最高至统计日', align: 'right', num: true, value: (row) => delta(row.high_to_current_change_pct), tone: (row) => tone(row.high_to_current_change_pct), sortValue: (row) => row.high_to_current_change_pct ?? 0 },
    { key: 'date', label: '统计日', width: '86px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date }
  ];

  const corporateColumns: Column<SpecialSituationRecord>[] = [
    { key: 'primary', label: '上市公司', width: '140px', value: securityName, sub: (row) => row.primary_security.security_id },
    { key: 'kind', label: '阶段', width: '120px', value: (row) => row.kind_label, sub: (row) => text(row.status) },
    { key: 'type', label: '交易类型', width: '130px', value: (row) => text(row.transaction_type), sub: (row) => text(row.industry) },
    { key: 'buyer', label: '标的获得方', width: '220px', wrap: true, value: (row) => text(row.acquiring_party) },
    { key: 'seller', label: '标的出让方', width: '200px', wrap: true, value: (row) => text(row.disposing_party) },
    { key: 'amount', label: '涉及额', align: 'right', num: true, value: (row) => money(row.transaction_amount_yuan), sortValue: (row) => row.transaction_amount_yuan ?? 0 },
    { key: 'date', label: '公告日', width: '86px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date }
  ];

  const transferColumns: Column<SpecialSituationRecord>[] = [
    { key: 'primary', label: '公司', width: '140px', value: securityName, sub: (row) => row.primary_security.security_id },
    { key: 'kind', label: '阶段', width: '110px', value: (row) => row.kind_label, sub: (row) => text(row.status) },
    { key: 'board', label: '拟上市 / 转板去向', width: '150px', value: (row) => text(row.target_board || row.listing_venue_after), sub: (row) => text(row.listing_venue_before) },
    { key: 'profit', label: '净利润', align: 'right', num: true, value: (row) => money(row.net_profit_yuan), sub: (row) => `上期 ${money(row.prior_net_profit_yuan)}` },
    { key: 'revenue', label: '营业收入', align: 'right', num: true, value: (row) => money(row.revenue_yuan), sub: (row) => `净资产 ${money(row.net_assets_yuan)}` },
    { key: 'adviser', label: '辅导机构', width: '180px', value: (row) => text(row.adviser) },
    { key: 'date', label: '报告 / 上市日', width: '96px', num: true, value: (row) => date(row.report_period || row.listing_date || row.date) }
  ];

  const regulationColumns: Column<SpecialSituationRecord>[] = [
    { key: 'primary', label: '公司', width: '140px', value: securityName, sub: (row) => row.primary_security.security_id },
    { key: 'reason', label: '监管原因', width: '260px', wrap: true, value: (row) => text(row.regulation_reason), sub: (row) => text(row.industry) },
    { key: 'measure', label: '监管措施', width: '180px', wrap: true, value: (row) => text(row.regulation_measure) },
    { key: 'case', label: '案情', width: '420px', wrap: true, value: (row) => text(row.description) },
    { key: 'date', label: '公告日', width: '86px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date }
  ];

  const columns = $derived.by<Column<SpecialSituationRecord>[]>(() => {
    if (view === 'mergers') return mergerColumns;
    if (view === 'b-to-h') return bToHColumns;
    if (view === 'market-cap-risk') return riskColumns;
    if (view === 'corporate-actions') return corporateColumns;
    if (view === 'neeq-transfers') return transferColumns;
    if (view === 'neeq-regulation') return regulationColumns;
    return allColumns;
  });

  const selectedStats = $derived.by<Stat[]>(() => {
    if (!selected) return [];
    if (selected.kind === 'market-cap-risk') return [
      { label: '近20日涨幅', value: delta(selected.twenty_day_change_pct), tone: tone(selected.twenty_day_change_pct) },
      { label: '一年最高价', value: price(selected.one_year_high_price) },
      { label: '最高至统计日', value: delta(selected.high_to_current_change_pct), tone: tone(selected.high_to_current_change_pct) },
      { label: '最高至实时', value: delta(selected.live_high_to_current_change_pct), tone: tone(selected.live_high_to_current_change_pct) }
    ];
    if (selected.kind === 'b-to-h') return [
      { label: 'B股现价', value: price(selected.primary_quote?.last_price), tone: tone(selected.primary_quote?.change_pct) },
      { label: '现金选择权', value: fixed(selected.cash_option_price, 3), note: text(selected.currency) },
      { label: '选择权溢价', value: delta(selected.cash_option_premium_pct), tone: tone(selected.cash_option_premium_pct) },
      { label: '关联A股', value: relatedName(selected) }
    ];
    if (selected.kind.startsWith('major-') || selected.kind === 'ordinary-merger-plan') return [
      { label: '涉及额', value: money(selected.transaction_amount_yuan) },
      { label: '交易类型', value: text(selected.transaction_type) },
      { label: '标的获得方', value: text(selected.acquiring_party) },
      { label: '标的出让方', value: text(selected.disposing_party) }
    ];
    if (selected.kind === 'neeq-transfer-plan') return [
      { label: '拟上市板块', value: text(selected.target_board) },
      { label: '净利润', value: money(selected.net_profit_yuan) },
      { label: '营业收入', value: money(selected.revenue_yuan) },
      { label: '辅导机构', value: text(selected.adviser) }
    ];
    if (selected.kind === 'neeq-transfer-completed') return [
      { label: '受理日', value: date(selected.acceptance_date) },
      { label: '注册日', value: date(selected.registration_date) },
      { label: '上市日', value: date(selected.listing_date) },
      { label: '上市地', value: `${text(selected.listing_venue_before)} → ${text(selected.listing_venue_after)}` }
    ];
    if (selected.kind === 'neeq-regulation') return [
      { label: '监管措施', value: text(selected.regulation_measure) },
      { label: '监管原因', value: text(selected.regulation_reason) },
      { label: '行业', value: text(selected.industry) },
      { label: '公告日', value: date(selected.date) }
    ];
    return [
      { label: '吸收方换股价', value: fixed(selected.absorber_exchange_price, 2), note: delta(selected.absorber_exchange_premium_pct) },
      { label: '现金选择权', value: fixed(selected.absorbed_cash_option_price, 2), note: delta(selected.cash_option_premium_pct) },
      { label: '被吸收方换股价', value: fixed(selected.absorbed_exchange_price, 2), note: delta(selected.absorbed_exchange_premium_pct) },
      { label: '被吸收方', value: relatedName(selected) }
    ];
  });

  onMount(() => load());
</script>

<PageHeader
  eyebrow="AGTL · CDGC · QXFA · XSBTJ"
  title="并购重组、转板与特殊事项"
  description="覆盖吸收合并、B转H、市值管理预警、重大重组四阶段，以及新三板拟转A股、已转板和自律监管；定价型视图才请求公开L1。"
  {stats}
>
  {#snippet actions()}
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="特殊事项类型" />
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="410px">
  {#snippet main()}
    <Panel title="特殊事项关系" subtitle={doc ? `${count(doc.match_count)} 条 · L1 ${count(doc.quote_source?.received ?? 0)}/${count(doc.quote_source?.requested ?? 0)}` : '本地JSN + 公开L1'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="当前筛选下没有特殊事项记录。" flush scroll>
      {#snippet toolbar()}
        <TextInput bind:value={query} icon="search" width="260px" label="检索事项" placeholder="股票、指数、状态或方案正文" onEnter={() => load()} />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedId = row.event_id)} isActive={(row) => row.event_id === selected?.event_id} stickyFirst numbered minWidth={view === 'corporate-actions' || view === 'neeq-regulation' ? '1280px' : '1000px'}>
        {#snippet cell({ row })}<Badge tone={kindTone(row)}>{row.kind_label}</Badge>{/snippet}
      </DataTable>
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.kind_label ?? 'SPECIAL SITUATION'} title={selected ? securityName(selected) : '事项详情'} subtitle={selected ? `${selected.primary_security.security_id} · ${text(selected.status)}` : '点击左侧记录展开'} empty={!selected && !resource.busy} emptyText="选择一条记录查看定价关系与触发证据。" scroll>
      {#if selected}
        <button class="jump" type="button" onclick={gotoWorkbench}>在个股工作台打开</button>
        <StatGrid stats={selectedStats} inline />
        {#if selected.kind === 'market-cap-risk'}
          <h3>监管触发证据</h3>
          <p>样本指数：{text(selected.sample_index)}</p>
          <p class="note">20日阈值：累计跌幅达到20%；一年阈值：最新收盘价低于近一年最高收盘价的50%。统计口径以 {date(selected.date)} 原表为准，实时值只作公开L1补充。</p>
        {:else}
          <h3>方案说明</h3><p class="copy">{text(selected.description)}</p>
          {#if selected.announcement_url}<a class="source-link" href={selected.announcement_url} target="_blank" rel="noreferrer">打开预案公告</a>{/if}
          {#if selected.kind === 'merger' || selected.kind === 'b-to-h'}<p class="note">页面溢价严格复现客户端 `(现价-参考价)/参考价×100`，正负仅代表现价相对参考价方向，不构成收益承诺。</p>{/if}
        {/if}
        {#each doc?.quote_errors ?? [] as failure}<p class="warn">L1：{failure.message}</p>{/each}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .jump { width: 100%; height: 24px; margin-bottom: var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  h3 { margin: var(--sp-4) 0 var(--sp-2); font-size: var(--fs-micro); color: var(--fg-dim); }
  p { font-size: var(--fs-xs); line-height: 1.65; }
  .copy { white-space: pre-wrap; }
  .note { margin-top: var(--sp-3); color: var(--fg-mute); font-size: 10px; }
  .source-link { display: inline-block; margin-top: var(--sp-2); color: var(--focus); font-size: var(--fs-xs); }
  .warn { margin-top: var(--sp-2); color: var(--warn); }
</style>
