<script lang="ts">
  /**
   * 评级雷达。
   *
   * 港股目标价与一级研究行业评级是两套完全不同的口径（前者是港元绝对价格，
   * 后者是看多/看空计数），所以分两个视图而不是硬拼成一张表；右栏统一按动态键
   * 展开近六月研报理由。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, fixed, num, percent, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    HongKongRating,
    IndustryRating,
    MarketRatingsDocument,
    RatingStance
  } from '../../types';

  type View = 'hong-kong' | 'industries';

  const VIEWS = [
    { id: 'hong-kong', label: '港股评级', hint: '目标价（港元）· 最新评级 · 近六月覆盖' },
    { id: 'industries', label: '行业评级', hint: '30 个一级研究行业的看多 / 看空' }
  ];

  const STANCES = [
    { id: 'all', label: '全部评级' },
    { id: 'positive', label: '看多' },
    { id: 'neutral', label: '中性' },
    { id: 'negative', label: '看空' },
    { id: 'unknown', label: '未分类' }
  ];

  let view = $state<View>('hong-kong');
  let stance = $state('all');
  let query = $state('');

  const catalog = new Resource<MarketRatingsDocument>();
  const detail = new Resource<MarketRatingsDocument>();

  const doc = $derived(catalog.data);
  const isHongKong = $derived(view === 'hong-kong');
  const rows = $derived(isHongKong ? (doc?.hong_kong ?? []) : (doc?.industries ?? []));

  function stanceLabel(value: RatingStance): string {
    if (value === 'positive') return '看多';
    if (value === 'negative') return '看空';
    if (value === 'neutral') return '中性';
    return '未分类';
  }

  /** 看多用涨色、看空用跌色，与 A 股红涨绿跌一致。 */
  function stanceTone(value: RatingStance): 'up' | 'down' | 'flat' | '' {
    if (value === 'positive') return 'up';
    if (value === 'negative') return 'down';
    return 'flat';
  }

  /** 港股研报目标价一律港元，标注写进单元格而不是只写在表头。 */
  function hkd(value: unknown): string {
    const parsed = num(value);
    return parsed === null ? text(null) : `HK$ ${fixed(parsed)}`;
  }

  const hkSummary = $derived(doc?.hong_kong_summary ?? null);
  const industrySummary = $derived(doc?.industry_summary ?? null);

  const stats = $derived.by(() => {
    if (isHongKong) {
      if (!hkSummary) return [];
      return [
        {
          label: '覆盖港股',
          value: count(hkSummary.securities),
          note: `${date(hkSummary.earliest_latest_report_date)} — ${date(hkSummary.latest_report_date)}`
        },
        { label: '近六月研报', value: count(hkSummary.six_month_reports), note: '按股票主表覆盖数合计' },
        { label: '最新评级看多', value: count(hkSummary.positive_latest_ratings), tone: 'up' as const },
        { label: '最新评级看空', value: count(hkSummary.negative_latest_ratings), tone: 'down' as const },
        {
          label: '中性 / 未分类',
          value: `${count(hkSummary.neutral_latest_ratings)} / ${count(hkSummary.unknown_latest_ratings)}`
        },
        {
          label: '有目标价（港元）',
          value: count(hkSummary.latest_target_prices),
          note: `六月均价覆盖 ${count(hkSummary.average_target_prices)} 只`
        }
      ];
    }
    if (!industrySummary) return [];
    return [
      {
        label: '一级研究行业',
        value: count(industrySummary.industries),
        note: `${date(industrySummary.earliest_latest_report_date)} — ${date(industrySummary.latest_report_date)}`
      },
      { label: '近六月研报', value: count(industrySummary.six_month_reports), note: '行业主表报告总数' },
      { label: '看多研报', value: count(industrySummary.six_month_bullish_reports), tone: 'up' as const },
      { label: '看空研报', value: count(industrySummary.six_month_bearish_reports), tone: 'down' as const },
      {
        label: '未分类',
        value: count(industrySummary.six_month_unclassified_reports),
        note: '总数减看多与看空，不强行归入中性'
      },
      { label: '最新报告日', value: date(industrySummary.latest_report_date) }
    ];
  });

  const hongKongColumns: Column<HongKongRating>[] = [
    {
      key: 'security',
      label: '港股',
      width: '132px',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => row.security.security_id
    },
    {
      key: 'rating',
      label: '最新评级',
      width: '92px',
      value: (row) => text(row.latest_rating),
      sub: (row) => stanceLabel(row.stance),
      tone: (row) => stanceTone(row.stance)
    },
    { key: 'institution', label: '评级机构', wrap: true, value: (row) => text(row.latest_institution) },
    {
      key: 'target',
      label: '最新目标价（港元）',
      align: 'right',
      num: true,
      value: (row) => hkd(row.latest_target_price_hkd),
      sortValue: (row) => row.latest_target_price_hkd ?? 0
    },
    {
      key: 'average',
      label: '六月均目标价（港元）',
      align: 'right',
      num: true,
      value: (row) => hkd(row.six_month_average_target_price_hkd),
      sortValue: (row) => row.six_month_average_target_price_hkd ?? 0
    },
    {
      key: 'reports',
      label: '近六月覆盖',
      align: 'right',
      num: true,
      value: (row) => count(row.six_month_report_count),
      sortValue: (row) => row.six_month_report_count ?? 0
    },
    {
      key: 'date',
      label: '最新报告日',
      width: '90px',
      num: true,
      value: (row) => date(row.latest_report_date),
      sortValue: (row) => row.latest_report_date ?? ''
    }
  ];

  const industryColumns: Column<IndustryRating>[] = [
    {
      key: 'industry',
      label: '一级研究行业',
      width: '140px',
      value: (row) => row.industry.name || row.industry.code,
      sub: (row) => row.industry.code
    },
    {
      key: 'rating',
      label: '最新评级',
      width: '92px',
      value: (row) => text(row.latest_rating),
      sub: (row) => stanceLabel(row.stance),
      tone: (row) => stanceTone(row.stance)
    },
    { key: 'change', label: '评级变化', width: '92px', value: (row) => text(row.latest_rating_change) },
    { key: 'institution', label: '评级机构', wrap: true, value: (row) => text(row.latest_institution) },
    {
      key: 'reports',
      label: '近六月研报',
      align: 'right',
      num: true,
      value: (row) => count(row.six_month_report_count),
      sortValue: (row) => row.six_month_report_count ?? 0
    },
    {
      key: 'bullish',
      label: '看多',
      align: 'right',
      width: '64px',
      num: true,
      value: (row) => count(row.six_month_bullish_count),
      tone: () => 'up',
      sortValue: (row) => row.six_month_bullish_count ?? 0
    },
    {
      key: 'bearish',
      label: '看空',
      align: 'right',
      width: '64px',
      num: true,
      value: (row) => count(row.six_month_bearish_count),
      tone: () => 'down',
      sortValue: (row) => row.six_month_bearish_count ?? 0
    },
    {
      key: 'unclassified',
      label: '未分类',
      align: 'right',
      width: '68px',
      num: true,
      value: (row) => count(row.six_month_unclassified_count),
      sortValue: (row) => row.six_month_unclassified_count ?? 0
    },
    {
      key: 'ratio',
      label: '看多率',
      align: 'right',
      width: '76px',
      num: true,
      // 上游给的是 0..1 的比值，换算成百分比再展示
      value: (row) => percent(row.six_month_bullish_ratio === null ? null : row.six_month_bullish_ratio * 100, 1),
      tone: (row) => ((row.six_month_bullish_ratio ?? 0) >= 0.5 ? 'up' : 'down'),
      sortValue: (row) => row.six_month_bullish_ratio ?? 0
    },
    {
      key: 'date',
      label: '最新报告日',
      width: '90px',
      num: true,
      value: (row) => date(row.latest_report_date),
      sortValue: (row) => row.latest_report_date ?? ''
    }
  ];

  function load(refresh = false) {
    void catalog.load(
      `/api/v1/market/ratings?${queryString({
        view,
        stance,
        q: query.trim(),
        limit: 5000,
        refresh: refresh ? 1 : 0
      })}`
    );
    detail.reset();
  }

  function switchView(next: string) {
    view = next as View;
    stance = 'all';
    query = '';
    load();
  }

  function openHongKong(row: HongKongRating) {
    void detail.load(
      `/api/v1/market/ratings?${queryString({
        view: 'hong-kong',
        market: 'hk',
        code: row.security.code,
        include_details: 1,
        detail_limit: 3000
      })}`
    );
  }

  function openIndustry(row: IndustryRating) {
    void detail.load(
      `/api/v1/market/ratings?${queryString({
        view: 'industries',
        industry: row.industry.code,
        include_details: 1,
        detail_limit: 5000
      })}`
    );
  }

  const selectedHongKong = $derived(detail.data?.selected_hong_kong ?? null);
  const selectedIndustry = $derived(detail.data?.selected_industry ?? null);
  const reports = $derived(detail.data?.reports ?? []);
  const failures = $derived(detail.data?.detail_errors ?? []);

  // 空态不能吞掉 detail_errors——动态键取不到时那条提示就是唯一有效信息
  const asideEmpty = $derived(
    detail.busy ? false : !detail.loaded || (reports.length === 0 && failures.length === 0)
  );

  const asideTitle = $derived(
    selectedHongKong
      ? selectedHongKong.security.name || selectedHongKong.security.security_id
      : selectedIndustry
        ? selectedIndustry.industry.name || selectedIndustry.industry.code
        : '研报理由'
  );

  onMount(() => load());
