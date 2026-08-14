<script lang="ts">
  /**
   * 机构龙虎。四个滚动周期的机构席位买卖排行，右栏展开选中证券的异动日明细。
   *
   * 主表是「机构席位买卖额」，右栏是「异动日全榜买卖总额」——两个口径不能相加，
   * 所以两块面板的副标题各自写清楚统计对象。
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
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    InstitutionLhbRanking,
    MarketInstitutionLhbDocument,
    ValuationSecurity
  } from '../../types';

  type Period = 'week' | 'month' | 'quarter' | 'year';

  const PERIODS = [
    { id: 'week', label: '最近一周', hint: 'lsyd22801' },
    { id: 'month', label: '最近一月', hint: 'lsyd22802' },
    { id: 'quarter', label: '最近三月', hint: 'lsyd22803' },
    { id: 'year', label: '最近一年', hint: 'lsyd22804' }
  ];

  const DIRECTIONS = [
    { id: 'all', label: '全部方向' },
    { id: 'net-buy', label: '机构净买入' },
    { id: 'net-sell', label: '机构净卖出' },
    { id: 'flat', label: '买卖持平' }
  ];

  let period = $state<Period>('week');
  let direction = $state('all');
  let query = $state('');

  const catalog = new Resource<MarketInstitutionLhbDocument>();
  const detail = new Resource<MarketInstitutionLhbDocument>();

  const doc = $derived(catalog.data);
  const summary = $derived(doc?.summary);

  const stats = $derived.by<Stat[]>(() => {
    if (!summary) return [];
    return [
      {
        label: '覆盖股票',
        value: count(summary.securities),
        note: `${date(summary.earliest_trade_date)} — ${date(summary.latest_trade_date)}`
      },
      {
        label: '机构参与席位',
        value: count(summary.institution_participations),
        note: '主表参与数合计'
      },
      {
        label: '机构买入',
        value: compact(summary.institution_buy_amount_yuan, '元'),
        tone: 'up',
        note: `${count(summary.net_buy_securities)} 只净买`
      },
      {
        label: '机构卖出',
        value: compact(summary.institution_sell_amount_yuan, '元'),
        tone: 'down',
        note: `${count(summary.net_sell_securities)} 只净卖`
      },
      {
        label: '机构净买入',
        value: compact(summary.net_institution_amount_yuan, '元'),
        tone: tone(summary.net_institution_amount_yuan),
        note: '买入减卖出'
      }
    ];
  });

  const selected = $derived(detail.data?.selected_ranking ?? null);

  const selectedStats = $derived.by<Stat[]>(() => {
    if (!selected) return [];
    return [
      {
        label: '机构净额',
        value: compact(selected.net_institution_amount_yuan, '元'),
        tone: tone(selected.net_institution_amount_yuan)
      },
      { label: '参与席位', value: count(selected.institution_participations) },
      { label: '机构买入', value: compact(selected.institution_buy_amount_yuan, '元'), tone: 'up' },
      { label: '机构卖出', value: compact(selected.institution_sell_amount_yuan, '元'), tone: 'down' },
      { label: '买卖比', value: fixed(selected.buy_sell_ratio) },
      {
        label: '交易区间',
        value: `${date(selected.earliest_trade_date)} — ${date(selected.latest_trade_date)}`
      }
    ];
  });

  function load(refresh = false) {
    void catalog.load(
      `/api/v1/market/institution-lhb?${queryString({
        period,
        direction,
        q: query.trim(),
        limit: 5000,
        refresh: refresh ? 1 : 0
      })}`
    );
    detail.reset();
  }

  function switchPeriod(next: string) {
    period = next as Period;
    load();
  }

  function openRanking(row: InstitutionLhbRanking) {
    void detail.load(
      `/api/v1/market/institution-lhb?${queryString({
        period,
        market: row.security.market,
        code: row.security.code,
        include_details: 1,
        detail_limit: 1000
      })}`
    );
  }

  function gotoWorkbench(security: ValuationSecurity) {
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code));
  }

  const rankingColumns: Column<InstitutionLhbRanking>[] = [
    {
      key: 'security',
      label: '股票',
      width: '128px',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => row.security.security_id
    },
    {
      key: 'net',
      label: '机构净买入',
      align: 'right',
      num: true,
      value: (row) => compact(row.net_institution_amount_yuan, '元'),
      tone: (row) => tone(row.net_institution_amount_yuan),
      sortValue: (row) => row.net_institution_amount_yuan
    },
    {
      key: 'buy',
      label: '机构买入',
      align: 'right',
      num: true,
      value: (row) => compact(row.institution_buy_amount_yuan, '元'),
      tone: () => 'up',
      sortValue: (row) => row.institution_buy_amount_yuan
    },
    {
      key: 'sell',
      label: '机构卖出',
      align: 'right',
      num: true,
      value: (row) => compact(row.institution_sell_amount_yuan, '元'),
      tone: () => 'down',
      sortValue: (row) => row.institution_sell_amount_yuan
    },
    {
      key: 'ratio',
      label: '买卖比',
      align: 'right',
      num: true,
      value: (row) => fixed(row.buy_sell_ratio),
      sortValue: (row) => row.buy_sell_ratio ?? 0
    },
    {
      key: 'seats',
      label: '参与席位',
      align: 'right',
      num: true,
      value: (row) => count(row.institution_participations),
      sortValue: (row) => row.institution_participations
    },
    {
      key: 'range',
      label: '交易区间',
      num: true,
      value: (row) => `${date(row.earliest_trade_date)} — ${date(row.latest_trade_date)}`
    }
  ];

  onMount(() => load());
</script>

<PageHeader
  eyebrow="JGLHTJ · lsyd22801—22804"
  title="机构龙虎"
  description="四个滚动周期聚合机构席位买卖，点击股票展开该周期内的异动日与当日龙虎榜买卖总额。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={catalog.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="360px">
  {#snippet main()}
    <Panel
      flush
      scroll
      busy={catalog.busy}
      error={catalog.error}
      empty={catalog.loaded && !catalog.busy && (doc?.rankings.length ?? 0) === 0}
      emptyText="该周期与方向下没有机构席位记录，放宽筛选条件再试。"
      onRetry={() => load()}
      title="机构席位买卖排行"
      subtitle={doc
        ? `${doc.period_label} · ${count(doc.rankings.length)} 只 · 金额口径：机构席位买卖额`
        : '金额口径：机构席位买卖额'}
    >
      {#snippet toolbar()}
        <Segmented options={PERIODS} value={period} onChange={switchPeriod} ariaLabel="统计周期" />
        <Select
          value={direction}
          options={DIRECTIONS}
          label="方向"
          width="150px"
          onChange={(next) => {
            direction = next;
            load();
          }}
        />
        <TextInput
          bind:value={query}
          icon="search"
          width="220px"
          label="检索排行"
          placeholder="股票名称或六位代码"
          onEnter={() => load()}
        />
      {/snippet}

      <DataTable
        numbered
        columns={rankingColumns}
        rows={doc?.rankings ?? []}
        rowKey={(row) => row.security.security_id}
        onRowClick={openRanking}
        isActive={(row) => row.security.security_id === selected?.security.security_id}
      />
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="LINKED DETAIL"
      title={selected ? selected.security.name || selected.security.code : '异动日明细'}
      subtitle="金额口径：异动日龙虎榜买卖总额"
      busy={detail.busy}
      error={detail.error}
      empty={!detail.loaded && !detail.busy}
      emptyText="点击左侧股票，读取该周期内的异动日期、上榜类型与当日买卖总额。"
    >
      {#if selected}
        {@const security = selected.security}
        <button class="jump" type="button" onclick={() => gotoWorkbench(security)}>
          在个股工作台打开 {security.name || security.code}
        </button>

        <StatGrid stats={selectedStats} inline />

        <h3>异动日榜单（全榜口径）</h3>
        <ul class="events">
          {#each detail.data?.events ?? [] as event, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(event.event_date)}</time>
                <span class="num {tone(event.event_change_pct)}">
                  {percent(event.event_change_pct, 2, true)}
                </span>
              </div>
              <strong>{text(event.event_type)}</strong>
              <p class="num">
                买 {compact(event.event_total_buy_amount_yuan, '元')} · 卖
                {compact(event.event_total_sell_amount_yuan, '元')}
              </p>
              <div class="row">
                <span class="label">当日净额</span>
                <span class="num {tone(event.event_net_buy_amount_yuan)}">
                  {compact(event.event_net_buy_amount_yuan, '元')}
                </span>
              </div>
              {#if event.outside_master_date_range}
                <span class="warn-line">主表与详情更新时间不同：该事件超出主表日期区间</span>
              {/if}
            </li>
          {/each}
        </ul>

        <p class="note">
          右栏金额是异动日龙虎榜买卖总额，不等同于主表的机构席位买卖额；营业部名称请在龙虎榜功能查看。
        </p>
      {/if}

      {#each detail.data?.detail_errors ?? [] as failure, index (index)}
        <p class="warn-line">{failure.resource || '详情'}：{failure.message}</p>
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

  .events strong {
    font-size: var(--fs-micro);
    font-weight: 500;
    line-height: var(--lh-tight);
  }

  .events p {
    font-size: var(--fs-micro);
    color: var(--fg-dim);
  }

  .label {
    color: var(--fg-mute);
  }

  .note {
    margin-top: var(--sp-3);
    font-size: 10px;
    line-height: 1.5;
    color: var(--fg-mute);
  }

  .warn-line {
    display: block;
    margin-top: var(--sp-2);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }
</style>
