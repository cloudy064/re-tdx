<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import type { MarginRecord, MarketMarginDocument } from '../../types';

  const CATEGORIES = [
    { id: 'market', label: '市场日序列' },
    { id: 'transfer', label: '转融资 / 转融券' },
    { id: 'balance', label: '两融差额排行' },
    { id: 'financing-ratio', label: '融资余额占比' },
    { id: 'short-ratio', label: '融券余量占比' },
    { id: 'financing-up-continuous', label: '融资连续增加' },
    { id: 'financing-down-continuous', label: '融资连续减少' },
    { id: 'financing-up-large', label: '融资大幅增加' },
    { id: 'financing-down-large', label: '融资大幅减少' },
    { id: 'short-up-continuous', label: '融券连续增加' },
    { id: 'short-down-continuous', label: '融券连续减少' },
    { id: 'short-up-large', label: '融券大幅增加' },
    { id: 'short-down-large', label: '融券大幅减少' },
    { id: 'short-balance', label: '融券余额排行' },
    { id: 'etf', label: 'ETF 两融' },
    { id: 'class-industry', label: '行业两融' },
    { id: 'class-concept', label: '概念两融' },
    { id: 'class-style', label: '风格两融' }
  ];
  let category = $state('market');
  const resource = new Resource<MarketMarginDocument>();
  const detail = new Resource<MarketMarginDocument>();
  const doc = $derived(resource.data);
  const rows = $derived(doc?.records ?? []);
  const historyRows = $derived(detail.data?.records ?? []);
  const classificationMode = $derived(category.startsWith('class-'));
  const transferMode = $derived(category === 'transfer');
  const selectedClassification = $derived(detail.data?.group_id ?? '');
  const latest = $derived(doc?.latest ?? null);
  const stats = $derived([
    { label: '数据行', value: count(doc?.count), note: `缓存 ${count(doc?.cache.age_seconds)}s` },
    { label: '最新日期', value: date(latest?.date) },
    { label: transferMode ? '转融资余额' : '融资余额', value: compact(transferMode ? latest?.transfer_financing_balance_yuan : latest?.financing_balance_yuan, '元') },
    { label: transferMode ? '转融资偿还' : '融资占流通市值', value: transferMode ? compact(latest?.transfer_financing_repaid_yuan, '元') : percent(latest?.financing_balance_float_market_pct) },
    { label: transferMode ? '转融券余量' : '融券余额', value: transferMode ? compact(latest?.securities_lending_balance_shares, '股') : compact(latest?.short_balance_yuan ?? latest?.short_balance_shares, latest?.short_balance_yuan ? '元' : '股') },
    { label: transferMode ? '转融资净变动' : '融资净变动', value: compact(transferMode ? latest?.transfer_financing_net_change_yuan : latest?.financing_net_change_yuan ?? latest?.financing_net_buy_yuan, '元'), tone: tone(transferMode ? latest?.transfer_financing_net_change_yuan : latest?.financing_net_change_yuan ?? latest?.financing_net_buy_yuan) }
  ]);

  const columns: Column<MarginRecord>[] = [
    { key: 'name', label: '证券 / 日期', width: '130px', value: (row) => row.security?.name || date(row.date), sub: (row) => row.security?.security_id || '' },
    { key: 'date', label: '统计日', width: '92px', value: (row) => date(row.date) },
    { key: 'financing', label: '融资余额', align: 'right', num: true, value: (row) => compact(row.financing_balance_yuan, '元'), sortValue: (row) => row.financing_balance_yuan ?? 0 },
    { key: 'financing-change', label: '融资净变动', align: 'right', num: true, value: (row) => compact(row.financing_net_change_yuan ?? row.financing_net_buy_yuan, '元'), tone: (row) => tone(row.financing_net_change_yuan ?? row.financing_net_buy_yuan) },
    { key: 'financing-ratio', label: '融资占流通市值', align: 'right', num: true, value: (row) => percent(row.financing_balance_float_market_pct), sortValue: (row) => row.financing_balance_float_market_pct ?? 0 },
    { key: 'short', label: '融券余量', align: 'right', num: true, value: (row) => compact(row.short_balance_shares ?? row.short_balance_yuan, row.short_balance_shares !== undefined ? '股' : '元') },
    { key: 'short-ratio', label: '融券占比', align: 'right', num: true, value: (row) => percent(row.short_balance_float_shares_pct ?? row.short_balance_float_market_pct) },
    { key: 'difference', label: '两融差额', align: 'right', num: true, value: (row) => compact(row.financing_short_difference_yuan, '元'), sortValue: (row) => row.financing_short_difference_yuan ?? 0 }
  ];
  const classificationColumns: Column<MarginRecord>[] = [
    { key: 'name', label: '分类', width: '180px', value: (row) => row.classification_name || row.classification_code || '—', sub: (row) => row.classification_code || '' },
    { key: 'date', label: '统计日', width: '92px', value: (row) => date(row.date) },
    { key: 'financing', label: '融资余额', align: 'right', num: true, value: (row) => compact(row.financing_balance_yuan, '元'), sortValue: (row) => row.financing_balance_yuan ?? 0 },
    { key: 'buy', label: '融资买入', align: 'right', num: true, value: (row) => compact(row.financing_buy_yuan, '元') },
    { key: 'net', label: '融资净买入', align: 'right', num: true, value: (row) => compact(row.financing_net_buy_yuan, '元'), tone: (row) => tone(row.financing_net_buy_yuan) },
    { key: 'short', label: '融券余额', align: 'right', num: true, value: (row) => compact(row.short_balance_yuan, '元') },
    { key: 'difference', label: '两融差额', align: 'right', num: true, value: (row) => compact(row.financing_short_difference_yuan, '元'), sortValue: (row) => row.financing_short_difference_yuan ?? 0 }
  ];
  const transferColumns: Column<MarginRecord>[] = [
    { key: 'date', label: '统计日', width: '100px', value: (row) => date(row.date) },
    { key: 'lent', label: '转融资融出', align: 'right', num: true, value: (row) => compact(row.transfer_financing_lent_yuan, '元') },
    { key: 'repaid', label: '转融资偿还', align: 'right', num: true, value: (row) => compact(row.transfer_financing_repaid_yuan, '元') },
    { key: 'net', label: '转融资净增', align: 'right', num: true, value: (row) => compact(row.transfer_financing_net_change_yuan, '元'), tone: (row) => tone(row.transfer_financing_net_change_yuan) },
    { key: 'balance', label: '转融资余额', align: 'right', num: true, value: (row) => compact(row.transfer_financing_balance_yuan, '元') },
    { key: 'sl-lent', label: '转融券融出', align: 'right', num: true, value: (row) => compact(row.securities_lending_lent_shares, '股') },
    { key: 'sl-repaid', label: '转融券偿还', align: 'right', num: true, value: (row) => compact(row.securities_lending_repaid_shares, '股') },
    { key: 'sl-net', label: '转融券净增', align: 'right', num: true, value: (row) => compact(row.securities_lending_net_change_shares, '股'), tone: (row) => tone(row.securities_lending_net_change_shares) },
    { key: 'sl-balance', label: '转融券余量', align: 'right', num: true, value: (row) => compact(row.securities_lending_balance_shares, '股') },
    { key: 'sl-value', label: '转融券余额', align: 'right', num: true, value: (row) => compact(row.securities_lending_balance_yuan, '元') }
  ];
  const historyColumns: Column<MarginRecord>[] = [
    { key: 'date', label: '日期', width: '96px', value: (row) => date(row.date) },
    { key: 'financing', label: '融资余额', align: 'right', num: true, value: (row) => compact(row.financing_balance_yuan, '元'), sortValue: (row) => row.financing_balance_yuan ?? 0 },
    { key: 'short', label: '融券余额', align: 'right', num: true, value: (row) => compact(row.short_balance_yuan, '元'), sortValue: (row) => row.short_balance_yuan ?? 0 },
    { key: 'difference', label: '两融差额', align: 'right', num: true, value: (row) => compact(row.financing_short_difference_yuan, '元'), tone: (row) => tone(row.financing_short_difference_yuan) }
  ];

  function load(refresh = false) {
    const classification = category.startsWith('class-') ? category.slice(6) : '';
    const view = category === 'market' ? 'market' : category === 'transfer' ? 'transfer' : classification ? 'classifications' : 'ranking';
    detail.reset();
    void resource.load(`/api/v1/market/margin?${queryString({ view, category: category === 'market' || category === 'transfer' ? 'balance' : classification || category, limit: 5000, refresh: refresh ? 1 : 0 })}`);
  }
  function open(row: MarginRecord) {
    if (classificationMode && row.classification_code) {
      void detail.load(`/api/v1/market/margin?${queryString({ view: 'classification-history', group_id: row.classification_code, limit: 5000 })}`);
      return;
    }
    if (!row.security) return;
    app.setStock({ market: row.security.market, code: row.security.code, name: row.security.name });
    router.go(stockPath(row.security.market, row.security.code, 'leverage'));
  }
  onMount(() => load());
