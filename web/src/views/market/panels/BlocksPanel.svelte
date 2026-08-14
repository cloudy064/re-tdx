<script lang="ts">
  /**
   * 行业与板块归属。
   *
   * 两条独立数据链：本地板块库（/api/v1/securities/blocks）给出通达信行业、
   * 研究行业、概念、风格、指数的全部归属；动态行业画像
   * （/api/v1/market/industry-profile）给出该股所属研究行业的机构与股东口径。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, num, percent, text, tone } from '../../../lib/fmt';
  import { router } from '../../../lib/router.svelte';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type {
    Block,
    IndustryHoldingPeriod,
    IndustryProfileDocument,
    SecurityBlocksResult
  } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const blocks = new Resource<SecurityBlocksResult>();
  const industry = new Resource<IndustryProfileDocument>();

  const blockDoc = $derived(blocks.data);
  const profileDoc = $derived(industry.data);
  const security = $derived(profileDoc?.selected_security_profile ?? null);

  function loadBlocks() {
    void blocks.load(`/api/v1/securities/blocks?${queryString({ market, code })}`);
  }

  function loadIndustry(refresh = false) {
    void industry.load(
      `/api/v1/market/industry-profile?${queryString({
        market,
        code,
        detail_limit: 20,
        refresh: refresh ? 1 : 0
      })}`
    );
  }

  interface FamilyGroup {
    key: string;
    label: string;
    items: Block[];
  }

  /** 板块族之间不可比（行业互斥、概念可多归属），所以分组展示而不是一张平表。 */
  const families = $derived.by<FamilyGroup[]>(() => {
    const groups = new Map<string, FamilyGroup>();
    for (const block of blockDoc?.blocks ?? []) {
      const key = block.family || 'other';
      let group = groups.get(key);
      if (!group) {
        group = { key, label: block.family_name || key, items: [] };
        groups.set(key, group);
      }
      group.items.push(block);
    }
    return [...groups.values()];
  });

  const shareholderStats = $derived.by<Stat[]>(() => {
    if (!security) return [];
    return [
      {
        label: '股东户数',
        value: compact(num(security.shareholders.households), '户'),
        note: `${date(security.shareholders.start_date)} → ${date(security.shareholders.end_date)}`
      },
      {
        label: '户数变动',
        value: percent(security.shareholders.households_change_pct, 2, true),
        tone: tone(num(security.shareholders.households_change_pct)),
        note: `日均 ${percent(security.shareholders.daily_change_pct, 2, true)}`
      },
      {
        label: '户均自由流通股',
        value: compact(num(security.per_capita.float_shares), '股'),
        note: '行业逐股口径'
      },
      {
        label: '机构持股',
        value: compact(num(security.institution.shares), '股'),
        note: date(security.institution.report_date)
      },
      {
        label: '机构占流通',
        value: percent(security.institution.float_share_pct),
        note: profileDoc?.shareholder_industries[0]?.name ?? '二级研究行业'
      },
      {
        label: '十大流通股东占比',
        value: percent(security.top10_float.share_pct),
        note: compact(num(security.top10_float.shares), '股')
      },
      {
        label: '十大股东占比',
        value: percent(security.top10.share_pct),
        note: date(security.top10.report_date)
      }
    ];
  });

  const holdingRows = $derived<IndustryHoldingPeriod[]>(profileDoc?.holdings_history ?? []);

  const holdingColumns: Column<IndustryHoldingPeriod>[] = [
    {
      key: 'period',
      label: '报告期',
      width: '88px',
      num: true,
      value: (row) => date(row.report_date)
    },
    {
      key: 'value',
      label: '机构持仓市值',
      align: 'right',
      num: true,
      value: (row) => compact(num(row.metrics.market_value), '元'),
      sortValue: (row) => num(row.metrics.market_value) ?? 0
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
      key: 'firms',
      label: '机构家数',
      align: 'right',
      num: true,
      value: (row) => count(row.metrics.institution_count),
      sub: (row) => `环比 ${count(row.metrics.institution_count_change)}`,
      tone: (row) => tone(num(row.metrics.institution_count_change))
    },
    {
      key: 'shares',
      label: '持股数',
      align: 'right',
      num: true,
      value: (row) => compact(num(row.metrics.shares_held), '股'),
      sub: (row) => compact(num(row.metrics.shares_change), '股'),
      tone: (row) => tone(num(row.metrics.shares_change))
    },
    {
      key: 'floatPct',
      label: '占流通',
      align: 'right',
      num: true,
      value: (row) => percent(row.metrics.float_share_pct),
      sub: (row) => `环比 ${percent(row.metrics.float_share_change_pct, 2, true)}`
    },
    {
      key: 'totalPct',
      label: '占总股本',
      align: 'right',
      num: true,
      value: (row) => percent(row.metrics.total_share_pct)
    }
  ];

  $effect(() => {
    void market;
    void code;
    loadBlocks();
    loadIndustry();
  });
