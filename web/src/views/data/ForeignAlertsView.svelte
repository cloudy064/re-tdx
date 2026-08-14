<script lang="ts">
  /**
   * 外资持股预警。主表是通达信当前预警清单，右栏按股票展开持股与状态历史。
   *
   * 持股占比逼近或触发上限属于风险提示而不是涨跌，所以一律用 warn 语义色；
   * 只有持股数量的当日增减才用红涨绿跌。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { Resource } from '../../lib/resource.svelte';
  import { compact, count, date, percent, text, tone } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    ForeignAlertRecord,
    ForeignAlertStatusKey,
    MarketForeignAlertsDocument,
    ValuationSecurity
  } from '../../types';

  const STATUSES = [
    { id: 'all', label: '全部状态' },
    { id: 'forced-reduction', label: '强制减仓' },
    { id: 'suspended-buy', label: '暂停买入' },
    { id: 'warning', label: '预警' },
    { id: 'approaching', label: '逼近预警' },
    { id: 'unknown', label: '其他状态' }
  ];

  let status = $state('all');
  let query = $state('');

  const catalog = new Resource<MarketForeignAlertsDocument>();
  const detail = new Resource<MarketForeignAlertsDocument>();

  const doc = $derived(catalog.data);
  const summary = $derived(doc?.summary);
  const selected = $derived(detail.data?.selected_alert ?? null);

  /** 预警强度分三档：已触发上限 / 已预警 / 逼近，都不是涨跌语义。 */
  function statusTone(key: ForeignAlertStatusKey): 'warn' | 'focus' | 'neutral' {
    if (key === 'forced-reduction' || key === 'suspended-buy' || key === 'warning') return 'warn';
    if (key === 'approaching') return 'focus';
    return 'neutral';
  }

  const stats = $derived.by<Stat[]>(() => {
    if (!summary) return [];
    return [
      { label: '数据日期', value: date(summary.date), note: '当前上游清单' },
      {
        label: '预警股票',
        value: count(summary.securities),
        note: `预警 ${summary.warning} · 逼近 ${summary.approaching}`
      },
      {
        label: '强制减仓 / 暂停买入',
        value: `${summary.forced_reduction} / ${summary.suspended_buy}`,
        note: `其他状态 ${summary.unknown}`
      },
      {
        label: '清单持股合计',
        value: compact(summary.foreign_holding_shares, '股'),
        note: '仅当前清单，不代表全市场外资持仓'
      },
      {
        label: '当日持股变动',
        value: compact(summary.daily_change_shares, '股'),
        tone: tone(summary.daily_change_shares),
        note: '清单内合计'
      },
      {
        label: '最高持股比例',
        value: percent(summary.maximum_holding_ratio_pct),
        note: '距离上限最近的一只'
      }
    ];
  });

  function load(refresh = false) {
    void catalog.load(
      `/api/v1/market/foreign-alerts?${queryString({
        status,
        q: query.trim(),
        limit: 5000,
        refresh: refresh ? 1 : 0
      })}`
    );
    detail.reset();
  }

  function openAlert(row: ForeignAlertRecord) {
    void detail.load(
      `/api/v1/market/foreign-alerts?${queryString({
        market: row.security.market,
        code: row.security.code,
        include_details: 1,
        detail_limit: 2000
      })}`
    );
  }

  function gotoWorkbench(security: ValuationSecurity) {
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code));
  }

  const selectedStats = $derived.by<Stat[]>(() => {
    if (!selected) return [];
    return [
      { label: '当前状态', value: text(selected.status) },
      { label: '持股比例', value: percent(selected.foreign_holding_ratio_pct) },
      { label: '外资持股', value: compact(selected.foreign_holding_shares, '股') },
      {
        label: '当日变化',
        value: compact(selected.daily_change_shares, '股'),
        tone: tone(selected.daily_change_shares)
      },
      { label: '历史记录', value: count(detail.data?.counts.history ?? 0) },
      { label: '记录日期', value: date(selected.date) }
    ];
  });

  const alertColumns: Column<ForeignAlertRecord>[] = [
    {
      key: 'security',
      label: '股票',
      width: '128px',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => row.security.security_id
    },
    { key: 'status', label: '状态', width: '92px', slot: true },
    {
      key: 'shares',
      label: '外资持股',
      align: 'right',
      num: true,
      value: (row) => compact(row.foreign_holding_shares, '股'),
      sortValue: (row) => row.foreign_holding_shares ?? 0
    },
    { key: 'ratio', label: '占总股本', align: 'right', num: true, slot: true, sortValue: (row) => row.foreign_holding_ratio_pct ?? 0 },
    {
      key: 'change',
      label: '当日变化',
      align: 'right',
      num: true,
      value: (row) => compact(row.daily_change_shares, '股'),
      tone: (row) => tone(row.daily_change_shares),
      sortValue: (row) => row.daily_change_shares ?? 0
    },
    {
      key: 'change_pct',
      label: '变化比例',
      align: 'right',
      num: true,
      value: (row) => percent(row.daily_change_pct, 2, true),
      tone: (row) => tone(row.daily_change_pct),
      sortValue: (row) => row.daily_change_pct ?? 0
    },
    { key: 'date', label: '日期', width: '84px', num: true, value: (row) => date(row.date) }
  ];

  const historyColumns: Column<ForeignAlertRecord>[] = [
    { key: 'date', label: '日期', width: '78px', num: true, value: (row) => date(row.date) },
    { key: 'status', label: '状态', width: '84px', slot: true },
    { key: 'ratio', label: '占比', align: 'right', num: true, slot: true },
    {
      key: 'shares',
      label: '持股',
      align: 'right',
      num: true,
      value: (row) => compact(row.foreign_holding_shares, '股')
    },
    {
      key: 'change',
      label: '当日变化',
      align: 'right',
      num: true,
      value: (row) => compact(row.daily_change_shares, '股'),
      tone: (row) => tone(row.daily_change_shares)
    }
  ];

  onMount(() => load());
