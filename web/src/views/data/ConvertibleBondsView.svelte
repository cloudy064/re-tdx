<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, percent, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { ConvertibleBondDetailEvent, ConvertibleBondPricingRecord, ConvertibleBondRecord, ConvertibleBondSubscriptionRecord, MarketConvertibleBondsDocument, NewConvertibleBondProjectionRecord, PendingConvertibleBondRecord } from '../../types';

  let view = $state<'listed' | 'pending' | 'subscriptions' | 'pricing'>('pricing');
  let query = $state('');
  let selected = $state<ConvertibleBondRecord | null>(null);
  const catalog = new Resource<MarketConvertibleBondsDocument>();
  const detail = new Resource<MarketConvertibleBondsDocument>();
  const doc = $derived(catalog.data);
  const rows = $derived(doc?.bonds ?? []);
  const pendingRows = $derived(doc?.pending_issues ?? []);
  const subscriptionRows = $derived(doc?.subscriptions ?? []);
  const newBondRows = $derived(doc?.new_bond_projection ?? []);
  const pricingRows = $derived(doc?.pricing ?? []);
  const detailed = $derived(detail.data?.bonds[0] ?? selected);
  const history = $derived(detail.data?.details[0] ?? null);

  const stats = $derived.by(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return view === 'pending' ? [
      { label: '待发方案', value: count(summary.pending_issues) },
      { label: '计划规模', value: `${fixed(summary.planned_issue_size_100m_yuan, 2)} 亿元` },
      { label: '进度分类', value: count(summary.by_progress?.length) },
      { label: '来源', value: `${count(doc?.sources.length)} 张主表`, note: doc?.sources[0]?.endpoint ?? '' }
    ] : view === 'subscriptions' ? [
      { label: '已发记录', value: count(summary.subscriptions) },
      { label: '已经上市', value: count(summary.listed) },
      { label: '尚未上市', value: count(summary.not_listed) },
      { label: '公式可复算', value: count(summary.formula_complete), note: '转股价值 + 溢价率' },
      { label: '累计发行规模', value: `${fixed(summary.issue_size_100m_yuan, 2)} 亿元` },
      { label: '客户端新债投影', value: count(summary.new_bond_projection_rows), note: `${count(summary.new_bond_hybrid_or_stale)} 条字段冲突` }
    ] : view === 'pricing' ? [
      { label: '定价样本', value: count(summary.pricing_rows) },
      { label: '当前存续', value: count(summary.active_bonds) },
      { label: '完整估值', value: count(summary.complete_valuations), note: `${count(summary.bond_quotes)} 债券 / ${count(summary.underlying_quotes)} 正股行情` },
      { label: '可交换债', value: count(summary.exchangeable_bonds) },
      { label: '行情状态', value: doc?.quote_availability === 'live' ? '公开 L1 正常' : '仅静态条款', note: doc?.sources[0]?.endpoint ?? '' }
    ] : [
      { label: '上市债券', value: count(summary.bonds) },
      { label: '可交换债', value: count(summary.exchangeable_bonds), note: `${count(summary.exchangeable_bonds_supplemented)} 只条款已补齐 · ${count(summary.exchangeable_bonds_projection_verified)} 只二次验证` },
      { label: '条款完整', value: count(summary.core_terms_complete), note: `${count(summary.core_terms_missing)} 只仍缺核心字段` },
      { label: '剩余余额', value: `${fixed(summary.remaining_balance_100m_yuan, 2)} 亿元` },
      { label: '发生回售 / 赎回', value: `${count(summary.sellback_triggered_bonds)} / ${count(summary.redemption_triggered_bonds)}` },
      { label: '转股价调整', value: count(summary.conversion_price_revisions) },
      { label: '来源', value: `${count(doc?.sources.length)} 张主表`, note: doc?.sources[0]?.endpoint ?? '' }
    ];
  });

  function load(refresh = false) {
    const params: Record<string, string | number> = {
      view, q: query.trim(), include_details: 0, limit: 2000, refresh: refresh ? 1 : 0
    };
    if (view === 'pending') {
      params.sort = 'progress-date';
      params.order = 'desc';
    } else if (view === 'subscriptions') {
      params.sort = 'subscription-date';
      params.order = 'desc';
    } else if (view === 'pricing') {
      params.sort = 'double-low';
      params.order = 'asc';
      params.include_quotes = 1;
    }
    void catalog.load(`/api/v1/market/convertible-bonds?${queryString(params)}`);
    selected = null;
    detail.reset();
  }

  function switchView(next: 'listed' | 'pending' | 'subscriptions' | 'pricing') {
    if (view === next) return;
    view = next;
    load();
  }

  function select(row: ConvertibleBondRecord) {
    selected = row;
    void detail.load(`/api/v1/market/convertible-bonds?${queryString({ market: row.bond.market, code: row.bond.code, include_details: 1 })}`);
  }

  function openUnderlying() {
    if (detailed?.underlying) router.go(stockPath(detailed.underlying.market, detailed.underlying.code, 'convertible-bonds'));
  }

  function resourceName(value: string) {
    return value.split('/').slice(-1)[0] ?? value;
  }

  const columns: Column<ConvertibleBondRecord>[] = [
    { key: 'bond', label: '转债 / 可交换债', width: '152px', value: (row) => row.bond.name || row.bond.code, sub: (row) => `${row.bond.security_id} · ${row.instrument_type === 'exchangeable-bond' ? '可交换债' : '可转债'}`, sortValue: (row) => row.bond.code },
    { key: 'stock', label: '正股', width: '132px', value: (row) => row.underlying?.name || '—', sub: (row) => row.underlying?.security_id ?? '', sortValue: (row) => row.underlying?.code ?? '' },
    { key: 'balance', label: '剩余余额', align: 'right', num: true, value: (row) => `${fixed(row.overview.remaining_balance_100m_yuan, 2)} 亿`, sub: (row) => percent(row.overview.remaining_ratio_pct), sortValue: (row) => row.overview.remaining_balance_100m_yuan ?? 0 },
    { key: 'conversion', label: '转股价', align: 'right', num: true, value: (row) => fixed(row.overview.conversion_price, 3), sub: (row) => `${date(row.overview.conversion_start_date)} 起`, sortValue: (row) => row.overview.conversion_price ?? 0 },
    { key: 'maturity', label: '到期日', num: true, value: (row) => date(row.overview.maturity_date), sub: (row) => `${fixed(row.overview.remaining_years, 2)} 年`, sortValue: (row) => row.overview.maturity_date },
    { key: 'rating', label: '评级', value: (row) => text(row.overview.bond_rating), sub: (row) => row.overview.current_state },
    { key: 'sellback', label: '回售', value: (row) => text(row.sellback.status), sub: (row) => `历史 ${count(row.sellback.history_count)} 次` },
    { key: 'redemption', label: '赎回', value: (row) => text(row.redemption.status), sub: (row) => `历史 ${count(row.redemption.history_count)} 次` },
    { key: 'revision', label: '下修 / 调价', value: (row) => text(row.revision.status), sub: (row) => `${count(row.revision.history_count)} 次`, sortValue: (row) => row.revision.history_count ?? 0 }
  ];

  const pendingColumns: Column<PendingConvertibleBondRecord>[] = [
    { key: 'stock', label: '正股', width: '145px', value: (row) => row.underlying.name || row.underlying.code, sub: (row) => row.underlying.security_id },
    { key: 'type', label: '发行类型', width: '90px', value: (row) => text(row.issue_type) },
    { key: 'progress', label: '方案进度', width: '150px', wrap: true, value: (row) => text(row.plan_progress), sub: (row) => date(row.progress_date) },
    { key: 'size', label: '计划规模', width: '100px', align: 'right', num: true, value: (row) => `${fixed(row.planned_issue_size_100m_yuan, 2)} 亿`, sortValue: (row) => row.planned_issue_size_100m_yuan ?? 0 },
    { key: 'rights', label: '每股配售', width: '90px', align: 'right', num: true, value: (row) => fixed(row.stock_rights_yuan, 4), sub: (row) => row.shareholder_placement_ratio == null ? '' : percent(row.shareholder_placement_ratio * 100) },
    { key: 'conversion', label: '转股价', width: '85px', align: 'right', num: true, value: (row) => fixed(row.conversion_price_yuan, 3) },
    { key: 'subscription', label: '申购', width: '120px', value: (row) => text(row.subscription_name), sub: (row) => [row.subscription_code, date(row.subscription_date)].filter(Boolean).join(' · ') },
    { key: 'lottery', label: '中签率 / 日期', width: '105px', align: 'right', num: true, value: (row) => percent(row.lottery_rate), sub: (row) => date(row.lottery_date) }
  ];

  const pricingColumns: Column<ConvertibleBondPricingRecord>[] = [
    { key: 'bond', label: '转债', width: '145px', value: (row) => row.bond.name || row.bond.code, sub: (row) => `${row.bond.security_id} · ${row.terms.bond_rating || '未评级'}`, sortValue: (row) => row.bond.code },
    { key: 'bond-price', label: '债券现价', width: '95px', align: 'right', num: true, value: (row) => fixed(row.quote.bond_last_price, 3), sub: (row) => row.quote.bond_price_source === 'pre-close' ? '昨收口径' : percent(row.quote.bond_change_pct), sortValue: (row) => row.quote.bond_last_price ?? -Infinity },
    { key: 'stock', label: '正股', width: '125px', value: (row) => row.underlying?.name || '—', sub: (row) => row.underlying ? `${fixed(row.quote.underlying_last_price, 2)} · ${row.quote.underlying_price_source === 'pre-close' ? '昨收' : percent(row.quote.underlying_change_pct)}` : '', sortValue: (row) => row.underlying?.code ?? '' },
    { key: 'conversion', label: '转股价值', width: '90px', align: 'right', num: true, value: (row) => fixed(row.valuation.conversion_value, 3), sub: (row) => `转股价 ${fixed(row.terms.conversion_price, 3)}`, sortValue: (row) => row.valuation.conversion_value ?? -Infinity },
    { key: 'premium', label: '转股溢价', width: '90px', align: 'right', num: true, value: (row) => percent(row.valuation.conversion_premium_pct), sub: (row) => `全价 ${fixed(row.valuation.full_price, 3)}`, sortValue: (row) => row.valuation.conversion_premium_pct ?? Infinity },
    { key: 'ytm', label: '到期收益率', width: '90px', align: 'right', num: true, value: (row) => percent(row.valuation.maturity_yield_pct), sub: (row) => `${count(row.valuation.cash_flow_count)} 笔现金流`, sortValue: (row) => row.valuation.maturity_yield_pct ?? -Infinity },
    { key: 'pure', label: '纯债价值', width: '90px', align: 'right', num: true, value: (row) => fixed(row.valuation.pure_bond_value, 3), sub: (row) => `溢价 ${percent(row.valuation.pure_bond_premium_pct)}`, sortValue: (row) => row.valuation.pure_bond_value ?? -Infinity },
    { key: 'double-low', label: '双低值', width: '82px', align: 'right', num: true, value: (row) => fixed(row.valuation.double_low_score, 2), sub: (row) => row.valuation.availability === 'complete' ? '本地推导' : '数据不足', sortValue: (row) => row.valuation.double_low_score ?? Infinity },
    { key: 'maturity', label: '到期', width: '95px', num: true, value: (row) => date(row.terms.maturity_date), sub: (row) => `${fixed(row.terms.remaining_years, 2)} 年`, sortValue: (row) => row.terms.maturity_date }
  ];

  const subscriptionColumns: Column<ConvertibleBondSubscriptionRecord>[] = [
    { key: 'bond', label: '转债', width: '145px', value: (row) => row.bond.name || row.bond.code, sub: (row) => row.bond.security_id, sortValue: (row) => row.bond.code },
    { key: 'stock', label: '正股', width: '135px', value: (row) => row.underlying.name || row.underlying.code, sub: (row) => row.underlying.security_id },
    { key: 'subscription', label: '申购日 / 代码', width: '115px', num: true, value: (row) => date(row.subscription_date), sub: (row) => row.subscription_code, sortValue: (row) => row.subscription_date },
    { key: 'size', label: '发行规模', width: '90px', align: 'right', num: true, value: (row) => `${fixed(row.issue_size_100m_yuan, 2)} 亿`, sub: (row) => `上限 ${fixed(row.subscription_limit_10k_yuan, 2)} 万`, sortValue: (row) => row.issue_size_100m_yuan ?? 0 },
    { key: 'lottery', label: '中签率 / 公布日', width: '105px', align: 'right', num: true, value: (row) => percent(row.lottery_rate_pct, 6), sub: (row) => date(row.lottery_date), sortValue: (row) => row.lottery_rate_pct ?? Infinity },
    { key: 'conversion', label: '转股价值', width: '95px', align: 'right', num: true, value: (row) => fixed(row.conversion_value_yuan, 3), sub: (row) => `转股价 ${fixed(row.conversion_price_yuan, 3)}`, sortValue: (row) => row.conversion_value_yuan ?? 0 },
    { key: 'premium', label: '转股溢价', width: '92px', align: 'right', num: true, value: (row) => percent(row.conversion_premium_pct), sub: (row) => `债收盘 ${fixed(row.bond_close_yuan, 3)}`, sortValue: (row) => row.conversion_premium_pct ?? Infinity },
    { key: 'listing', label: '上市 / 转股', width: '105px', num: true, value: (row) => row.listed ? date(row.listing_date) : '尚未上市', sub: (row) => `转股 ${date(row.conversion_start_date)}` }
  ];

  const newBondColumns: Column<NewConvertibleBondProjectionRecord>[] = [
    { key: 'stock', label: '正股', width: '140px', value: (row) => row.underlying.name || row.underlying.code, sub: (row) => row.underlying.security_id },
    { key: 'subscription', label: '申购代码 / 日期', width: '125px', value: (row) => row.subscription_code, sub: (row) => date(row.subscription_date), sortValue: (row) => row.subscription_date },
    { key: 'progress', label: '客户端阶段', width: '135px', value: (row) => text(row.plan_progress), sub: (row) => date(row.progress_date) },
    { key: 'size', label: '投影规模', width: '95px', align: 'right', num: true, value: (row) => `${fixed(row.issue_size_100m_yuan, 2)} 亿`, sub: (row) => row.subscription_match.issue_size_mismatch ? `主表 ${fixed(row.subscription_match.primary_issue_size_100m_yuan, 2)} 亿` : '与申购主表一致', sortValue: (row) => row.issue_size_100m_yuan ?? 0 },
    { key: 'rights', label: '每股配售', width: '90px', align: 'right', num: true, value: (row) => fixed(row.stock_rights_yuan, 4), sub: (row) => percent((row.shareholder_placement_ratio ?? 0) * 100) },
    { key: 'conversion', label: '转股价 / 起始', width: '105px', align: 'right', num: true, value: (row) => fixed(row.conversion_price_yuan, 3), sub: (row) => date(row.conversion_start_date) },
    { key: 'match', label: '申购主表对账', width: '115px', value: (row) => row.subscription_match.match_method === 'subscription-code' ? '申购代码命中' : row.subscription_match.matched ? '仅正股命中' : '未命中', sub: (row) => row.subscription_match.issue_size_mismatch || row.subscription_match.subscription_date_mismatch ? '字段存在冲突' : '字段一致' }
  ];

  const historyColumns: Column<ConvertibleBondDetailEvent>[] = [
    { key: 'date', label: '日期', num: true, value: (row) => date(row.date ?? row.start_date), sub: (row) => row.end_date ? `至 ${date(row.end_date)}` : '' },
    { key: 'kind', label: '类型', value: (row) => row.kind === 'sellback' ? '回售' : row.kind === 'redemption' ? '赎回' : '转股价调整' },
    { key: 'price', label: '价格', align: 'right', num: true, value: (row) => fixed(row.conversion_price ?? row.price, 3) },
    { key: 'quantity', label: '数量', align: 'right', num: true, value: (row) => compact(row.quantity, '张') },
    { key: 'amount', label: '金额', align: 'right', num: true, value: (row) => compact(row.amount, '元') },
    { key: 'reason', label: '原因 / 付款日', wrap: true, value: (row) => text(row.reason || (row.payment_date ? `付款 ${date(row.payment_date)}` : '')) }
  ];

  const historyRows = $derived<ConvertibleBondDetailEvent[]>(history ? [...history.sellback, ...history.redemption, ...history.revision] : []);
  onMount(() => load());
