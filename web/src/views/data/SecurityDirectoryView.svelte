<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, fixed, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import Icon from '../../ui/Icon.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { SecuritySearchDocument, SecuritySearchRecord } from '../../types';

  const MARKETS = [
    { id: 'all', label: '三市' }, { id: 'sz', label: '深圳' },
    { id: 'sh', label: '上海' }, { id: 'bj', label: '北京' }
  ];
  const CATEGORIES = [
    { id: 'all', label: '全部品种' }, { id: 'a_share', label: 'A 股' },
    { id: 'index', label: '指数' }, { id: 'etf', label: 'ETF' },
    { id: 'fund', label: '其他基金' }, { id: 'convertible_bond', label: '可转债' },
    { id: 'bond', label: '其他债券' }, { id: 'repo', label: '回购' },
    { id: 'b_share', label: 'B 股' }, { id: 'unknown', label: '待分类品种' }
  ];
  const CATEGORY_LABEL = Object.fromEntries(CATEGORIES.map((item) => [item.id, item.label]));

  let market = $state('all');
  let category = $state('all');
  let query = $state('');
  const directory = new Resource<SecuritySearchDocument>();
  const doc = $derived(directory.data);
  const rows = $derived(doc?.securities ?? []);

  const stats = $derived.by(() => {
    const counts = doc?.summary?.by_category;
    if (!doc?.summary || !counts) return [];
    return [
      { label: '服务端证券', value: count(doc.summary.available), note: `${doc.command_count} + ${doc.command_list}` },
      { label: 'A 股', value: count(counts.a_share ?? 0) },
      { label: '指数 / ETF', value: `${count(counts.index ?? 0)} / ${count(counts.etf ?? 0)}` },
      { label: '可转债 / 债券', value: `${count(counts.convertible_bond ?? 0)} / ${count(counts.bond ?? 0)}` },
      { label: '当前命中', value: count(doc.match_count), note: doc.truncated ? `展示前 ${count(doc.returned)} 条` : '已完整返回' },
      { label: '市场源', value: doc.sources?.map((source) => `${source.market.toUpperCase()} ${count(source.received_count)}`).join(' · ') ?? '—' }
    ];
  });

  function load(refresh = false) {
    void directory.load(`/api/v1/market/securities?${queryString({
      market, category, q: query.trim(), limit: 5000, refresh: refresh ? 1 : 0
    })}`);
  }

  function open(row: SecuritySearchRecord) {
    app.setStock({ market: row.market, code: row.code, name: row.name });
    router.go(stockPath(row.market, row.code));
  }

  const columns: Column<SecuritySearchRecord>[] = [
    { key: 'security', label: '证券', width: '152px', value: (row) => row.name || row.code, sub: (row) => row.security_id, sortValue: (row) => row.code },
    { key: 'category', label: '品种', width: '88px', value: (row) => CATEGORY_LABEL[row.category ?? 'unknown'] ?? text(row.category), sub: (row) => text(row.board) },
    { key: 'close', label: '昨收', align: 'right', num: true, value: (row) => fixed(row.previous_close_price, row.decimal ?? 2), sortValue: (row) => row.previous_close_price ?? 0 },
    { key: 'decimal', label: '价格精度', align: 'right', num: true, value: (row) => count(row.decimal), sortValue: (row) => row.decimal ?? 0 },
    { key: 'multiple', label: '每手数量', align: 'right', num: true, value: (row) => count(row.multiple), sortValue: (row) => row.multiple ?? 0 },
    { key: 'ratio', label: '量比基数', align: 'right', num: true, value: (row) => fixed(row.volume_ratio_base, 4), sortValue: (row) => row.volume_ratio_base ?? 0 },
    { key: 'reason', label: '分类依据', wrap: true, value: (row) => text(row.category_reason) },
    { key: 'raw', label: '尾部原始值', num: true, value: (row) => text(row.raw_tail_hex) }
  ];

  onMount(() => load());
</script>

<PageHeader
  eyebrow="0x044E COUNT · 0x044D DIRECTORY"
  title="服务端证券目录"
  description="直接从行情主站更新完整品种表，覆盖股票、指数、ETF、基金、债券与可转债，并保留服务端价格精度。"
  {stats}
>
  {#snippet actions()}<Button icon="refresh" busy={directory.busy} onclick={() => load(true)}>强制更新三市</Button>{/snippet}
</PageHeader>

<Panel
  title="证券主表"
  subtitle={doc?.sources?.[0] ? `${doc.sources[0].endpoint} · ${doc.sources[0].server_name}` : '首次打开会从行情主站分页获取三市目录'}
  busy={directory.busy}
  error={directory.error}
  onRetry={() => load()}
  empty={directory.loaded && !directory.busy && rows.length === 0}
  emptyText="当前市场、品种与检索条件下没有证券。"
  flush
  scroll
>
  {#snippet toolbar()}
    <Segmented options={MARKETS} value={market} ariaLabel="证券市场" onChange={(next) => { market = next; load(); }} />
    <Select options={CATEGORIES} value={category} width="160px" label="品种" onChange={(next) => { category = next; load(); }} />
    <TextInput bind:value={query} icon="search" width="230px" label="检索" placeholder="名称、代码、SH/SZ/BJ 全代码" onEnter={() => load()} />
    <p class="notice"><Icon name="info" size={11} />分类由交易所与代码前缀推断；“待分类”仍保留原始目录记录。</p>
  {/snippet}
  <DataTable {columns} {rows} stickyFirst numbered rowKey={(row) => row.security_id} onRowClick={open} />
</Panel>

<style>
  .notice { display: inline-flex; align-items: center; gap: var(--sp-1); margin-left: auto; font-size: 10px; color: var(--warn); }
</style>