</script>

<Panel
  title="所属板块"
  eyebrow="LOCAL BLOCK INDEX"
  subtitle={blockDoc
    ? `${count(blockDoc.block_count)} 个归属 · ${families.length} 个板块族`
    : '通达信行业 / 研究行业 / 概念 / 风格 / 指数'}
  busy={blocks.busy}
  error={blocks.error}
  onRetry={() => loadBlocks()}
  empty={blocks.loaded && !blocks.busy && families.length === 0}
  emptyText="本地板块库里没有该证券的归属关系"
  scroll
>
  {#snippet actions()}
    <Button icon="sectors" onclick={() => router.go('/sectors')}>板块浏览器</Button>
  {/snippet}

  <div class="families">
    {#each families as group (group.key)}
      <section class="family">
        <header>
          <Badge tone="focus">{group.label}</Badge>
          <span class="mute">{count(group.items.length)} 个</span>
        </header>
        <div class="chips">
          {#each group.items as block (block.block_id)}
            <button
              class="chip"
              type="button"
              title={`${block.block_id} · 成分 ${count(block.member_count)} 只`}
              onclick={() => router.go('/sectors')}
            >
              <strong>{text(block.name)}</strong>
              <small class="num">{block.block_code || block.block_id}</small>
            </button>
          {/each}
        </div>
      </section>
    {/each}
  </div>
</Panel>

<Panel
  title="研究行业画像"
  eyebrow="INDUSTRY PROFILE"
  subtitle={profileDoc?.industry_path.length
    ? profileDoc.industry_path.map((item) => `${item.name}(${item.code})`).join(' › ')
    : '三级研究行业 → 一级机构持仓 → 二级股东画像'}
  busy={industry.busy}
  error={industry.error}
  onRetry={() => loadIndustry()}
  empty={industry.loaded && !industry.busy && !security}
  emptyText="当前股票所属研究行业尚无逐股股东画像"
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={industry.busy} onclick={() => loadIndustry(true)}>强制更新</Button>
  {/snippet}

  <StatGrid stats={shareholderStats} columns={4} />

  {#each profileDoc?.detail_errors ?? [] as failure (failure.resource)}
    <p class="warn-line">{failure.resource}：{failure.message}</p>
  {/each}
</Panel>

<Panel
  title="一级行业机构持仓历史"
  eyebrow="HOLDINGS BY PERIOD"
  subtitle={profileDoc?.holdings_industry
    ? `${profileDoc.holdings_industry.name} ${profileDoc.holdings_industry.code} · ${count(profileDoc.holdings_industry.member_count)} 只成分`
    : '按报告期给出所属一级行业的机构持仓口径'}
  empty={industry.loaded && !industry.busy && holdingRows.length === 0}
  emptyText="所属一级行业没有机构持仓历史"
  flush
  scroll
>
  <DataTable
    columns={holdingColumns}
    rows={holdingRows}
    rowKey={(row, index) => row.period_key ?? index}
    maxHeight="320px"
  />
</Panel>

<style>
  .families {
    display: flex;
    flex-direction: column;
    gap: var(--sp-4);
  }

  .family header {
    display: flex;
    align-items: center;
    gap: var(--sp-2);
    margin-bottom: var(--sp-2);
    font-size: var(--fs-micro);
  }

  .chips {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-1);
  }

  .chip {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 1px;
    padding: var(--sp-1) var(--sp-3);
    text-align: left;
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .chip:hover {
    background: var(--bg-hover);
    border-color: var(--line-strong);
  }

  .chip strong {
    font-size: var(--fs-micro);
    font-weight: 500;
    line-height: var(--lh-tight);
    color: var(--fg);
  }

  .chip small {
    font-size: 9px;
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
