<script lang="ts">
  /**
   * 一致预期与机构研报。左主表是九张客户端榜单，右栏是选中证券近六个月的逐份研报。
   * 评级、目标价、三年预测全部沿用通达信原始口径，不做归一化。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { Resource } from '../../lib/resource.svelte';
  import { count, date, fixed, num, percent, text, tone } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    ConsensusRecord,
    MarketConsensusDocument,
    ValuationSecurity
  } from '../../types';

  // 榜单 id 与客户端 YZYQ.sp 的九个业务视图一一对应。
  const CATEGORIES = [
    { id: 'latest', label: '最新预期', hint: '最新一致预期 func_yzyq101' },
    { id: 'rating-up', label: '评级调高', hint: '最新评级调高 func_yzyq102' },
    { id: 'rating-down', label: '评级调低', hint: '最新评级调低 func_yzyq108' },
    { id: 'first-rating', label: '首次评级', hint: '机构首次评级 func_yzyq103' },
    { id: 'revenue-growth', label: '营收复合增速', hint: 'Top100 func_yzyq104' },
    { id: 'profit-growth', label: '净利复合增速', hint: 'Top100 func_yzyq105' },
    { id: 'year-high-drawdown', label: '年高点回撤', hint: '年高点至今 func_yzyq106' },
    { id: 'consecutive-rise', label: '连续上涨', hint: '连涨天数 func_yzyq107' },
    { id: 'year-low-rise', label: '年低点涨幅', hint: '年低点至今 func_yzyq109' }
  ];

  let category = $state('latest');
  let query = $state('');

  const catalog = new Resource<MarketConsensusDocument>();
  const detail = new Resource<MarketConsensusDocument>();

  const doc = $derived(catalog.data);
  const selected = $derived(detail.data?.selected_security ?? null);
  const consensus = $derived(detail.data?.selected_consensus ?? null);
  const reportSummary = $derived(detail.data?.report_summary);
  const isGrowth = $derived(category === 'revenue-growth' || category === 'profit-growth');

  const stats = $derived.by(() => {
    if (!doc) return [];
    return [
      { label: '九榜合计', value: `${count(doc.counts.total_rows)} 行` },
      { label: '去重证券', value: count(doc.counts.unique_securities) },
      {
        label: '当前榜单',
        value: `${count(doc.counts.returned_records)} / ${count(doc.counts.category_rows)}`
      },
      { label: '榜单数量', value: count(doc.counts.categories) },
      { label: '选中证券', value: selected ? selected.security_id : '—' },
      { label: '近六月研报', value: count(detail.data?.counts.reports ?? 0) }
    ];
  });

  function load(refresh = false) {
    void catalog.load(
      `/api/v1/market/consensus?${queryString({
        category,
        q: query.trim(),
        limit: 500,
        include_report_text: 0,
        refresh: refresh ? 1 : 0
      })}`
    );
    detail.reset();
  }

  function openSecurity(record: ConsensusRecord) {
    void detail.load(
      `/api/v1/market/consensus?${queryString({
        category,
        market: record.security.market,
        code: record.security.code,
        report_limit: 100,
        include_report_text: 1
      })}`
    );
  }

  function gotoWorkbench(security: ValuationSecurity) {
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code));
  }

  /** 评级变化只有中文文本，按「上调 / 下调」词形映射到涨跌色。 */
  function ratingTone(change: string | null): 'up' | 'down' | 'flat' {
    const raw = change ?? '';
    if (/调高|上调|上升|升级|提高/.test(raw)) return 'up';
    if (/调低|下调|下降|降级|降低/.test(raw)) return 'down';
    return 'flat';
  }

  function forecastEps(record: ConsensusRecord, index: number): string {
    return fixed(record.forecasts[index]?.eps, 3);
  }

  const baseColumns: Column<ConsensusRecord>[] = [
    {
      key: 'security',
      label: '证券',
      width: '132px',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => `${row.security.security_id} · ${text(row.industry)}`
    },
    {
      key: 'date',
      label: '最新日期',
      width: '84px',
      num: true,
      value: (row) => date(row.latest_date)
    },
    {
      key: 'institutions',
      label: '机构数',
      align: 'right',
      num: true,
      value: (row) => count(row.institution_count),
      sortValue: (row) => num(row.institution_count) ?? 0
    },
    {
      key: 'rating',
      label: '综合评级',
      align: 'right',
      num: true,
      value: (row) => fixed(row.rating_score),
      tone: () => (category === 'rating-up' ? 'up' : category === 'rating-down' ? 'down' : ''),
      sortValue: (row) => num(row.rating_score) ?? 0
    },
    {
      key: 'target',
      label: '目标价',
      align: 'right',
      num: true,
      value: (row) => fixed(row.target_price),
      sortValue: (row) => num(row.target_price) ?? 0
    },
    {
      key: 'pe',
      label: 'PE',
      align: 'right',
      num: true,
      value: (row) => fixed(row.pe),
      sortValue: (row) => num(row.pe) ?? 0
    },
    {
      key: 'peg',
      label: 'PEG',
      align: 'right',
      num: true,
      value: (row) => fixed(row.peg),
      sortValue: (row) => num(row.peg) ?? 0
    },
    {
      key: 'eps-growth',
      label: '预期 EPS 增速',
      align: 'right',
      num: true,
      value: (row) => percent(row.expected_eps_growth_pct),
      tone: (row) => tone(row.expected_eps_growth_pct),
      sortValue: (row) => num(row.expected_eps_growth_pct) ?? 0
    },
    { key: 'eps-t', label: 'EPS T', align: 'right', num: true, value: (row) => forecastEps(row, 0) },
    {
      key: 'eps-t1',
      label: 'EPS T+1',
      align: 'right',
      num: true,
      value: (row) => forecastEps(row, 1)
    },
    {
      key: 'eps-t2',
      label: 'EPS T+2',
      align: 'right',
      num: true,
      value: (row) => forecastEps(row, 2)
    }
  ];

  const growthColumns: Column<ConsensusRecord>[] = [
    {
      key: 'revenue-cagr',
      label: '营收复合增速',
      align: 'right',
      num: true,
      value: (row) => percent(row.growth.revenue_cagr_pct),
      tone: (row) => tone(row.growth.revenue_cagr_pct),
      sortValue: (row) => num(row.growth.revenue_cagr_pct) ?? 0
    },
    {
      key: 'profit-cagr',
      label: '净利复合增速',
      align: 'right',
      num: true,
      value: (row) => percent(row.growth.profit_cagr_pct),
      tone: (row) => tone(row.growth.profit_cagr_pct),
      sortValue: (row) => num(row.growth.profit_cagr_pct) ?? 0
    }
  ];

  const stageColumns = $derived.by<Column<ConsensusRecord>[]>(() => {
    if (category === 'year-high-drawdown') return [
      { key: 'high', label: '近一年最高价', align: 'right', num: true, value: (row) => fixed(row.year_high_price), sortValue: (row) => num(row.year_high_price) ?? 0 },
      { key: 'close', label: '源表最新收盘', align: 'right', num: true, value: (row) => fixed(row.latest_close), sortValue: (row) => num(row.latest_close) ?? 0 },
      { key: 'from-high', label: '年高点至今', align: 'right', num: true, value: (row) => percent(row.change_from_year_high_pct), tone: (row) => tone(row.change_from_year_high_pct), sortValue: (row) => num(row.change_from_year_high_pct) ?? 0 }
    ];
    if (category === 'consecutive-rise') return [
      { key: 'days', label: '连涨天数', align: 'right', num: true, value: (row) => count(row.consecutive_rise_days), sortValue: (row) => num(row.consecutive_rise_days) ?? 0 },
      { key: 'from-high', label: '年高点至今', align: 'right', num: true, value: (row) => percent(row.change_from_year_high_pct), tone: (row) => tone(row.change_from_year_high_pct), sortValue: (row) => num(row.change_from_year_high_pct) ?? 0 },
      { key: 'close', label: '源表最新收盘', align: 'right', num: true, value: (row) => fixed(row.latest_close), sortValue: (row) => num(row.latest_close) ?? 0 }
    ];
    if (category === 'year-low-rise') return [
      { key: 'low', label: '近一年最低价', align: 'right', num: true, value: (row) => fixed(row.year_low_price), sortValue: (row) => num(row.year_low_price) ?? 0 },
      { key: 'close', label: '源表最新收盘', align: 'right', num: true, value: (row) => fixed(row.latest_close), sortValue: (row) => num(row.latest_close) ?? 0 },
      { key: 'from-low', label: '年低点至今', align: 'right', num: true, value: (row) => percent(row.change_from_year_low_pct), tone: (row) => tone(row.change_from_year_low_pct), sortValue: (row) => num(row.change_from_year_low_pct) ?? 0 }
    ];
    return [];
  });

  const columns = $derived(isGrowth ? [...baseColumns, ...growthColumns] : [...baseColumns, ...stageColumns]);
  const rows = $derived(doc?.records ?? []);

  onMount(() => load());
