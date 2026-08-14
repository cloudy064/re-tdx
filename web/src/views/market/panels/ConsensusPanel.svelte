<script lang="ts">
  /**
   * 一致预期与机构研报（YZYQ · 709 / 1721）。
   *
   * 一致预期主表给的是「汇总口径」（覆盖机构数、综合评级分、目标价均值），
   * 逐份研报给的是「个体口径」。两者经常打架——均值被一两家极端目标价拉偏，
   * 所以这里把汇总与逐份并列呈现，并保留每份研报的原始正文。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { DASH, count, date, fixed, num, percent, text, tone } from '../../../lib/fmt';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    ConsensusForecast,
    ConsensusReport,
    MarketConsensusDocument
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const consensus = new Resource<MarketConsensusDocument>();
  const doc = $derived(consensus.data);
  const record = $derived(doc?.selected_consensus ?? null);
  const summary = $derived(doc?.report_summary ?? null);
  const reports = $derived<ConsensusReport[]>(doc?.reports ?? []);

  function load(refresh = false) {
    void consensus.load(
      `/api/v1/market/consensus?${queryString({
        market,
        code,
        category: 'latest',
        report_limit: 100,
        include_report_text: 1,
        refresh: refresh ? 1 : 0
      })}`
    );
  }

  /**
   * 上游评级与评级变化都是自由文本（「买入」「上调至增持」「首次覆盖」……），
   * 没有枚举字段，只能按关键词判方向；判不出来时保持中性，不硬凑涨跌色。
   */
  function ratingTone(value: unknown): 'up' | 'down' | 'flat' {
    const label = value === null || value === undefined ? '' : String(value);
    if (/调高|上调|买入|增持|推荐|强烈|跑赢|优于|看好|超配/.test(label)) return 'up';
    if (/调低|下调|减持|卖出|回避|跑输|弱于|谨慎|低配/.test(label)) return 'down';
    return 'flat';
  }

  /** 逐年预测排成「T / T+1 / T+2」一串，年份放到副行，避免列数爆炸。 */
  function series(
    list: ConsensusForecast[],
    pick: (item: ConsensusForecast) => unknown,
    digits = 2
  ): string {
    if (!list.length) return DASH;
    return list.map((item) => fixed(pick(item), digits)).join(' / ');
  }

  function years(list: ConsensusForecast[]): string {
    if (!list.length) return '';
    return list.map((item) => (item.year === null ? DASH : String(item.year))).join(' / ');
  }

  /** 卡片里的逐年预测排成一行文字，避免在卡片内再嵌一张表。 */
  function forecastLine(list: ConsensusForecast[]): string {
    if (!list.length) return '该份研报没有逐年盈利预测';
    return list
      .map(
        (item) =>
          `${text(item.year)} EPS ${fixed(item.eps, 3)} · 净利 ${fixed(item.net_profit_yi)} 亿`
      )
      .join('；');
  }

  const overview = $derived.by<Stat[]>(() => {
    if (!doc) return [];
    const price = summary?.target_price ?? null;
    return [
      {
        label: '近六月研报',
        value: count(summary?.report_count ?? 0),
        note: `${count(summary?.institution_count ?? 0)} 家机构 · ${count(summary?.analyst_count ?? 0)} 名分析师`
      },
      {
        label: '目标价均值',
        value: fixed(price?.average),
        note: `${fixed(price?.minimum)} — ${fixed(price?.maximum)} · ${count(price?.count ?? 0)} 份给价`
      },
      {
        label: '主表一致目标价',
        value: fixed(record?.target_price),
        note: `基准年 ${text(record?.base_year)}`
      },
      {
        label: '综合评级分',
        value: fixed(record?.rating_score),
        note: `覆盖 ${count(record?.institution_count)} 家`
      },
      { label: 'PE / PEG', value: `${fixed(record?.pe)} / ${fixed(record?.peg)}` },
      {
        label: '预期 EPS 增速',
        value: percent(record?.expected_eps_growth_pct, 2, true),
        tone: tone(num(record?.expected_eps_growth_pct))
      },
      {
        label: '营收 CAGR',
        value: percent(record?.growth.revenue_cagr_pct, 2, true),
        tone: tone(num(record?.growth.revenue_cagr_pct))
      },
      {
        label: '净利 CAGR',
        value: percent(record?.growth.profit_cagr_pct, 2, true),
        tone: tone(num(record?.growth.profit_cagr_pct))
      },
      {
        label: '主表更新日',
        value: date(record?.latest_date),
        note: `最新研报 ${date(summary?.latest_report_date)}`
      }
    ];
  });

  /** 评级分布来自研报口径的计数字典，按份数倒序排，方便一眼看主流评级。 */
  const distribution = $derived<Array<[string, number]>>(
    Object.entries(summary?.rating_counts ?? {}).sort((left, right) => right[1] - left[1])
  );

  const forecastColumns: Column<ConsensusForecast>[] = [
    { key: 'year', label: '预测年度', width: '84px', num: true, value: (row) => text(row.year) },
    { key: 'eps', label: 'EPS', align: 'right', num: true, value: (row) => fixed(row.eps, 3) },
    { key: 'pe', label: 'PE', align: 'right', num: true, value: (row) => fixed(row.pe) },
    {
      key: 'profit',
      label: '净利润（亿元）',
      align: 'right',
      num: true,
      value: (row) => fixed(row.net_profit_yi)
    },
    {
      key: 'revenue',
      label: '营业收入（亿元）',
      align: 'right',
      num: true,
      value: (row) => fixed(row.revenue_yi)
    }
  ];

  const reportColumns: Column<ConsensusReport>[] = [
    {
      key: 'date',
      label: '报告日',
      width: '84px',
      num: true,
      value: (row) => date(row.report_date),
      sortValue: (row) => row.report_date ?? ''
    },
    {
      key: 'institution',
      label: '机构 / 分析师',
      wrap: true,
      value: (row) => text(row.institution),
      sub: (row) => `${text(row.analyst)} · ${text(row.institution_grade)}`
    },
    {
      key: 'rating',
      label: '评级',
      width: '78px',
      value: (row) => text(row.rating),
      sub: (row) => text(row.rating_change),
      tone: (row) => ratingTone(row.rating_change || row.rating)
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
      key: 'eps',
      label: '三年 EPS 预测',
      align: 'right',
      num: true,
      value: (row) => series(row.forecasts, (item) => item.eps, 3),
      sub: (row) => years(row.forecasts)
    },
    {
      key: 'profit',
      label: '三年净利（亿元）',
      align: 'right',
      num: true,
      value: (row) => series(row.forecasts, (item) => item.net_profit_yi)
    },
    {
      key: 'revenue',
      label: '三年营收（亿元）',
      align: 'right',
      num: true,
      value: (row) => series(row.forecasts, (item) => item.revenue_yi)
    },
    {
      key: 'text',
      label: '正文字节',
      align: 'right',
      num: true,
      value: (row) => count(row.report_text_length),
      sortValue: (row) => row.report_text_length ?? 0
    }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="一致预期摘要"
  eyebrow="YZYQ · 709 / 1721"
  subtitle={doc
    ? `${doc.category_label} · 主表缓存 ${doc.cache.master_age_seconds}s · 研报缓存 ${doc.cache.detail_age_seconds ?? 0}s`
    : '汇总口径与逐份研报并列，均值不替代个体判断'}
  busy={consensus.busy}
  error={consensus.error}
  onRetry={() => load()}
  empty={consensus.loaded && !consensus.busy && !record && reports.length === 0}
  emptyText="九张一致预期主表里没有这只股票，也没有近六月机构研报"
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={consensus.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  <StatGrid stats={overview} columns={5} />

  {#if distribution.length || doc?.category_memberships.length}
    <div class="tags">
      {#each distribution as [label, value] (label)}
        <Badge tone={ratingTone(label)}>{label} {count(value)}</Badge>
      {/each}
      {#each doc?.category_memberships ?? [] as item (item.category)}
        <Badge tone="focus">{item.category_label}</Badge>
      {/each}
    </div>
  {/if}

  {#if record?.forecasts.length}
    <h3 class="sub">主表逐年一致预测</h3>
    <DataTable
      columns={forecastColumns}
      rows={record.forecasts}
      rowKey={(row, index) => `${row.year ?? index}`}
    />
  {/if}
</Panel>

<Panel
  title="逐份机构研报"
  eyebrow="RESEARCH REPORTS"
  subtitle={`${count(doc?.counts.reports ?? 0)} 份 · 含正文 ${count(doc?.counts.full_reports ?? 0)} 份`}
  busy={consensus.busy}
  empty={!consensus.busy && !consensus.error && reports.length === 0}
  emptyText="当前股票没有近六月机构研报"
  flush
  scroll
>
  <DataTable
    columns={reportColumns}
    rows={reports}
    rowKey={(row, index) => `${row.report_date}-${index}`}
    maxHeight="260px"
    sortKey="date"
  />
</Panel>

<Panel
  title="研报理由与正文"
  eyebrow="REPORT TEXT"
  subtitle="长正文不进表格单元格，逐份卡片展开"
  busy={consensus.busy}
  empty={!consensus.busy && !consensus.error && reports.length === 0}
  emptyText="没有可展开的研报正文"
  scroll
>
  <ul class="events">
    {#each reports as report, index (`${report.report_date}-${index}`)}
      <li>
        <div class="row">
          <time class="num">{date(report.report_date)}</time>
          <span class={ratingTone(report.rating_change || report.rating)}>
            {text(report.rating)} · {text(report.rating_change)}
          </span>
        </div>
        <strong>{text(report.institution)}</strong>
        <p class="num">
          目标价 {fixed(report.target_price)} · {text(report.analyst)} · 基准年
          {text(report.base_year)}
        </p>
        <small>{forecastLine(report.forecasts)}</small>
        {#if report.report_text}
          <details open={index === 0}>
            <summary>展开研报正文（{count(report.report_text_length)} 字节）</summary>
            <pre>{report.report_text}</pre>
          </details>
        {:else}
          <small class="muted">该份研报没有返回正文。</small>
        {/if}
      </li>
    {/each}
  </ul>
</Panel>

<style>
  .tags {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-1);
    margin-top: var(--sp-3);
  }

  .sub {
    margin: var(--sp-4) 0 var(--sp-2);
    font-size: var(--fs-micro);
    font-weight: 600;
    color: var(--fg-dim);
  }

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

  .muted {
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

  pre {
    max-height: 260px;
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
</style>