</script>

<PageHeader
  eyebrow="WZMRYJ · FOREIGN OWNERSHIP"
  title="外资预警"
  description="通达信当前外资持股预警清单，展开单票可查持股数量、占总股本比例与预警状态的历史变化。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={catalog.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="380px">
  {#snippet main()}
    <Panel
      flush
      scroll
      busy={catalog.busy}
      error={catalog.error}
      empty={catalog.loaded && !catalog.busy && (doc?.alerts.length ?? 0) === 0}
      emptyText="当前筛选条件下没有外资持股预警记录。"
      onRetry={() => load()}
      title="外资持股预警清单"
      subtitle={doc ? `${count(doc.counts.alerts)} 条 · 持股单位已由万股换算为股` : '持股单位已由万股换算为股'}
    >
      {#snippet toolbar()}
        <Select
          value={status}
          options={STATUSES}
          label="状态"
          width="160px"
          onChange={(next) => {
            status = next;
            load();
          }}
        />
        <TextInput
          bind:value={query}
          icon="search"
          width="220px"
          label="检索预警清单"
          placeholder="股票名称、代码或状态"
          onEnter={() => load()}
        />
      {/snippet}

      <DataTable
        columns={alertColumns}
        rows={doc?.alerts ?? []}
        rowKey={(row) => row.security.security_id}
        onRowClick={openAlert}
        isActive={(row) => row.security.security_id === selected?.security.security_id}
      >
        {#snippet cell({ row, column })}
          {#if column.key === 'status'}
            <Badge tone={statusTone(row.status_key)}>{text(row.status)}</Badge>
          {:else}
            <span class:risk={row.status_key !== 'unknown'}>
              {percent(row.foreign_holding_ratio_pct)}
            </span>
          {/if}
        {/snippet}
      </DataTable>
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="LINKED DETAIL"
      title={selected ? selected.security.name || selected.security.code : '持股状态历史'}
      subtitle={detail.data?.mode ?? '点击左侧任意股票展开'}
      busy={detail.busy}
      error={detail.error}
      empty={!detail.loaded && !detail.busy}
      emptyText="点击左侧股票，查看外资持股数量、比例与预警状态的历史变化。"
    >
      {#if selected}
        {@const security = selected.security}
        <button class="jump" type="button" onclick={() => gotoWorkbench(security)}>
          在个股工作台打开 {security.name || security.code}
        </button>

        <StatGrid stats={selectedStats} inline />

        <h3>状态历史</h3>
        {#if (detail.data?.history.length ?? 0) === 0}
          <p class="note">该股票在主表中，但动态详情暂未返回历史记录。</p>
        {:else}
          <DataTable
            columns={historyColumns}
            rows={detail.data?.history ?? []}
            rowKey={(row, index) => `${row.date}-${index}`}
          >
            {#snippet cell({ row, column })}
              {#if column.key === 'status'}
                <Badge tone={statusTone(row.status_key)}>{text(row.status)}</Badge>
              {:else}
                <span class:risk={row.status_key !== 'unknown'}>
                  {percent(row.foreign_holding_ratio_pct)}
                </span>
              {/if}
            {/snippet}
          </DataTable>
        {/if}

        <p class="note">
          状态名称按通达信上游原文展示；主表持股由万股换算为股，详情本身已经是股。
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

  /* 逼近或触发持股上限是风险提示，不用涨跌色 */
  .risk {
    color: var(--warn);
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
