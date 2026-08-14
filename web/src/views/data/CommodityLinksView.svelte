<script lang="ts">
  /** 通达信 SPHQ/ZJTC：商品报价、涨价题材及其股票关系链。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, fixed, percent, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    CommodityDetailRecord,
    CommodityQuoteRecord,
    CommodityRelatedSecurityRecord,
    CommodityStockRecord,
    DriverDetailRecord,
    MarketCommodityLinksDocument,
    PriceThemeRecord,
    ThemeDetailRecord,
    ThemeDriverRecord,
    ThemeStockRecord,
    ValuationSecurity
  } from '../../types';

  const SECTIONS = [
    { id: 'commodities', label: '商品行情' },
    { id: 'themes', label: '涨价题材' }
  ];

  let section = $state<'commodities' | 'themes'>('commodities');
  let query = $state('');
  let selectedCommodity = $state<CommodityQuoteRecord | null>(null);
  let selectedTheme = $state<PriceThemeRecord | null>(null);
  let selectedDriver = $state<ThemeDriverRecord | null>(null);
  const masterResource = new Resource<MarketCommodityLinksDocument>();
  const detailResource = new Resource<MarketCommodityLinksDocument>();
  const driverResource = new Resource<MarketCommodityLinksDocument>();
  const doc = $derived(masterResource.data);
  const commodities = $derived(section === 'commodities'
    ? (doc?.records ?? []) as CommodityQuoteRecord[] : []);
  const themes = $derived(section === 'themes'
    ? (doc?.records ?? []) as PriceThemeRecord[] : []);
  const commodityDetail = $derived(section === 'commodities'
    ? (detailResource.data?.records[0] as CommodityDetailRecord | undefined) : undefined);
  const themeDetail = $derived(section === 'themes'
    ? (detailResource.data?.records[0] as ThemeDetailRecord | undefined) : undefined);
  const driverDetail = $derived(driverResource.data?.records[0] as DriverDetailRecord | undefined);

  const stats = $derived.by<Stat[]>(() => section === 'commodities'
    ? [
        { label: '商品报价', value: count(doc?.summary.quote_rows), note: '期货/现货报价行' },
        { label: '关联商品', value: count(doc?.summary.unique_commodity_ids), note: '股票关系选择键' },
        { label: '当前命中', value: count(doc?.counts.matched) },
        { label: '关联股票', value: count(commodityDetail?.counts.stocks) },
        { label: '行业 / ETF', value: count(commodityDetail?.counts.related_securities) },
        { label: '数据状态', value: doc?.availability === 'stale-cache' ? '缓存' : '在线' }
      ]
    : [
        { label: '涨价题材', value: count(doc?.summary.themes) },
        { label: '当前命中', value: count(doc?.counts.matched) },
        { label: '长期关联股', value: count(themeDetail?.counts.stocks) },
        { label: '历史驱动', value: count(themeDetail?.counts.drivers) },
        { label: '事件关联股', value: count(driverDetail?.stocks.length) },
        { label: '数据状态', value: doc?.availability === 'stale-cache' ? '缓存' : '在线' }
      ]);

  async function loadMaster(refresh = false) {
    detailResource.reset();
    driverResource.reset();
    selectedDriver = null;
    const result = await masterResource.load(`/api/v1/market/commodity-links?${queryString({
      view: section, q: query.trim(),
      sort: section === 'commodities' ? 'quote-date' : 'latest-driver-date',
      order: 'desc', limit: 500, refresh: refresh ? 1 : 0
    })}`);
    if (section === 'commodities') {
      const rows = (result?.records ?? []) as CommodityQuoteRecord[];
      selectedCommodity = rows[0] ?? null;
      selectedTheme = null;
      if (selectedCommodity) loadCommodity(selectedCommodity, refresh);
    } else {
      const rows = (result?.records ?? []) as PriceThemeRecord[];
      selectedTheme = rows[0] ?? null;
      selectedCommodity = null;
      if (selectedTheme) loadTheme(selectedTheme, refresh);
    }
  }

  function switchSection(next: string) {
    section = next as typeof section;
    query = '';
    void loadMaster();
  }

  function loadCommodity(row: CommodityQuoteRecord, refresh = false) {
    selectedCommodity = row;
    void detailResource.load(`/api/v1/market/commodity-links?${queryString({
      view: 'commodity', commodity_id: row.commodity_id,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function loadTheme(row: PriceThemeRecord, refresh = false) {
    selectedTheme = row;
    selectedDriver = null;
    driverResource.reset();
    void detailResource.load(`/api/v1/market/commodity-links?${queryString({
      view: 'theme', theme_id: row.theme_id,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function loadDriver(row: ThemeDriverRecord, refresh = false) {
    if (!selectedTheme) return;
    selectedDriver = row;
    void driverResource.load(`/api/v1/market/commodity-links?${queryString({
      view: 'driver', theme_id: selectedTheme.theme_id, driver_id: row.driver_id,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function openSecurity(security: ValuationSecurity) {
    if (!security.market || !security.code) return;
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code, 'commodity-links'));
  }

  const commodityColumns: Column<CommodityQuoteRecord>[] = [
    { key: 'name', label: '品种', width: '130px', value: (row) => row.name, sub: (row) => row.commodity_id, sortValue: (row) => row.name },
    { key: 'price', label: '最新价', width: '96px', align: 'right', num: true, value: (row) => fixed(row.latest_price, 2), sub: (row) => row.unit, sortValue: (row) => row.latest_price ?? 0 },
    { key: 'day', label: '当日', width: '72px', align: 'right', num: true, value: (row) => percent(row.day_change_pct), tone: (row) => tone(row.day_change_pct), sortValue: (row) => row.day_change_pct ?? 0 },
    { key: '5d', label: '5 日', width: '72px', align: 'right', num: true, value: (row) => percent(row.change_5d_pct), tone: (row) => tone(row.change_5d_pct), sortValue: (row) => row.change_5d_pct ?? 0 },
    { key: '10d', label: '10 日', width: '72px', align: 'right', num: true, value: (row) => percent(row.change_10d_pct), tone: (row) => tone(row.change_10d_pct), sortValue: (row) => row.change_10d_pct ?? 0 },
    { key: '30d', label: '30 日', width: '72px', align: 'right', num: true, value: (row) => percent(row.change_30d_pct), tone: (row) => tone(row.change_30d_pct), sortValue: (row) => row.change_30d_pct ?? 0 },
    { key: '60d', label: '60 日', width: '72px', align: 'right', num: true, value: (row) => percent(row.change_60d_pct), tone: (row) => tone(row.change_60d_pct), sortValue: (row) => row.change_60d_pct ?? 0 },
    { key: 'industry', label: '关联行业', width: '100px', value: (row) => row.associated_industry || '—' },
    { key: 'date', label: '报价日', width: '88px', num: true, value: (row) => date(row.quote_date), sortValue: (row) => row.quote_date }
  ];

  const themeColumns: Column<PriceThemeRecord>[] = [
    { key: 'name', label: '题材', width: '108px', value: (row) => row.name, sub: (row) => `ID ${row.theme_id}`, sortValue: (row) => row.name },
    { key: 'stocks', label: '关联股', width: '66px', align: 'right', num: true, value: (row) => count(row.stock_count), sortValue: (row) => row.stock_count },
    { key: 'driver', label: '最新驱动', width: '360px', wrap: true, value: (row) => row.latest_driver_title || '—', sub: (row) => date(row.latest_driver_date), sortValue: (row) => row.latest_driver_date },
    { key: 'start', label: '首次触发', width: '88px', num: true, value: (row) => date(row.trigger_date), sortValue: (row) => row.trigger_date },
    { key: 'commodity', label: '关联商品 ID', width: '122px', num: true, value: (row) => row.associated_commodity_id || '—' }
  ];

  const commodityStockColumns: Column<CommodityStockRecord>[] = [
    { key: 'security', label: '股票', width: '118px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'reference', label: '三月前复权价', width: '98px', align: 'right', num: true, value: (row) => fixed(row.reference_price_3m, 2) },
    { key: 'logic', label: '投资逻辑', width: '430px', wrap: true, value: (row) => row.investment_logic || row.description || '—' }
  ];
  const relatedColumns: Column<CommodityRelatedSecurityRecord>[] = [
    { key: 'security', label: '行业 / ETF', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id }
  ];
  const themeStockColumns: Column<ThemeStockRecord>[] = [
    { key: 'security', label: '股票', width: '118px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'price', label: '触发参考价', width: '88px', align: 'right', num: true, value: (row) => fixed(row.trigger_price, 2) },
    { key: 'content', label: '关联说明', width: '460px', wrap: true, value: (row) => row.content || '—' }
  ];
  const driverColumns: Column<ThemeDriverRecord>[] = [
    { key: 'date', label: '日期', width: '88px', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'title', label: '驱动事件', width: '380px', wrap: true, value: (row) => row.title },
    { key: 'stocks', label: '关联股', width: '64px', align: 'right', num: true, value: (row) => count(row.stock_count) }
  ];
  const driverStockColumns: Column<ThemeStockRecord>[] = [
    { key: 'security', label: '事件股票', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'price', label: '事件参考价', align: 'right', num: true, value: (row) => fixed(row.driver_price, 2) }
  ];

  onMount(() => void loadMaster());
</script>

<PageHeader eyebrow="709/1721 · SPHQ / ZJTC101—104" title="商股联动与涨价题材" description="商品多周期报价 → 关联股票/行业/ETF，以及涨价题材 → 历史驱动事件 → 事件股票的两条完整关系链。" {stats}>
  {#snippet actions()}
    <Segmented options={SECTIONS} value={section} onChange={switchSection} ariaLabel="商股联动分区" />
    <Button icon="refresh" busy={masterResource.busy} onclick={() => void loadMaster(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<div class="workspace">
  <Panel title={section === 'commodities' ? '商品报价主表' : '涨价题材主表'} subtitle={section === 'commodities' ? `${count(commodities.length)} 条报价 · 点击展开关联股票` : `${count(themes.length)} 个题材 · 点击展开驱动历史`} busy={masterResource.busy} error={masterResource.error} onRetry={() => void loadMaster()} empty={masterResource.loaded && (section === 'commodities' ? commodities.length === 0 : themes.length === 0)} emptyText="当前检索没有匹配记录。" flush scroll fill>
    {#snippet toolbar()}
      <TextInput bind:value={query} icon="search" width="240px" label="检索" placeholder="商品 / 行业 / 题材 / 驱动" onEnter={() => void loadMaster()} />
      <Button icon="search" onclick={() => void loadMaster()}>查询</Button>
    {/snippet}
    {#if section === 'commodities'}
      <DataTable columns={commodityColumns} rows={commodities} rowKey={(row) => row.quote_id} onRowClick={(row) => loadCommodity(row)} isActive={(row) => row.quote_id === selectedCommodity?.quote_id} stickyFirst sortKey="date" />
    {:else}
      <DataTable columns={themeColumns} rows={themes} rowKey={(row) => row.theme_id} onRowClick={(row) => loadTheme(row)} isActive={(row) => row.theme_id === selectedTheme?.theme_id} stickyFirst sortKey="driver" />
    {/if}
  </Panel>

  <div class="detail">
    {#if section === 'commodities'}
      <Panel title={commodityDetail?.quotes.map((row) => row.name).join(' / ') || selectedCommodity?.name || '商品关系'} subtitle={commodityDetail ? `${commodityDetail.commodity_id} · ${count(commodityDetail.quote_count)} 条共享关系的报价` : '从左侧选择商品'} busy={detailResource.busy} error={detailResource.error} onRetry={selectedCommodity ? () => loadCommodity(selectedCommodity!, true) : undefined} empty={!selectedCommodity || (detailResource.loaded && !commodityDetail)} emptyText="选择商品后查看关联股票、行业与 ETF。">
        {#if commodityDetail?.quotes.length}
          <div class="summary-copy">
            {#each commodityDetail.quotes as quote (quote.quote_id)}
              <span><strong>{quote.name}</strong> <i class={tone(quote.day_change_pct)}>{fixed(quote.latest_price, 2)} {quote.unit} · {percent(quote.day_change_pct)}</i></span>
            {/each}
            {#if commodityDetail.quotes[0].description}<p>{commodityDetail.quotes[0].description}</p>{/if}
          </div>
        {/if}
      </Panel>
      <div class="relation-grid">
        <Panel title="关联股票" subtitle="三月参考价来自 JSN；当前行情列不伪造" busy={detailResource.busy} flush scroll fill>
          <DataTable columns={commodityStockColumns} rows={commodityDetail?.stocks ?? []} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} stickyFirst />
        </Panel>
        <Panel title="行业 / ETF" subtitle="点击进入证券工作台" busy={detailResource.busy} flush scroll fill>
          <DataTable columns={relatedColumns} rows={commodityDetail?.related_securities ?? []} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} />
        </Panel>
      </div>
    {:else}
      <Panel title={themeDetail?.name || selectedTheme?.name || '题材关系'} subtitle={themeDetail ? `${date(themeDetail.trigger_date)} 首次触发 · 关联商品 ${themeDetail.associated_commodity_id || '—'}` : '从左侧选择题材'} busy={detailResource.busy} error={detailResource.error} onRetry={selectedTheme ? () => loadTheme(selectedTheme!, true) : undefined} empty={!selectedTheme || (detailResource.loaded && !themeDetail)} emptyText="选择题材后查看关联股与历史驱动。">
        {#if themeDetail}
          <div class="summary-copy"><strong>{themeDetail.latest_driver_title}</strong><span>{date(themeDetail.latest_driver_date)}</span><p>{themeDetail.logic}</p></div>
        {/if}
      </Panel>
      <div class="theme-grid">
        <Panel title="历史驱动" subtitle="点击事件精确展开当次关联股" busy={detailResource.busy} flush scroll fill>
          <DataTable columns={driverColumns} rows={themeDetail?.drivers ?? []} rowKey={(row) => row.driver_id} onRowClick={(row) => loadDriver(row)} isActive={(row) => row.driver_id === selectedDriver?.driver_id} sortKey="date" />
        </Panel>
        <Panel title={selectedDriver ? `${selectedDriver.title} · 事件股票` : '事件股票'} subtitle={selectedDriver ? date(selectedDriver.date) : '从左侧驱动历史中选择'} busy={driverResource.busy} error={driverResource.error} onRetry={selectedDriver ? () => loadDriver(selectedDriver!, true) : undefined} empty={!selectedDriver || (driverResource.loaded && !driverDetail?.stocks.length)} emptyText="选择一条历史驱动，查看该次事件精确关联的股票。" flush scroll fill>
          <DataTable columns={driverStockColumns} rows={driverDetail?.stocks ?? []} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} />
        </Panel>
      </div>
      <Panel title="题材长期关联股票" subtitle="点击进入个股工作台" busy={detailResource.busy} flush scroll fill>
        <DataTable columns={themeStockColumns} rows={themeDetail?.stocks ?? []} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} stickyFirst />
      </Panel>
    {/if}
  </div>
</div>

<style>
  .workspace { display: grid; min-height: 0; flex: 1; grid-template-columns: minmax(610px, 1.08fr) minmax(500px, .92fr); gap: var(--sp-2); }
  .detail { display: flex; min-height: 0; flex-direction: column; gap: var(--sp-2); overflow: hidden; }
  .relation-grid { display: grid; min-height: 0; flex: 1; grid-template-columns: minmax(0, 1fr) minmax(180px, .34fr); gap: var(--sp-2); }
  .theme-grid { display: grid; min-height: 190px; flex: .7; grid-template-columns: minmax(0, 1fr) minmax(210px, .45fr); gap: var(--sp-2); }
  .summary-copy { display: flex; flex-wrap: wrap; align-items: baseline; gap: var(--sp-1) var(--sp-3); font-size: var(--fs-micro); color: var(--fg-dim); }
  .summary-copy strong { color: var(--fg); }
  .summary-copy i { font-style: normal; }
  .summary-copy p { width: 100%; max-height: 4.5em; overflow: auto; line-height: 1.5; }
  @media (max-width: 1180px) { .workspace { grid-template-columns: 1fr; overflow: auto; } .detail { min-height: 680px; } }
</style>