</script>

<PageHeader eyebrow="RZRQ · TRANSFER / SECURITY / ETF / INDUSTRY / CONCEPT / STYLE" title="融资融券" description="市场总量、转融资/转融券历史、证券与 ETF 排行，以及行业、概念、风格分类和可下钻曲线。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>{/snippet}
</PageHeader>
<Panel flush scroll fill title={CATEGORIES.find((item) => item.id === category)?.label ?? '融资融券'} busy={resource.busy} error={resource.error} empty={resource.loaded && rows.length === 0} emptyText="该口径当前没有记录。" onRetry={() => load()}>
  {#snippet toolbar()}<Select options={CATEGORIES} value={category} width="210px" label="口径" onChange={(next) => { category = next; load(); }} />{/snippet}
  {#if classificationMode}
    <DataTable columns={classificationColumns} {rows} rowKey={(row, index) => `${row.classification_code ?? 'class'}-${row.date}-${index}`} onRowClick={open} isActive={(row) => row.classification_code === selectedClassification} stickyFirst numbered />
    {#if detail.loaded || detail.busy}
      <section class="history">
        <div><strong>{rows.find((row) => row.classification_code === selectedClassification)?.classification_name || selectedClassification}</strong><span>{count(historyRows.length)} 个交易日</span></div>
        {#if detail.error}<p>{detail.error}</p>{:else}<DataTable columns={historyColumns} rows={historyRows} rowKey={(row, index) => `${row.date}-${index}`} numbered />{/if}
      </section>
    {/if}
  {:else if transferMode}
    <DataTable columns={transferColumns} {rows} rowKey={(row, index) => `${row.date}-${index}`} numbered />
  {:else}
    <DataTable {columns} {rows} rowKey={(row, index) => `${row.security?.security_id ?? 'market'}-${row.date}-${index}`} onRowClick={open} stickyFirst numbered />
  {/if}
</Panel>

<style>
  .history { margin: var(--sp-3); border: 1px solid var(--line-strong); border-radius: var(--radius); overflow: hidden; }
  .history > div { display: flex; justify-content: space-between; padding: var(--sp-2) var(--sp-3); background: var(--surface-2); }
  .history span, .history p { color: var(--fg-mute); font-size: var(--fs-micro); }
  .history p { padding: var(--sp-3); }
</style>
