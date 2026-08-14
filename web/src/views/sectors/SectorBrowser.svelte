<script lang="ts">
  /**
   * 板块浏览器。左栏板块主表，右栏成分股。
   * 成分股行直接落到个股工作台——重构前只能弹一个抽屉，路由丢失且无法分享。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, marketKey, marketLabel, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { Block, BlockResult, Member } from '../../types';

  const FAMILIES = [
    { id: 'industry', label: '通达信行业', hint: '三级行业分类，一只股票只归一个行业' },
    { id: 'research-industry', label: '研究行业', hint: '研究板块与连板天梯使用的行业分类' },
    { id: 'concept', label: '概念板块', hint: '题材概念，一只股票可归多个概念' },
    { id: 'style', label: '风格板块', hint: '规模、地域等风格分类' },
    { id: 'index', label: '指数板块', hint: '指数成分股' }
  ];

  /** 成分股单独按 block_id 精确查询，一次最多取这么多。 */
  const MEMBER_LIMIT = 2000;

  const routeFamily = router.segment(1);
  const routeCode = router.segment(2);
  const initialFamily = FAMILIES.some((item) => item.id === routeFamily)
    ? routeFamily
    : 'industry';
  let family = $state(initialFamily);
  // 空检索词即列出该族全部板块。旧版默认「银行」只命中 1 个板块，
  // 一进页面几乎是空屏，对「浏览器」这个定位是错的默认值。
  let query = $state(routeCode);
  let selectedId = $state('');

  const catalog = new Resource<BlockResult>();
  const members = new Resource<BlockResult>();

  const blocks = $derived(catalog.data?.blocks ?? []);
  const selected = $derived(blocks.find((item) => item.block_id === selectedId) ?? null);
  const memberRows = $derived(members.data?.members ?? []);

  const familyLabel = $derived(FAMILIES.find((item) => item.id === family)?.label ?? '');

  const stats = $derived.by(() => [
    { label: '板块数', value: count(catalog.data?.block_count ?? 0) },
    { label: '成分关系', value: count(catalog.data?.membership_count ?? 0) },
    { label: '板块族', value: familyLabel },
    { label: '当前板块', value: selected ? selected.name : '—' },
    { label: '当前成分', value: selected ? count(selected.member_count) : '—' },
    { label: '检索词', value: query.trim() || '全部' }
  ]);

  const memberEmptyText = $derived(
    selected ? '该板块在本地数据里没有成分股记录。' : '选择左侧任意板块，查看它的成分股。'
  );

  /**
   * 接口要求 q 必填，所以空搜索用 `<family>:` 当通配符——
   * block_id 一律是 `族名:代码`，配合 family 过滤即可列出整族板块。
   */
  async function loadBlocks() {
    members.reset();
    selectedId = '';
    const result = await catalog.load(
      `/api/v1/blocks?${queryString({ q: query.trim() || `${family}:`, family, limit: 1 })}`
    );
    const first = result?.blocks?.[0];
    if (first) selectBlock(first);
  }

  function selectBlock(block: Block) {
    selectedId = block.block_id;
    void members.load(
      `/api/v1/blocks?${queryString({ q: block.block_id, family, limit: MEMBER_LIMIT })}`
    );
  }

  function switchFamily(next: string) {
    family = next;
    void loadBlocks();
  }

  function openStock(member: Member) {
    const market = marketKey(member.market_id);
    app.setStock({ market, code: member.code, name: member.security_name });
    router.go(stockPath(market, member.code));
  }

  const blockColumns: Column<Block>[] = [
    {
      key: 'name',
      label: '板块名称',
      value: (row) => row.name,
      sub: (row) => row.family_name,
      sortValue: (row) => row.name
    },
    {
      key: 'code',
      label: '板块代码',
      width: '88px',
      num: true,
      value: (row) => row.block_code,
      sortValue: (row) => row.block_code
    },
    {
      key: 'level',
      label: '层级',
      width: '62px',
      num: true,
      value: (row) => `L${row.level}${row.is_leaf ? '' : ' +'}`,
      sortValue: (row) => row.level
    },
    {
      key: 'members',
      label: '成分数',
      width: '84px',
      align: 'right',
      num: true,
      value: (row) => count(row.member_count),
      sortValue: (row) => row.member_count ?? 0
    }
  ];

  const memberColumns: Column<Member>[] = [
    { key: 'code', label: '代码', width: '66px', num: true, value: (row) => row.code },
    {
      key: 'name',
      label: '名称',
      value: (row) => text(row.security_name),
      sub: (row) => text(row.membership)
    },
    {
      key: 'market',
      label: '市场',
      width: '48px',
      num: true,
      value: (row) => marketLabel(row.market_id)
    }
  ];

  onMount(() => void loadBlocks());
</script>

<PageHeader
  eyebrow="BLOCK*.DAT · 本地板块库"
  title="板块浏览器"
  description="通达信行业、概念、风格与指数四族板块及其成分股；点击成分股直接进入个股工作台。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={catalog.busy} onclick={() => void loadBlocks()}>重新载入</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="380px">
  {#snippet main()}
    <Panel
      flush
      scroll
      title="板块列表"
      subtitle={`${familyLabel} · ${count(blocks.length)} 个板块`}
      busy={catalog.busy}
      error={catalog.error}
      onRetry={() => void loadBlocks()}
      empty={catalog.loaded && !catalog.busy && blocks.length === 0}
      emptyText="没有匹配的板块，换个关键词或清空检索列出整族板块。"
    >
      {#snippet toolbar()}
        <Segmented options={FAMILIES} value={family} onChange={switchFamily} ariaLabel="板块族" />
        <TextInput
          bind:value={query}
          icon="search"
          width="240px"
          label="检索板块"
          placeholder="板块名称或代码，留空列出全部"
          onEnter={() => void loadBlocks()}
        />
        <Button icon="search" onclick={() => void loadBlocks()}>查询</Button>
      {/snippet}

      <DataTable
        columns={blockColumns}
        rows={blocks}
        rowKey={(row) => row.block_id}
        sortKey="members"
        onRowClick={selectBlock}
        isActive={(row) => row.block_id === selectedId}
      />
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      flush
      scroll
      eyebrow="MEMBERS"
      title={selected ? selected.name : '成分股'}
      subtitle={selected
        ? `${selected.block_code} · ${count(memberRows.length)} / ${count(selected.member_count)} 只`
        : '点击左侧板块'}
      busy={members.busy}
      error={members.error}
      onRetry={() => selected && selectBlock(selected)}
      empty={!selected || (members.loaded && !members.busy && memberRows.length === 0)}
      emptyText={memberEmptyText}
    >
      <DataTable
        numbered
        stickyFirst
        columns={memberColumns}
        rows={memberRows}
        rowKey={(row) => `${row.market_id}-${row.code}`}
        onRowClick={openStock}
        isActive={(row) => marketKey(row.market_id) === app.stock.market && row.code === app.stock.code}
      />

      {#if members.data?.truncated}
        <p class="warn-line">成分股超过 {count(MEMBER_LIMIT)} 条，已截断显示。</p>
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .warn-line {
    margin: var(--sp-2) var(--sp-3);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }
</style>
