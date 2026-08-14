<script lang="ts">
  /**
   * 持有该股的主动基金（ZDJJZCZC · 股票 → 基金）。
   *
   * 数据来自公开季报，天然滞后且只覆盖披露口径，所以金额/股数/净值占比
   * 一律保留原始披露单位，不做任何折算或推断。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { compact, count, date, percent, signedCompact, text, tone } from '../../../lib/fmt';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Icon from '../../../ui/Icon.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { ActiveFundPosition, MarketActiveFundsDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const DIRECTIONS: Record<string, string> = {
    new: '新进',
    increased: '增持',
    decreased: '减持',
    unchanged: '不变'
  };

  const funds = new Resource<MarketActiveFundsDocument>();
  const doc = $derived(funds.data);
  const holding = $derived(doc?.selected_holding ?? null);
  const positions = $derived<ActiveFundPosition[]>(doc?.funds ?? []);

  function load(refresh = false) {
    void funds.load(
      `/api/v1/market/active-funds?${queryString({
        view: 'funds',
        market,
        code,
        detail_limit: 5000,
        refresh: refresh ? 1 : 0
      })}`
    );
  }

  const stats = $derived.by<Stat[]>(() => {
    if (!holding || !doc) return [];
    return [
      {
        label: '报告区间',
        value: `${date(holding.start_date)} — ${date(holding.end_date)}`,
        note: holding.industry || '行业未知'
      },
      {
        label: '持股市值',
        value: compact(holding.holding_market_value_yuan, '元'),
        note: `较上期 ${signedCompact(holding.holding_market_value_change_yuan, '元')}`,
        tone: tone(holding.holding_market_value_change_yuan)
      },
      {
        label: '持股数量',
        value: compact(holding.holding_shares, '股'),
        note: `较上期 ${signedCompact(holding.holding_share_change, '股')}`,
        tone: tone(holding.holding_share_change)
      },
      {
        label: '基金家数',
        value: count(holding.fund_count),
        note: `详情返回 ${count(positions.length)} 家`
      },
      {
        label: '占流通市值',
        value: percent(holding.holding_pct_float),
        note: '季末流通市值口径'
      },
      {
        label: '季末流通 / 总市值',
        value: compact(holding.period_end_float_market_cap_yuan, '元'),
        note: compact(holding.period_end_total_market_cap_yuan, '元')
      }
    ];
  });

  const columns: Column<ActiveFundPosition>[] = [
    {
      key: 'fund',
      label: '基金名称',
      wrap: true,
      value: (row) => text(row.fund.name || row.fund.code),
      sub: (row) => row.fund.fund_id
    },
    {
      key: 'code',
      label: '代码 / 市场',
      width: '104px',
      num: true,
      value: (row) => text(row.fund.code),
      sub: (row) => `市场 ${row.fund.market_id}`
    },
    {
      key: 'value',
      label: '持股市值',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_market_value_yuan, '元'),
      sortValue: (row) => row.holding_market_value_yuan ?? 0
    },
    {
      key: 'shares',
      label: '持股数量',
      align: 'right',
      num: true,
      value: (row) => compact(row.holding_shares, '股'),
      sortValue: (row) => row.holding_shares ?? 0
    },
    {
      key: 'nav',
      label: '占基金净值',
      align: 'right',
      num: true,
      value: (row) => percent(row.nav_pct),
      sortValue: (row) => row.nav_pct ?? 0
    },
    {
      key: 'rank',
      label: '重仓排名',
      align: 'right',
      num: true,
      value: (row) => (row.holding_rank === null ? text(null) : `第 ${row.holding_rank} 位`),
      // 缺排名排到最后，否则升序时会冒充第 1 位
      sortValue: (row) => row.holding_rank ?? Number.MAX_SAFE_INTEGER
    }
  ];

  $effect(() => {
    void market;
    void code;
    load();
  });
</script>

<Panel
  title="主动基金季度持仓汇总"
  eyebrow="ZDJJZCZC · STOCK → FUNDS"
  subtitle={doc ? `主表缓存 ${doc.cache.master_age_seconds}s · 详情缓存 ${doc.cache.detail_age_seconds}s` : ''}
  busy={funds.busy}
  error={funds.error}
  onRetry={() => load()}
  empty={funds.loaded && !funds.busy && !holding}
  emptyText="当前证券不在本期主动基金持仓汇总中"
  scroll
>
  {#snippet actions()}
    {#if holding}
      <Badge
        tone={holding.direction === 'decreased'
          ? 'down'
          : holding.direction === 'unchanged'
            ? 'neutral'
            : 'up'}
      >
        {DIRECTIONS[holding.direction] ?? holding.direction}
      </Badge>
    {/if}
    <Button icon="refresh" busy={funds.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  <StatGrid {stats} columns={3} />
</Panel>

<Panel
  title="持有该股的主动基金"
  eyebrow="FUND POSITIONS"
  subtitle="金额为元、数量为股、净值占比为百分比，均为季报原始披露单位"
  empty={!funds.busy && Boolean(holding) && positions.length === 0}
  emptyText="主表命中，但当前动态详情没有返回基金记录"
  flush
  scroll
>
  <DataTable
    {columns}
    rows={positions}
    rowKey={(row) => row.fund.fund_id}
    sortKey="value"
    numbered
  />

  <!-- 季报是滞后且不完整的披露口径，缺了这句提示很容易被当成实时持仓 -->
  <p class="note">
    <Icon name="info" size={12} />
    <span>
      数据来自公开基金季报，仅覆盖已披露的重仓与主动基金，<strong>不代表基金完整持仓，也不是实时持仓</strong>；
      季报披露存在滞后，持仓在报告期后可能已经变化。
    </span>
  </p>
</Panel>

<style>
  .note {
    display: flex;
    align-items: flex-start;
    gap: var(--sp-2);
    padding: var(--sp-2) var(--sp-4);
    font-size: var(--fs-micro);
    line-height: var(--lh-body);
    color: var(--warn);
    background: var(--warn-soft);
    border-top: 1px solid var(--line);
  }

  .note strong {
    font-weight: 600;
  }
</style>
