<script lang="ts">
  /**
   * 业绩预告。左主表两个口径：30 个研究行业加 880001 全 A 汇总的预告统计，
   * 以及独立建模的港股预告（市场号 31/48）。净利润、同比、股本按各自真实单位
   * 输出——A 股是人民币元，港股是公告原始币种，两者不做换算。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { Resource } from '../../lib/resource.svelte';
  import { DASH, compact, count, date, fixed, num, percent, text } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    ForecastIndustry,
    HongKongForecast,
    MarketForecastDocument,
    SecurityForecast,
    ValuationSecurity
  } from '../../types';

  type View = 'latest' | 'industries' | 'hong-kong';

  const VIEWS = [
    { id: 'latest', label: '全市场最新', hint: 'A 股当前主表，含提前预告的未来报告期' },
    { id: 'industries', label: '行业预告统计', hint: '研究行业 + 全 A 汇总，可下钻 A 股' },
    { id: 'hong-kong', label: '港股业绩预告', hint: '市场号 31/48，独立建模' }
  ];

  const SENTIMENTS = [
    { id: 'all', label: '全部方向' },
    { id: 'positive', label: '正向' },
    { id: 'negative', label: '负向' },
    { id: 'uncertain', label: '不确定' }
  ];

  let view = $state<View>('latest');
  let category = $state('all');
  let query = $state('');
  let hongKong = $state<HongKongForecast | null>(null);

  const catalog = new Resource<MarketForecastDocument>();
  const detail = new Resource<MarketForecastDocument>();

  const doc = $derived(catalog.data);
  const summary = $derived(doc?.summary);
  const industry = $derived(detail.data?.selected_industry ?? null);

  const stats = $derived.by(() => {
    if (!summary) return [];
    return [
      { label: '研究行业', value: count(summary.industries), note: '另含全 A 汇总' },
      { label: 'A 股公司', value: `${count(summary.companies)}` },
      { label: '已预告 / 覆盖率', value: `${count(summary.forecast_companies)} · ${percent(summary.coverage_pct)}` },
      { label: '预喜 / 不利', value: `${summary.favorable} / ${summary.adverse}` },
      { label: '最新主表', value: count(summary.latest_forecasts), note: `${count(summary.future_report_rows)} 条未来报告期` },
      { label: '截面差异', value: count(summary.current_report_gap), note: '较行业动态全 A 表少' },
      { label: '港股预告 / 股票', value: `${summary.hong_kong_forecasts} / ${summary.hong_kong_securities}` },
      {
        label: '港股正向 / 负向',
        value: `${summary.hong_kong_positive} / ${summary.hong_kong_negative}`,
        note: `不确定 ${summary.hong_kong_uncertain}`
      }
    ];
  });

  /** 预告区间：上下限任一缺失就退化成单值，单位由调用方按真实口径传入。 */
  function amountRange(lower: unknown, upper: unknown, unit: string): string {
    const left = num(lower);
    const right = num(upper);
    if (left === null && right === null) return DASH;
    if (left === null) return compact(right, unit);
    if (right === null || left === right) return compact(left, unit);
    return `${compact(left, unit)} — ${compact(right, unit)}`;
  }

  function pctRange(lower: unknown, upper: unknown): string {
    const left = num(lower);
    const right = num(upper);
    if (left === null && right === null) return DASH;
    if (left === null) return percent(right);
    if (right === null || left === right) return percent(left);
    return `${percent(left)} — ${percent(right)}`;
  }

  function sentimentTone(sentiment: string): 'up' | 'down' | 'flat' {
    return sentiment === 'negative' ? 'down' : sentiment === 'uncertain' ? 'flat' : 'up';
  }

  function load(refresh = false) {
    void catalog.load(
      `/api/v1/market/forecasts?${queryString({
        view,
        category: view === 'industries' ? 'all' : category,
        q: query.trim(),
        limit: 5000,
        refresh: refresh ? 1 : 0
      })}`
    );
    detail.reset();
    hongKong = null;
  }

  function switchView(next: string) {
    view = next as View;
    category = 'all';
    query = '';
    load();
  }

  function openIndustry(row: ForecastIndustry) {
    hongKong = null;
    void detail.load(
      `/api/v1/market/forecasts?${queryString({
        view: 'securities',
        industry: row.industry.code,
        report_period: row.report_period,
        include_details: 1,
        detail_limit: 5000
      })}`
    );
  }

  function openHongKong(row: HongKongForecast) {
    detail.reset();
    hongKong = row;
  }

  function gotoWorkbench(security: ValuationSecurity) {
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code));
  }

  const industryColumns: Column<ForecastIndustry>[] = [
    {
      key: 'industry',
      label: '行业 / 市场',
      width: '146px',
      value: (row) => row.industry.name || row.industry.code,
      sub: (row) => `${row.industry.code} · ${row.is_market_summary ? '全 A 汇总' : '研究行业'}`
    },
    { key: 'period', label: '报告期', width: '84px', num: true, value: (row) => date(row.report_period) },
    {
      key: 'companies',
      label: '公司数',
      align: 'right',
      num: true,
      value: (row) => count(row.company_count),
      sortValue: (row) => row.company_count
    },
    {
      key: 'forecasts',
      label: '已预告',
      align: 'right',
      num: true,
      value: (row) => count(row.forecast_count),
      sortValue: (row) => row.forecast_count
    },
    {
      key: 'coverage',
      label: '覆盖率',
      align: 'right',
      num: true,
      value: (row) => percent(row.coverage_pct),
      sortValue: (row) => row.coverage_pct ?? 0
    },
    {
      key: 'favorable',
      label: '预喜',
      align: 'right',
      num: true,
      value: (row) => count(row.favorable_count),
      sub: (row) => percent(row.favorable_pct),
      tone: () => 'up',
      sortValue: (row) => row.favorable_pct ?? 0
    },
    {
      key: 'adverse',
      label: '不利',
      align: 'right',
      num: true,
      value: (row) => count(row.adverse_count),
      tone: () => 'down',
      sortValue: (row) => row.adverse_count
    },
    {
      key: 'big',
      label: '大增 / 大降',
      align: 'right',
      num: true,
      value: (row) => `${row.category_counts.big_increase ?? 0} / ${row.category_counts.big_decrease ?? 0}`
    },
    {
      key: 'turn',
      label: '扭亏 / 预亏',
      align: 'right',
      num: true,
      value: (row) => `${row.category_counts.turnaround ?? 0} / ${row.category_counts.loss ?? 0}`
    },
    {
      key: 'reduce',
      label: '减亏 / 不确定',
      align: 'right',
      num: true,
      value: (row) =>
        `${(row.category_counts.loss_reduction ?? 0) + (row.category_counts.big_loss_reduction ?? 0)} / ${row.category_counts.uncertain ?? 0}`
    }
  ];

  const hongKongColumns: Column<HongKongForecast>[] = [
    {
      key: 'security',
      label: '证券',
      width: '132px',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => `${row.security.security_id} · ${text(row.currency)}`
    },
    { key: 'date', label: '公告日', width: '84px', num: true, value: (row) => date(row.forecast_date) },
    {
      key: 'report',
      label: '报告期',
      width: '104px',
      num: true,
      value: (row) => `${text(row.report_type)} · ${date(row.end_date)}`
    },
    {
      key: 'type',
      label: '预告类型',
      width: '76px',
      value: (row) => text(row.forecast_type),
      tone: (row) => sentimentTone(row.sentiment)
    },
    {
      key: 'profit',
      label: '净利润区间',
      align: 'right',
      num: true,
      value: (row) => amountRange(row.profit_lower, row.profit_upper, row.currency || ''),
      sortValue: (row) => row.profit_lower ?? row.profit_upper ?? 0
    },
    {
      key: 'growth',
      label: '同比区间',
      align: 'right',
      num: true,
      value: (row) => pctRange(row.growth_lower_pct, row.growth_upper_pct),
      tone: (row) => sentimentTone(row.sentiment),
      sortValue: (row) => row.growth_lower_pct ?? row.growth_upper_pct ?? 0
    },
    {
      key: 'capital',
      label: '总股本',
      align: 'right',
      num: true,
      value: (row) => compact(row.total_capital_shares, '股')
    },
    { key: 'industry', label: '行业', wrap: true, value: (row) => text(row.industry) }
  ];

  const latestColumns: Column<SecurityForecast>[] = [
    { key: 'security', label: '证券', width: '132px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'date', label: '预告日', width: '84px', num: true, value: (row) => date(row.forecast_date), sortValue: (row) => row.forecast_date },
    { key: 'period', label: '报告期', width: '84px', num: true, value: (row) => date(row.report_period), sortValue: (row) => row.report_period },
    { key: 'type', label: '预告类型', width: '88px', value: (row) => text(row.forecast_type), tone: (row) => sentimentTone(row.sentiment) },
    { key: 'profit', label: '净利润区间', width: '170px', align: 'right', num: true, value: (row) => amountRange(row.profit_lower_yuan, row.profit_upper_yuan, '元'), sortValue: (row) => row.profit_lower_yuan ?? row.profit_upper_yuan ?? 0 },
    { key: 'growth', label: '同比区间', width: '135px', align: 'right', num: true, value: (row) => pctRange(row.growth_lower_pct, row.growth_upper_pct), tone: (row) => sentimentTone(row.sentiment), sortValue: (row) => row.growth_lower_pct ?? row.growth_upper_pct ?? 0 },
    { key: 'prior', label: '上年同期', width: '110px', align: 'right', num: true, value: (row) => compact(row.prior_profit_yuan, '元'), sortValue: (row) => row.prior_profit_yuan ?? 0 },
    { key: 'reason', label: '变动原因', width: '300px', wrap: true, value: (row) => text(row.reason || row.contents) }
  ];

  const industryRows = $derived(doc?.industries ?? []);
  const latestRows = $derived(doc?.securities ?? []);
  const hongKongRows = $derived(doc?.hong_kong ?? []);
  const mainRows = $derived(view === 'latest' ? latestRows.length : view === 'industries' ? industryRows.length : hongKongRows.length);

  onMount(() => load());
</script>

<PageHeader
  eyebrow="CBPL101 + YJYGTJ + GGYJYG"
  title="业绩预告"
  description="全市场 A 股最新预告、行业覆盖统计和港股预告统一查看；静态主表与行业动态表的截面差异单独对账。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={catalog.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="380px">
  {#snippet main()}
    <Panel
      flush
      scroll
      busy={catalog.busy}
      error={catalog.error}
      onRetry={() => load()}
      empty={catalog.loaded && !catalog.busy && mainRows === 0}
      emptyText="当前视图与检索条件下没有业绩预告记录。"
      title={view === 'latest' ? 'A 股全市场最新预告' : view === 'industries' ? '行业与全市场统计' : '港股最新业绩预告'}
      subtitle={doc
        ? `${count(view === 'latest' ? doc.counts.securities : view === 'industries' ? doc.counts.industries : doc.counts.hong_kong)} 行`
        : ''}
    >
      {#snippet toolbar()}
        <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="业绩预告视图" />
        {#if view !== 'industries'}
          <Select
            value={category}
            options={SENTIMENTS}
            label="方向"
            width="140px"
            onChange={(next) => {
              category = next;
              load();
            }}
          />
        {/if}
        <TextInput
          bind:value={query}
          icon="search"
          width="230px"
          label="检索当前视图"
          placeholder={view === 'latest' ? 'A 股代码、名称、类型或正文' : view === 'industries' ? '行业名称或 88xxxx 代码' : '港股代码、行业、类型或正文'}
          onEnter={() => load()}
        />
      {/snippet}

      {#if view === 'latest'}
        <DataTable
          columns={latestColumns}
          rows={latestRows}
          stickyFirst
          numbered
          rowKey={(row, index) => `${row.security.security_id}-${row.report_period}-${index}`}
          onRowClick={(row) => gotoWorkbench(row.security)}
          sortKey="date"
          sortDesc
        />
      {:else if view === 'industries'}
        <DataTable
          columns={industryColumns}
          rows={industryRows}
          stickyFirst
          rowKey={(row) => row.detail_id}
          onRowClick={openIndustry}
          isActive={(row) => row.detail_id === industry?.detail_id}
        />
      {:else}
        <DataTable
          columns={hongKongColumns}
          rows={hongKongRows}
          stickyFirst
          rowKey={(row, index) => `${row.security.security_id}-${index}`}
          onRowClick={openHongKong}
          isActive={(row) =>
            row.security.security_id === hongKong?.security.security_id &&
            row.forecast_date === hongKong?.forecast_date}
        />
      {/if}
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow={view === 'latest' ? 'A-SHARE → WORKBENCH' : view === 'industries' ? 'INDUSTRY → A-SHARES' : 'HK FORECAST'}
      title={hongKong
        ? hongKong.security.name || hongKong.security.code
        : industry
          ? industry.industry.name || industry.industry.code
          : '逐股预告'}
      subtitle={hongKong
        ? `${text(hongKong.report_type)} · ${date(hongKong.start_date)} — ${date(hongKong.end_date)}`
        : industry
          ? `${date(industry.report_period)} · ${count(detail.data?.securities.length ?? 0)} 只 A 股`
          : '点击左侧任意行展开'}
      busy={detail.busy}
      error={detail.error}
      empty={!hongKong && !detail.loaded && !detail.busy}
      emptyText={view === 'latest'
        ? '点击左侧 A 股预告直接进入个股工作台查看完整上下文。'
        : view === 'industries'
        ? '选择一个行业与报告期，下钻该区间内已披露预告的 A 股。'
        : '选择一条港股预告，查看公告正文与变动原因。'}
    >
      {#if hongKong}
        <StatGrid
          inline
          stats={[
            {
              label: '净利润区间',
              value: amountRange(hongKong.profit_lower, hongKong.profit_upper, hongKong.currency || '')
            },
            {
              label: '同比区间',
              value: pctRange(hongKong.growth_lower_pct, hongKong.growth_upper_pct),
              tone: sentimentTone(hongKong.sentiment)
            },
            {
              label: '上年同期',
              value: compact(hongKong.prior_profit, hongKong.currency || '')
            },
            { label: '每股收益', value: fixed(hongKong.eps, 4) },
            { label: '总股本', value: compact(hongKong.total_capital_shares, '股') },
            { label: '行业 / 币种', value: `${text(hongKong.industry)} · ${text(hongKong.currency)}` }
          ]}
        />

        <section class="prose">
          <h3>预告内容</h3>
          <p>{text(hongKong.contents)}</p>
          <h3>变动原因</h3>
          <p>{text(hongKong.reason)}</p>
        </section>
      {:else if industry}
        <StatGrid
          inline
          stats={[
            { label: '公司数 / 已预告', value: `${industry.company_count} / ${industry.forecast_count}` },
            { label: '覆盖率', value: percent(industry.coverage_pct) },
            { label: '预喜', value: `${industry.favorable_count} · ${percent(industry.favorable_pct)}`, tone: 'up' },
            { label: '不利', value: count(industry.adverse_count), tone: 'down' },
            { label: '客户端预喜率', value: percent(industry.client_favorable_pct) },
            { label: '上期收盘', value: fixed(industry.previous_period_close) }
          ]}
        />

        <ul class="events">
          {#each detail.data?.securities ?? [] as row, index (`${row.security.security_id}-${index}`)}
            <li>
              <div class="row">
                <time class="num">{date(row.forecast_date)}</time>
                <span class={sentimentTone(row.sentiment)}>{text(row.forecast_type)}</span>
              </div>
              <button class="entity" type="button" onclick={() => gotoWorkbench(row.security)}>
                {row.security.name || row.security.code}
                <span class="mute">{row.security.security_id}</span>
              </button>
              <p class="num">
                净利 {amountRange(row.profit_lower_yuan, row.profit_upper_yuan, '元')} · 同比
                {pctRange(row.growth_lower_pct, row.growth_upper_pct)}
              </p>
              <small>
                上年同期 {compact(row.prior_profit_yuan, '元')} · 每股收益 {fixed(row.eps, 4)} · 股本
                {compact(row.total_capital_shares, '股')}
              </small>
              {#if row.contents || row.reason}
                <details>
                  <summary>预告内容与变动原因</summary>
                  <p class="text">{text(row.contents)}</p>
                  <p class="text">{text(row.reason)}</p>
                </details>
              {/if}
            </li>
          {/each}
        </ul>

        {#if (detail.data?.securities.length ?? 0) === 0}
          <p class="warn-line">该行业在当前报告期没有逐股预告。</p>
        {/if}

        {#each detail.data?.detail_errors ?? [] as failure, index (index)}
          <p class="warn-line">{text(failure.resource)}：{failure.message}</p>
        {/each}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .prose {
    margin-top: var(--sp-3);
  }

  .prose h3 {
    margin-bottom: var(--sp-1);
    font-size: var(--fs-micro);
    font-weight: 600;
    color: var(--fg-dim);
  }

  .prose p {
    margin-bottom: var(--sp-3);
    font-size: 10px;
    line-height: 1.6;
    color: var(--fg-dim);
    white-space: pre-wrap;
    word-break: break-word;
  }

  .events {
    display: flex;
    flex-direction: column;
    gap: var(--sp-2);
    margin: var(--sp-3) 0 0;
    padding: 0;
    list-style: none;
  }

  .events li {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: var(--sp-2) var(--sp-3);
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .row {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: var(--sp-3);
    font-size: var(--fs-micro);
  }

  time {
    color: var(--fg-mute);
  }

  .row .up {
    color: var(--up);
  }

  .row .down {
    color: var(--down);
  }

  .row .flat {
    color: var(--fg-dim);
  }

  .entity {
    display: flex;
    align-items: baseline;
    gap: var(--sp-2);
    padding: 0;
    font-size: var(--fs-micro);
    font-weight: 500;
    line-height: var(--lh-tight);
    text-align: left;
  }

  .entity:hover {
    color: var(--focus);
  }

  .entity .mute {
    font-family: var(--font-num);
    font-size: 9px;
    color: var(--fg-mute);
  }

  .events p {
    font-size: var(--fs-micro);
    color: var(--fg-dim);
  }

  .events small {
    font-size: 10px;
    line-height: 1.4;
    color: var(--fg-mute);
  }

  details {
    margin-top: var(--sp-1);
    border-top: 1px solid var(--line);
  }

  summary {
    padding-top: var(--sp-1);
    font-size: 10px;
    color: var(--focus);
    cursor: pointer;
  }

  .text {
    margin-top: var(--sp-1);
    font-size: 10px;
    line-height: 1.6;
    color: var(--fg-dim);
    white-space: pre-wrap;
    word-break: break-word;
  }

  .warn-line {
    margin-top: var(--sp-2);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }
</style>
