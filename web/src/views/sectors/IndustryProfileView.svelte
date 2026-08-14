<script lang="ts">
  /**
   * 行业机构画像。
   *
   * 上游是两条链：`hycgmx/<一级行业>` 给一级行业最近 10 个报告期的机构持仓，
   * `hygdrs/<市场号><二级行业>` 给二级行业内逐股的股东结构。两者层级不同，
   * 所以左侧用两级行业树选中，右侧按选中层级决定哪一条链有数据。
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
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    IndustryHoldingPeriod,
    IndustryProfileDocument,
    IndustryProfileNode,
    IndustrySecurityProfile
  } from '../../types';

  const master = new Resource<IndustryProfileDocument>();
  const detail = new Resource<IndustryProfileDocument>();

  let filter = $state('');
  let selectedCode = $state('');
  /** 记录被折叠的一级行业。默认全展开——一级只有 30 个，藏起来只会增加点击成本。 */
  let collapsed = $state<Set<string>>(new Set());

  const tree = $derived(master.data?.tree ?? []);

  const visibleTree = $derived.by(() => {
    const keyword = filter.trim();
    if (!keyword) return tree;
    return tree
      .map((node) => {
        const children = (node.children ?? []).filter((child) => child.name.includes(keyword));
        if (node.name.includes(keyword)) return node;
        return children.length ? { ...node, children } : null;
      })
      .filter((node): node is IndustryProfileNode => node !== null);
  });

  const stats = $derived.by<Stat[]>(() => {
    const counts = master.data?.counts;
    if (!counts) return [];
    return [
      { label: '行业节点', value: count(counts.industries) },
      { label: '有机构持仓', value: count(counts.holding_industries) },
      { label: '有股东结构', value: count(counts.shareholder_industries) },
      { label: '持仓主表行', value: count(counts.holding_master_rows) },
      { label: '股东主表行', value: count(counts.shareholder_master_rows) },
      { label: '持仓历史点', value: count(counts.holding_history_points) }
    ];
  });

  const selectedNode = $derived(detail.data?.selected_industry ?? null);
  const holdings = $derived(detail.data?.holdings_history ?? []);
  const securities = $derived(detail.data?.shareholder_securities ?? []);
  const shareholderProfile = $derived(selectedNode?.shareholder_profile ?? null);

  function loadMaster(refresh = false) {
    void master.load(`/api/v1/market/industry-profile?${queryString({ refresh: refresh ? 1 : 0 })}`);
  }

  function selectIndustry(node: IndustryProfileNode) {
    selectedCode = node.code;
    void detail.load(
      `/api/v1/market/industry-profile?${queryString({ industry: node.code, detail_limit: 2000 })}`
    );
  }

  function toggle(code: string) {
    const next = new Set(collapsed);
    if (next.has(code)) next.delete(code);
    else next.add(code);
    collapsed = next;
  }

  function openStock(profile: IndustrySecurityProfile) {
    const security = profile.security;
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code));
  }

  const holdingColumns: Column<IndustryHoldingPeriod>[] = [
    {
      key: 'period',
      label: '报告期',
      width: '92px',
      num: true,
      value: (row) => row.period_label || date(row.report_date)
    },
    {
      key: 'value',
      label: '持仓市值',
      align: 'right',
      num: true,
      value: (row) => compact(row.metrics.market_value, '元')
    },
    {
      key: 'valueChange',
      label: '市值环比',
      align: 'right',
      num: true,
      value: (row) => percent(row.metrics.market_value_change_pct, 2, true),
      tone: (row) => tone(num(row.metrics.market_value_change_pct))
    },
    {
      key: 'institutions',
      label: '机构家数',
      align: 'right',
      num: true,
      value: (row) => count(row.metrics.institution_count),
      sub: (row) => {
        const change = num(row.metrics.institution_count_change);
        return change ? `${change > 0 ? '+' : ''}${change}` : '';
      }
    },
    {
      key: 'shares',
      label: '持股数',
      align: 'right',
      num: true,
      value: (row) => compact(row.metrics.shares_held, '股')
    },
    {
      key: 'sharesChange',
      label: '持股环比',
      align: 'right',
      num: true,
      value: (row) => percent(row.metrics.shares_change_pct, 2, true),
      tone: (row) => tone(num(row.metrics.shares_change_pct))
    },
    {
      key: 'floatPct',
      label: '占流通',
      align: 'right',
      num: true,
      value: (row) => percent(row.metrics.float_share_pct)
    },
    {
      key: 'totalPct',
      label: '占总股本',
      align: 'right',
      num: true,
      value: (row) => percent(row.metrics.total_share_pct)
    }
  ];

  const securityColumns: Column<IndustrySecurityProfile>[] = [
    {
      key: 'security',
      label: '证券',
      width: '120px',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => row.security.security_id
    },
    {
      key: 'households',
      label: '股东户数',
      align: 'right',
      num: true,
      value: (row) => count(row.shareholders.households),
      sortValue: (row) => num(row.shareholders.households) ?? 0
    },
    {
      key: 'change',
      label: '户数变动',
      align: 'right',
      num: true,
      value: (row) => percent(row.shareholders.households_change_pct, 2, true),
      /* 股东户数下降意味着筹码集中，是偏多信号，因此取反再着色 */
      tone: (row) => tone(-(num(row.shareholders.households_change_pct) ?? 0)),
      sortValue: (row) => num(row.shareholders.households_change_pct) ?? 0
    },
    {
      key: 'perCapita',
      label: '户均自由流通',
      align: 'right',
      num: true,
      value: (row) => compact(row.per_capita.float_shares, '股')
    },
    {
      key: 'top10Float',
      label: '十大流通占比',
      align: 'right',
      num: true,
      value: (row) => percent(row.top10_float.share_pct),
      sortValue: (row) => num(row.top10_float.share_pct) ?? 0
    },
    {
      key: 'top10',
      label: '十大股东占比',
      align: 'right',
      num: true,
      value: (row) => percent(row.top10.share_pct)
    },
    {
      key: 'institution',
      label: '机构持股',
      align: 'right',
      num: true,
      value: (row) => compact(row.institution.shares, '股'),
      sub: (row) => percent(row.institution.float_share_pct)
    },
    {
      key: 'report',
      label: '报告期',
      num: true,
      value: (row) => date(row.top10_float.report_date)
    },
    { key: 'open', label: '', width: '30px', slot: true }
  ];

  const shareholderStats = $derived.by<Stat[]>(() => {
    if (!shareholderProfile) return [];
    const metrics = shareholderProfile.metrics;
    return [
      {
        label: '统计区间',
        value: `${date(shareholderProfile.start_date)} — ${date(shareholderProfile.end_date)}`
      },
      {
        label: '户均自由流通股',
        value: compact(metrics.per_capita_float_shares.current, '股'),
        note: percent(metrics.per_capita_float_shares.change_pct, 2, true)
      },
      {
        label: '十大流通持股',
        value: percent(metrics.top10_float_holding.share_pct ?? metrics.top10_float_holding.current),
        note: percent(metrics.top10_float_holding.change_pct, 2, true)
      },
      {
        label: '十大股东持股',
        value: percent(metrics.top10_holding.share_pct ?? metrics.top10_holding.current),
        note: percent(metrics.top10_holding.change_pct, 2, true)
      },
      {
        label: '机构持股',
        value: percent(metrics.institution_holding.share_pct ?? metrics.institution_holding.current),
        note: percent(metrics.institution_holding.change_pct, 2, true)
      }
    ];
  });

  onMount(() => loadMaster());