</script>

<PageHeader eyebrow="KZZ · PRICING + ISSUANCE + TERMS + PENDING" title="可转债定价、发行与条款" description={view === 'pricing' ? '把收益表、公开 L1 债券/正股行情与披露现金流合并，复算全价、转股溢价、到期收益率、纯债价值和双低值。' : view === 'listed' ? '统一查看转股、票息、回售、赎回、下修条件及触发历史；可交换债基础条款由独立收益表补齐。' : view === 'subscriptions' ? '查看已发可转债的申购、中签、上市、转股日期，并严格复算客户端转股价值与溢价率公式。' : '查看尚未上市的可转债方案进度、计划规模、含权、申购与中签信息。'} {stats}>
  {#snippet actions()}
    <Button variant={view === 'pricing' ? 'primary' : 'default'} onclick={() => switchView('pricing')}>实时定价</Button>
    <Button variant={view === 'listed' ? 'primary' : 'default'} onclick={() => switchView('listed')}>已上市条款</Button>
    <Button variant={view === 'subscriptions' ? 'primary' : 'default'} onclick={() => switchView('subscriptions')}>发行申购</Button>
    <Button variant={view === 'pending' ? 'primary' : 'default'} onclick={() => switchView('pending')}>待发方案</Button>
    <Button icon="refresh" busy={catalog.busy} onclick={() => load(true)}>刷新当前视图</Button>
  {/snippet}
</PageHeader>

{#if view === 'pricing'}
  <Panel title="可转债实时定价" subtitle={doc ? `${count(doc.match_count)} 条 · 双低值升序 · 点击进入正股工作台` : ''} busy={catalog.busy} error={catalog.error} onRetry={() => load()} empty={catalog.loaded && !catalog.busy && pricingRows.length === 0} flush scroll>
    {#snippet toolbar()}<TextInput bind:value={query} icon="search" width="260px" label="检索" placeholder="转债 / 正股名称或代码" onEnter={() => load()} />{/snippet}
    <DataTable columns={pricingColumns} rows={pricingRows} stickyFirst numbered rowKey={(row) => row.bond.security_id} onRowClick={(row) => row.underlying && router.go(stockPath(row.underlying.market, row.underlying.code, 'convertible-bonds'))} sortKey="double-low" />
  </Panel>
  <p class="method-note">估值口径：全价 = 公开 L1 净价 + 按票息日程及 Actual/365 推导的应计利息；YTM 与纯债价值由剩余现金流本地复算。它们是可审计的重建值，不冒充 JSN 原始字段。</p>
{:else if view === 'pending'}
  <Panel title="待发可转债方案" subtitle={doc ? `${count(doc.match_count)} 条 · 点击正股进入个股工作台` : ''} busy={catalog.busy} error={catalog.error} onRetry={() => load()} empty={catalog.loaded && !catalog.busy && pendingRows.length === 0} flush scroll>
    {#snippet toolbar()}<TextInput bind:value={query} icon="search" width="260px" label="检索" placeholder="正股、进度或申购代码" onEnter={() => load()} />{/snippet}
    <DataTable columns={pendingColumns} rows={pendingRows} stickyFirst numbered rowKey={(row) => row.underlying.security_id} onRowClick={(row) => router.go(stockPath(row.underlying.market, row.underlying.code, 'convertible-bonds'))} sortKey="progress" />
  </Panel>
  {#if doc?.projection_reconciliation?.length}
    <div class="reconciliation-strip">
      <span class="reconciliation-title">客户端待发投影对账</span>
      {#each doc.projection_reconciliation as item (item.projection_resource)}
        <Badge tone={item.exact_security_set ? 'up' : 'warn'}>
          {resourceName(item.projection_resource)} · 覆盖 {count(item.common_securities)}/{count(item.projection_securities)} · 主表独有 {count(item.primary_only_security_ids.length)} · 投影独有 {count(item.projection_only_security_ids.length)}
        </Badge>
      {/each}
    </div>
  {/if}
{:else if view === 'subscriptions'}
  <Panel title="已发可转债申购与上市" subtitle={doc ? `${count(doc.match_count)} 条 · 点击正股进入个股工作台` : ''} busy={catalog.busy} error={catalog.error} onRetry={() => load()} empty={catalog.loaded && !catalog.busy && subscriptionRows.length === 0} flush scroll>
    {#snippet toolbar()}<TextInput bind:value={query} icon="search" width="260px" label="检索" placeholder="转债、正股或申购代码" onEnter={() => load()} />{/snippet}
    <DataTable columns={subscriptionColumns} rows={subscriptionRows} stickyFirst numbered rowKey={(row) => row.event_id} onRowClick={(row) => router.go(stockPath(row.underlying.market, row.underlying.code, 'convertible-bonds'))} sortKey="subscription" />
  </Panel>
  <Panel title="客户端“新可转债”投影" subtitle={doc?.new_bond_reconciliation ? `${count(doc.new_bond_reconciliation.projection_rows)} 条 · 申购代码命中 ${count(doc.new_bond_reconciliation.exact_subscription_code_matches)} 条 · 字段冲突 ${count(doc.new_bond_reconciliation.hybrid_or_stale_count)} 条` : '等待投影数据'} busy={catalog.busy} empty={catalog.loaded && !catalog.busy && newBondRows.length === 0} emptyText="客户端当前没有新可转债投影。" flush scroll>
    <DataTable columns={newBondColumns} rows={newBondRows} stickyFirst numbered rowKey={(row) => row.event_id} onRowClick={(row) => router.go(stockPath(row.underlying.market, row.underlying.code, 'convertible-bonds'))} sortKey="subscription" />
  </Panel>
  <p class="method-note">客户端公式：转股价值 = 正股昨收 × 100 ÷ 转股价；转股溢价率 =（债券收盘价 − 转股价值）× 100 ÷ 转股价值。中签率保持 CFG 的百分数点口径。</p>
  <p class="method-note">“新可转债”是客户端筛选投影，不是独立发行主表；按申购代码对账并保留规模冲突，不按正股强行映射。当前冲突通常来自旧申购代码与较新待发方案字段混合。</p>
{:else}
<Split asideWidth="390px">
  {#snippet main()}
    <Panel title="可转债主表" subtitle={doc ? `${count(doc.match_count)} 条 · 点击展开动态历史` : ''} busy={catalog.busy} error={catalog.error} onRetry={() => load()} empty={catalog.loaded && !catalog.busy && rows.length === 0} flush scroll>
      {#snippet toolbar()}<TextInput bind:value={query} icon="search" width="260px" label="检索" placeholder="转债 / 正股名称或代码" onEnter={() => load()} />{/snippet}
      <DataTable {columns} {rows} stickyFirst numbered rowKey={(row) => row.bond.security_id} onRowClick={select} isActive={(row) => row.bond.security_id === selected?.bond.security_id} sortKey="balance" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <div class="aside">
      <Panel title={detailed?.bond.name ?? '转债详情'} eyebrow="BOND → UNDERLYING" subtitle={detailed ? `${detailed.bond.security_id} · ${detailed.underlying?.security_id ?? '无正股映射'}` : '点击左侧转债'} busy={detail.busy} error={detail.error} empty={!detailed && !detail.busy} emptyText="选择一只可转债展开条款和历史。">
        {#if detailed}
          {#if detailed.underlying}<button class="jump" type="button" onclick={openUnderlying}>在个股工作台打开 {detailed.underlying.name}</button>{/if}
          <StatGrid inline stats={[
            { label: '转股价', value: fixed(detailed.overview.conversion_price, 3), note: `${date(detailed.overview.conversion_start_date)} — ${date(detailed.overview.conversion_end_date)}` },
            { label: '剩余余额', value: `${fixed(detailed.overview.remaining_balance_100m_yuan, 2)} 亿`, note: percent(detailed.overview.remaining_ratio_pct) },
            { label: '到期赎回价', value: fixed(detailed.overview.maturity_redemption_price, 3), note: date(detailed.overview.maturity_date) },
            { label: '未付息合计', value: fixed(detailed.overview.unpaid_coupon_sum, 4), note: detailed.exchangeable_projection_verified ? '可交换债第二投影' : '上游未提供' },
            { label: '评级', value: text(detailed.overview.bond_rating), note: `发行人 ${text(detailed.overview.issuer_rating)}` },
            { label: '回售', value: text(detailed.sellback.status), note: `${text(detailed.sellback.condition)} · ${count(detailed.sellback.history_count)} 次` },
            { label: '赎回', value: text(detailed.redemption.status), note: `${text(detailed.redemption.condition)} · ${count(detailed.redemption.history_count)} 次` }
          ]} />
          <p class="coupons">票息：{detailed.coupons.rates_pct.map((rate, index) => `第${index + 1}年 ${fixed(rate, 3)}%`).join(' · ')}</p>
        {/if}
      </Panel>
      <Panel title="回售、赎回与转股价历史" subtitle={`${count(historyRows.length)} 条动态记录`} busy={detail.busy} empty={Boolean(detailed) && !detail.busy && historyRows.length === 0} emptyText="该转债当前没有动态触发历史。" flush scroll>
        <DataTable columns={historyColumns} rows={historyRows} rowKey={(row, index) => `${row.kind}:${row.date ?? row.start_date}:${index}`} />
      </Panel>
    </div>
  {/snippet}
</Split>
{/if}

<style>
  .aside { display: flex; min-height: 0; flex-direction: column; gap: var(--sp-2); }
  .jump { width: 100%; height: 24px; margin-bottom: var(--sp-2); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  .jump:hover { background: var(--bg-hover); }
  .coupons { margin-top: var(--sp-3); font-size: var(--fs-micro); line-height: var(--lh-body); color: var(--fg-dim); }
  .method-note { margin: var(--sp-2) var(--sp-1) 0; color: var(--fg-dim); font-size: var(--fs-micro); line-height: var(--lh-body); }
  .reconciliation-strip { display: flex; flex-wrap: wrap; align-items: center; gap: var(--sp-2); margin: var(--sp-2) var(--sp-1); color: var(--fg-dim); font-size: var(--fs-micro); }
  .reconciliation-title { color: var(--fg); font-weight: 600; }
</style>
