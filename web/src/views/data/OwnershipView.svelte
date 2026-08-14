<script lang="ts">
  /**
   * 股权变动。数据页的参考实现——其余数据页沿用同一骨架：
   * PageHeader（来源 + 摘要指标） → Panel/toolbar（视图切换 + 口径 + 检索）
   * → Split（左主表 DataTable，右关联详情）。
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
    InsiderChange,
    MarketOwnershipDocument,
    NoReductionCommitment,
    OwnershipChange,
    OwnershipPlan,
    PledgeInstitution,
    PledgeRecord,
    ValuationSecurity
  } from '../../types';

  type View =
    | 'changes'
    | 'plans'
    | 'insiders'
    | 'commitments'
    | 'pledges'
    | 'statistics'
    | 'institutions';

  const VIEWS = [
    { id: 'changes', label: '实际增减持', hint: '股东已完成的变动历史' },
    { id: 'plans', label: '拟增减持', hint: '计划规模与实施窗口' },
    { id: 'insiders', label: '董监高', hint: '内部人持股变动' },
    { id: 'commitments', label: '承诺不减持', hint: '承诺人、身份与期限' },
    { id: 'pledges', label: '股权质押', hint: '质押、预警、平仓与解押' },
    { id: 'statistics', label: '市场统计', hint: '月度与年度资金' },
    { id: 'institutions', label: '质押机构', hint: '信托与券商' }
  ];

  const CATEGORIES: Record<View, Array<{ id: string; label: string }>> = {
    changes: [
      { id: 'all', label: '全部变动' },
      { id: 'increase', label: '增持' },
      { id: 'decrease', label: '减持' }
    ],
    plans: [
      { id: 'all', label: '全部计划' },
      { id: 'increase', label: '拟增持' },
      { id: 'decrease', label: '拟减持' }
    ],
    insiders: [
      { id: 'all', label: '全部变动' },
      { id: 'increase', label: '增持' },
      { id: 'decrease', label: '减持' }
    ],
    commitments: [
      { id: 'all', label: '全部承诺' },
      { id: 'active', label: '承诺期内' },
      { id: 'upcoming', label: '即将生效' },
      { id: 'expired', label: '已到期' }
    ],
    pledges: [
      { id: 'latest', label: '最新质押' },
      { id: 'warning', label: '预警提示' },
      { id: 'liquidation', label: '平仓提示' },
      { id: 'release', label: '最新解押' },
      { id: 'all', label: '全部' }
    ],
    statistics: [
      { id: 'all', label: '全部统计' },
      { id: 'changes', label: '增减持统计' },
      { id: 'pledges', label: '质押统计' }
    ],
    institutions: [
      { id: 'all', label: '全部机构' },
      { id: 'trust', label: '信托' },
      { id: 'broker', label: '券商' }
    ]
  };

  let view = $state<View>('changes');
  let category = $state('all');
  let query = $state('');

  const catalog = new Resource<MarketOwnershipDocument>();
  const detail = new Resource<MarketOwnershipDocument>();

  const doc = $derived(catalog.data);
  const summary = $derived(doc?.summary);

  const stats = $derived.by(() => {
    if (!summary) return [];
    return [
      { label: '实际增/减持', value: `${summary.increase_changes} / ${summary.decrease_changes}` },
      { label: '董监高增/减', value: `${summary.insider_increases} / ${summary.insider_decreases}` },
      { label: '有效/全部承诺', value: `${summary.active_commitments} / ${summary.commitments}` },
      { label: '拟增减持计划', value: count(summary.plans) },
      { label: '质押风险记录', value: count(summary.pledge_records) },
      { label: '覆盖证券/机构', value: `${summary.unique_securities} / ${summary.institutions}` }
    ];
  });

  function load(refresh = false) {
    void catalog.load(
      `/api/v1/market/ownership?${queryString({
        view,
        category,
        q: query.trim(),
        limit: 2000,
        refresh: refresh ? 1 : 0
      })}`
    );
    detail.reset();
  }

  function switchView(next: string) {
    view = next as View;
    category = CATEGORIES[view][0].id;
    query = '';
    load();
  }

  function openSecurity(security: ValuationSecurity | undefined) {
    if (!security) return;
    void detail.load(
      `/api/v1/market/ownership?${queryString({
        view,
        category,
        market: security.market,
        code: security.code,
        include_details: 1,
        detail_limit: 2000
      })}`
    );
  }

  function openInstitution(row: PledgeInstitution) {
    void detail.load(
      `/api/v1/market/ownership?${queryString({
        view: 'institutions',
        category: row.category,
        institution_id: row.institution_id,
        include_details: 1,
        detail_limit: 2000
      })}`
    );
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

  const changeColumns: Column<OwnershipChange>[] = [
    securityColumn<OwnershipChange>(),
    {
      key: 'direction',
      label: '方向',
      width: '54px',
      value: (row) => row.direction_label,
      tone: (row) => (row.direction === 'increase' ? 'up' : 'down')
    },
    { key: 'announce', label: '公告日', width: '84px', num: true, value: (row) => date(row.announcement_date) },
    {
      key: 'shares',
      label: '变动股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.change_shares, '股'),
      tone: (row) => tone(row.change_shares),
      sortValue: (row) => row.change_shares ?? 0
    },
    { key: 'price', label: '成交均价', align: 'right', num: true, value: (row) => fixed(row.average_price) },
    {
      key: 'after',
      label: '变动后持股',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_after_shares, '股'),
      sub: (row) => percent(row.holding_after_capital_pct)
    },
    { key: 'actor', label: '变动人', wrap: true, value: (row) => text(row.actor) },
    {
      key: 'window',
      label: '区间',
      num: true,
      value: (row) => `${date(row.start_date)} — ${date(row.end_date)}`
    }
  ];

  const planColumns: Column<OwnershipPlan>[] = [
    securityColumn<OwnershipPlan>(),
    {
      key: 'direction',
      label: '方向',
      width: '54px',
      value: (row) => row.direction_label,
      tone: (row) => (row.direction === 'increase' ? 'up' : 'down')
    },
    { key: 'announce', label: '公告日', width: '84px', num: true, value: (row) => date(row.announcement_date) },
    { key: 'scale', label: '计划规模', align: 'right', num: true, value: (row) => `${row.range} ${row.scale}` },
    {
      key: 'pct',
      label: '占股本',
      align: 'right',
      num: true,
      value: (row) => percent(row.capital_pct),
      sortValue: (row) => row.capital_pct ?? 0
    },
    { key: 'actor', label: '计划人', wrap: true, value: (row) => text(row.actor) },
    { key: 'method', label: '方式', value: (row) => text(row.method) },
    {
      key: 'window',
      label: '实施窗口',
      num: true,
      value: (row) => `${date(row.start_date)} — ${date(row.end_date)}`
    }
  ];

  const insiderColumns: Column<InsiderChange>[] = [
    securityColumn<InsiderChange>(),
    {
      key: 'direction',
      label: '方向',
      width: '54px',
      value: (row) => row.direction_label,
      tone: (row) => (row.direction === 'increase' ? 'up' : 'down')
    },
    { key: 'date', label: '变动日', width: '84px', num: true, value: (row) => date(row.date) },
    {
      key: 'shares',
      label: '股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.change_shares, '股'),
      tone: (row) => tone(row.change_shares),
      sortValue: (row) => row.change_shares ?? 0
    },
    {
      key: 'amount',
      label: '金额',
      align: 'right',
      num: true,
      value: (row) => compact(row.change_amount_yuan, '元'),
      tone: (row) => tone(row.change_amount_yuan)
    },
    { key: 'actor', label: '变动人', value: (row) => text(row.actor) },
    { key: 'position', label: '职务', wrap: true, value: (row) => text(row.position) },
    { key: 'reason', label: '原因', value: (row) => text(row.reason) }
  ];

  const commitmentColumns: Column<NoReductionCommitment>[] = [
    securityColumn<NoReductionCommitment>(),
    {
      key: 'status',
      label: '状态',
      width: '66px',
      value: (row) => row.status_label,
      tone: (row) => (row.status === 'expired' ? 'down' : row.status === 'active' ? 'up' : 'flat')
    },
    { key: 'announce', label: '公告日', width: '84px', num: true, value: (row) => date(row.announcement_date) },
    { key: 'actor', label: '承诺人', wrap: true, value: (row) => text(row.actor) },
    { key: 'identity', label: '身份', wrap: true, value: (row) => text(row.identity) },
    {
      key: 'window',
      label: '承诺期',
      num: true,
      value: (row) => `${date(row.start_date)} — ${date(row.end_date)}`
    },
    { key: 'purpose', label: '目的', wrap: true, value: (row) => text(row.purpose) }
  ];

  const pledgeColumns: Column<PledgeRecord>[] = [
    securityColumn<PledgeRecord>(),
    {
      key: 'kind',
      label: '类型',
      width: '66px',
      value: (row) => row.kind_label,
      tone: (row) =>
        row.kind === 'liquidation' ? 'down' : row.kind === 'release' ? 'up' : 'flat'
    },
    {
      key: 'date',
      label: '日期 / 区间',
      width: '100px',
      num: true,
      value: (row) =>
        row.release_date
          ? date(row.release_date)
          : row.announcement_date
            ? date(row.announcement_date)
            : text(row.price_range)
    },
    {
      key: 'shares',
      label: '影响股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.released_shares ?? row.affected_shares ?? row.cumulative_shares, '股')
    },
    {
      key: 'pct',
      label: '占比',
      align: 'right',
      num: true,
      value: (row) => percent(row.affected_pct ?? row.cumulative_capital_pct)
    },
    { key: 'holder', label: '股东 / 关系', wrap: true, value: (row) => text(row.shareholder || row.relationship) },
    { key: 'status', label: '状态', value: (row) => text(row.risk_status || row.pledgee) }
  ];

  const institutionColumns: Column<PledgeInstitution>[] = [
    { key: 'name', label: '机构', wrap: true, value: (row) => row.name, sub: (row) => `ID ${row.institution_id}` },
    { key: 'category', label: '类型', width: '54px', value: (row) => row.category_label },
    {
      key: 'pledges',
      label: '质押笔数',
      align: 'right',
      num: true,
      value: (row) => count(row.pledge_count),
      sortValue: (row) => row.pledge_count ?? 0
    },
    {
      key: 'companies',
      label: '公司数',
      align: 'right',
      num: true,
      value: (row) => count(row.company_count),
      sortValue: (row) => row.company_count ?? 0
    },
    {
      key: 'value',
      label: '质押市值',
      align: 'right',
      num: true,
      value: (row) => compact(row.market_value_yuan, '元'),
      sortValue: (row) => row.market_value_yuan ?? 0
    },
    { key: 'warn', label: '预警占比', align: 'right', num: true, value: (row) => percent(row.warning_pct) },
    {
      key: 'liq',
      label: '平仓占比',
      align: 'right',
      num: true,
      value: (row) => percent(row.liquidation_pct),
      tone: () => 'down'
    }
  ];

  const selected = $derived(detail.data?.selected_security ?? null);

  onMount(() => load());
</script>

<PageHeader
  eyebrow="ZCJC + GQZY · 709/1721"
  title="股权变动"
  description="股东增减持计划与实际变动、董监高交易、承诺不减持、质押风险与质押机构，合并在同一条关联链上。"
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
      subtitle={doc ? `${count(doc.counts[view] ?? 0)} 行` : ''}
    >
      {#snippet toolbar()}
        <Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="股权变动视图" />
        <Select
          value={category}
          options={CATEGORIES[view]}
          label="口径"
          width="150px"
          onChange={(next) => {
            category = next;
            load();
          }}
        />
        {#if view !== 'statistics'}
          <TextInput
            bind:value={query}
            icon="search"
            width="220px"
            label="检索当前视图"
            placeholder={view === 'institutions' ? '机构名称或 ID' : '股票、股东、方式或风险状态'}
            onEnter={() => load()}
          />
        {/if}
      {/snippet}

      {#if view === 'changes'}
        <DataTable
          columns={changeColumns}
          rows={doc?.changes ?? []}
          onRowClick={(row) => openSecurity(row.security)}
          isActive={(row) => row.security?.security_id === selected?.security_id}
        />
      {:else if view === 'plans'}
        <DataTable
          columns={planColumns}
          rows={doc?.plans ?? []}
          onRowClick={(row) => openSecurity(row.security)}
          isActive={(row) => row.security?.security_id === selected?.security_id}
        />
      {:else if view === 'insiders'}
        <DataTable
          columns={insiderColumns}
          rows={doc?.insiders ?? []}
          onRowClick={(row) => openSecurity(row.security)}
          isActive={(row) => row.security?.security_id === selected?.security_id}
        />
      {:else if view === 'commitments'}
        <DataTable
          columns={commitmentColumns}
          rows={doc?.commitments ?? []}
          onRowClick={(row) => openSecurity(row.security)}
          isActive={(row) => row.security?.security_id === selected?.security_id}
        />
      {:else if view === 'pledges'}
        <DataTable
          columns={pledgeColumns}
          rows={doc?.pledges ?? []}
          onRowClick={(row) => openSecurity(row.security)}
          isActive={(row) => row.security?.security_id === selected?.security_id}
        />
      {:else if view === 'institutions'}
        <DataTable
          columns={institutionColumns}
          rows={doc?.institutions ?? []}
          onRowClick={openInstitution}
          isActive={(row) => row.institution_id === detail.data?.selected_institution_id}
        />
      {:else}
        <div class="stat-stack">
          <h3>增减持月度资金</h3>
          <DataTable
            columns={[
              { key: 'period', label: '月份', num: true, value: (row) => date(row.period) },
              { key: 'inc', label: '增持额', align: 'right', num: true, value: (row) => compact(row.increase_amount_yuan, '元'), tone: () => 'up' },
              { key: 'dec', label: '减持额', align: 'right', num: true, value: (row) => compact(row.decrease_amount_yuan, '元'), tone: () => 'down' },
              { key: 'net', label: '净额', align: 'right', num: true, value: (row) => compact(row.net_amount_yuan, '元'), tone: (row) => tone(row.net_amount_yuan) },
              { key: 'co', label: '增/减公司', align: 'right', num: true, value: (row) => `${row.increase_companies ?? '—'} / ${row.decrease_companies ?? '—'}` }
            ]}
            rows={doc?.change_monthly ?? []}
          />
          {#if doc?.change_count_reconciliation}
            <h3>增减持公司数图表投影</h3>
            <p class:warn-line={!doc.change_count_reconciliation.all_overlaps_match} class="reconciliation">
              主表 {count(doc.change_count_reconciliation.master_rows)} 期，图表 {count(doc.change_count_reconciliation.chart_rows)} 期；
              重叠月份匹配 {count(doc.change_count_reconciliation.matched_rows)} 期，差异 {count(doc.change_count_reconciliation.mismatch_rows)} 期。
              图表快照可能晚于或早于主表刷新，并非强制要求完全一致。
            </p>
            <DataTable
              columns={[
                { key: 'period', label: '月份', num: true, value: (row) => date(row.period) },
                { key: 'inc', label: '增持公司', align: 'right', num: true, value: (row) => count(row.increase_companies) },
                { key: 'dec', label: '减持公司', align: 'right', num: true, value: (row) => count(row.decrease_companies) }
              ]}
              rows={doc.change_count_trend ?? []}
            />
          {/if}
          <h3>股权质押月度统计</h3>
          <DataTable
            columns={[
              { key: 'month', label: '月份', num: true, value: (row) => date(row.month) },
              { key: 'co', label: '公司数', align: 'right', num: true, value: (row) => count(row.company_count) },
              { key: 'cnt', label: '质押笔数', align: 'right', num: true, value: (row) => count(row.pledge_count) },
              { key: 'shares', label: '累计质押', align: 'right', num: true, value: (row) => compact((row.restricted_shares ?? 0) + (row.unrestricted_shares ?? 0), '股') },
              { key: 'pct', label: '占总股本', align: 'right', num: true, value: (row) => percent(row.cumulative_capital_pct) }
            ]}
            rows={doc?.pledge_monthly ?? []}
          />
        </div>
      {/if}
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="LINKED DETAIL"
      title={selected?.name || detail.data?.selected_institution_id
        ? selected?.name || `机构 ${detail.data?.selected_institution_id}`
        : '关联历史'}
      subtitle={detail.data?.mode ?? '点击左侧任意行展开'}
      busy={detail.busy}
      error={detail.error}
      empty={!detail.loaded && !detail.busy}
      emptyText="点击左侧任意行，读取该证券或机构的完整关联历史。"
    >
      {#if selected}
        <button class="jump" type="button" onclick={() => gotoWorkbench(selected)}>
          在个股工作台打开 {selected.name || selected.code}
        </button>
      {/if}

      {#if view === 'changes' || view === 'plans'}
        <ul class="events">
          {#each detail.data?.change_history ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(row.announcement_date)}</time>
                <span class={row.direction === 'increase' ? 'up' : 'down'}>{row.direction_label}</span>
              </div>
              <strong>{text(row.actor)}</strong>
              <p class="num">{compact(row.change_shares, '股')} · 均价 {fixed(row.average_price)}</p>
              <small>
                {date(row.start_date)} — {date(row.end_date)} · 变动后持有
                {compact(row.holding_after_shares, '股')}
              </small>
            </li>
          {/each}
        </ul>
      {:else if view === 'insiders'}
        <ul class="events">
          {#each detail.data?.insider_history ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(row.date)}</time>
                <span class={row.direction === 'increase' ? 'up' : 'down'}>{row.direction_label}</span>
              </div>
              <strong>{text(row.actor)} · {text(row.position)}</strong>
              <p class="num">{compact(row.change_shares, '股')} · {compact(row.change_amount_yuan, '元')}</p>
              <small>{text(row.reason)} · {text(row.relationship)}</small>
            </li>
          {/each}
        </ul>
      {:else if view === 'pledges'}
        <ul class="events">
          {#each detail.data?.pledge_history ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(row.pledge_date)}</time>
                <span class={row.risk_status && row.risk_status !== '安全' ? 'down' : 'dim'}>
                  {text(row.risk_status)}
                </span>
              </div>
              <strong>{text(row.shareholder)}</strong>
              <p class="num">{compact(row.shares, '股')} · {text(row.pledgee)}</p>
              <small>
                占所持 {percent(row.single_holder_pct)} · 占总股本 {percent(row.single_capital_pct)} ·
                到期 {date(row.maturity_date)}
              </small>
            </li>
          {/each}
        </ul>
      {:else if view === 'institutions'}
        <ul class="events">
          {#each detail.data?.institution_details ?? [] as row, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(row.pledge_date)}</time>
                <span class="dim">{text(row.risk_status)}</span>
              </div>
              <strong>{row.security.name || row.security.code}</strong>
              <p class="num">{compact(row.shares, '股')} · 占股本 {percent(row.capital_pct)}</p>
              <small>{text(row.shareholder)} · {row.security.security_id}</small>
            </li>
          {/each}
        </ul>
      {:else if view === 'statistics'}
        <ul class="events">
          {#each doc?.change_annual ?? [] as row (row.period)}
            <li>
              <div class="row">
                <time class="num">{row.period}</time>
                <span class={tone(row.net_amount_yuan)}>{compact(row.net_amount_yuan, '元')}</span>
              </div>
              <p class="num">
                增 {compact(row.increase_amount_yuan, '元')} · 减
                {compact(row.decrease_amount_yuan, '元')}
              </p>
              <small>{row.increase_companies ?? '—'} / {row.decrease_companies ?? '—'} 家公司</small>
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
  .stat-stack h3 {
    padding: var(--sp-3) var(--sp-4) var(--sp-2);
    font-size: var(--fs-title);
    font-weight: 600;
    border-top: 1px solid var(--line);
  }

  .stat-stack h3:first-child {
    border-top: 0;
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

  .reconciliation {
    margin: 0;
    padding: var(--sp-2) var(--sp-4);
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }
</style>
