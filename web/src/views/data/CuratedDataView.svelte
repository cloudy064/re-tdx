<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, money, percent, text, tone } from '../../lib/fmt';
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
  import type { CuratedDataRecord, MarketCuratedDataDocument } from '../../types';

  type View = MarketCuratedDataDocument['view'];
  const VIEWS = [
    { id: 'all', label: '全部' },
    { id: 'low-valuation-smallcap', label: '低估袖珍' },
    { id: 'high-dividend', label: '高分红' },
    { id: 'below-book-soe', label: '破净国企' },
    { id: 'dividend-fundraising', label: '分红募资' },
    { id: 'high-refinancing-lending', label: '转融券' },
    { id: 'hk-performance', label: '港股表现' },
    { id: 'media-entertainment', label: '传媒娱乐' },
    { id: 'buyback-statistics', label: '拟回购统计' }
  ];

  let view = $state<View>('low-valuation-smallcap');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketCuratedDataDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);

  const stats = $derived.by<Stat[]>(() => {
    const summary = doc?.summary;
    if (!summary) return [];
    return [
      { label: '低估袖珍', value: count(summary.low_valuation_smallcap) },
      { label: '高分红', value: count(summary.high_dividend) },
      { label: '破净国企', value: count(summary.below_book_soe) },
      { label: '分红募资', value: count(summary.dividend_fundraising) },
      { label: '港股表现', value: count(summary.hk_performance) },
      { label: '覆盖证券', value: count(summary.unique_securities) }
    ];
  });

  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/curated-data?${queryString({
      view, q: query.trim(), limit: 5000, refresh: refresh ? 1 : 0
    })}`);
  }

  function switchView(next: string) {
    view = next as View;
    load();
  }

  function securityName(row: CuratedDataRecord): string {
    return row.security?.name || row.security?.code || '全市场';
  }

  function gotoWorkbench() {
    if (selected?.security && ['sz', 'sh', 'bj'].includes(selected.security.market)) {
      router.go(stockPath(selected.security.market, selected.security.code, 'curated-data'));
    }
  }

  function metric(row: CuratedDataRecord): string {
    if (row.kind === 'low-valuation-smallcap') return `PEG ${fixed(row.estimated_peg, 3)}`;
    if (row.kind === 'high-dividend') return percent(row.payout_ratio_pct);
    if (row.kind === 'below-book-soe') return `PB ${fixed(row.price_to_book_ratio, 3)}`;
    if (row.kind === 'dividend-fundraising') return fixed(row.dividend_fundraising_ratio, 2);
    if (row.kind === 'high-refinancing-lending') return money(row.refinancing_lending_balance_yuan);
    if (row.kind === 'hk-performance') return delta(row.change_5d_pct);
    if (row.kind === 'media-entertainment') return text(row.title);
    return money(row.planned_buyback_yuan);
  }

  const genericColumns: Column<CuratedDataRecord>[] = [
    { key: 'kind', label: '客户端功能', width: '142px', slot: true, sortValue: (row) => row.kind },
    { key: 'security', label: '证券 / 范围', width: '158px', value: securityName, sub: (row) => row.security?.security_id ?? 'A股全市场' },
    { key: 'date', label: '日期', width: '90px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'metric', label: '核心指标', align: 'right', num: true, value: metric },
    { key: 'source', label: '原始资源', width: '220px', value: (row) => row.source_resource }
  ];

  const lowColumns: Column<CuratedDataRecord>[] = [
    { key: 'security', label: '低估值袖珍股', width: '158px', value: securityName, sub: (row) => row.security?.security_id ?? '' },
    { key: 'date', label: '评级日期', width: '90px', num: true, value: (row) => date(row.rating_date) },
    { key: 'rating', label: '评级 / 机构', align: 'right', num: true, value: (row) => fixed(row.composite_rating, 2), sub: (row) => `${count(row.institution_count)} 家` },
    { key: 'pe', label: 'PE / PE-TTM', align: 'right', num: true, value: (row) => `${fixed(row.pe, 2)} / ${fixed(row.pe_ttm, 2)}` },
    { key: 'peg', label: 'PEG估算', align: 'right', num: true, value: (row) => fixed(row.estimated_peg, 3), sortValue: (row) => row.estimated_peg ?? Infinity },
    { key: 'growth', label: '预测EPS增长', align: 'right', num: true, value: (row) => percent(row.forecast_eps_growth_pct), tone: (row) => tone(row.forecast_eps_growth_pct) },
    { key: 'cap', label: '总市值', align: 'right', num: true, value: (row) => money(row.market_cap_yuan) },
    { key: 'target', label: '目标价', align: 'right', num: true, value: (row) => fixed(row.target_price, 2) }
  ];

  const returnColumns: Column<CuratedDataRecord>[] = [
    { key: 'security', label: '证券', width: '158px', value: securityName, sub: (row) => row.security?.security_id ?? '' },
    { key: 'dividend', label: '累计分红', align: 'right', num: true, value: (row) => money(row.cumulative_dividend_yuan), sub: (row) => `${count(row.dividend_count)} 次` },
    { key: 'raising', label: '累计募集', align: 'right', num: true, value: (row) => money(row.cumulative_fundraising_yuan), sub: (row) => `${count(row.fundraising_count)} 次` },
    { key: 'ratio', label: '分红募资比', align: 'right', num: true, value: (row) => fixed(row.dividend_fundraising_ratio, 2), sortValue: (row) => row.dividend_fundraising_ratio ?? -Infinity },
    { key: 'yield', label: '股息率', align: 'right', num: true, value: (row) => percent(row.dividend_yield_pct), sortValue: (row) => row.dividend_yield_pct ?? -Infinity }
  ];

  const highDividendColumns: Column<CuratedDataRecord>[] = [
    { key: 'security', label: '高分红证券', width: '158px', value: securityName, sub: (row) => row.security?.security_id ?? '' },
    { key: 'year', label: '年度', width: '90px', num: true, value: (row) => date(row.fiscal_year_end) },
    { key: 'dividend', label: '分红金额', align: 'right', num: true, value: (row) => money(row.dividend_yuan) },
    { key: 'profit', label: '最近年报净利润', align: 'right', num: true, value: (row) => money(row.latest_annual_profit_yuan) },
    { key: 'payout', label: '股利支付率', align: 'right', num: true, value: (row) => percent(row.payout_ratio_pct), sortValue: (row) => row.payout_ratio_pct ?? -Infinity }
  ];

  const soeColumns: Column<CuratedDataRecord>[] = [
    { key: 'security', label: '破净国企', width: '158px', value: securityName, sub: (row) => row.security?.security_id ?? '' },
    { key: 'date', label: '统计日期', width: '90px', num: true, value: (row) => date(row.snapshot_date) },
    { key: 'pb', label: '市净率', align: 'right', num: true, value: (row) => fixed(row.price_to_book_ratio, 3), sortValue: (row) => row.price_to_book_ratio ?? Infinity },
    { key: 'cap', label: '总市值 / 净资产', align: 'right', num: true, value: (row) => money(row.market_cap_yuan), sub: (row) => money(row.net_assets_yuan) },
    { key: 'holder', label: '控股股东', width: '220px', value: (row) => text(row.controlling_shareholder), sub: (row) => `${text(row.controlling_shareholder_nature)} · ${percent(row.controlling_shareholder_pct)}` },
    { key: 'controller', label: '实际控制人', width: '220px', value: (row) => text(row.actual_controller), sub: (row) => text(row.actual_controller_nature) }
  ];

  const lendingColumns: Column<CuratedDataRecord>[] = [
    { key: 'security', label: '转融券关注证券', width: '158px', value: securityName, sub: (row) => row.security?.security_id ?? '' },
    { key: 'date', label: '源交易日', width: '90px', num: true, value: (row) => date(row.trade_date) },
    { key: 'balance', label: '转融券余额', align: 'right', num: true, value: (row) => money(row.refinancing_lending_balance_yuan), sub: (row) => row.has_lending_data ? '有源数据' : '源字段为空' },
    { key: 'ratio', label: '余额占比', align: 'right', num: true, value: (row) => percent(row.balance_ratio_pct) },
    { key: 'cap', label: '流通 / 总市值', align: 'right', num: true, value: (row) => money(row.circulating_market_cap_yuan), sub: (row) => money(row.market_cap_yuan) }
  ];

  const hkColumns: Column<CuratedDataRecord>[] = [
    { key: 'security', label: '港股 / 产品', width: '158px', value: securityName, sub: (row) => `${row.security?.security_id ?? ''} · 源市场${row.security?.market_id ?? ''}` },
    { key: 'close', label: '快照收盘', align: 'right', num: true, value: (row) => fixed(row.close_price_hkd, 3), sub: (row) => date(row.snapshot_date) },
    { key: '5d', label: '5日', align: 'right', num: true, value: (row) => delta(row.change_5d_pct), tone: (row) => tone(row.change_5d_pct), sortValue: (row) => row.change_5d_pct ?? -Infinity },
    { key: '20d', label: '20日', align: 'right', num: true, value: (row) => delta(row.change_20d_pct), tone: (row) => tone(row.change_20d_pct) },
    { key: '60d', label: '60日', align: 'right', num: true, value: (row) => delta(row.change_60d_pct), tone: (row) => tone(row.change_60d_pct) },
    { key: 'month', label: '本月', align: 'right', num: true, value: (row) => delta(row.change_month_pct), tone: (row) => tone(row.change_month_pct) },
    { key: 'ytd', label: '年初至今', align: 'right', num: true, value: (row) => delta(row.change_ytd_pct), tone: (row) => tone(row.change_ytd_pct) },
    { key: 'turnover', label: '5日成交额', align: 'right', num: true, value: (row) => money(row.turnover_5d_yuan) }
  ];

  const mediaColumns: Column<CuratedDataRecord>[] = [
    { key: 'security', label: '参与公司', width: '158px', value: securityName, sub: (row) => row.security?.security_id ?? '' },
    { key: 'title', label: '片名 / 节目', width: '220px', value: (row) => text(row.title) },
    { key: 'date', label: '上映日期', width: '90px', num: true, value: (row) => date(row.release_date) },
    { key: 'movie', label: '电影', align: 'right', num: true, value: (row) => count(row.movie_count) },
    { key: 'drama', label: '电视剧', align: 'right', num: true, value: (row) => count(row.drama_count) },
    { key: 'variety', label: '综艺', align: 'right', num: true, value: (row) => count(row.variety_count) },
    { key: 'background', label: '参与背景', width: '320px', value: (row) => text(row.background_excerpt) }
  ];

  const buybackColumns: Column<CuratedDataRecord>[] = [
    { key: 'month', label: '拟回购月份', width: '100px', num: true, value: (row) => date(row.month) },
    { key: 'companies', label: '家数', align: 'right', num: true, value: (row) => count(row.company_count) },
    { key: 'shares', label: '拟回购数量', align: 'right', num: true, value: (row) => compact(row.planned_buyback_shares, '股') },
    { key: 'amount', label: '拟回购金额', align: 'right', num: true, value: (row) => money(row.planned_buyback_yuan), sortValue: (row) => row.planned_buyback_yuan ?? 0 },
    { key: 'market', label: '占总市值', align: 'right', num: true, value: (row) => percent(row.market_cap_ratio_pct, 3) },
    { key: 'float', label: '占流通市值', align: 'right', num: true, value: (row) => percent(row.circulating_market_cap_ratio_pct, 3) },
    { key: 'profit', label: '占年利润', align: 'right', num: true, value: (row) => percent(row.annual_profit_ratio_pct) }
  ];

  const columns = $derived.by<Column<CuratedDataRecord>[]>(() => {
    if (view === 'low-valuation-smallcap') return lowColumns;
    if (view === 'dividend-fundraising') return returnColumns;
    if (view === 'high-dividend') return highDividendColumns;
    if (view === 'below-book-soe') return soeColumns;
    if (view === 'high-refinancing-lending') return lendingColumns;
    if (view === 'hk-performance') return hkColumns;
    if (view === 'media-entertainment') return mediaColumns;
    if (view === 'buyback-statistics') return buybackColumns;
    return genericColumns;
  });

  const selectedStats = $derived.by<Stat[]>(() => {
    if (!selected) return [];
    if (selected.kind === 'low-valuation-smallcap') return [
      { label: 'PE', value: fixed(selected.pe, 2) },
      { label: 'PEG估算', value: fixed(selected.estimated_peg, 3) },
      { label: 'EPS增长', value: percent(selected.forecast_eps_growth_pct), tone: tone(selected.forecast_eps_growth_pct) },
      { label: '目标价', value: fixed(selected.target_price, 2) }
    ];
    if (selected.kind === 'dividend-fundraising') return [
      { label: '累计分红', value: money(selected.cumulative_dividend_yuan) },
      { label: '累计募集', value: money(selected.cumulative_fundraising_yuan) },
      { label: '分红募资比', value: fixed(selected.dividend_fundraising_ratio, 2) },
      { label: '股息率', value: percent(selected.dividend_yield_pct) }
    ];
    if (selected.kind === 'below-book-soe') return [
      { label: '市净率', value: fixed(selected.price_to_book_ratio, 3) },
      { label: '总市值', value: money(selected.market_cap_yuan) },
      { label: '净资产', value: money(selected.net_assets_yuan) },
      { label: '控股比例', value: percent(selected.controlling_shareholder_pct) }
    ];
    if (selected.kind === 'hk-performance') return [
      { label: '5日', value: delta(selected.change_5d_pct), tone: tone(selected.change_5d_pct) },
      { label: '20日', value: delta(selected.change_20d_pct), tone: tone(selected.change_20d_pct) },
      { label: '60日', value: delta(selected.change_60d_pct), tone: tone(selected.change_60d_pct) },
      { label: '年初至今', value: delta(selected.change_ytd_pct), tone: tone(selected.change_ytd_pct) }
    ];
    if (selected.kind === 'buyback-statistics') return [
      { label: '拟回购金额', value: money(selected.planned_buyback_yuan) },
      { label: '拟回购家数', value: count(selected.company_count) },
      { label: '占总市值', value: percent(selected.market_cap_ratio_pct, 3) },
      { label: '占年利润', value: percent(selected.annual_profit_ratio_pct) }
    ];
    return [
      { label: '日期', value: date(selected.date) },
      { label: '核心指标', value: metric(selected) },
      { label: '源市场', value: selected.security ? String(selected.security.market_id) : '全市场' },
      { label: '原始字段', value: count(Object.keys(selected.raw).length) }
    ];
  });

  onMount(() => load());
</script>

<PageHeader
  eyebrow="TDX CLIENT CURATED · 8 SOURCES"
  title="通达信客户端精选数据"
  description="还原客户端内置的低估值、高分红、破净国企、转融券风险、传媒项目、港股表现、分红募资和拟回购统计；静态快照不会伪装成实时行情。"
  {stats}
>
  {#snippet actions()}
    <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="客户端精选视图" />
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="410px">
  {#snippet main()}
    <Panel title="客户端精选主表" subtitle={doc ? `${count(doc.match_count)} 条 · ${count(doc.sources.length)} 个原始资源` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && !resource.busy && rows.length === 0} emptyText="当前筛选下没有命中记录。" flush scroll>
      {#snippet toolbar()}
        <TextInput bind:value={query} icon="search" width="260px" label="检索精选数据" placeholder="代码、名称、片名或控制人" onEnter={() => load()} />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedId = row.event_id)} isActive={(row) => row.event_id === selected?.event_id} stickyFirst numbered minWidth="1040px">
        {#snippet cell({ row })}<Badge tone={row.kind === 'high-refinancing-lending' ? 'warn' : row.kind === 'below-book-soe' ? 'focus' : 'neutral'}>{row.kind_label}</Badge>{/snippet}
      </DataTable>
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.kind_label ?? 'CURATED DATA'} title={selected ? securityName(selected) : '精选数据详情'} subtitle={selected ? selected.source_resource : '点击左侧记录展开'} empty={!selected && !resource.busy} emptyText="选择一条记录查看客户端口径和原始边界。" scroll>
      {#if selected}
        {#if selected.security && ['sz', 'sh', 'bj'].includes(selected.security.market)}
          <button class="jump" type="button" onclick={gotoWorkbench}>在证券工作台打开</button>
        {/if}
        <StatGrid stats={selectedStats} inline />
        {#if selected.kind === 'media-entertainment'}
          <h3>{text(selected.title)}</h3><p class="copy">{text(selected.background)}</p>
        {:else if selected.kind === 'below-book-soe'}
          <h3>控制关系</h3>
          <p>控股股东：{text(selected.controlling_shareholder)}（{text(selected.controlling_shareholder_nature)}，{percent(selected.controlling_shareholder_pct)}）</p>
          <p>实际控制人：{text(selected.actual_controller)}（{text(selected.actual_controller_nature)}，{percent(selected.actual_controller_pct)}）</p>
        {:else if selected.kind === 'low-valuation-smallcap'}
          <h3>预测边界</h3>
          <p>报告期 {date(selected.report_period)}，净利润 {money(selected.forecast_profit_lower_yuan)} — {money(selected.forecast_profit_upper_yuan)}，同比 {percent(selected.forecast_profit_growth_pct_lower)} — {percent(selected.forecast_profit_growth_pct_upper)}。</p>
          <p class="note">PEG 按客户端 PE / 预测 EPS 增长率复算；增长字段在 API 中以直观百分比点输出。</p>
        {:else if selected.kind === 'high-refinancing-lending'}
          <p class="warn">该资源最后日期为 {date(doc?.summary.refinancing_lending_latest_date)}，300 行中仅 {count(doc?.summary.valid_refinancing_lending_rows)} 行余额非空，属于历史风险快照。</p>
        {:else if selected.kind === 'buyback-statistics'}
          <p class="note">金额源字段为亿元、数量为万股；页面显示值已换算为元和股，三个占比严格复现客户端公式。</p>
        {:else if selected.kind === 'hk-performance'}
          <p class="note">市场号 31 为港股证券，49 为港股基金/产品；两类均保留，避免静默丢失 31 条产品记录。</p>
        {/if}
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
  .warn { margin-top: var(--sp-3); color: var(--warn); }
</style>
