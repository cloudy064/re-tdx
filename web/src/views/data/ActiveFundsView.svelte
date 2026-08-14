<script lang="ts">
  /**
   * 主动基金持仓。左主表是基金季报汇总出的股票口径，右栏按股票展开持有它的基金。
   * 金额 / 股数 / 净值占比 / 重仓排名一律保留通达信原始披露口径，不做二次换算。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { Resource } from '../../lib/resource.svelte';
  import { compact, count, date, num, percent, text, tone } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import Icon from '../../ui/Icon.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    ActiveFundSecurityHolding,
    MarketActiveFundsDocument,
    ValuationSecurity
  } from '../../types';

  const DIRECTIONS = [
    { id: 'all', label: '全部', hint: '不过滤变动方向' },
    { id: 'increased', label: '增持', hint: '含新进' },
    { id: 'new', label: '新进', hint: '本季首次出现在季报里' },
    { id: 'decreased', label: '减持', hint: '持仓市值下降' },
    { id: 'unchanged', label: '不变', hint: '持仓市值持平' }
  ];

  const DIRECTION_LABEL: Record<ActiveFundSecurityHolding['direction'], string> = {
    increased: '增持',
    new: '新进',
    decreased: '减持',
    unchanged: '不变'
  };

  let direction = $state('all');
  let query = $state('');

  const catalog = new Resource<MarketActiveFundsDocument>();
  const detail = new Resource<MarketActiveFundsDocument>();

  const doc = $derived(catalog.data);
  const summary = $derived(doc?.summary);
  const holding = $derived(detail.data?.selected_holding ?? null);

  const stats = $derived.by(() => {
    if (!summary) return [];
    return [
      { label: '覆盖股票', value: count(summary.securities), note: summary.periods[0] ?? '当前季报' },
      { label: '增持 / 新进', value: `${summary.increased} / ${summary.newly_held}` },
      { label: '减持 / 不变', value: `${summary.decreased} / ${summary.unchanged}` },
      { label: '基金持仓关系', value: count(summary.fund_positions) },
      { label: '合计持仓市值', value: compact(summary.total_holding_market_value_yuan, '元') },
      {
        label: '合计市值变动',
        value: compact(summary.total_holding_market_value_change_yuan, '元'),
        tone: tone(summary.total_holding_market_value_change_yuan)
      }
    ];
  });

  function load(refresh = false) {
    void catalog.load(
      `/api/v1/market/active-funds?${queryString({
        view: 'securities',
        direction,
        q: query.trim(),
        limit: 5000,
        refresh: refresh ? 1 : 0
      })}`
    );
    detail.reset();
  }

  function openSecurity(row: ActiveFundSecurityHolding) {
    void detail.load(
      `/api/v1/market/active-funds?${queryString({
        view: 'funds',
        market: row.security.market,
        code: row.security.code,
        detail_limit: 5000
      })}`
    );
  }

  function gotoWorkbench(security: ValuationSecurity) {
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code));
  }

  const columns: Column<ActiveFundSecurityHolding>[] = [
    {
      key: 'security',
      label: '股票',
      width: '132px',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => `${row.security.security_id} · ${date(row.end_date)}`
    },
    {
      key: 'direction',
      label: '方向',
      width: '52px',
      value: (row) => DIRECTION_LABEL[row.direction] ?? text(row.direction),
      tone: (row) =>
        row.direction === 'decreased' ? 'down' : row.direction === 'unchanged' ? 'flat' : 'up'
    },
    {
      key: 'funds',
      label: '基金数',
      align: 'right',
      num: true,
      value: (row) => count(row.fund_count),
      sortValue: (row) => row.fund_count ?? 0
    },
    {
      key: 'value',
      label: '持仓市值',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_market_value_yuan, '元'),
      sortValue: (row) => row.holding_market_value_yuan ?? 0
    },
    {
      key: 'value-change',
      label: '市值变动',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_market_value_change_yuan, '元'),
      tone: (row) => tone(row.holding_market_value_change_yuan),
      sortValue: (row) => row.holding_market_value_change_yuan ?? 0
    },
    {
      key: 'shares',
      label: '持仓股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_shares, '股'),
      sortValue: (row) => row.holding_shares ?? 0
    },
    {
      key: 'share-change',
      label: '股数变动',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_share_change, '股'),
      tone: (row) => tone(row.holding_share_change),
      sortValue: (row) => row.holding_share_change ?? 0
    },
    {
      key: 'float',
      label: '占流通市值',
      align: 'right',
      num: true,
      value: (row) => percent(row.holding_pct_float),
      sortValue: (row) => num(row.holding_pct_float) ?? 0
    },
    { key: 'industry', label: '行业', wrap: true, value: (row) => text(row.industry) }
  ];

  const rows = $derived(doc?.securities ?? []);

  onMount(() => load());
</script>

<PageHeader
  eyebrow="ZDJJZCZC · list/func_zdjjzczc101_1.jsn"
  title="主动基金持仓"
  description="基金公开季报汇总出的股票口径持仓：先看全市场增减持方向，再下钻到持有该股的每一只基金。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={catalog.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="368px">
  {#snippet main()}
    <Panel
      flush
      scroll
      busy={catalog.busy}
      error={catalog.error}
      onRetry={() => load()}
      empty={catalog.loaded && !catalog.busy && rows.length === 0}
      emptyText="当前方向与检索条件下没有季报持仓记录。"
      title="基金季报股票持仓"
      subtitle={doc ? `${count(doc.counts.securities ?? 0)} 行 · ${text(summary?.periods[0])}` : ''}
    >
      {#snippet toolbar()}
        <Segmented
          options={DIRECTIONS}
          value={direction}
          ariaLabel="持仓变动方向"
          onChange={(next) => {
            direction = next;
            load();
          }}
        />
        <TextInput
          bind:value={query}
          icon="search"
          width="220px"
          label="检索股票"
          placeholder="股票名称、代码或行业"
          onEnter={() => load()}
        />
        <p class="notice">
          <Icon name="info" size={11} />
          季报口径：仅披露期末重仓，不是基金全部持仓，也不是实时持仓。
        </p>
      {/snippet}

      <DataTable
        {columns}
        {rows}
        stickyFirst
        rowKey={(row) => row.security.security_id}
        onRowClick={openSecurity}
        isActive={(row) => row.security.security_id === holding?.security.security_id}
      />
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="STOCK → FUNDS"
      title={holding ? holding.security.name || holding.security.code : '持有基金明细'}
      subtitle={holding
        ? `${date(holding.start_date)} — ${date(holding.end_date)} · 主表 ${holding.fund_count ?? 0} 家 / 明细 ${detail.data?.funds.length ?? 0} 家`
        : '点击左侧任意股票展开'}
      busy={detail.busy}
      error={detail.error}
      empty={!detail.loaded && !detail.busy}
      emptyText="选择一只股票，查看哪些主动基金在季报里持有它。"
      onRetry={() => holding && openSecurity(holding)}
    >
      {#if holding}
        <button class="jump" type="button" onclick={() => gotoWorkbench(holding.security)}>
          在个股工作台打开 {holding.security.name || holding.security.code}
        </button>

        <StatGrid
          inline
          stats={[
            { label: '持仓市值', value: compact(holding.holding_market_value_yuan, '元') },
            {
              label: '市值变动',
              value: compact(holding.holding_market_value_change_yuan, '元'),
              tone: tone(holding.holding_market_value_change_yuan)
            },
            { label: '持仓股数', value: compact(holding.holding_shares, '股') },
            {
              label: '股数变动',
              value: compact(holding.holding_share_change, '股'),
              tone: tone(holding.holding_share_change)
            },
            { label: '占流通市值', value: percent(holding.holding_pct_float) },
            { label: '所属行业', value: text(holding.industry) }
          ]}
        />

        <ul class="events">
          {#each detail.data?.funds ?? [] as row, index (`${row.fund.fund_id}-${index}`)}
            <li>
              <div class="row">
                <time class="num">{row.fund.code}</time>
                <span class="rank">第 {row.holding_rank ?? '—'} 大重仓</span>
              </div>
              <strong>{row.fund.name || row.fund.code}</strong>
              <p class="num">
                {compact(row.holding_market_value_yuan, '元')} · {compact(row.holding_shares, '股')}
              </p>
              <small>占基金净值 {percent(row.nav_pct)} · 市场 {row.fund.market_id}</small>
            </li>
          {/each}
        </ul>

        {#if (detail.data?.funds.length ?? 0) === 0}
          <p class="warn-line">该股票在本期季报里没有返回基金明细。</p>
        {/if}

        {#each detail.data?.detail_errors ?? [] as failure, index (index)}
          <p class="warn-line">{text(failure.resource)}：{failure.message}</p>
        {/each}
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .notice {
    display: inline-flex;
    align-items: center;
    gap: var(--sp-1);
    margin-left: auto;
    font-size: 10px;
    color: var(--warn);
  }

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

  .rank {
    color: var(--focus);
    font-size: 10px;
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