</script>

<PageHeader
  eyebrow="HYJGCC.sp + HYGDRS.sp · 709/1721"
  title="行业机构画像"
  description="一级研究行业的四个报告期机构持仓，以及二级行业内逐股的股东结构与机构持仓。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={master.busy} onclick={() => loadMaster(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="248px">
  {#snippet aside()}
    <Panel
      title="研究行业树"
      eyebrow="LEVEL 1 / LEVEL 2"
      subtitle={master.data ? `${master.data.counts.industries} 个节点` : ''}
      busy={master.busy}
      error={master.error}
      onRetry={() => loadMaster()}
      empty={master.loaded && !master.busy && tree.length === 0}
      emptyText="没有可用的行业树"
      scroll
      flush
    >
      {#snippet toolbar()}
        <TextInput
          bind:value={filter}
          icon="search"
          width="100%"
          label="筛选行业"
          placeholder="筛选行业名称"
        />
      {/snippet}

      <ul class="tree">
        {#each visibleTree as node (node.code)}
          <li>
            <div class="node">
              {#if node.children?.length}
                <button
                  class="twist"
                  type="button"
                  aria-label={collapsed.has(node.code) ? '展开' : '折叠'}
                  onclick={() => toggle(node.code)}
                >
                  <Icon
                    name={collapsed.has(node.code) ? 'chevron-right' : 'chevron-down'}
                    size={11}
                  />
                </button>
              {:else}
                <span class="twist"></span>
              {/if}
              <button
                class="label"
                class:on={selectedCode === node.code}
                type="button"
                onclick={() => selectIndustry(node)}
              >
                <span class="truncate">{node.name}</span>
                <span class="meta num">{node.member_count}</span>
              </button>
            </div>

            {#if node.children?.length && !collapsed.has(node.code)}
              <ul class="children">
                {#each node.children as child (child.code)}
                  <li>
                    <button
                      class="label child"
                      class:on={selectedCode === child.code}
                      type="button"
                      onclick={() => selectIndustry(child)}
                    >
                      <span class="truncate">{child.name}</span>
                      <span class="meta num">{child.member_count}</span>
                    </button>
                  </li>
                {/each}
              </ul>
            {/if}
          </li>
        {/each}
      </ul>
    </Panel>
  {/snippet}

  {#snippet main()}
    <Panel
      title={selectedNode ? `${selectedNode.name} · 机构持仓历史` : '机构持仓历史'}
      eyebrow="hycgmx"
      subtitle={detail.data?.holdings_industry
        ? `口径行业 ${detail.data.holdings_industry.name} · ${detail.data.holdings_industry.member_count} 只成分`
        : '一级行业口径，最近 10 个已完成报告期'}
      busy={detail.busy}
      error={detail.error}
      onRetry={() => selectedNode && selectIndustry(selectedNode)}
      empty={detail.loaded && !detail.busy && holdings.length === 0}
      emptyText={selectedCode ? '该行业没有机构持仓历史' : '在左侧选择一个研究行业'}
      flush
      scroll
    >
      <DataTable
        columns={holdingColumns}
        rows={holdings}
        rowKey={(row, index) => row.period_key ?? index}
      />
    </Panel>

    {#if shareholderProfile}
      <Panel title="行业股东结构汇总" eyebrow="hygdrs">
        <StatGrid stats={shareholderStats} columns={5} />
      </Panel>
    {/if}

    <Panel
      title="行业内逐股股东结构"
      eyebrow="hygdrs"
      subtitle={securities.length ? `${securities.length} 只成分证券` : '二级行业口径'}
      empty={detail.loaded && !detail.busy && securities.length === 0}
      emptyText="该层级没有逐股股东结构，请选择一个二级行业"
      flush
      scroll
    >
      <DataTable
        columns={securityColumns}
        rows={securities}
        rowKey={(row) => row.security.security_id}
        onRowClick={openStock}
      >
        {#snippet cell({ row })}
          <button class="jump" type="button" title="在个股工作台打开">
            <Icon name="external" size={11} />
          </button>
        {/snippet}
      </DataTable>
    </Panel>

    {#each detail.data?.detail_errors ?? [] as failure (failure.resource)}
      <p class="warn-line">{failure.resource}：{text(failure.message)}</p>
    {/each}
  {/snippet}
</Split>

<style>
  .tree,
  .children {
    margin: 0;
    padding: 0;
    list-style: none;
  }

  .node {
    display: flex;
    align-items: center;
  }

  .twist {
    display: flex;
    flex: none;
    align-items: center;
    justify-content: center;
    width: 18px;
    height: 22px;
    color: var(--fg-mute);
  }

  .label {
    display: flex;
    flex: 1;
    align-items: center;
    justify-content: space-between;
    gap: var(--sp-2);
    min-width: 0;
    height: 22px;
    padding: 0 var(--sp-3) 0 0;
    font-size: var(--fs-micro);
    color: var(--fg-dim);
    text-align: left;
  }

  .label.child {
    padding-left: 26px;
  }

  .label:hover {
    color: var(--fg);
    background: var(--bg-hover);
  }

  .label.on {
    color: var(--fg);
    font-weight: 500;
    background: var(--bg-active);
    box-shadow: inset 2px 0 0 var(--focus);
  }

  .meta {
    flex: none;
    font-size: 9px;
    color: var(--fg-mute);
  }

  .jump {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 18px;
    height: 18px;
    color: var(--focus);
    border-radius: var(--radius);
  }

  .warn-line {
    flex: none;
    padding: var(--sp-1) var(--sp-3);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }
</style>
