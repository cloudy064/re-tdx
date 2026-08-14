<script lang="ts">
  /**
   * 股份回购。四类视图共用一套主从骨架：
   * A 股方案 / 滚动月度进度 / 年度统计（全市场·A股·港股）/ 港股逐笔。
   * 主表点某只证券 → 右栏展开该票的完整方案或逐笔历史。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { Resource } from '../../lib/resource.svelte';
  import { compact, count, date, fixed, money, percent, text, tone } from '../../lib/fmt';
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
    HongKongRepurchase,
    MarketRepurchasesDocument,
    RepurchaseAnnual,
    RepurchaseMonth,
    RepurchasePlan
  } from '../../types';

  type View = 'plans' | 'monthly' | 'annual' | 'hong-kong';

  const VIEWS = [
    { id: 'plans', label: 'A股方案', hint: '计划规模与实施进度' },
    { id: 'monthly', label: '月度进度', hint: '拟回购与实际实施对照' },
    { id: 'annual', label: '年度统计', hint: '全市场 / A股 / 港股' },
    { id: 'hong-kong', label: '港股逐笔', hint: '交易日逐笔回购' }
  ];

  const SEGMENTS = [
    { id: 'a', label: 'A股' },
    { id: 'hk', label: '港股' },
    { id: 'all', label: '全市场' }
  ];

  let view = $state<View>('plans');
  let segment = $state('a');
  let query = $state('');
  /** 月度视图没有二次请求，选中行只是本地高亮 */
  let month = $state<RepurchaseMonth | null>(null);

  const catalog = new Resource<MarketRepurchasesDocument>();
  const detail = new Resource<MarketRepurchasesDocument>();

  const doc = $derived(catalog.data);
  const summary = $derived(doc?.summary);
  const selected = $derived(detail.data?.selected_security ?? null);

  const stats = $derived.by(() => {
    if (!summary) return [];
    return [
      { label: 'A股方案', value: count(summary.plans), note: `${summary.unique_securities} 只证券` },
      { label: '已完成 / 进行中', value: `${summary.completed_plans} / ${summary.active_plans}` },
      { label: '计划上限', value: money(summary.planned_amount_upper_yuan) },
      { label: '实际回购金额', value: money(summary.actual_amount_yuan) },
      { label: '实际回购股数', value: compact(summary.actual_shares, '股') },
      { label: '金额完成率', value: percent(summary.amount_completion_pct) }
    ];
  });

  // 月份口径是 6 位年月，fmt.date 只规整 8 位日期
  function month6(value: string | null | undefined): string {
    const digits = (value ?? '').replace(/\D/g, '');
    return digits.length >= 6 ? `${digits.slice(0, 4)}-${digits.slice(4, 6)}` : text(value);
  }

  function load(refresh = false) {
    void catalog.load(
      `/api/v1/market/repurchases?${queryString({
        view,
        segment,
        q: query.trim(),
        limit: view === 'plans' ? 1500 : 1000,
        refresh: refresh ? 1 : 0
      })}`
    );
    detail.reset();
    month = null;
  }

  function switchView(next: string) {
    view = next as View;
    query = '';
    load();
  }

  function openPlan(row: RepurchasePlan) {
    void detail.load(
      `/api/v1/market/repurchases?${queryString({
        view: 'plans',
        market: row.security.market,
        code: row.security.code,
        include_details: 1,
        detail_limit: 2000
      })}`
    );
  }

  function openYear(row: RepurchaseAnnual) {
    void detail.load(
      `/api/v1/market/repurchases?${queryString({
        view: 'annual',
        segment,
        year: row.year ?? '',
        detail_limit: 2000
      })}`
    );
  }

  function openHongKong(row: HongKongRepurchase) {
    if (!row.security) return;
    void detail.load(
      `/api/v1/market/repurchases?${queryString({
        view: 'hong-kong',
        market: row.security.market,
        code: row.security.code,
        include_details: 1,
        detail_limit: 2000
      })}`
    );
  }

  function gotoWorkbench() {
    if (!selected) return;
    app.setStock({ market: selected.market, code: selected.code, name: selected.name });
    router.go(stockPath(selected.market, selected.code));
  }

  const planColumns: Column<RepurchasePlan>[] = [
    {
      key: 'security',
      label: '证券',
      width: '128px',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => row.security.security_id
    },
    { key: 'board', label: '通过日', width: '84px', num: true, value: (row) => date(row.board_approval_date) },
    {
      key: 'status',
      label: '状态',
      width: '58px',
      value: (row) => (row.completed ? '已完成' : text(row.status || '进行中')),
      tone: (row) => (row.completed ? 'up' : '')
    },
    {
      key: 'planned',
      label: '计划上限',
      align: 'right',
      num: true,
      value: (row) => money(row.planned_amount_upper_yuan),
      sub: (row) => compact(row.planned_shares, '股'),
      sortValue: (row) => row.planned_amount_upper_yuan ?? 0
    },
    {
      key: 'actual',
      label: '实际金额',
      align: 'right',
      num: true,
      value: (row) => money(row.actual_amount_yuan),
      sub: (row) => compact(row.actual_shares, '股'),
      sortValue: (row) => row.actual_amount_yuan ?? 0
    },
    {
      key: 'completion',
      label: '完成率',
      align: 'right',
      num: true,
      value: (row) => percent(row.amount_completion_pct),
      sortValue: (row) => row.amount_completion_pct ?? 0
    },
    {
      key: 'pct',
      label: '占股本',
      align: 'right',
      num: true,
      value: (row) => percent(row.actual_capital_pct)
    },
    { key: 'purpose', label: '用途', wrap: true, value: (row) => text(row.purpose) },
    {
      key: 'window',
      label: '实施窗口',
      num: true,
      value: (row) => `${date(row.start_date)} — ${date(row.end_date)}`
    }
  ];

  const monthColumns: Column<RepurchaseMonth>[] = [
    { key: 'month', label: '月份', width: '76px', num: true, value: (row) => month6(row.month) },
    {
      key: 'planned-shares',
      label: '拟回购股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.planned_shares, '股'),
      sortValue: (row) => row.planned_shares ?? 0
    },
    {
      key: 'planned-amount',
      label: '拟回购金额',
      align: 'right',
      num: true,
      value: (row) => money(row.planned_amount_yuan),
      sortValue: (row) => row.planned_amount_yuan ?? 0
    },
    {
      key: 'actual-shares',
      label: '实际股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.actual_shares, '股'),
      sortValue: (row) => row.actual_shares ?? 0
    },
    {
      key: 'actual-amount',
      label: '实际金额',
      align: 'right',
      num: true,
      value: (row) => money(row.actual_amount_yuan),
      sortValue: (row) => row.actual_amount_yuan ?? 0
    },
    {
      key: 'completion',
      label: '完成率',
      align: 'right',
      num: true,
      value: (row) => percent(row.completion_pct),
      sortValue: (row) => row.completion_pct ?? 0
    },
    {
      key: 'capital',
      label: '实际占股本',
      align: 'right',
      num: true,
      value: (row) => percent(row.actual_capital_pct)
    }
  ];

  const annualColumns: Column<RepurchaseAnnual>[] = [
    { key: 'year', label: '年份', width: '64px', num: true, value: (row) => text(row.year) },
    { key: 'segment', label: '市场', width: '64px', value: (row) => row.segment_label },
    {
      key: 'companies',
      label: '回购家数',
      align: 'right',
      num: true,
      value: (row) => count(row.company_count),
      sortValue: (row) => row.company_count ?? 0
    },
    {
      key: 'shares',
      label: '回购数量',
      align: 'right',
      num: true,
      value: (row) => compact(row.shares, '股'),
      sortValue: (row) => row.shares ?? 0
    },
    {
      key: 'amount',
      label: '回购金额',
      align: 'right',
      num: true,
      value: (row) => money(row.amount_yuan),
      sortValue: (row) => row.amount_yuan ?? 0
    },
    {
      key: 'financing',
      label: '融资金额',
      align: 'right',
      num: true,
      value: (row) => money(row.financing_amount_yuan),
      sortValue: (row) => row.financing_amount_yuan ?? 0
    }
  ];

  const hongKongColumns: Column<HongKongRepurchase>[] = [
    {
      key: 'security',
      label: '证券',
      width: '128px',
      value: (row) => row.security?.name || row.security?.code || '—',
      sub: (row) => row.security?.security_id ?? ''
    },
    { key: 'date', label: '交易日', width: '84px', num: true, value: (row) => date(row.date) },
    { key: 'price', label: '均价', align: 'right', num: true, value: (row) => fixed(row.average_price, 3) },
    { key: 'close', label: '收盘', align: 'right', num: true, value: (row) => fixed(row.close, 3) },
    {
      key: 'premium',
      label: '溢价率',
      align: 'right',
      num: true,
      value: (row) => percent(row.premium_pct),
      tone: (row) => tone(row.premium_pct),
      sortValue: (row) => row.premium_pct ?? 0
    },
    {
      key: 'shares',
      label: '数量',
      align: 'right',
      num: true,
      value: (row) => compact(row.shares, '股'),
      sortValue: (row) => row.shares ?? 0
    },
    {
      key: 'amount',
      label: '金额',
      align: 'right',
      num: true,
      value: (row) => compact(row.amount, row.currency || ''),
      sortValue: (row) => row.amount ?? 0
    },
    { key: 'range', label: '最高 / 最低', num: true, value: (row) => `${fixed(row.high, 3)} / ${fixed(row.low, 3)}` },
    { key: 'method', label: '方式', wrap: true, value: (row) => text(row.method) }
  ];

  const rows = $derived(
    view === 'plans'
      ? doc?.counts.plans
      : view === 'monthly'
        ? doc?.counts.monthly
        : view === 'annual'
          ? doc?.counts.annual
          : doc?.counts.hong_kong
  );

  const asideTitle = $derived.by(() => {
    if (view === 'monthly') return month ? month6(month.month) : '月度进度';
    if (view === 'annual') return `${detail.data?.selected_year ?? '年度'} · ${SEGMENTS.find((item) => item.id === segment)?.label ?? ''}`;
    return selected ? selected.name || selected.code : '关联历史';
  });

  const asideEmpty = $derived(
    view === 'monthly' ? !month : !detail.loaded && !detail.busy
  );

  onMount(() => load());
