<script lang="ts">
  /** 7727 option directory + expiry rules + chain/IV/Greeks reconstruction. */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, percent, price, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    OptionCatalogDocument,
    OptionChainDocument,
    OptionChainLeg,
    OptionChainStrike,
    OptionExpiryDocument,
    OptionInstrument,
    OptionMarketKey,
    OptionVolatilityDocument
  } from '../../types';

  type Stage = 'catalog' | 'chain';

  const MARKETS = [
    { id: 'all', label: '全部交易所' },
    { id: 'czce', label: '郑商所' },
    { id: 'dce', label: '大商所' },
    { id: 'shfe', label: '上期所' },
    { id: 'cffex', label: '中金所' },
    { id: 'gfex', label: '广期所' }
  ];
  const TYPES = [
    { id: 'all', label: '看涨 + 看跌' },
    { id: 'call', label: '仅看涨' },
    { id: 'put', label: '仅看跌' }
  ];
  const LIMITS = [200, 500, 1000, 5000].map((value) => ({
    id: String(value), label: `最多 ${value} 条`
  }));
  const MARKET_KEYS: Record<number, Exclude<OptionMarketKey, 'all'>> = {
    4: 'czce', 5: 'dce', 6: 'shfe', 7: 'cffex', 67: 'gfex'
  };

  let stage = $state<Stage>('catalog');
  let market = $state<OptionMarketKey>('dce');
  let underlying = $state('');
  let contract = $state('');
  let query = $state('');
  let type = $state<'all' | 'call' | 'put'>('all');
  let limit = $state('500');
  let selectedContract = $state<{ market: Exclude<OptionMarketKey, 'all'>; contract: string } | null>(null);
  let selectedLeg = $state<OptionChainLeg | null>(null);

  const catalog = new Resource<OptionCatalogDocument>();
  const chain = new Resource<OptionChainDocument>();
  const expiry = new Resource<OptionExpiryDocument>();
  const volatility = new Resource<OptionVolatilityDocument>();
  const catalogRows = $derived(catalog.data?.options ?? []);
  const strikeRows = $derived(chain.data?.strikes ?? []);
  const detailBusy = $derived(expiry.busy || volatility.busy);
  const detailError = $derived(expiry.error || volatility.error);

  function catalogPath(refresh = false): string {
    return `/api/v1/market/options?${queryString({
      market: market === 'all' ? '' : market,
      underlying: underlying.trim(),
      contract: contract.trim(),
      type,
      query: query.trim(),
      limit: Number(limit),
      refresh: refresh ? 1 : 0
    })}`;
  }

  async function loadCatalog(refresh = false) {
    stage = 'catalog';
    selectedContract = null;
    selectedLeg = null;
    chain.reset();
    expiry.reset();
    volatility.reset();
    if (!refresh) catalog.reset();
    await catalog.load(catalogPath(refresh));
  }

  async function openChain(option: OptionInstrument, refresh = false) {
    const marketKey = MARKET_KEYS[option.market_id];
    if (!marketKey || !option.contract) return;
    const switching = selectedContract?.market !== marketKey ||
      selectedContract?.contract !== option.contract;
    if (switching) chain.reset();
    expiry.reset();
    volatility.reset();
    selectedLeg = null;
    selectedContract = { market: marketKey, contract: option.contract };
    stage = 'chain';
    const document = await chain.load(`/api/v1/market/option-chain?${queryString({
      market: marketKey,
      contract: option.contract,
      lookback: 60,
      limit: 2000,
      refresh: refresh ? 1 : 0
    })}`);
    const initial = document?.at_the_money.call ?? document?.at_the_money.put ?? null;
    if (initial) void loadLeg(initial);
  }

  async function loadLeg(leg: OptionChainLeg) {
    const switching = selectedLeg?.security !== leg.security;
    if (switching) {
      expiry.reset();
      volatility.reset();
    }
    selectedLeg = leg;
    const params = {
      market: leg.market_id,
      code: leg.code,
      name: leg.name
    };
    await Promise.all([
      expiry.load(`/api/v1/market/option-expiry?${queryString(params)}`),
      volatility.load(`/api/v1/market/option-volatility?${queryString({
        ...params, lookback: 60
      })}`)
    ]);
  }

  function refreshCurrent() {
    if (stage === 'chain' && selectedContract) {
      const representative = chain.data?.at_the_money.call ??
        chain.data?.at_the_money.put ??
        chain.data?.strikes.flatMap((row) => [row.call, row.put])
          .find((leg): leg is OptionChainLeg => leg !== null);
      if (representative) void openChain(representative, true);
    } else {
      void loadCatalog(true);
    }
  }

  function returnToCatalog() {
    stage = 'catalog';
    selectedContract = null;
    selectedLeg = null;
    chain.reset();
    expiry.reset();
    volatility.reset();
  }

  function percentFromRatio(value: number | null | undefined): string {
    return value == null ? '—' : percent(value * 100);
  }

  const stats = $derived.by<Stat[]>(() => {
    if (stage === 'catalog') {
      const doc = catalog.data;
      if (!doc) return [];
      return [
        { label: '返回期权', value: count(doc.returned), note: `命中 ${count(doc.matched)}` },
        { label: '扫描证券', value: count(doc.scanned) },
        { label: '结果状态', value: doc.truncated ? '已截断' : '完整', note: doc.transport },
        { label: '交易所', value: market === 'all' ? '全部' : market.toUpperCase() }
      ];
    }
    const doc = chain.data;
    if (!doc) return [];
    return [
      { label: '标的价格', value: price(doc.underlying_price), note: doc.underlying_price_source },
      { label: '到期日', value: date(doc.expiry), note: `${count(doc.calendar_days_to_expiry)} 个自然日` },
      { label: '执行价', value: count(doc.summary.strike_count), note: `${count(doc.summary.quoted_count)} 份有报价` },
      { label: '持仓 Put/Call', value: fixed(doc.summary.put_call_open_interest_ratio, 3) },
      { label: '最大痛点', value: doc.summary.max_pain ? price(doc.summary.max_pain.strike) : '—' }
    ];
  });

  const catalogColumns: Column<OptionInstrument>[] = [
    { key: 'name', label: '期权', width: '165px', value: (row) => row.name, sub: (row) => row.security },
    { key: 'contract', label: '合约', width: '90px', value: (row) => row.contract, sub: (row) => row.market },
    { key: 'type', label: '方向', width: '68px', value: (row) => row.type === 'call' ? '看涨' : '看跌' },
    { key: 'strike', label: '执行价', width: '90px', align: 'right', num: true, value: (row) => price(row.strike) },
    { key: 'underlying', label: '标的', width: '130px', value: (row) => row.underlying_code, sub: (row) => row.underlying_security },
    { key: 'style', label: '行权 / 模型', width: '130px', value: (row) => row.exercise_style === 'american' ? '美式' : '欧式', sub: (row) => row.pricing_family },
    { key: 'action', label: '分析', width: '82px', align: 'center', slot: true }
  ];

  const chainColumns: Column<OptionChainStrike>[] = [
    { key: 'call', label: '看涨合约', width: '155px', slot: true },
    { key: 'call-mark', label: '看涨标记价', width: '90px', align: 'right', num: true, value: (row) => price(row.call?.mark_price) },
    { key: 'call-iv', label: '看涨 IV', width: '80px', align: 'right', num: true, value: (row) => percent(row.call?.implied_volatility_percent) },
    { key: 'call-oi', label: '看涨持仓', width: '90px', align: 'right', num: true, value: (row) => compact(row.call?.open_interest) },
    { key: 'strike', label: '执行价', width: '90px', align: 'center', num: true, value: (row) => price(row.strike), sub: (row) => `距标的 ${fixed(row.distance_to_underlying)}` },
    { key: 'put-oi', label: '看跌持仓', width: '90px', align: 'right', num: true, value: (row) => compact(row.put?.open_interest) },
    { key: 'put-iv', label: '看跌 IV', width: '80px', align: 'right', num: true, value: (row) => percent(row.put?.implied_volatility_percent) },
    { key: 'put-mark', label: '看跌标记价', width: '90px', align: 'right', num: true, value: (row) => price(row.put?.mark_price) },
    { key: 'put', label: '看跌合约', width: '155px', slot: true }
  ];

  onMount(() => { void loadCatalog(); });
