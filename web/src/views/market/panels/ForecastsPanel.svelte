<script lang="ts">
  /**
   * 业绩预告（YJYGTJ + GGYJYG）。
   *
   * A 股走「一级研究行业 + 当前报告期」的组合键：单票先映射行业，再在该行业本期
   * 详情里挑出自己那几行；港股（市场号 31/48）是另一套独立建模的表，币种、报告
   * 区间都由公告原样给出，因此单独走 hong-kong 视图。
   *
   * 净利润、同比、股本三个口径互不换算：净利润是金额（A 股人民币元、港股公告币种），
   * 同比是百分比，股本是股数。把它们统一成「亿元」会直接读错数量级。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { DASH, compact, count, date, fixed, num, percent, text } from '../../../lib/fmt';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { HongKongForecast, MarketForecastDocument, SecurityForecast } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  /** 港股在客户端里是市场号 31/48，路由上写成 hk。 */
  const HONG_KONG_MARKETS = new Set(['hk', '31', '48']);
  const isHongKong = $derived(HONG_KONG_MARKETS.has(market.trim().toLowerCase()));

  const forecasts = new Resource<MarketForecastDocument>();
  const doc = $derived(forecasts.data);
  const industry = $derived(doc?.selected_industry ?? null);

  const shares = $derived<SecurityForecast[]>(doc?.securities ?? []);
  // 港股视图只支持全文检索，命中的可能是别的票，按代码再收一次
  const hkShares = $derived<HongKongForecast[]>(
    (doc?.hong_kong ?? []).filter((row) => row.security.code === code)
  );
  const rowCount = $derived(isHongKong ? hkShares.length : shares.length);

  function load(refresh = false) {
    const query = isHongKong
      ? queryString({ view: 'hong-kong', q: code, limit: 500, refresh: refresh ? 1 : 0 })
      : queryString({
          view: 'securities',
          market,
          code,
          include_details: 1,
          detail_limit: 5000,
          refresh: refresh ? 1 : 0
        });
    void forecasts.load(`/api/v1/market/forecasts?${query}`);
  }

  /** 预告区间上下限任一缺失就退化成单值；单位由调用方按真实口径传入。 */
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
    if (left === null) return percent(right, 2, true);
    if (right === null || left === right) return percent(left, 2, true);
    return `${percent(left, 2, true)} — ${percent(right, 2, true)}`;
  }

  /** 预喜按涨色、预亏按跌色；不确定保持中性，不猜方向。 */
  function sentimentTone(sentiment: string): 'up' | 'down' | 'flat' {
    if (sentiment === 'negative') return 'down';
    if (sentiment === 'uncertain') return 'flat';
    return 'up';
  }

  function sentimentLabel(sentiment: string): string {
    if (sentiment === 'negative') return '负向';
    if (sentiment === 'uncertain') return '不确定';
    return '正向';
  }

  const industryStats = $derived.by<Stat[]>(() => {
    if (!industry) return [];
    return [
      { label: '一级研究行业', value: industry.industry.name || industry.industry.code },
      { label: '行业代码', value: text(industry.industry.code) },
      { label: '当前报告期', value: date(industry.report_period) },
      {
        label: '已预告 / 公司数',
        value: `${count(industry.forecast_count)} / ${count(industry.company_count)}`
      },
      { label: '行业覆盖率', value: percent(industry.coverage_pct) },
      {
        label: '预喜',
        value: count(industry.favorable_count),
        tone: 'up',
        note: percent(industry.favorable_pct)
      },
      { label: '不利', value: count(industry.adverse_count), tone: 'down' },
      { label: '上期收盘', value: fixed(industry.previous_period_close) }
    ];
  });

  const hkStats = $derived.by<Stat[]>(() => {
    const row = hkShares[0];
    if (!row) return [];
    return [
      { label: '最新公告日', value: date(row.forecast_date) },
      { label: '报告类型', value: text(row.report_type) },
      { label: '报告区间', value: `${date(row.start_date)} — ${date(row.end_date)}` },
      { label: '公告币种', value: text(row.currency) },
      { label: '所属行业', value: text(row.industry) },
      { label: '本期预告条数', value: count(hkShares.length) }
    ];
  });

  const shareColumns: Column<SecurityForecast>[] = [
    {
      key: 'period',
      label: '报告期',
      width: '86px',
      num: true,
      value: (row) => date(row.report_period),
      sub: (row) => `公告 ${date(row.forecast_date)}`
    },
    {
      key: 'type',
      label: '预告类型',
      width: '92px',
      wrap: true,
      value: (row) => text(row.forecast_type),
      sub: (row) => sentimentLabel(row.sentiment),
      tone: (row) => sentimentTone(row.sentiment)
    },
    {
      key: 'profit',
      label: '净利润区间（元）',
      align: 'right',
      num: true,
      value: (row) => amountRange(row.profit_lower_yuan, row.profit_upper_yuan, '元'),
      tone: (row) => sentimentTone(row.sentiment)
    },
    {
      key: 'growth',
      label: '同比区间',
      align: 'right',
      num: true,
      value: (row) => pctRange(row.growth_lower_pct, row.growth_upper_pct),
      tone: (row) => sentimentTone(row.sentiment)
    },
    {
      key: 'prior',
      label: '上年同期（元）',
      align: 'right',
      num: true,
      value: (row) => compact(row.prior_profit_yuan, '元')
    },
    {
      key: 'annualized',
      label: '年化净利（元）',
      align: 'right',
      num: true,
      value: (row) =>
        amountRange(row.annualized_profit_lower_yuan, row.annualized_profit_upper_yuan, '元')
    },
    {
      key: 'eps',
      label: '每股收益（元）',
      align: 'right',
      num: true,
      value: (row) => fixed(row.eps, 4)
    },
    {
      key: 'capital',
      label: '总股本（股）',
      align: 'right',
      num: true,
      value: (row) => compact(row.total_capital_shares, '股')
    }
  ];

  const hkColumns: Column<HongKongForecast>[] = [
    {
      key: 'period',
      label: '报告期',
      width: '104px',
      num: true,
      value: (row) => `${text(row.report_type)} · ${date(row.end_date)}`,
      sub: (row) => `公告 ${date(row.forecast_date)}`
    },
    {
      key: 'type',
      label: '预告类型',
      width: '92px',
      wrap: true,
      value: (row) => text(row.forecast_type),
      sub: (row) => sentimentLabel(row.sentiment),
      tone: (row) => sentimentTone(row.sentiment)
    },
    {
      key: 'profit',
      label: '净利润区间（公告币种）',
      align: 'right',
      num: true,
      value: (row) => amountRange(row.profit_lower, row.profit_upper, row.currency || ''),
      tone: (row) => sentimentTone(row.sentiment)
    },
    {
      key: 'growth',
      label: '同比区间',
      align: 'right',
      num: true,
      value: (row) => pctRange(row.growth_lower_pct, row.growth_upper_pct),
      tone: (row) => sentimentTone(row.sentiment)
    },
    {
      key: 'prior',
      label: '上年同期',
      align: 'right',
      num: true,
      value: (row) => compact(row.prior_profit, row.currency || '')
    },
    {
      key: 'annualized',
      label: '年化净利',
      align: 'right',
      num: true,
      value: (row) =>
        amountRange(row.annualized_profit_lower, row.annualized_profit_upper, row.currency || '')
    },
    { key: 'eps', label: '每股收益', align: 'right', num: true, value: (row) => fixed(row.eps, 4) },
    {
      key: 'capital',
      label: '总股本（股）',
      align: 'right',
      num: true,
      value: (row) => compact(row.total_capital_shares, '股')
    }
  ];

  /** 两套记录的正文字段同名不同类型，卡片列表只取共同的几项。 */
  interface ForecastNote {
    key: string;
    period: string;
    type: string;
    sentiment: string;
    contents: string;
    reason: string;
  }

  const notes = $derived<ForecastNote[]>(
    isHongKong
      ? hkShares.map((row, index) => ({
          key: `hk-${row.forecast_date}-${index}`,
          period: `${text(row.report_type)} · ${date(row.end_date)}`,
          type: text(row.forecast_type),
          sentiment: row.sentiment,
          contents: row.contents,
          reason: row.reason
        }))
      : shares.map((row, index) => ({
          key: `cn-${row.forecast_date}-${index}`,
          period: date(row.report_period),
          type: text(row.forecast_type),
          sentiment: row.sentiment,
          contents: row.contents,
          reason: row.reason
        }))
  );

  const failures = $derived(doc?.detail_errors ?? []);
  const headline = $derived(isHongKong ? (hkShares[0] ?? null) : (shares[0] ?? null));

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title={isHongKong ? '港股预告口径' : '所属研究行业与报告期'}
  eyebrow={isHongKong ? 'GGYJYG · HK 31/48' : 'YJYGTJ · INDUSTRY PERIOD → SECURITY'}
  subtitle={isHongKong
    ? '港股预告独立建模，金额保留公告原始币种'
    : '单票先映射一级研究行业，再用「行业代码 + 报告期」组合键读取本期详情'}
  busy={forecasts.busy}
  error={forecasts.error}
  onRetry={() => load()}
  empty={forecasts.loaded && !forecasts.busy && !industry && rowCount === 0}
  emptyText={isHongKong
    ? '港股预告表里没有这只股票的记录'
    : '当前证券没有一级研究行业映射，或该行业本期没有预告统计'}
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={forecasts.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  <StatGrid stats={isHongKong ? hkStats : industryStats} columns={4} />

  {#if headline}
    <div class="tags">
      <Badge tone={sentimentTone(headline.sentiment)}>
        {text(headline.forecast_type)} · {sentimentLabel(headline.sentiment)}
      </Badge>
      <Badge tone="neutral">{count(rowCount)} 条本期预告</Badge>
    </div>
  {/if}

  {#each failures as failure, index (index)}
    <p class="warn-line">{text(failure.resource)}：{failure.message}</p>
  {/each}
</Panel>

<Panel
  title="当前业绩预告"
  eyebrow="SECURITY FORECAST"
  subtitle="净利润按公告金额、同比按百分比、股本按股数，三个口径各自成列不做换算"
  busy={forecasts.busy}
  empty={!forecasts.busy && !forecasts.error && rowCount === 0}
  emptyText={isHongKong ? '该港股本期没有业绩预告' : '该行业本报告期的逐股详情里没有这只股票'}
  flush
  scroll
>
  {#if isHongKong}
    <DataTable
      columns={hkColumns}
      rows={hkShares}
      rowKey={(row, index) => `${row.forecast_date}-${index}`}
      maxHeight="240px"
    />
  {:else}
    <DataTable
      columns={shareColumns}
      rows={shares}
      rowKey={(row, index) => `${row.forecast_date}-${index}`}
      maxHeight="240px"
    />
  {/if}
</Panel>

<Panel
  title="预告内容与变动原因"
  eyebrow="ANNOUNCEMENT TEXT"
  subtitle="公告正文按条卡片展开，不压进表格单元格"
  busy={forecasts.busy}
  empty={!forecasts.busy && !forecasts.error && notes.length === 0}
  emptyText="没有可展开的预告正文"
  scroll
>
  <ul class="events">
    {#each notes as note (note.key)}
      <li>
        <div class="row">
          <time class="num">{note.period}</time>
          <span class={sentimentTone(note.sentiment)}>
            {note.type} · {sentimentLabel(note.sentiment)}
          </span>
        </div>
        <strong>预告内容</strong>
        <p>{text(note.contents)}</p>
        <strong>变动原因</strong>
        <p>{text(note.reason)}</p>
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
    margin-top: var(--sp-1);
    font-size: 10px;
    font-weight: 600;
    color: var(--fg-dim);
  }

  .events p {
    font-size: 10px;
    line-height: 1.6;
    color: var(--fg-dim);
    white-space: pre-wrap;
    word-break: break-word;
  }
</style>
