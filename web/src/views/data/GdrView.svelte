<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, price } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { GdrRecord, MarketGdrDocument } from '../../types';
  type Sort = MarketGdrDocument['sort'];
  const SORTS = [
    { id: 'date', label: '日期' }, { id: 'premium', label: '溢价率' },
    { id: 'issuance', label: '发行量' }, { id: 'price', label: 'GDR价格' },
    { id: 'underlying-price', label: 'A股价格' }
  ];
  let sort = $state<Sort>('date');
  let order = $state<'asc' | 'desc'>('desc');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketGdrDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.record_id === selectedId) ?? rows[0] ?? null);
  const stats = $derived.by<Stat[]>(() => doc ? [
    { label: 'GDR数量', value: count(doc.match_count) },
    { label: '关联A股', value: count(doc.summary.unique_underlyings) },
    { label: '交易币种', value: count(doc.summary.currency_count) },
    { label: '上市地点', value: count(doc.summary.listing_location_count) },
    { label: '行情日期', value: date(doc.summary.latest_quote_date) }
  ] : []);
  function load(refresh = false) {
    selectedId = '';
    void resource.load(`/api/v1/market/gdr?${queryString({ sort, order, q: query.trim(), limit: 1000, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  function switchSort(next: string) { sort = next as Sort; load(); }
  function toggleOrder() { order = order === 'desc' ? 'asc' : 'desc'; load(); }
  function openStock() { if (selected) router.go(stockPath(selected.underlying.market, selected.underlying.code, 'gdr')); }
  const columns: Column<GdrRecord>[] = [
    { key: 'gdr', label: 'GDR', width: '190px', value: (row) => row.gdr_name, sub: (row) => `${row.gdr_code} · ${row.currency}` },
    { key: 'underlying', label: 'A股标的', width: '160px', value: (row) => row.underlying.name || row.underlying.code, sub: (row) => row.underlying.security_id },
    { key: 'price', label: 'GDR / A股价格', align: 'right', num: true, value: (row) => `${price(row.gdr_price)} ${row.currency}`, sub: (row) => price(row.underlying_price) },
    { key: 'converted', label: '折算GDR / 溢价', align: 'right', num: true, value: (row) => price(row.converted_gdr_price), sub: (row) => delta(row.premium_pct) },
    { key: 'terms', label: '发行量 / 兑换比', align: 'right', num: true, value: (row) => compact(row.issuance_units), sub: (row) => `1 : ${fixed(row.conversion_ratio)}` },
    { key: 'venue', label: '上市地点', width: '190px', value: (row) => row.listing_location, sub: (row) => date(row.quote_date) }
  ];
  const detailStats = $derived.by<Stat[]>(() => selected ? [
    { label: 'GDR价格', value: `${price(selected.gdr_price)} ${selected.currency}` },
    { label: 'A股价格', value: price(selected.underlying_price) },
    { label: '折算GDR价格', value: price(selected.converted_gdr_price) },
    { label: '溢价率', value: delta(selected.premium_pct) }
  ] : []);
  onMount(() => load());
</script>

<PageHeader eyebrow="GDR · A-SHARE LINK" title="GDR跨市场映射" description="GDR 行情、币种、上市地点、发行量和兑换比例，并关联 A 股标的、源折算价格与溢价率快照。" {stats}>
  {#snippet actions()}
    <Button onclick={toggleOrder}>{order === 'desc' ? '降序 ↓' : '升序 ↑'}</Button>
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>
<Split asideWidth="380px">
  {#snippet main()}
    <Panel title="GDR列表" subtitle={doc ? `${count(doc.match_count)} 条 · ${date(doc.summary.latest_quote_date)}` : '本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={() => load()} empty={resource.loaded && rows.length === 0} emptyText="暂无 GDR 数据。" flush scroll>
      {#snippet toolbar()}
        <TextInput bind:value={query} icon="search" width="220px" label="检索GDR或A股" placeholder="代码或名称" onEnter={() => load()} />
        <Segmented options={SORTS} value={sort} onChange={switchSort} ariaLabel="GDR排序" />
      {/snippet}
      <DataTable {columns} {rows} rowKey={(row) => row.record_id} onRowClick={(row) => (selectedId = row.record_id)} isActive={(row) => row.record_id === selected?.record_id} stickyFirst numbered minWidth="1100px" />
    </Panel>
  {/snippet}
  {#snippet aside()}
    <Panel eyebrow={selected?.gdr_code ?? 'GDR'} title={selected?.gdr_name ?? 'GDR详情'} subtitle={selected ? `${selected.underlying.security_id} · ${selected.listing_location}` : '选择左侧记录'} empty={!selected && !resource.busy}>
      {#if selected}
        <button type="button" onclick={openStock}>打开关联A股</button>
        <StatGrid stats={detailStats} inline />
        <p>发行量 {compact(selected.issuance_units)}，兑换比例 1 : {fixed(selected.conversion_ratio)}。价格与溢价均为 {date(selected.quote_date)} 的源快照。</p>
      {/if}
    </Panel>
  {/snippet}
</Split>
<style>
  button { min-width: 72px; height: 28px; padding: 0 var(--sp-3); color: var(--focus); border: 1px solid var(--line-strong); border-radius: var(--radius); }
  p { margin-top: var(--sp-3); font-size: var(--fs-xs); line-height: 1.7; color: var(--fg-mute); }
</style>