</script>

<PageHeader
  eyebrow="7727 OPTION DIRECTORY · TQQCALC RECONSTRUCTION"
  title={stage === 'catalog' ? '期权目录与定价模型' : `${chain.data?.contract ?? selectedContract?.contract ?? ''} 期权链`}
  description={stage === 'catalog'
    ? '浏览商品与股指期权目录；选择合约后批量计算 IV、Greeks、持仓结构和最大痛点。'
    : '标记价来自公开行情的现价、盘口中值或昨结；IV 与 Greeks 是本地可审计重建值，不冒充交易所原始字段。'}
  {stats}
>
  {#snippet actions()}
    {#if stage === 'chain'}<Button icon="chevron-left" onclick={returnToCatalog}>返回目录</Button>{/if}
    <Button icon="refresh" busy={stage === 'chain' ? chain.busy : catalog.busy} onclick={refreshCurrent}>刷新</Button>
  {/snippet}
</PageHeader>

{#if stage === 'catalog'}
  <Panel
    title="期权目录"
    subtitle={catalog.data ? `${count(catalog.data.returned)} 条${catalog.data.truncated ? ' · 已按上限截断' : ''}` : '首次冷加载可能需要约半分钟，随后复用缓存。'}
    busy={catalog.busy}
    error={catalog.error}
    onRetry={() => loadCatalog()}
    empty={catalog.loaded && !catalog.busy && catalogRows.length === 0}
    emptyText="当前筛选没有匹配期权。"
    flush
    scroll
  >
    {#snippet toolbar()}
      <Select value={market} options={MARKETS} width="135px" label="市场" onChange={(value) => { market = value as OptionMarketKey; }} />
      <TextInput bind:value={underlying} width="125px" label="标的" placeholder="标的代码" onEnter={() => loadCatalog()} />
      <TextInput bind:value={contract} width="120px" label="合约" placeholder="如 A2609" onEnter={() => loadCatalog()} />
      <Select value={type} options={TYPES} width="135px" label="方向" onChange={(value) => { type = value as 'all' | 'call' | 'put'; }} />
      <TextInput bind:value={query} icon="search" width="180px" label="检索" placeholder="代码或名称" onEnter={() => loadCatalog()} />
      <Select value={limit} options={LIMITS} width="125px" onChange={(value) => { limit = value; }} />
      <Button variant="primary" onclick={() => loadCatalog()}>查询</Button>
    {/snippet}
    <DataTable columns={catalogColumns} rows={catalogRows} rowKey={(row) => row.security} stickyFirst numbered minWidth="930px">
      {#snippet cell({ row, column })}
        {#if column.key === 'action'}
          <Button onclick={() => openChain(row)}>查看链</Button>
        {/if}
      {/snippet}
    </DataTable>
  </Panel>
{:else}
  <Panel
    title="执行价矩阵"
    subtitle={chain.data
      ? `${chain.data.market} · 标的 ${chain.data.underlying_code} ${price(chain.data.underlying_price)} · 历史波动率 ${percent(chain.data.historical_volatility_percent)} · ATM ${price(chain.data.at_the_money.strike)}`
      : '正在加载整条合约链'}
    busy={chain.busy}
    error={chain.error}
    onRetry={() => {
      const representative = catalogRows.find((row) => row.contract === selectedContract?.contract);
      if (representative) void openChain(representative);
    }}
    empty={chain.loaded && !chain.busy && strikeRows.length === 0}
    emptyText="该合约没有可配对的执行价。"
    flush
    scroll
  >
    <DataTable columns={chainColumns} rows={strikeRows} rowKey={(row) => row.strike} stickyFirst minWidth="1110px">
      {#snippet cell({ row, column })}
        {#if column.key === 'call'}
          {#if row.call}
            <button class="leg" class:active={selectedLeg?.security === row.call.security} type="button" onclick={() => loadLeg(row.call!)}>
              {row.call.name}<small>{row.call.quote_status}</small>
            </button>
          {:else}—{/if}
        {:else if column.key === 'put'}
          {#if row.put}
            <button class="leg" class:active={selectedLeg?.security === row.put.security} type="button" onclick={() => loadLeg(row.put!)}>
              {row.put.name}<small>{row.put.quote_status}</small>
            </button>
          {:else}—{/if}
        {/if}
      {/snippet}
    </DataTable>
  </Panel>

  <Panel
    title={selectedLeg ? `${selectedLeg.name} · 到期与波动率` : '选择看涨或看跌合约'}
    subtitle={selectedLeg ? `${selectedLeg.security} · ${selectedLeg.exercise_style} · ${selectedLeg.pricing_family}` : '点击执行价矩阵两侧的合约名称查看单腿详情。'}
    busy={detailBusy}
    error={detailError}
    onRetry={selectedLeg ? () => loadLeg(selectedLeg!) : undefined}
    empty={!selectedLeg}
    emptyText="尚未选择期权腿。"
  >
    {#if selectedLeg}
      <div class="detail-grid">
        <div><span>到期日</span><strong>{date(expiry.data?.expiry)}</strong><small>{text(expiry.data?.status)}</small></div>
        <div><span>剩余期限</span><strong>{count(volatility.data?.calendar_days_to_expiry)} 日</strong><small>{fixed(volatility.data?.time_to_expiry_years, 6)} 年</small></div>
        <div><span>期权 / 标的价</span><strong>{price(volatility.data?.option_price)} / {price(volatility.data?.underlying_price)}</strong><small>{text(volatility.data?.option_price_source)}</small></div>
        <div><span>历史波动率</span><strong>{percent(volatility.data?.historical_volatility_percent)}</strong><small>{count(volatility.data?.lookback_used)} 根样本</small></div>
        <div><span>隐含波动率</span><strong>{percent(volatility.data?.implied_volatility_percent)}</strong><small>差值 {percent(volatility.data?.volatility_spread_percent)}</small></div>
        <div><span>无风险利率</span><strong>{percentFromRatio(volatility.data?.risk_free)}</strong><small>{text(volatility.data?.risk_free_source)}</small></div>
        <div><span>Delta / Gamma</span><strong>{fixed(volatility.data?.delta, 4)} / {fixed(volatility.data?.gamma, 4)}</strong><small>本地模型</small></div>
        <div><span>Theta / Vega / Rho</span><strong>{fixed(volatility.data?.theta, 4)} / {fixed(volatility.data?.vega, 4)} / {fixed(volatility.data?.rho, 4)}</strong><small>{text(volatility.data?.greeks_volatility_source)}</small></div>
      </div>
      <div class="badges">
        <Badge tone={volatility.data?.implied_status === 'calculated' ? 'up' : 'warn'}>{text(volatility.data?.implied_status)}</Badge>
        <Badge>{text(expiry.data?.resource_mode)}</Badge>
        <Badge>{text(volatility.data?.model_source)}</Badge>
      </div>
    {/if}
  </Panel>

  {#if chain.data}
    <p class="method-note">
      看涨持仓加权 IV {percentFromRatio(chain.data.summary.call_open_interest_weighted_iv)}；
      看跌持仓加权 IV {percentFromRatio(chain.data.summary.put_open_interest_weighted_iv)}；
      最大痛点按“持仓量 × 到期内在价值”总额最小化计算。页面不展示本地规则文件路径，也不提供文件入口。
    </p>
  {/if}
{/if}

<style>
  :global(.panel + .panel) { margin-top: var(--sp-3); }

  .leg {
    display: flex;
    width: 100%;
    flex-direction: column;
    align-items: flex-start;
    gap: 1px;
    color: var(--focus);
    text-align: left;
  }

  .leg:hover,
  .leg.active { color: var(--fg); }

  .leg.active { font-weight: 600; }

  .leg small {
    color: var(--fg-mute);
    font-size: 9px;
    font-weight: 400;
  }

  .detail-grid {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: var(--sp-3);
  }

  .detail-grid > div {
    display: flex;
    min-width: 0;
    flex-direction: column;
    gap: 2px;
    padding: var(--sp-3);
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .detail-grid span,
  .detail-grid small,
  .method-note { color: var(--fg-mute); font-size: var(--fs-micro); }

  .detail-grid strong { font-size: var(--fs-small); font-variant-numeric: tabular-nums; }

  .badges {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-2);
    margin-top: var(--sp-3);
  }

  .method-note { margin: var(--sp-2) 0 0; }

  @media (max-width: 980px) {
    .detail-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  }
</style>