</script>

<PageHeader
  eyebrow="GGPJ + HYPJ · 709/1721"
  title="评级雷达"
  description="港股机构目标价（港元口径）与 30 个一级研究行业的看多 / 看空统计分开呈现，点击实体按动态键读取近六月研报正文。"
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
      empty={catalog.loaded && !catalog.busy && rows.length === 0}
      emptyText="当前评级口径与检索条件下没有记录。"
      onRetry={() => load()}
      title={isHongKong ? '港股评级与目标价' : '一级研究行业评级'}
      subtitle={isHongKong
        ? `${count(doc?.counts.hong_kong ?? 0)} 只 · 目标价单位为港元（HK$）`
        : `${count(doc?.counts.industries ?? 0)} 个行业 · 看多 / 看空沿用主表分类`}
    >
      {#snippet toolbar()}
        <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="评级视图" />
        <Select
          value={stance}
          options={STANCES}
          label="口径"
          width="140px"
          onChange={(next) => {
            stance = next;
            load();
          }}
        />
        <TextInput
          bind:value={query}
          icon="search"
          width="240px"
          label="检索当前视图"
          placeholder={isHongKong ? '港股代码、名称或评级机构' : '行业代码、名称或评级机构'}
          onEnter={() => load()}
        />
      {/snippet}

      {#if isHongKong}
        <DataTable
          columns={hongKongColumns}
          rows={doc?.hong_kong ?? []}
          stickyFirst
          rowKey={(row) => row.security.security_id}
          onRowClick={openHongKong}
          isActive={(row) => row.security.security_id === selectedHongKong?.security.security_id}
        />
      {:else}
        <DataTable
          columns={industryColumns}
          rows={doc?.industries ?? []}
          stickyFirst
          rowKey={(row) => row.industry.code}
          onRowClick={openIndustry}
          isActive={(row) => row.industry.code === selectedIndustry?.industry.code}
        />
      {/if}
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="RESEARCH REPORTS"
      title={asideTitle}
      subtitle={detail.loaded ? `${count(reports.length)} 篇近六月研报` : '点击左侧任意行展开'}
      busy={detail.busy}
      error={detail.error}
      empty={asideEmpty}
      emptyText={detail.loaded
        ? '该实体的动态键没有返回研报正文。'
        : isHongKong
          ? '点击左侧港股，读取机构、评级变化与港元目标价明细。'
          : '点击左侧行业，读取机构评级、评级变化与研究理由。'}
    >
      <ul class="events">
        {#each reports as report, index (index)}
          <li>
            <div class="row">
              <time class="num">{date(report.report_date)}</time>
              <span class={stanceTone(report.stance)}>{text(report.rating)}</span>
            </div>
            <strong>{text(report.institution)}</strong>
            <p class="num">
              {#if isHongKong}
                目标价 {hkd(report.target_price_hkd)} · 上次评级 {text(report.previous_rating)}
              {:else}
                评级变化 {text(report.rating_change)} · 上次评级 {text(report.previous_rating)}
              {/if}
            </p>
            {#if report.reason}
              <details>
                <summary>展开研究理由</summary>
                <pre>{report.reason}</pre>
              </details>
            {:else}
              <small>该条研报没有返回研究理由正文。</small>
            {/if}
          </li>
        {/each}
      </ul>

      {#each failures as failure, index (index)}
        <p class="warn-line">{failure.resource ?? '详情'}：{failure.message}</p>
      {/each}

      {#if detail.loaded}
        <p class="note">
          {isHongKong
            ? '港股目标价与均价均为港元（HK$），不做汇率折算；港股代码为五位，暂不接入个股工作台。'
            : '行业「看多 / 看空」沿用通达信主表分类，未分类研报不强行归入中性。'}
        </p>
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .events {
    display: flex;
    flex-direction: column;
    gap: var(--sp-2);
    margin: 0;
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

  .events strong {
    font-size: var(--fs-micro);
    font-weight: 500;
    line-height: var(--lh-tight);
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
  }

  summary {
    font-size: 10px;
    color: var(--focus);
    cursor: pointer;
  }

  pre {
    max-height: 240px;
    margin: var(--sp-2) 0 0;
    padding: var(--sp-2);
    overflow: auto;
    font-family: var(--font-ui);
    font-size: 10px;
    line-height: 1.6;
    color: var(--fg-dim);
    white-space: pre-wrap;
    word-break: break-word;
    background: var(--bg-panel);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .warn-line {
    margin-top: var(--sp-2);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }

  .note {
    margin-top: var(--sp-3);
    font-size: 10px;
    line-height: 1.5;
    color: var(--fg-mute);
  }
</style>
