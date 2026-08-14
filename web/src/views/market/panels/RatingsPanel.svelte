<script lang="ts">
  /**
   * 机构评级（HYPJ + GGPJ · 709 / 1721）。
   *
   * A 股没有逐股评级表，只有 30 个一级研究行业的评级：单票先映射到自己的一级
   * 研究行业，再按行业动态键读近六月研报。港股走的是另一张表，给的是逐股目标价，
   * 且一律以港元计价——不做汇率折算，所以口径必须写在标签和单元格里。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { count, date, fixed, num, percent, text } from '../../../lib/fmt';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { MarketRatingsDocument, RatingReport, RatingStance } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  /** 港股在客户端里是市场号 31/48，路由上写成 hk。 */
  const HONG_KONG_MARKETS = new Set(['hk', '31', '48']);
  const isHongKong = $derived(HONG_KONG_MARKETS.has(market.trim().toLowerCase()));

  const ratings = new Resource<MarketRatingsDocument>();
  const doc = $derived(ratings.data);
  const industry = $derived(doc?.selected_industry ?? null);
  const hongKong = $derived(doc?.selected_hong_kong ?? null);
  const reports = $derived<RatingReport[]>(doc?.reports ?? []);
  const failures = $derived(doc?.detail_errors ?? []);
  const anchor = $derived(isHongKong ? hongKong : industry);

  function load(refresh = false) {
    const query = isHongKong
      ? queryString({
          view: 'hong-kong',
          market: 'hk',
          code,
          include_details: 1,
          detail_limit: 3000,
          refresh: refresh ? 1 : 0
        })
      : queryString({
          view: 'industries',
          market,
          code,
          include_details: 1,
          detail_limit: 5000,
          refresh: refresh ? 1 : 0
        });
    void ratings.load(`/api/v1/market/ratings?${query}`);
  }

  function stanceLabel(value: RatingStance): string {
    if (value === 'positive') return '看多';
    if (value === 'negative') return '看空';
    if (value === 'neutral') return '中性';
    return '未分类';
  }

  /** 看多用涨色、看空用跌色，与红涨绿跌一致；未分类不强行归入中性色。 */
  function stanceTone(value: RatingStance): 'up' | 'down' | 'flat' {
    if (value === 'positive') return 'up';
    if (value === 'negative') return 'down';
    return 'flat';
  }

  /** 评级变化是自由文本（「上调」「调低至中性」「维持」），按关键词判方向。 */
  function changeTone(value: unknown): 'up' | 'down' | 'flat' {
    const label = value === null || value === undefined ? '' : String(value);
    if (/调高|上调|提升|首次买入/.test(label)) return 'up';
    if (/调低|下调|降低/.test(label)) return 'down';
    return 'flat';
  }

  /** 港股研报目标价一律港元，口径写进单元格而不是只写在表头。 */
  function hkd(value: unknown): string {
    const parsed = num(value);
    return parsed === null ? text(null) : `HK$ ${fixed(parsed)}`;
  }

  const industryStats = $derived.by<Stat[]>(() => {
    if (!industry) return [];
    const ratio = industry.six_month_bullish_ratio;
    return [
      { label: '一级研究行业', value: industry.industry.name || industry.industry.code },
      {
        label: '行业代码',
        value: text(industry.industry.code),
        note: text(doc?.selected_security?.security_id)
      },
      {
        label: '最新评级',
        value: text(industry.latest_rating),
        tone: stanceTone(industry.stance),
        note: stanceLabel(industry.stance)
      },
      {
        label: '评级变化',
        value: text(industry.latest_rating_change),
        tone: changeTone(industry.latest_rating_change)
      },
      {
        label: '最新评级机构',
        value: text(industry.latest_institution),
        note: date(industry.latest_report_date)
      },
      { label: '近六月研报', value: count(industry.six_month_report_count) },
      { label: '看多研报', value: count(industry.six_month_bullish_count), tone: 'up' },
      { label: '看空研报', value: count(industry.six_month_bearish_count), tone: 'down' },
      {
        label: '看多率',
        // 上游给的是 0..1 的比值，换算成百分比再展示
        value: percent(ratio === null ? null : ratio * 100, 1),
        tone: (ratio ?? 0) >= 0.5 ? 'up' : 'down',
        note: `未分类 ${count(industry.six_month_unclassified_count)} 篇`
      }
    ];
  });

  const hongKongStats = $derived.by<Stat[]>(() => {
    if (!hongKong) return [];
    return [
      { label: '港股', value: hongKong.security.name || hongKong.security.code },
      {
        label: '最新评级',
        value: text(hongKong.latest_rating),
        tone: stanceTone(hongKong.stance),
        note: stanceLabel(hongKong.stance)
      },
      {
        label: '最新评级机构',
        value: text(hongKong.latest_institution),
        note: date(hongKong.latest_report_date)
      },
      {
        label: '最新目标价（港元）',
        value: hkd(hongKong.latest_target_price_hkd),
        note: '港元口径，不做汇率折算'
      },
      {
        label: '六月均目标价（港元）',
        value: hkd(hongKong.six_month_average_target_price_hkd),
        note: '港元口径，不做汇率折算'
      },
      { label: '近六月覆盖', value: count(hongKong.six_month_report_count) }
    ];
  });

  const reportColumns = $derived.by<Column<RatingReport>[]>(() => {
    const columns: Column<RatingReport>[] = [
      {
        key: 'date',
        label: '报告日',
        width: '84px',
        num: true,
        value: (row) => date(row.report_date),
        sortValue: (row) => row.report_date ?? ''
      },
      { key: 'institution', label: '评级机构', wrap: true, value: (row) => text(row.institution) },
      {
        key: 'rating',
        label: '评级',
        width: '84px',
        value: (row) => text(row.rating),
        sub: (row) => stanceLabel(row.stance),
        tone: (row) => stanceTone(row.stance)
      },
      { key: 'previous', label: '上次评级', width: '84px', value: (row) => text(row.previous_rating) },
      {
        key: 'change',
        label: '评级变化',
        width: '92px',
        value: (row) => text(row.rating_change),
        tone: (row) => changeTone(row.rating_change)
      }
    ];
    if (isHongKong) {
      columns.push({
        key: 'target',
        label: '目标价（港元 HK$）',
        align: 'right',
        num: true,
        value: (row) => hkd(row.target_price_hkd),
        sortValue: (row) => row.target_price_hkd ?? 0
      });
    }
    columns.push({
      key: 'reason',
      label: '理由字数',
      align: 'right',
      width: '76px',
      num: true,
      value: (row) => count(row.reason ? row.reason.length : 0),
      sortValue: (row) => (row.reason ? row.reason.length : 0)
    });
    return columns;
  });

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title={isHongKong ? '港股机构评级' : '所属行业机构评级'}
  eyebrow={isHongKong ? 'GGPJ · HK TARGET PRICE' : 'HYPJ · SECURITY → LEVEL-ONE INDUSTRY'}
  subtitle={isHongKong
    ? '目标价与均价均为港元（HK$），不做汇率折算'
    : 'A 股没有逐股评级表，单票先映射一级研究行业再取该行业近六月研报'}
  busy={ratings.busy}
  error={ratings.error}
  onRetry={() => load()}
  empty={ratings.loaded && !ratings.busy && !anchor}
  emptyText={isHongKong
    ? '港股评级主表里没有这只股票'
    : '当前证券没有一级研究行业评级映射'}
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={ratings.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  <StatGrid stats={isHongKong ? hongKongStats : industryStats} columns={5} />

  <div class="tags">
    {#if isHongKong}
      <Badge tone="warn">目标价口径：港元 HK$</Badge>
    {/if}
    {#if anchor}
      <Badge tone={stanceTone(anchor.stance)}>{stanceLabel(anchor.stance)}</Badge>
    {/if}
    <Badge tone="neutral">近六月 {count(reports.length)} 篇研报</Badge>
  </div>

  {#each failures as failure, index (index)}
    <p class="warn-line">{text(failure.resource)}：{failure.message}</p>
  {/each}
</Panel>

<Panel
  title="近六月评级研报"
  eyebrow="RATING REPORTS"
  subtitle={isHongKong
    ? '目标价单位为港元（HK$）'
    : '看多 / 看空沿用主表分类，未分类不强行算作中性'}
  busy={ratings.busy}
  empty={!ratings.busy && !ratings.error && reports.length === 0}
  emptyText="主表命中，但动态详情没有返回研报"
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
  title="研报理由"
  eyebrow="RATING RATIONALE"
  subtitle="研究理由是长文本，逐篇卡片展开，不压进表格单元格"
  busy={ratings.busy}
  empty={!ratings.busy && !ratings.error && reports.length === 0}
  emptyText="没有可展开的研报理由"
  scroll
>
  <ul class="events">
    {#each reports as report, index (`${report.report_date}-${index}`)}
      <li>
        <div class="row">
          <time class="num">{date(report.report_date)}</time>
          <span class={stanceTone(report.stance)}>
            {text(report.rating)} · {stanceLabel(report.stance)}
          </span>
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
          <details open={index === 0}>
            <summary>展开研究理由</summary>
            <pre>{report.reason}</pre>
          </details>
        {:else}
          <small>该条研报没有返回研究理由正文。</small>
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

  .warn-line {
    margin-top: var(--sp-2);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
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