</script>

<PageHeader
  eyebrow="YZYQ · list/func_yzyq101…109_1.jsn"
  title="一致预期与机构研报"
  description="九张客户端榜单覆盖评级、增速、年内高低点与连涨证券，再展开近六个月逐份研报。价格阶段字段沿用源表时点，不跨日重算。"
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
      empty={catalog.loaded && !catalog.busy && rows.length === 0}
      emptyText="当前榜单与检索条件下没有一致预期记录。"
      title={doc?.category_label ?? CATEGORIES.find((item) => item.id === category)?.label ?? ''}
      subtitle={doc
        ? `${count(doc.counts.returned_records)} / ${count(doc.counts.category_rows)} 行`
        : ''}
    >
      {#snippet toolbar()}
        <Segmented
          options={CATEGORIES}
          value={category}
          ariaLabel="一致预期榜单"
          onChange={(next) => {
            category = next;
            query = '';
            load();
          }}
        />
        <TextInput
          bind:value={query}
          icon="search"
          width="220px"
          label="检索当前榜单"
          placeholder="股票名称、代码或行业"
          onEnter={() => load()}
        />
      {/snippet}

      <DataTable
        {columns}
        {rows}
        stickyFirst
        rowKey={(row) => row.security.security_id}
        onRowClick={openSecurity}
        isActive={(row) => row.security.security_id === selected?.security_id}
      />
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="SECURITY CONSENSUS"
      title={selected ? selected.name || selected.code : '机构研报'}
      subtitle={selected
        ? `${selected.security_id} · ${count(detail.data?.counts.reports ?? 0)} 份 · 最新 ${date(reportSummary?.latest_report_date)}`
        : '点击左侧任意证券展开'}
      busy={detail.busy}
      error={detail.error}
      empty={!detail.loaded && !detail.busy}
      emptyText="选择一只证券，读取它的机构覆盖与逐份研报。"
    >
      {#if selected}
        <button class="jump" type="button" onclick={() => gotoWorkbench(selected)}>
          在个股工作台打开 {selected.name || selected.code}
        </button>

        <StatGrid
          inline
          stats={[
            {
              label: '研报 / 机构',
              value: `${reportSummary?.report_count ?? 0} / ${reportSummary?.institution_count ?? 0}`
            },
            { label: '分析师', value: count(reportSummary?.analyst_count ?? 0) },
            { label: '目标价均值', value: fixed(reportSummary?.target_price?.average) },
            {
              label: '目标价区间',
              value: `${fixed(reportSummary?.target_price?.minimum)} — ${fixed(reportSummary?.target_price?.maximum)}`
            },
            { label: '综合评级', value: fixed(consensus?.rating_score) },
            { label: 'PEG / PE', value: `${fixed(consensus?.peg)} / ${fixed(consensus?.pe)}` }
          ]}
        />

        {#if consensus?.forecasts.length}
          <ul class="chips">
            {#each consensus.forecasts as forecast, index (index)}
              <li>
                <b class="num">{forecast.year ?? 'T'}</b>
                <span class="num">EPS {fixed(forecast.eps, 3)}</span>
                <small class="num">
                  净利 {fixed(forecast.net_profit_yi)} 亿 · 营收 {fixed(forecast.revenue_yi)} 亿
                </small>
              </li>
            {/each}
          </ul>
        {/if}

        {#if detail.data?.category_memberships.length}
          <div class="tags">
            {#each detail.data.category_memberships as membership (membership.category)}
              <Badge
                tone={membership.category === 'rating-down'
                  ? 'down'
                  : membership.category === 'rating-up'
                    ? 'up'
                    : 'neutral'}
              >
                {membership.category_label}
              </Badge>
            {/each}
          </div>
        {/if}

        <ul class="events">
          {#each detail.data?.reports ?? [] as report, index (`${report.report_date}-${index}`)}
            <li>
              <div class="row">
                <time class="num">{date(report.report_date)}</time>
                <span class={ratingTone(report.rating_change)}>
                  {text(report.rating)} · {text(report.rating_change)}
                </span>
              </div>
              <strong>{text(report.institution)}</strong>
              <p class="num">
                目标价 {fixed(report.target_price)} · {report.forecasts
                  .map((item) => `${item.year ?? 'T'} EPS ${fixed(item.eps, 3)}`)
                  .join(' · ') || '无预测'}
              </p>
              <small>
                {text(report.analyst)} · {text(report.institution_grade)} · 基准年
                {text(report.base_year)}
              </small>
              {#if report.report_text}
                <details>
                  <summary>研报正文 {count(report.report_text_length)} 字</summary>
                  <p class="text">{report.report_text}</p>
                </details>
              {/if}
            </li>
          {/each}
        </ul>

        {#if (detail.data?.reports.length ?? 0) === 0}
          <p class="warn-line">该证券近六个月没有返回机构研报。</p>
        {/if}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .jump {
    display: block;
    width: 100%;
    height: 22px;
    margin-bottom: var(--sp-3);
    font-size: var(--fs-micro);
    color: var(--focus);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  .jump:hover {
    background: var(--bg-hover);
  }

  .chips {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: var(--sp-1);
    margin: var(--sp-3) 0 0;
    padding: 0;
    list-style: none;
  }

  .chips li {
    display: flex;
    flex-direction: column;
    gap: 1px;
    padding: var(--sp-2);
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .chips b {
    font-size: 10px;
    font-weight: 500;
    color: var(--fg-mute);
  }

  .chips span {
    font-size: var(--fs-micro);
    color: var(--fg);
  }

  .chips small {
    font-size: 9px;
    line-height: 1.3;
    color: var(--fg-mute);
  }

  .tags {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-1);
    margin-top: var(--sp-3);
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
    border-top: 1px solid var(--line);
  }

  summary {
    padding-top: var(--sp-1);
    font-size: 10px;
    color: var(--focus);
    cursor: pointer;
  }

  .text {
    max-height: 240px;
    margin-top: var(--sp-1);
    overflow: auto;
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