</script>

<PageHeader
  eyebrow="QXFA + HGTJ + GGHG · 回购"
  title="股份回购"
  description="A 股回购方案与实施进度、滚动月度完成率、分市场年度统计与港股逐笔回购，落在同一条关联链上。"
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
      onRetry={() => load()}
      title={VIEWS.find((item) => item.id === view)?.label ?? ''}
      subtitle={doc ? `${count(rows ?? 0)} 行` : ''}
      empty={catalog.loaded && !catalog.busy && (rows ?? 0) === 0}
      emptyText="当前口径下没有回购记录"
    >
      {#snippet toolbar()}
        <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="股份回购视图" />
        {#if view === 'annual'}
          <Select
            value={segment}
            options={SEGMENTS}
            label="市场口径"
            width="140px"
            onChange={(next) => {
              segment = next;
              load();
            }}
          />
        {/if}
        {#if view === 'plans' || view === 'hong-kong'}
          <TextInput
            bind:value={query}
            icon="search"
            width="240px"
            label="检索当前视图"
            placeholder={view === 'plans' ? '股票、代码、用途、地区或状态' : '港股代码、币种或回购方式'}
            onEnter={() => load()}
          />
        {/if}
      {/snippet}

      {#if view === 'plans'}
        <DataTable
          columns={planColumns}
          rows={doc?.plans ?? []}
          onRowClick={openPlan}
          isActive={(row) => row.security.security_id === selected?.security_id}
        />
      {:else if view === 'monthly'}
        <DataTable
          columns={monthColumns}
          rows={doc?.monthly ?? []}
          onRowClick={(row) => (month = row)}
          isActive={(row) => row.month === month?.month}
        />
      {:else if view === 'annual'}
        <DataTable
          columns={annualColumns}
          rows={doc?.annual ?? []}
          onRowClick={openYear}
          isActive={(row) => row.year === detail.data?.selected_year}
        />
      {:else}
        <DataTable
          columns={hongKongColumns}
          rows={doc?.hong_kong ?? []}
          onRowClick={openHongKong}
          isActive={(row) => row.security?.security_id === selected?.security_id}
        />
      {/if}
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="LINKED DETAIL"
      title={asideTitle}
      subtitle={view === 'monthly' ? '拟回购与实际实施对照' : (detail.data?.mode ?? '点击左侧任意行展开')}
      busy={view === 'monthly' ? false : detail.busy}
      error={view === 'monthly' ? '' : detail.error}
      empty={asideEmpty}
      emptyText={view === 'plans'
        ? '点击左侧股票，读取该票的全部回购方案历史。'
        : view === 'monthly'
          ? '点击左侧月份，对照当月计划与实际实施。'
          : view === 'annual'
            ? '点击左侧年份，展开该年逐月回购统计。'
            : '点击左侧港股，读取逐交易日回购历史。'}
    >
      {#if selected && view === 'plans'}
        <button class="jump" type="button" onclick={gotoWorkbench}>
          在个股工作台打开 {selected.name || selected.code}
        </button>
      {/if}

      {#if view === 'plans'}
        <ul class="events">
          {#each detail.data?.plans ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(row.board_approval_date)}</time>
                <span class={row.completed ? 'up' : 'dim'}>{row.completed ? '已完成' : '进行中'}</span>
              </div>
              <strong>{text(row.purpose)}</strong>
              <p class="num">
                上限 {money(row.planned_amount_upper_yuan)} · 实际 {money(row.actual_amount_yuan)} ·
                完成 {percent(row.amount_completion_pct)}
              </p>
              <small>
                {date(row.start_date)} — {date(row.end_date)} · 截止 {date(row.cutoff_date)} ·
                实际 {compact(row.actual_shares, '股')} · 价格上限 {fixed(row.planned_price_upper)}
              </small>
            </li>
          {/each}
        </ul>
      {:else if view === 'monthly' && month}
        <dl class="pairs">
          <div><dt>拟回购金额</dt><dd class="num">{money(month.planned_amount_yuan)}</dd></div>
          <div><dt>实际金额</dt><dd class="num">{money(month.actual_amount_yuan)}</dd></div>
          <div><dt>拟回购股数</dt><dd class="num">{compact(month.planned_shares, '股')}</dd></div>
          <div><dt>实际股数</dt><dd class="num">{compact(month.actual_shares, '股')}</dd></div>
          <div><dt>拟占股本</dt><dd class="num">{percent(month.planned_capital_pct)}</dd></div>
          <div><dt>实际占股本</dt><dd class="num">{percent(month.actual_capital_pct)}</dd></div>
          <div><dt>金额完成率</dt><dd class="num">{percent(month.completion_pct)}</dd></div>
        </dl>
      {:else if view === 'annual'}
        <ul class="events">
          {#each detail.data?.annual_monthly ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{month6(row.month)}</time>
                <span class="num">{money(row.amount_yuan)}</span>
              </div>
              <p class="num">{compact(row.shares, '股')} · {count(row.company_count)} 家公司</p>
              <small>融资 {money(row.financing_amount_yuan)}</small>
            </li>
          {/each}
        </ul>
      {:else if view === 'hong-kong'}
        <ul class="events">
          {#each detail.data?.hong_kong_history ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(row.date)}</time>
                <span class={tone(row.premium_pct)}>{percent(row.premium_pct)}</span>
              </div>
              <strong class="num">{fixed(row.average_price, 3)} {text(row.currency)}</strong>
              <p class="num">{compact(row.shares, '股')} · {compact(row.amount, row.currency || '')}</p>
              <small>{text(row.method)}</small>
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

  .pairs {
    display: flex;
    flex-direction: column;
    margin: 0;
  }

  .pairs div {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: var(--sp-3);
    padding: var(--sp-2) 0;
    border-bottom: 1px solid var(--line);
  }

  .pairs div:last-child {
    border-bottom: 0;
  }

  dt {
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  dd {
    margin: 0;
    font-size: var(--fs-body);
    color: var(--fg);
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
