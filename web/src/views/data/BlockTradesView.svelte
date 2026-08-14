<script lang="ts">
  /**
   * 大宗交易。月度总览 / 近期成交 / 意向申报 / 营业部排行 / 月度行业分布 五个视图，
   * 主表任意一行都能沿真实关联键展开右栏明细（证券、营业部、行业各一套动态资源）。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { Resource } from '../../lib/resource.svelte';
  import { compact, count, date, fixed, percent, text, tone } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    BlockTradeBrokerRow,
    BlockTradeIndustryRow,
    BlockTradeIntentionRow,
    BlockTradeMonthRow,
    BlockTradeRow,
    MarketBlockTradesDocument,
    ValuationSecurity
  } from '../../types';

  type View = 'monthly' | 'trades' | 'intentions' | 'brokers' | 'industries';

  const VIEWS = [
    { id: 'monthly', label: '月度总览', hint: '全市场月度成交序列' },
    { id: 'trades', label: '近期成交', hint: '逐笔大宗成交主表' },
    { id: 'intentions', label: '意向申报', hint: '买卖意向与申报价' },
    { id: 'brokers', label: '营业部', hint: '四个周期的席位排行' },
    { id: 'industries', label: '行业分布', hint: '按月统计的行业成交' }
  ];

  const PERIODS = [
    { id: '1m', label: '近一月' },
    { id: '3m', label: '近三月' },
    { id: '6m', label: '近半年' },
    { id: '1y', label: '近一年' }
  ];

  let view = $state<View>('monthly');
  let query = $state('');
  let period = $state('1m');
  let month = $state('');

  const catalog = new Resource<MarketBlockTradesDocument>();
  const detail = new Resource<MarketBlockTradesDocument>();

  const doc = $derived(catalog.data);
  const summary = $derived(doc?.summary);

  // 月度序列在任何视图下都会随主文档返回，所以月份下拉不依赖当前视图
  const monthOptions = $derived(
    (doc?.monthly ?? []).map((row) => ({ id: row.month, label: row.month }))
  );

  const stats = $derived.by(() => {
    if (!summary) return [];
    return [
      { label: '近期成交证券', value: count(summary.recent_securities), note: `${count(summary.recent_trades)} 条记录` },
      { label: '近期成交额', value: compact(summary.recent_amount_yuan, '元'), note: '主表覆盖窗口' },
      { label: '意向申报', value: count(summary.intention_rows), note: `${count(summary.intention_securities)} 只证券` },
      { label: '意向金额', value: compact(summary.intention_amount_yuan, '元'), note: '买卖意向合计' },
      { label: '月度序列', value: count(summary.monthly_points), note: doc?.monthly[0]?.month ?? '等待载入' }
    ];
  });

  const rowCount = $derived.by(() => {
    if (!doc) return 0;
    if (view === 'monthly') return doc.monthly.length;
    if (view === 'trades') return doc.trades.length;
    if (view === 'intentions') return doc.intentions.length;
    if (view === 'brokers') return doc.brokers.length;
    return doc.industries.length;
  });

  // 后端只认 trades/intentions/brokers/industries，月度总览复用 trades 的主文档
  const apiView = $derived(view === 'monthly' ? 'trades' : view);

  async function load(refresh = false) {
    const result = await catalog.load(
      `/api/v1/market/block-trades?${queryString({
        view: apiView,
        q: query.trim(),
        period,
        month: view === 'industries' ? month : '',
        limit: view === 'brokers' ? 800 : 500,
        refresh: refresh ? 1 : 0
      })}`
    );
    if (result && !month) month = result.filters.month || result.monthly[0]?.month || '';
    detail.reset();
  }

  function switchView(next: string) {
    view = next as View;
    query = '';
    void load();
  }

  function openSecurity(security: ValuationSecurity | undefined) {
    if (!security) return;
    void detail.load(
      `/api/v1/market/block-trades?${queryString({
        view: 'trades',
        market: security.market,
        code: security.code,
        include_details: 1,
        detail_limit: 1000
      })}`
    );
  }

  function openBroker(row: BlockTradeBrokerRow) {
    void detail.load(
      `/api/v1/market/block-trades?${queryString({
        view: 'brokers',
        period,
        broker_id: row.broker_id,
        include_details: 1,
        detail_limit: 1000
      })}`
    );
  }

  function openIndustry(row: BlockTradeIndustryRow) {
    void detail.load(
      `/api/v1/market/block-trades?${queryString({
        view: 'industries',
        month,
        industry: row.industry_id,
        include_details: 1,
        detail_limit: 1000
      })}`
    );
  }

  // 月度总览没有自己的明细资源，点某月直接落到该月的行业分布
  function openMonth(row: BlockTradeMonthRow) {
    month = row.month;
    view = 'industries';
    void load();
  }

  function gotoWorkbench(security: ValuationSecurity) {
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code));
  }

  const securityColumn = <T extends { security?: ValuationSecurity }>(): Column<T> => ({
    key: 'security',
    label: '证券',
    width: '128px',
    value: (row) => row.security?.name || row.security?.code || '—',
    sub: (row) => row.security?.security_id ?? ''
  });

  const monthlyColumns: Column<BlockTradeMonthRow>[] = [
    { key: 'month', label: '月份', width: '84px', num: true, value: (row) => row.month },
    {
      key: 'count',
      label: '成交笔数',
      align: 'right',
      num: true,
      value: (row) => count(row.trade_count),
      sortValue: (row) => row.trade_count ?? 0
    },
    {
      key: 'volume',
      label: '成交量',
      align: 'right',
      num: true,
      value: (row) => compact(row.volume_shares, '股'),
      sortValue: (row) => row.volume_shares ?? 0
    },
    {
      key: 'amount',
      label: '成交额',
      align: 'right',
      num: true,
      value: (row) => compact(row.amount_yuan, '元'),
      sortValue: (row) => row.amount_yuan ?? 0
    },
    {
      key: 'premium',
      label: '平均折溢价',
      align: 'right',
      num: true,
      value: (row) => percent(row.premium_pct, 2, true),
      tone: (row) => tone(row.premium_pct),
      sortValue: (row) => row.premium_pct ?? 0
    }
  ];

  const tradeColumns: Column<BlockTradeRow>[] = [
    securityColumn<BlockTradeRow>(),
    { key: 'date', label: '成交日', width: '84px', num: true, value: (row) => date(row.date) },
    { key: 'price', label: '成交价', align: 'right', num: true, value: (row) => fixed(row.price) },
    { key: 'close', label: '收盘价', align: 'right', num: true, value: (row) => fixed(row.close) },
    {
      key: 'premium',
      label: '折溢价',
      align: 'right',
      num: true,
      value: (row) => percent(row.premium_pct, 2, true),
      tone: (row) => tone(row.premium_pct),
      sortValue: (row) => row.premium_pct ?? 0
    },
    {
      key: 'amount',
      label: '成交额',
      align: 'right',
      num: true,
      value: (row) => compact(row.amount_yuan, '元'),
      sub: (row) => compact(row.volume_shares, '股'),
      sortValue: (row) => row.amount_yuan ?? 0
    },
    { key: 'buyer', label: '买方', wrap: true, value: (row) => text(row.buyer.name) },
    { key: 'seller', label: '卖方', wrap: true, value: (row) => text(row.seller.name) },
    {
      key: 'freq',
      label: '30 日上榜',
      align: 'right',
      num: true,
      value: (row) => count(row.frequency.days_30),
      sub: (row) => `7 日 ${count(row.frequency.days_7)}`,
      sortValue: (row) => row.frequency.days_30 ?? 0
    }
  ];

  const intentionColumns: Column<BlockTradeIntentionRow>[] = [
    securityColumn<BlockTradeIntentionRow>(),
    { key: 'date', label: '申报日', width: '84px', num: true, value: (row) => date(row.date) },
    {
      key: 'direction',
      label: '方向',
      width: '54px',
      value: (row) => text(row.direction),
      tone: (row) => (row.direction.includes('买') ? 'up' : row.direction.includes('卖') ? 'down' : 'flat')
    },
    { key: 'price', label: '申报价', align: 'right', num: true, value: (row) => fixed(row.declaration_price) },
    { key: 'close', label: '收盘价', align: 'right', num: true, value: (row) => fixed(row.close) },
    {
      key: 'premium',
      label: '折溢价',
      align: 'right',
      num: true,
      value: (row) => percent(row.premium_pct, 2, true),
      tone: (row) => tone(row.premium_pct),
      sortValue: (row) => row.premium_pct ?? 0
    },
    {
      key: 'quantity',
      label: '申报数量',
      align: 'right',
      num: true,
      value: (row) => compact(row.quantity_shares, '股'),
      sortValue: (row) => row.quantity_shares ?? 0
    },
    {
      key: 'amount',
      label: '申报金额',
      align: 'right',
      num: true,
      value: (row) => compact(row.amount_yuan, '元'),
      sortValue: (row) => row.amount_yuan ?? 0
    }
  ];

  const brokerColumns: Column<BlockTradeBrokerRow>[] = [
    {
      key: 'name',
      label: '营业部',
      width: '200px',
      wrap: true,
      value: (row) => row.name,
      sub: (row) => (row.hot_money_label ? `${row.broker_id} · ${row.hot_money_label}` : row.broker_id)
    },
    { key: 'latest', label: '最新上榜', width: '84px', num: true, value: (row) => date(row.latest_date) },
    {
      key: 'count',
      label: '成交次数',
      align: 'right',
      num: true,
      value: (row) => count(row.trade_count),
      sub: (row) => `${count(row.buy_count)} 买 / ${count(row.sell_count)} 卖`,
      sortValue: (row) => row.trade_count ?? 0
    },
    {
      key: 'buy',
      label: '买入额',
      align: 'right',
      num: true,
      value: (row) => compact(row.buy_amount_yuan, '元'),
      tone: () => 'up',
      sortValue: (row) => row.buy_amount_yuan ?? 0
    },
    {
      key: 'sell',
      label: '卖出额',
      align: 'right',
      num: true,
      value: (row) => compact(row.sell_amount_yuan, '元'),
      tone: () => 'down',
      sortValue: (row) => row.sell_amount_yuan ?? 0
    },
    {
      key: 'net',
      label: '净买入',
      align: 'right',
      num: true,
      value: (row) => compact(row.net_buy_yuan, '元'),
      tone: (row) => tone(row.net_buy_yuan),
      sortValue: (row) => row.net_buy_yuan ?? 0
    },
    {
      key: 'perf',
      label: '1 日均收益',
      align: 'right',
      num: true,
      value: (row) => percent(row.performance[0]?.average_return_pct, 2, true),
      sub: (row) => `胜率 ${percent(row.performance[0]?.success_pct)}`,
      tone: (row) => tone(row.performance[0]?.average_return_pct),
      sortValue: (row) => row.performance[0]?.average_return_pct ?? 0
    }
  ];

  const industryColumns: Column<BlockTradeIndustryRow>[] = [
    { key: 'name', label: '行业', width: '140px', value: (row) => row.name, sub: (row) => row.industry_id },
    {
      key: 'count',
      label: '成交次数',
      align: 'right',
      num: true,
      value: (row) => count(row.trade_count),
      sortValue: (row) => row.trade_count ?? 0
    },
    {
      key: 'volume',
      label: '成交量',
      align: 'right',
      num: true,
      value: (row) => compact(row.volume_shares, '股'),
      sortValue: (row) => row.volume_shares ?? 0
    },
    {
      key: 'amount',
      label: '成交额',
      align: 'right',
      num: true,
      value: (row) => compact(row.amount_yuan, '元'),
      sortValue: (row) => row.amount_yuan ?? 0
    },
    {
      key: 'premium',
      label: '平均折溢价',
      align: 'right',
      num: true,
      value: (row) => percent(row.premium_pct, 2, true),
      tone: (row) => tone(row.premium_pct),
      sortValue: (row) => row.premium_pct ?? 0
    }
  ];

  const selectedSecurity = $derived(detail.data?.selected_security ?? null);
  const selectedBroker = $derived(detail.data?.selected_broker ?? null);
  const selectedIndustry = $derived(detail.data?.selected_industry ?? null);

  const asideTitle = $derived(
    selectedSecurity?.name ||
      selectedSecurity?.code ||
      selectedBroker?.name ||
      selectedIndustry?.name ||
      '关联明细'
  );

  onMount(() => void load());
</script>

<PageHeader
  eyebrow="DZJY · 709/1721"
  title="大宗交易"
  description="月度成交序列、逐笔大宗成交、意向申报、营业部席位排行与行业分布，落在同一条关联链上。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={catalog.busy} onclick={() => void load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="360px">
  {#snippet main()}
    <Panel
      flush
      scroll
      busy={catalog.busy}
      error={catalog.error}
      empty={catalog.loaded && !catalog.busy && rowCount === 0}
      emptyText="当前条件下没有大宗交易记录，换个视图、周期或月份再试。"
      onRetry={() => void load()}
      title={VIEWS.find((item) => item.id === view)?.label ?? ''}
      subtitle={doc ? `${count(rowCount)} 行` : ''}
    >
      {#snippet toolbar()}
        <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="大宗交易视图" />
        {#if view === 'brokers'}
          <Select
            value={period}
            options={PERIODS}
            label="周期"
            width="128px"
            onChange={(next) => {
              period = next;
              void load();
            }}
          />
        {/if}
        {#if view === 'industries'}
          <Select
            value={month}
            options={monthOptions}
            label="月份"
            width="140px"
            onChange={(next) => {
              month = next;
              void load();
            }}
          />
        {/if}
        {#if view !== 'monthly'}
          <TextInput
            bind:value={query}
            icon="search"
            width="220px"
            label="检索当前视图"
            placeholder={view === 'brokers' ? '营业部名称或席位标签' : '股票、代码、行业或买卖方'}
            onEnter={() => void load()}
          />
        {/if}
      {/snippet}

      {#if view === 'monthly'}
        <DataTable
          columns={monthlyColumns}
          rows={doc?.monthly ?? []}
          rowKey={(row) => row.month}
          onRowClick={openMonth}
          isActive={(row) => row.month === month}
        />
      {:else if view === 'trades'}
        <DataTable
          columns={tradeColumns}
          rows={doc?.trades ?? []}
          onRowClick={(row) => openSecurity(row.security)}
          isActive={(row) => row.security.security_id === selectedSecurity?.security_id}
        />
      {:else if view === 'intentions'}
        <DataTable
          columns={intentionColumns}
          rows={doc?.intentions ?? []}
          onRowClick={(row) => openSecurity(row.security)}
          isActive={(row) => row.security?.security_id === selectedSecurity?.security_id}
        />
      {:else if view === 'brokers'}
        <DataTable
          columns={brokerColumns}
          rows={doc?.brokers ?? []}
          rowKey={(row) => row.broker_id}
          onRowClick={openBroker}
          isActive={(row) => row.broker_id === selectedBroker?.broker_id}
        />
      {:else}
        <DataTable
          columns={industryColumns}
          rows={doc?.industries ?? []}
          rowKey={(row) => row.detail_id}
          onRowClick={openIndustry}
          isActive={(row) => row.industry_id === selectedIndustry?.industry_id}
        />
      {/if}
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="LINKED DETAIL"
      title={asideTitle}
      subtitle={detail.data?.mode ?? '点击左侧任意行展开'}
      busy={detail.busy}
      error={detail.error}
      empty={!detail.loaded && !detail.busy}
      emptyText="点击左侧的证券、营业部或行业，按真实动态键拉取明细。"
    >
      {#if selectedSecurity}
        {@const security = selectedSecurity}
        <button class="jump" type="button" onclick={() => gotoWorkbench(security)}>
          在个股工作台打开 {security.name || security.code}
        </button>

        <h3>逐笔大宗成交</h3>
        <ul class="events">
          {#each detail.data?.security_history ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(row.date)}</time>
                <span class="num">{compact(row.amount_yuan, '元')}</span>
              </div>
              <strong class="num">成交价 {fixed(row.price)}</strong>
              <small>{text(row.buyer)} → {text(row.seller)}</small>
            </li>
          {/each}
        </ul>

        <h3>意向申报历史</h3>
        <ul class="events">
          {#each detail.data?.security_intentions ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(row.date)}</time>
                <span class={row.direction.includes('卖') ? 'down' : 'up'}>{text(row.direction)}</span>
              </div>
              <strong class="num">{compact(row.amount_yuan, '元')}</strong>
              <small>
                申报价 {fixed(row.declaration_price)} · 折溢价 {percent(row.premium_pct, 2, true)} ·
                {compact(row.quantity_shares, '股')}
              </small>
            </li>
          {/each}
        </ul>
      {:else if selectedBroker}
        {@const broker = selectedBroker}
        <p class="note">{broker.broker_id} · {broker.period_label} · {count(broker.trade_count)} 次上榜</p>
        <h3>席位后市表现</h3>
        <ul class="events">
          {#each broker.performance as item (item.days)}
            <li>
              <div class="row">
                <time>{item.days} 日</time>
                <span class={tone(item.average_return_pct)}>
                  {percent(item.average_return_pct, 2, true)}
                </span>
              </div>
              <small>胜率 {percent(item.success_pct)}</small>
            </li>
          {/each}
        </ul>

        <h3>营业部成交明细</h3>
        <ul class="events">
          {#each detail.data?.broker_trades ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(row.date)}</time>
                <span class={row.direction.includes('卖') ? 'down' : 'up'}>{text(row.direction)}</span>
              </div>
              <strong>{row.security.name || row.security.code}</strong>
              <p class="num">{compact(row.amount_yuan, '元')} · {fixed(row.price)}</p>
              <small>
                {row.security.security_id} · 折溢价 {percent(row.premium_pct, 2, true)} ·
                {compact(row.volume_shares, '股')}
              </small>
            </li>
          {/each}
        </ul>
      {:else if selectedIndustry}
        {@const industry = selectedIndustry}
        <p class="note">
          {industry.industry_id} · {month} · {count(industry.trade_count)} 笔 ·
          {compact(industry.amount_yuan, '元')}
        </p>
        <h3>行业内成交证券</h3>
        <ul class="events">
          {#each detail.data?.industry_securities ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{row.month}</time>
                <span class="num">{compact(row.amount_yuan, '元')}</span>
              </div>
              <strong>{row.security.name || row.security.code}</strong>
              <p class="num">{count(row.trade_count)} 笔 · {compact(row.volume_shares, '股')}</p>
              <small>{row.security.security_id} · {text(row.trade_detail_text)}</small>
            </li>
          {/each}
        </ul>
      {/if}

      {#each detail.data?.detail_errors ?? [] as failure (failure.resource)}
        <p class="warn-line">{failure.resource} 暂无动态数据</p>
      {/each}
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

  h3 {
    margin: var(--sp-3) 0 var(--sp-2);
    font-size: var(--fs-micro);
    font-weight: 600;
    color: var(--fg-dim);
  }

  h3:first-child {
    margin-top: 0;
  }

  .note {
    font-size: var(--fs-micro);
    color: var(--fg-mute);
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

  .warn-line {
    margin-top: var(--sp-2);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }
</style>
