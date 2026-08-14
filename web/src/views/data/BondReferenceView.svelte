<script lang="ts">
  /** 债券评级 / 利率 / 类别视图，政策性金融债额外核对沪深投影。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, percent, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { BondReferenceRecord, MarketBondReferenceDocument } from '../../types';

  const RATING_BUCKETS = [
    { id: 'aaa', label: 'AAA级 · 大表' }, { id: 'aa-plus', label: 'AA+级' },
    { id: 'aa', label: 'AA级' }, { id: 'aa-minus', label: 'AA-级' },
    { id: 'a-plus', label: 'A+级' }, { id: 'a', label: 'A级' },
    { id: 'a-minus', label: 'A-级' }, { id: 'a-minus-1', label: 'A-1级' },
    { id: 'bbb-plus-or-lower', label: 'BBB+级及以下' }
  ];
  const RATE_BUCKETS = [
    { id: 'fixed', label: '固定利率 · 大表' }, { id: 'floating', label: '浮动利率' },
    { id: 'progressive', label: '累进利率' }, { id: 'principal-at-maturity', label: '利随本清' },
    { id: 'discount', label: '贴现' }, { id: 'other', label: '其他类型' }
  ];
  const CATEGORY_BUCKETS = [
    { id: 'all', label: '全市场债券 · 4万+' },
    { id: 'government', label: '国债' },
    { id: 'local-government', label: '地方债 · 大表' },
    { id: 'corporate', label: '公司债' },
    { id: 'enterprise', label: '企业债' },
    { id: 'private', label: '私募债' },
    { id: 'asset-backed', label: '资产支持证券' },
    { id: 'recent-convertible', label: '次新可转债' },
    { id: 'policy-financial', label: '政策性金融债' }
  ];

  let group = $state<'rating' | 'rate' | 'category'>('rating');
  let bucket = $state('aa-plus');
  let query = $state('');
  let includeProjections = $state(false);
  let selected = $state<BondReferenceRecord | null>(null);
  const resource = new Resource<MarketBondReferenceDocument>();
  const doc = $derived(resource.data);
  const bucketOptions = $derived(group === 'rating' ? RATING_BUCKETS : group === 'rate' ? RATE_BUCKETS : CATEGORY_BUCKETS);
  const stats = $derived.by<Stat[]>(() => [
    { label: '当前分组', value: doc?.summary.source_name ?? '—', note: group === 'rating' ? '信用评级' : group === 'rate' ? '利率类型' : '债券类别' },
    { label: '源记录', value: count(doc?.summary.source_row_count), note: doc?.projection_reconciliation ? '主表 + 沪深投影' : '当前懒加载一张表' },
    { label: '筛选命中', value: count(doc?.match_count), note: `返回 ${count(doc?.returned)}` },
    { label: '债项评级', value: count(doc?.summary.credit_ratings.length), note: (doc?.summary.credit_ratings ?? []).slice(0, 3).map((row) => `${row.name} ${row.count}`).join(' · ') },
    { label: '利率类型', value: count(doc?.summary.rate_types.length), note: (doc?.summary.rate_types ?? []).slice(0, 2).map((row) => row.name).join(' · ') },
    { label: '到期区间', value: date(doc?.summary.earliest_maturity_date), note: `至 ${date(doc?.summary.latest_maturity_date)}` }
  ]);

  async function load(refresh = false) {
    const result = await resource.load(`/api/v1/market/bond-reference?${queryString({
      group, bucket, q: query.trim(), sort: 'maturity', order: 'asc', limit: 1000,
      include_projections: includeProjections ? 1 : 0,
      refresh: refresh ? 1 : 0, timeout_ms: 120000
    })}`);
    selected = result?.records[0] ?? null;
  }

  function switchGroup(next: string) {
    group = next === 'rate' ? 'rate' : next === 'category' ? 'category' : 'rating';
    bucket = group === 'rating' ? 'aa-plus' : group === 'rate' ? 'floating' : 'all';
    includeProjections = false;
    void load();
  }

  const columns: Column<BondReferenceRecord>[] = [
    { key: 'security', label: '债券', width: '145px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'rating', label: '债项 / 主体', width: '92px', value: (row) => text(row.bond_credit_rating), sub: (row) => text(row.issuer_credit_rating) },
    { key: 'rate', label: '票息', width: '75px', align: 'right', num: true, value: (row) => percent(row.current_coupon_rate_pct), sub: (row) => text(row.rate_type), sortValue: (row) => row.current_coupon_rate_pct ?? -1 },
    { key: 'maturity', label: '到期日', width: '90px', num: true, value: (row) => date(row.maturity_date), sub: (row) => `${fixed(row.remaining_years, 2)} 年`, sortValue: (row) => row.maturity_date },
    { key: 'coupon', label: '下次付息', width: '92px', num: true, value: (row) => date(row.next_coupon_date), sub: (row) => `剩余 ${count(row.remaining_coupon_count)} 次` },
    { key: 'type', label: '债券类型', width: '115px', wrap: true, value: (row) => text(row.bond_type), sub: (row) => `${count(row.coupon_frequency_months)} 月/次` },
    { key: 'size', label: '发行规模', width: '105px', align: 'right', num: true, value: (row) => compact(row.issue_size_yuan, '元'), sub: (row) => row.guarantee_status ? `担保 ${row.guarantee_status}` : '—', sortValue: (row) => row.issue_size_yuan ?? -1 },
    { key: 'face', label: '面值 / 发行价', width: '105px', align: 'right', num: true, value: (row) => compact(row.face_value_yuan, '元'), sub: (row) => fixed(row.issue_price_yuan, 2) }
  ];

  onMount(() => void load());
</script>

<PageHeader eyebrow="BOND TERMS · RATING / RATE / CATEGORY · COUPON SCHEDULE" title="债券条款资料库" description="还原通达信债券评级、利率与类别页的静态条款、主体/债项评级及付息序列；政策性金融债同时核对全市场主表与沪深投影。宿主实时行情列不在 JSN 中，因此不伪造。" {stats}>
  {#snippet actions()}
    {#if group === 'category'}<Button busy={resource.busy} onclick={() => { includeProjections = true; void load(); }}>核对市场投影</Button>{/if}
    <Button icon="refresh" busy={resource.busy} onclick={() => void load(true)}>刷新当前条款表</Button>
  {/snippet}
</PageHeader>

<div class="controls">
  <Segmented options={[{ id: 'rating', label: '信用评级' }, { id: 'rate', label: '利率类型' }, { id: 'category', label: '债券类别' }]} value={group} onChange={switchGroup} />
  <Select options={bucketOptions} value={bucket} width="190px" label="分组" onChange={(next) => { bucket = next; includeProjections = false; void load(); }} />
  <TextInput bind:value={query} icon="search" width="250px" label="检索" placeholder="债券代码、名称、类型" onEnter={() => void load()} />
</div>

{#if doc?.projection_reconciliation}
  <div class="reconciliation">
    <Badge tone={doc.projection_reconciliation.exact_match ? 'up' : 'warn'}>{doc.projection_reconciliation.exact_match ? '市场投影与主表一致' : '投影与主表存在差异'}</Badge>
    <span>主表 {count(doc.projection_reconciliation.master_count)} 只；沪市 {count(doc.projection_reconciliation.projections.find((item) => item.market === 'sh')?.count)} 只，深市 {count(doc.projection_reconciliation.projections.find((item) => item.market === 'sz')?.count)} 只。</span>
    {#if doc.projection_reconciliation.client_master_comparison}
      <Badge tone={doc.projection_reconciliation.client_master_comparison.exact_match ? 'up' : 'warn'}>客户端合并表 {count(doc.projection_reconciliation.client_master_comparison.security_count)} 只</Badge>
    {/if}
  </div>
{/if}

<Split asideWidth="410px">
  {#snippet main()}
    <Panel title="债券目录" subtitle={`${count(doc?.match_count)} 条 · 当前返回 ${count(doc?.returned)} 条`} eyebrow={doc?.sources[0]?.resource ?? 'BOND JSN'} busy={resource.busy} error={resource.error} onRetry={() => void load()} empty={resource.loaded && (doc?.records.length ?? 0) === 0} emptyText="当前条件没有债券。" flush scroll fill>
      <DataTable numbered stickyFirst columns={columns} rows={doc?.records ?? []} rowKey={(row) => row.security.security_id} onRowClick={(row) => selected = row} isActive={(row) => row.security.security_id === selected?.security.security_id} sortKey="maturity" minWidth="850px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel title={selected?.security.name ?? '债券条款'} eyebrow={selected?.security.security_id ?? 'TERMS'} subtitle={selected ? `${text(selected.bond_type)} · ${text(selected.bond_credit_rating)} · ${text(selected.rate_type)}` : ''} empty={!selected} emptyText="选择一只债券查看付息序列。" scroll fill>
      {#if selected}
        <dl class="terms">
          <div><dt>起息 / 到期</dt><dd>{date(selected.accrual_start_date)} → {date(selected.maturity_date)}</dd></div>
          <div><dt>当前票息</dt><dd>{percent(selected.current_coupon_rate_pct)} · 每 {count(selected.coupon_frequency_months)} 月</dd></div>
          <div><dt>面值 / 发行价</dt><dd>{fixed(selected.face_value_yuan, 2)} / {fixed(selected.issue_price_yuan, 2)} 元</dd></div>
          <div><dt>发行规模 / 担保</dt><dd>{compact(selected.issue_size_yuan, '元')} / {text(selected.guarantee_status)}{#if selected.issue_size_source_100m !== null && selected.issue_size_source_100m !== undefined}<small>（源 {fixed(selected.issue_size_source_100m, 2)} 亿元）</small>{/if}</dd></div>
          {#if selected.source_scale_semantics === 'client-master-hidden-unit' && selected.source_scale_raw !== null && selected.source_scale_raw !== undefined}
            <div><dt>主表原始规模</dt><dd>{fixed(selected.source_scale_raw, 4)}<small>客户端未声明统一单位，不换算</small></dd></div>
          {/if}
          {#if selected.underlying}
            <div><dt>正股 / 转股价</dt><dd>{selected.underlying.security_id} / {fixed(selected.conversion_price_yuan, 3)} 元</dd></div>
            <div><dt>转股期</dt><dd>{date(selected.conversion_start_date)} → {date(selected.conversion_end_date)}</dd></div>
            <div><dt>下修 / 回售 / 赎回</dt><dd>{percent(selected.revision_trigger_pct)} / {percent(selected.put_trigger_pct)} / {percent(selected.call_trigger_pct)}</dd></div>
          {/if}
          <div><dt>债项 / 主体评级</dt><dd>{text(selected.bond_credit_rating)} / {text(selected.issuer_credit_rating)}</dd></div>
        </dl>
        <h3>剩余付息序列</h3>
        <div class="schedule">
          {#each selected.remaining_coupon_schedule as point (`${point.date}-${point.rate_pct}`)}
            <div><span>{date(point.date)}</span><strong>{percent(point.rate_pct)}</strong></div>
          {:else}<p>当前没有剩余付息点。</p>{/each}
        </div>
        <details>
          <summary>完整付息序列 · {count(selected.coupon_schedule.length)} 期</summary>
          <div class="schedule">
            {#each selected.coupon_schedule as point (`all-${point.date}-${point.rate_pct}`)}
              <div><span>{date(point.date)}</span><strong>{percent(point.rate_pct)}</strong></div>
            {/each}
          </div>
        </details>
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .controls { display: flex; align-items: center; gap: var(--sp-3); margin-bottom: var(--sp-3); }
  .reconciliation { display: flex; align-items: center; gap: var(--sp-2); margin: 0 0 var(--sp-3); color: var(--fg-mute); font-size: var(--fs-micro); }
  .terms { display: grid; gap: var(--sp-2); margin: 0; }
  .terms div, .schedule div { display: flex; justify-content: space-between; gap: var(--sp-3); padding: var(--sp-2); border-bottom: 1px solid var(--line); }
  dt { color: var(--fg-mute); font-size: 10px; } dd { margin: 0; color: var(--fg); text-align: right; }
  dd small { display: block; color: var(--fg-mute); font-size: 9px; }
  h3 { margin: var(--sp-4) 0 var(--sp-2); font-size: 11px; color: var(--fg); }
  .schedule { display: grid; gap: 1px; font-size: 10px; }
  .schedule span { color: var(--fg-mute); } .schedule strong { color: var(--fg); }
  .schedule p { color: var(--fg-mute); }
  details { margin-top: var(--sp-3); } summary { cursor: pointer; color: var(--fg-mute); font-size: 10px; }
</style>
