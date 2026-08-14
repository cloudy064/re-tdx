<script lang="ts">
  /** BK_BKLSHC / 200199：板块区间表现与同一上游请求选出的成员。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, money, percent, price, signedCompact, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    BlockBacktestAdjustment,
    BlockBacktestBlockIdentity,
    BlockBacktestBlockRecord,
    BlockBacktestCategory,
    BlockBacktestMemberRecord,
    BlockBacktestSecurityIdentity,
    BlockBacktestSort,
    MarketBlockBacktestBlocksDocument,
    MarketBlockBacktestMembersDocument
  } from '../../types';

  type Stage = 'blocks' | 'members';

  const CATEGORIES = [
    { id: 'all', label: '全部' },
    { id: 'industry', label: '行业' },
    { id: 'concept', label: '概念' },
    { id: 'style', label: '风格' },
    { id: 'region', label: '地域' }
  ];
  const ADJUSTMENTS = [
    { id: 'none', label: '不复权' },
    { id: 'forward', label: '前复权' },
    { id: 'backward', label: '后复权' }
  ];
  const SORTS = [
    { id: 'return', label: '区间收益' },
    { id: 'net-inflow', label: '净流入' },
    { id: 'main-net-inflow', label: '主力净流入' },
    { id: 'max-drawdown', label: '最大回撤' },
    { id: 'turnover', label: '换手率' },
    { id: 'code', label: '代码' }
  ];
  const ORDERS = [
    { id: 'desc', label: '降序' },
    { id: 'asc', label: '升序' }
  ];
  const LIMITS = [100, 500, 1000, 5000].map((value) => ({
    id: String(value), label: `最多 ${value} 条`
  }));
  const SAFE_MARKETS = new Set(['sz', 'sh', 'bj']);

  function localDate(value: Date): string {
    const year = value.getFullYear();
    const month = String(value.getMonth() + 1).padStart(2, '0');
    const day = String(value.getDate()).padStart(2, '0');
    return `${year}-${month}-${day}`;
  }

  const today = new Date();
  const monthAgo = new Date(today);
  monthAgo.setDate(monthAgo.getDate() - 30);

  let stage = $state<Stage>('blocks');
  let category = $state<BlockBacktestCategory>('all');
  let begin = $state(localDate(monthAgo));
  let end = $state(localDate(today));
  let adjustment = $state<BlockBacktestAdjustment>('forward');
  let sort = $state<BlockBacktestSort>('return');
  let order = $state<'asc' | 'desc'>('desc');
  let limit = $state('1000');
  let selectedBlock = $state<BlockBacktestBlockIdentity | null>(null);
  let formError = $state('');

  const blocksResource = new Resource<MarketBlockBacktestBlocksDocument>();
  const membersResource = new Resource<MarketBlockBacktestMembersDocument>();
  const doc = $derived(stage === 'blocks' ? blocksResource.data : membersResource.data);
  const busy = $derived(stage === 'blocks' ? blocksResource.busy : membersResource.busy);
  const requestError = $derived(stage === 'blocks' ? blocksResource.error : membersResource.error);
  const categoryOptions = $derived(CATEGORIES.map((item) => ({
    ...item,
    disabled: stage === 'members'
  })));

  function categoryLabel(value: BlockBacktestCategory): string {
    return CATEGORIES.find((item) => item.id === value)?.label ?? value;
  }

  function validatePeriod(): boolean {
    if (!begin.trim() || !end.trim()) {
      formError = '开始日期和结束日期均为必填项。';
      return false;
    }
    formError = '';
    return true;
  }

  function requestPath(blockCode = '', refresh = false): string {
    return `/api/v1/market/block-backtest?${queryString({
      category,
      block_code: blockCode,
      begin: begin.trim(),
      end: end.trim(),
      adjustment,
      sort,
      order,
      limit: Number(limit),
      refresh: refresh ? 1 : 0
    })}`;
  }

  async function loadBlocks(refresh = false) {
    if (!validatePeriod()) return;
    stage = 'blocks';
    selectedBlock = null;
    await blocksResource.load(requestPath('', refresh));
  }

  function canOpenMembers(block: BlockBacktestBlockIdentity): boolean {
    return block.block_id !== null
      && block.local_match_count === 1
      && /^\d{6}$/.test(block.code);
  }

  async function loadMembers(block: BlockBacktestBlockIdentity, refresh = false) {
    if (!canOpenMembers(block) || !validatePeriod()) return;
    // A failed request for another block must never leave the previous
    // block's members visible under the newly selected identity.
    const switchingBlock = stage !== 'members'
      || selectedBlock?.code !== block.code;
    if (switchingBlock) membersResource.reset();
    selectedBlock = block;
    stage = 'members';
    await membersResource.load(requestPath(block.code, refresh));
  }

  function runCurrent(refresh = false) {
    if (stage === 'members' && selectedBlock) {
      void loadMembers(selectedBlock, refresh);
    } else {
      void loadBlocks(refresh);
    }
  }

  function returnToBlocks() {
    stage = 'blocks';
    selectedBlock = null;
    formError = '';
  }

  function canOpenSecurity(security: BlockBacktestSecurityIdentity): boolean {
    return SAFE_MARKETS.has(security.market) && /^\d{6}$/.test(security.code);
  }

  function openSecurity(security: BlockBacktestSecurityIdentity) {
    if (!canOpenSecurity(security)) return;
    app.setStock({
      market: security.market,
      code: security.code,
      name: security.name || security.security_id
    });
    router.go(stockPath(security.market, security.code));
  }

  const stats = $derived.by<Stat[]>(() => {
    if (!doc) return [];
    return [
      {
        label: stage === 'blocks' ? '板块' : '服务端选定成员',
        value: count(doc.counts.returned),
        note: `${count(doc.counts.identity_resolved)} 个身份已解析`
      },
      {
        label: '上涨 / 下跌',
        value: `${count(doc.summary.gainers)} / ${count(doc.summary.losers)}`,
        note: `${count(doc.summary.flat)} 个持平`
      },
      { label: '上涨占比', value: percent(doc.summary.positive_ratio_pct) },
      {
        label: '平均 / 中位收益',
        value: `${delta(doc.summary.average_return_pct)} / ${delta(doc.summary.median_return_pct)}`,
        tone: tone(doc.summary.average_return_pct)
      },
      {
        label: '实际交易日',
        value: count(doc.period.observed_trade_dates.length),
        note: `${date(doc.period.requested_begin)} — ${date(doc.period.requested_end)}`
      },
      {
        label: '数据状态',
        value: doc.availability === 'stale-cache' ? '陈旧缓存' : doc.cache.hit ? '命中缓存' : '在线',
        note: `缓存年龄 ${count(doc.cache.age_seconds)} 秒`
      }
    ];
  });

  const blockColumns: Column<BlockBacktestBlockRecord>[] = [
    { key: 'block', label: '板块', width: '150px', slot: true },
    { key: 'identity', label: '本地身份', width: '90px', align: 'center', slot: true },
    { key: 'trade-date', label: '交易日', width: '92px', num: true, value: (row) => date(row.trade_date) },
    {
      key: 'close', label: '前收 → 收盘', width: '116px', align: 'right', num: true,
      value: (row) => `${price(row.base_close)} → ${price(row.close)}`
    },
    {
      key: 'range', label: '最高 / 最低', width: '108px', align: 'right', num: true,
      value: (row) => `${price(row.high)} / ${price(row.low)}`
    },
    { key: 'return', label: '区间收益', width: '82px', align: 'right', num: true, value: (row) => delta(row.return_pct), tone: (row) => tone(row.return_pct) },
    { key: 'max-gain', label: '单日最大涨幅', width: '98px', align: 'right', num: true, value: (row) => delta(row.max_daily_gain_pct), tone: (row) => tone(row.max_daily_gain_pct) },
    { key: 'amplitude', label: '振幅', width: '72px', align: 'right', num: true, value: (row) => percent(row.amplitude_pct) },
    { key: 'drawdown', label: '最大回撤', width: '82px', align: 'right', num: true, value: (row) => delta(row.max_drawdown_pct), tone: (row) => tone(row.max_drawdown_pct) },
    { key: 'turnover', label: '换手率', width: '72px', align: 'right', num: true, value: (row) => percent(row.turnover_pct) },
    { key: 'amount', label: '成交额', width: '96px', align: 'right', num: true, value: (row) => money(row.amount), sub: (row) => `量 ${compact(row.volume)}` },
    { key: 'net', label: '净流入', width: '96px', align: 'right', num: true, value: (row) => signedCompact(row.net_inflow, '元'), tone: (row) => tone(row.net_inflow) },
    { key: 'main-net', label: '主力净流入', width: '100px', align: 'right', num: true, value: (row) => signedCompact(row.main_net_inflow, '元'), tone: (row) => tone(row.main_net_inflow) },
    { key: 'members', label: '成员', width: '90px', align: 'center', slot: true }
  ];

  const memberColumns: Column<BlockBacktestMemberRecord>[] = [
    { key: 'security', label: '股票', width: '140px', slot: true },
    { key: 'trade-date', label: '交易日', width: '92px', num: true, value: (row) => date(row.trade_date) },
    {
      key: 'close', label: '前收 → 收盘', width: '116px', align: 'right', num: true,
      value: (row) => `${price(row.base_close)} → ${price(row.close)}`
    },
    {
      key: 'range', label: '最高 / 最低', width: '108px', align: 'right', num: true,
      value: (row) => `${price(row.high)} / ${price(row.low)}`
    },
    { key: 'return', label: '区间收益', width: '82px', align: 'right', num: true, value: (row) => delta(row.return_pct), tone: (row) => tone(row.return_pct) },
    { key: 'max-gain', label: '单日最大涨幅', width: '98px', align: 'right', num: true, value: (row) => delta(row.max_daily_gain_pct), tone: (row) => tone(row.max_daily_gain_pct) },
    { key: 'amplitude', label: '振幅', width: '72px', align: 'right', num: true, value: (row) => percent(row.amplitude_pct) },
    { key: 'drawdown', label: '最大回撤', width: '82px', align: 'right', num: true, value: (row) => delta(row.max_drawdown_pct), tone: (row) => tone(row.max_drawdown_pct) },
    { key: 'turnover', label: '换手率', width: '72px', align: 'right', num: true, value: (row) => percent(row.turnover_pct) },
    { key: 'amount', label: '成交额', width: '96px', align: 'right', num: true, value: (row) => money(row.amount), sub: (row) => `量 ${compact(row.volume)}` },
    { key: 'net', label: '净流入', width: '96px', align: 'right', num: true, value: (row) => signedCompact(row.net_inflow, '元'), tone: (row) => tone(row.net_inflow) },
    { key: 'main-net', label: '主力净流入', width: '100px', align: 'right', num: true, value: (row) => signedCompact(row.main_net_inflow, '元'), tone: (row) => tone(row.main_net_inflow) }
  ];

  onMount(() => void loadBlocks());
</script>

<PageHeader
  eyebrow="BK_BKLSHC.xml · PBRPC 200199"
  title="板块历史回测"
  description="比较五类板块的区间行情与资金表现，并下钻同一服务端请求选出的成员。"
  {stats}
>
  {#snippet actions()}
    {#if formError}<Badge tone="warn">参数待修正</Badge>{/if}
    {#if requestError && doc}<Badge tone="warn">刷新失败，保留旧数据</Badge>{/if}
    {#if doc?.availability === 'stale-cache'}<Badge tone="warn">陈旧缓存</Badge>{:else if doc}<Badge tone="up">{doc.cache.hit ? '缓存' : '在线'}</Badge>{/if}
    <Button icon="refresh" busy={busy} onclick={() => runCurrent(true)}>刷新上游</Button>
  {/snippet}
</PageHeader>

<div class="membership-note" role="note">
  <Badge tone="warn">成员口径</Badge>
  <code>membership_basis=upstream-server-selection</code>
  <span>·</span>
  <code>historical_membership_reconstructed=false</code>
  <span>下钻成员是本次上游服务的选择，不是历史时点成分股。</span>
</div>

<Panel
  title={stage === 'blocks'
    ? `${categoryLabel(doc?.selection.category ?? category)}板块区间表现`
    : `${selectedBlock?.name || selectedBlock?.code || '所选板块'} · 服务端选定成员`}
  subtitle={doc
    ? `${date(doc.period.requested_begin)} — ${date(doc.period.requested_end)} · ${count(doc.counts.returned)} / ${count(doc.counts.normalized)} 条${doc.counts.truncated ? ' · 已按服务端结果截断' : ''}${requestError ? ` · 刷新失败：${requestError}` : ''}`
    : stage === 'blocks' ? '按服务端排序返回板块主表' : '正在读取同一 200199 端点的成员表'}
  busy={busy}
  error={doc ? '' : formError || requestError}
  onRetry={() => runCurrent(false)}
  empty={!busy && Boolean(doc) && (doc?.records.length ?? 0) === 0}
  emptyText={stage === 'blocks' ? '当前区间和分类没有板块记录。' : '上游没有为该板块返回成员记录。'}
  flush
  scroll
  fill
>
  {#snippet toolbar()}
    {#if stage === 'members'}
      <Button icon="chevron-left" onclick={returnToBlocks}>返回板块主表</Button>
    {/if}
    <Segmented options={categoryOptions} value={category} onChange={(value) => { category = value as BlockBacktestCategory; }} ariaLabel="板块分类" />
    <TextInput bind:value={begin} width="116px" label="开始日期（必填）" placeholder="开始 YYYY-MM-DD" onEnter={() => runCurrent()} />
    <TextInput bind:value={end} width="116px" label="结束日期（必填）" placeholder="结束 YYYY-MM-DD" onEnter={() => runCurrent()} />
    <Select options={ADJUSTMENTS} value={adjustment} width="118px" label="复权" onChange={(value) => { adjustment = value as BlockBacktestAdjustment; }} />
    <Select options={SORTS} value={sort} width="138px" label="服务端排序" onChange={(value) => { sort = value as BlockBacktestSort; }} />
    <Select options={ORDERS} value={order} width="80px" label="顺序" onChange={(value) => { order = value as 'asc' | 'desc'; }} />
    <Select options={LIMITS} value={limit} width="112px" label="返回上限" onChange={(value) => { limit = value; }} />
    <Button icon="search" onclick={() => runCurrent()}>{stage === 'blocks' ? '查询板块' : '更新成员'}</Button>
  {/snippet}

  {#if stage === 'blocks' && blocksResource.data}
    <DataTable
      columns={blockColumns}
      rows={blocksResource.data.records}
      numbered
      stickyFirst
      minWidth="1510px"
      rowKey={(row) => `block:${row.block.code}:${row.rank}`}
    >
      {#snippet cell({ row, column })}
        {#if column.key === 'block'}
          <span class="entity">
            <span>{row.block.name || row.block.code}</span>
            <small>{row.block.code}{row.block.family_name ? ` · ${row.block.family_name}` : ''}</small>
          </span>
        {:else if column.key === 'identity'}
          {#if row.block.block_id}
            <Badge tone={row.block.local_match_count > 1 ? 'warn' : 'focus'}>
              {row.block.local_match_count > 1 ? `${count(row.block.local_match_count)} 个映射` : '已解析'}
            </Badge>
          {:else}
            <Badge tone="neutral">未解析 identity</Badge>
          {/if}
        {:else if column.key === 'members'}
          <Button
            variant="ghost"
            icon="sectors"
            disabled={!canOpenMembers(row.block)}
            title={canOpenMembers(row.block) ? `展开 ${row.block.code} 的服务端选定成员` : '本地 block identity 未唯一解析，禁止下钻'}
            onclick={() => void loadMembers(row.block)}
          >{canOpenMembers(row.block) ? '查看成员' : '不可下钻'}</Button>
        {/if}
      {/snippet}
    </DataTable>
  {:else if stage === 'members' && membersResource.data}
    <DataTable
      columns={memberColumns}
      rows={membersResource.data.records}
      numbered
      stickyFirst
      minWidth="1320px"
      rowKey={(row) => `security:${row.security.security_id}:${row.rank}`}
    >
      {#snippet cell({ row, column })}
        {#if column.key === 'security'}
          {#if canOpenSecurity(row.security)}
            <button class="security-link" type="button" title="在个股工作台打开" onclick={() => openSecurity(row.security)}>
              <span>{row.security.name || row.security.code}</span>
              <small>{row.security.security_id}</small>
            </button>
          {:else}
            <span class="entity" title="证券身份不满足安全路由条件">
              <span>{row.security.name || row.security.code}</span>
              <small>{row.security.security_id} · 身份未解析</small>
            </span>
          {/if}
        {/if}
      {/snippet}
    </DataTable>
  {/if}
</Panel>

<style>
  .membership-note {
    display: flex;
    flex: none;
    flex-wrap: wrap;
    align-items: center;
    gap: var(--sp-2);
    padding: var(--sp-2) var(--sp-3);
    font-size: var(--fs-micro);
    color: var(--fg-dim);
    background: var(--warn-soft);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .membership-note code {
    font-family: var(--font-num);
    color: var(--fg);
  }

  .entity,
  .security-link {
    display: inline-flex;
    min-width: 0;
    flex-direction: column;
    align-items: flex-start;
    line-height: 1.2;
    text-align: left;
  }

  .entity small,
  .security-link small {
    margin-top: 1px;
    font-family: var(--font-num);
    font-size: 9px;
    color: var(--fg-mute);
  }

  .security-link {
    color: var(--focus);
  }

  .security-link:hover span:first-child {
    text-decoration: underline;
  }
</style>
