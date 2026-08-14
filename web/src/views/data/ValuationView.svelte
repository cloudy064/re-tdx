<script lang="ts">
  /**
   * 市场估值。指数主表放在右栏（Split 的 aside 是定宽列），
   * 把整个弹性主区让给 PE/PB 历史图——估值判读靠的是长周期形态，宽度比列表更值钱。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { Resource } from '../../lib/resource.svelte';
  import { compact, count, date, fixed, percent, text, tone } from '../../lib/fmt';
  import ValuationChart from '../../charts/ValuationChart.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import type { MarketValuationDocument, ValuationFund, ValuationIndex } from '../../types';

  const METRICS = [
    { id: 'pe', label: '市盈率 PE', hint: 'PE 与其历史百分位' },
    { id: 'pb', label: '市净率 PB', hint: 'PB 与其历史百分位' }
  ];

  let metric = $state<'pe' | 'pb'>('pe');
  let selectedKey = $state('');

  const master = new Resource<MarketValuationDocument>();
  const detail = new Resource<MarketValuationDocument>();

  const indices = $derived(master.data?.indices ?? []);
  const doc = $derived(detail.data);
  const selected = $derived(
    doc?.selected ?? indices.find((item) => key(item) === selectedKey) ?? null
  );
  const metrics = $derived(selected?.metrics ?? null);
  const range = $derived(doc?.summary?.[metric] ?? null);
  const percentile = $derived(
    metrics ? metrics[metric === 'pe' ? 'pe_percentile' : 'pb_percentile'] : null
  );

  function key(item: ValuationIndex): string {
    return `${item.security.market}:${item.security.code}`;
  }

  const stats = $derived.by<Stat[]>(() => [
    { label: '覆盖指数', value: count(master.data?.counts.indices ?? null) },
    {
      label: '历史交易日',
      value: count(doc?.counts.history_points ?? null),
      note: `${date(doc?.summary.first_date)} 起`
    },
    { label: '最新交易日', value: date(selected?.date), note: text(metrics?.valuation_label) },
    { label: '当前 PE', value: fixed(metrics?.pe), note: `百分位 ${percent(metrics?.pe_percentile)}` },
    { label: '当前 PB', value: fixed(metrics?.pb), note: `百分位 ${percent(metrics?.pb_percentile)}` },
    {
      label: '5 / 20 日涨跌',
      value: `${percent(selected?.returns.days_5, 2, true)} / ${percent(selected?.returns.days_20, 2, true)}`,
      tone: tone(selected?.returns.days_5)
    }
  ]);

  const keyStats = $derived.by<Stat[]>(() => {
    if (!metrics) return [];
    return [
      {
        label: `${metric.toUpperCase()} 历史百分位`,
        value: percent(percentile),
        note: text(metrics.valuation_label)
      },
      { label: '区间最低', value: fixed(range?.minimum), note: date(doc?.summary.first_date) },
      { label: '区间最高', value: fixed(range?.maximum), note: date(doc?.summary.last_date) },
      { label: 'ROE', value: percent(metrics.roe) },
      { label: '股息率', value: percent(metrics.dividend_yield) },
      { label: '盈利收益率', value: percent(metrics.earnings_yield) },
      {
        label: '10 日涨跌',
        value: percent(selected?.returns.days_10, 2, true),
        tone: tone(selected?.returns.days_10)
      },
      {
        label: '30 日涨跌',
        value: percent(selected?.returns.days_30, 2, true),
        tone: tone(selected?.returns.days_30)
      }
    ];
  });

  async function loadMaster(refresh = false) {
    const result = await master.load(
      `/api/v1/market/valuation?${queryString({ refresh: refresh ? 1 : 0 })}`
    );
    // 主表不含历史序列，首次进入直接展开第一只指数，避免右侧空图
    if (result?.indices.length && !selectedKey) select(result.indices[0]);
  }

  function select(item: ValuationIndex) {
    selectedKey = key(item);
    loadDetail();
  }

  function loadDetail(refresh = false) {
    if (!selectedKey) return;
    const [market, code] = selectedKey.split(':');
    void detail.load(
      `/api/v1/market/valuation?${queryString({
        market,
        code,
        include_details: 1,
        limit: 4000,
        refresh: refresh ? 1 : 0
      })}`
    );
  }

  const indexColumns: Column<ValuationIndex>[] = [
    {
      key: 'name',
      label: '指数',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => row.security.security_id
    },
    {
      key: 'pe',
      label: 'PE',
      align: 'right',
      num: true,
      value: (row) => fixed(row.metrics.pe),
      sub: (row) => percent(row.metrics.pe_percentile),
      sortValue: (row) => Number(row.metrics.pe) || 0
    },
    {
      key: 'pb',
      label: 'PB',
      align: 'right',
      num: true,
      value: (row) => fixed(row.metrics.pb),
      sub: (row) => percent(row.metrics.pb_percentile),
      sortValue: (row) => Number(row.metrics.pb) || 0
    },
    {
      key: 'days5',
      label: '5 日',
      align: 'right',
      num: true,
      value: (row) => percent(row.returns.days_5, 2, true),
      tone: (row) => tone(row.returns.days_5),
      sortValue: (row) => Number(row.returns.days_5) || 0
    }
  ];

  const fundColumns: Column<ValuationFund>[] = [
    {
      key: 'name',
      label: '基金',
      wrap: true,
      value: (row) => row.name || row.code,
      sub: (row) => `${row.security_id} · ${text(row.fund_type)}`
    },
    { key: 'nav', label: '净值', align: 'right', num: true, value: (row) => fixed(row.net_asset_value, 4) },
    {
      key: 'premium',
      label: '溢价率',
      align: 'right',
      num: true,
      value: (row) => percent(row.premium_pct, 3),
      tone: (row) => tone(row.premium_pct),
      sortValue: (row) => Number(row.premium_pct) || 0
    },
    {
      key: 'shares',
      label: '最新份额',
      align: 'right',
      num: true,
      value: (row) => compact(row.latest_shares, '份'),
      sortValue: (row) => Number(row.latest_shares) || 0
    },
    {
      key: 'unit',
      label: '最小赎回单位',
      align: 'right',
      num: true,
      value: (row) => count(row.minimum_redemption_unit)
    }
  ];

  onMount(() => void loadMaster());
</script>

<PageHeader
  eyebrow="TDX JSN · 指数估值"
  title="市场估值"
  description="通达信原生指数 PE / PB、历史百分位与关联指数基金；历史序列一次性载入，可滚轮缩放与拖拽平移。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={master.busy || detail.busy} onclick={() => void loadMaster(true)}>
      刷新源数据
    </Button>
  {/snippet}
</PageHeader>

<Split asideWidth="320px">
  {#snippet main()}
    <div class="stage">
      <Panel
        flush
        scroll
        eyebrow="HISTORY"
        title={`${selected?.security.name ?? '指数'} · ${metric.toUpperCase()} 与历史百分位`}
        subtitle={doc
          ? `${count(doc.counts.history_points)} 个交易日 · ${date(doc.summary.first_date)} — ${date(doc.summary.last_date)}`
          : ''}
        busy={detail.busy}
        error={detail.error}
        onRetry={() => loadDetail()}
        empty={detail.loaded && !detail.busy && (doc?.history.length ?? 0) === 0}
        emptyText="该指数没有返回估值历史"
      >
        {#snippet toolbar()}
          <Segmented
            options={METRICS}
            value={metric}
            onChange={(next) => (metric = next as 'pe' | 'pb')}
            ariaLabel="估值指标"
          />
        {/snippet}

        {#if doc?.history.length}
          <div class="chart-host">
            <ValuationChart history={doc.history} {metric} />
          </div>
        {/if}
      </Panel>

      <div class="bottom">
        <Panel
          flush
          scroll
          eyebrow="RELATED FUNDS"
          title="关联指数基金"
          subtitle={doc ? `${count(doc.counts.related_funds)} 只` : ''}
          busy={detail.busy}
          empty={detail.loaded && !detail.busy && (doc?.related_funds.length ?? 0) === 0}
          emptyText="当前指数没有关联基金"
        >
          <DataTable
            columns={fundColumns}
            rows={doc?.related_funds ?? []}
            rowKey={(row) => row.security_id}
          />
        </Panel>

        <Panel
          scroll
          eyebrow="KEY METRICS"
          title="关键指标"
          subtitle={selected ? selected.security.security_id : ''}
          busy={detail.busy}
          empty={!metrics && !detail.busy}
          emptyText="选择右侧指数后展示估值与收益指标"
        >
          <StatGrid stats={keyStats} inline />
        </Panel>
      </div>
    </div>
  {/snippet}

  {#snippet aside()}
    <Panel
      flush
      scroll
      eyebrow="MASTER"
      title="指数列表"
      subtitle={master.data ? `${count(master.data.counts.indices)} 只指数` : ''}
      busy={master.busy}
      error={master.error}
      onRetry={() => void loadMaster()}
      empty={master.loaded && !master.busy && indices.length === 0}
      emptyText="主站没有返回指数估值主表"
    >
      <DataTable
        columns={indexColumns}
        rows={indices}
        rowKey={(row) => row.detail_id || key(row)}
        onRowClick={select}
        isActive={(row) => key(row) === selectedKey}
      />
    </Panel>
  {/snippet}
</Split>

<style>
  /* 栅格项默认 stretch，面板才能撑满行高而不是缩到内容高——图表要吃掉主区 */
  .stage {
    display: grid;
    grid-template-rows: minmax(0, 1.7fr) minmax(0, 1fr);
    gap: var(--sp-2);
    flex: 1;
    min-height: 0;
  }

  .bottom {
    display: grid;
    grid-template-columns: minmax(0, 1fr) 260px;
    gap: var(--sp-2);
    min-height: 0;
  }

  .chart-host {
    display: flex;
    height: 100%;
    min-height: 260px;
  }

  @media (max-width: 1180px) {
    .bottom {
      grid-template-columns: minmax(0, 1fr);
      grid-template-rows: minmax(0, 1fr) auto;
    }
  }
</style>
