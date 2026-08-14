<script lang="ts">
  /**
   * 机构调研与投资者活动（TZZHD · 709 / 1721）。
   *
   * 三类活动共用同一套主表结构，但正文走各自的动态键（调研 j+代码、互动 h+代码），
   * 所以切换口径必须重新取详情，不能只在前端过滤。正文动辄几十 KB，
   * 因此列表与正文分成两块：上表选行，下面只渲染选中那一条。
   */
  import { queryString } from '../../../api';
  import { Resource } from '../../../lib/resource.svelte';
  import { count, date, num, percent, text, tone } from '../../../lib/fmt';
  import Badge from '../../../ui/Badge.svelte';
  import Button from '../../../ui/Button.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Icon from '../../../ui/Icon.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import Segmented from '../../../ui/Segmented.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { MarketResearchDocument, ResearchActivity } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();

  const CATEGORIES = [
    { id: 'institution-research', label: '机构调研', hint: '最新机构调研主表 · 动态键 j+代码' },
    { id: 'interaction', label: '互动问答', hint: '最新互动问答主表 · 动态键 h+代码' },
    { id: 'featured-interaction', label: '精选互动', hint: '精选互动问答主表 · 动态键 h+代码' }
  ];

  let category = $state('institution-research');
  let picked = $state<ResearchActivity | null>(null);

  const research = new Resource<MarketResearchDocument>();
  const doc = $derived(research.data);
  const record = $derived(doc?.selected ?? null);
  const data = $derived(record?.data ?? null);
  const activities = $derived<ResearchActivity[]>(doc?.activities ?? []);
  // 换口径或重取后旧对象已不在列表里，回落到第一条而不是留空
  const selected = $derived<ResearchActivity | null>(
    (picked && activities.includes(picked) ? picked : activities[0]) ?? null
  );
  const failures = $derived(doc?.detail_errors ?? []);

  function load(refresh = false) {
    void research.load(
      `/api/v1/market/research?${queryString({
        market,
        code,
        category,
        detail_limit: 100,
        include_text: 1,
        refresh: refresh ? 1 : 0
      })}`
    );
  }

  function switchCategory(next: string) {
    category = next;
    picked = null;
    load();
  }

  const stats = $derived.by<Stat[]>(() => {
    if (!doc) return [];
    return [
      {
        label: '近一周 / 一月调研',
        value: `${text(data?.research_count_1w)} / ${text(data?.research_count_1m)}`,
        note: `近三月 ${text(data?.research_count_3m)} · 近六月 ${text(data?.research_count_6m)}`
      },
      {
        label: '最近一次参与机构',
        value: text(data?.latest_institution_count),
        note: `近一月 ${text(data?.institution_count_1m)} 家 · 近三月 ${text(data?.institution_count_3m)} 家`
      },
      {
        label: '互动问答次数',
        value: `${text(data?.interaction_count_1m)} / ${text(data?.interaction_count_3m)}`,
        note: '近一月 / 近三月'
      },
      {
        label: '近一月区间涨幅',
        value: percent(data?.return_pct_1m, 2, true),
        tone: tone(num(data?.return_pct_1m))
      },
      {
        label: '近三月区间涨幅',
        value: percent(data?.return_pct_3m, 2, true),
        tone: tone(num(data?.return_pct_3m))
      },
      {
        label: '最新活动日',
        value: date(record?.latest_date),
        note: `所属行业 ${text(data?.industry)}`
      },
      {
        label: '正文条数',
        value: count(doc.counts.activities),
        note: `动态键 ${text(doc.cache.detail_id)}`
      }
    ];
  });

  const columns: Column<ResearchActivity>[] = [
    {
      key: 'date',
      label: '日期',
      width: '84px',
      num: true,
      value: (row) => date(row.date),
      sortValue: (row) => row.date ?? ''
    },
    { key: 'title', label: '活动标题', wrap: true, value: (row) => text(row.title) },
    {
      key: 'length',
      label: '正文字节',
      align: 'right',
      width: '84px',
      num: true,
      value: (row) => count(row.text_length),
      sortValue: (row) => row.text_length ?? 0
    },
    {
      key: 'attachments',
      label: '附件',
      align: 'right',
      width: '56px',
      num: true,
      value: (row) => count(row.source_urls.length)
    }
  ];

  $effect(() => {
    void market;
    void code;
    picked = null;
    load();
  });
</script>

<Panel
  title="调研热度与互动"
  eyebrow="TZZHD · 709 / 1721"
  subtitle={doc
    ? `${doc.category_label} · 主表缓存 ${doc.cache.master_age_seconds}s · 正文缓存 ${doc.cache.detail_age_seconds ?? 0}s`
    : '主表给热度，动态键给正文'}
  busy={research.busy}
  error={research.error}
  onRetry={() => load()}
  empty={research.loaded && !research.busy && !record && activities.length === 0}
  emptyText="该口径的活动主表里没有这只股票"
  scroll
>
  {#snippet actions()}
    <Button icon="refresh" busy={research.busy} onclick={() => load(true)}>强制更新</Button>
  {/snippet}

  {#snippet toolbar()}
    <Segmented options={CATEGORIES} value={category} onChange={switchCategory} ariaLabel="活动口径" />
  {/snippet}

  <StatGrid {stats} columns={4} />

  {#if doc?.category_memberships.length}
    <div class="tags">
      {#each doc.category_memberships as item (item.category)}
        <Badge tone="focus">{item.category_label}</Badge>
      {/each}
    </div>
  {/if}

  {#each failures as failure, index (index)}
    <p class="warn-line">{text(failure.resource)}：{failure.message}</p>
  {/each}
</Panel>

<Panel
  title="活动记录"
  eyebrow="ACTIVITY LIST"
  subtitle={`${count(activities.length)} 条 · 点击任意行在下方读正文`}
  busy={research.busy}
  empty={!research.busy && !research.error && activities.length === 0}
  emptyText="该股票在这个口径下没有可下载的活动正文"
  flush
  scroll
>
  <DataTable
    {columns}
    rows={activities}
    rowKey={(row, index) => `${row.date}-${index}`}
    onRowClick={(row) => (picked = row)}
    isActive={(row) => row === selected}
    maxHeight="240px"
  />
</Panel>

<Panel
  title="调研纪要正文"
  eyebrow="INVESTOR RELATIONS"
  subtitle={selected
    ? `${date(selected.date)} · ${count(selected.text_length)} 字节`
    : '在上表选择一条活动'}
  busy={research.busy}
  empty={!research.busy && !research.error && !selected}
  emptyText="尚未选中活动记录"
  scroll
>
  {#if selected}
    <h3 class="headline">{text(selected.title)}</h3>
    {#if selected.text}
      <pre>{selected.text}</pre>
    {:else}
      <p class="note">该条活动只返回了标题，没有正文。</p>
    {/if}
    {#if selected.source_urls.length}
      <div class="links">
        {#each selected.source_urls as url (url)}
          <a href={url} target="_blank" rel="noreferrer">
            <Icon name="external" size={11} />打开原始附件
          </a>
        {/each}
      </div>
    {/if}
  {/if}
</Panel>

<style>
  .tags {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-1);
    margin-top: var(--sp-3);
  }

  .warn-line {
    margin-top: var(--sp-2);
    padding: var(--sp-1) var(--sp-2);
    font-size: 10px;
    color: var(--warn);
    background: var(--warn-soft);
    border-radius: var(--radius);
  }

  .headline {
    margin-bottom: var(--sp-2);
    font-size: var(--fs-body);
    font-weight: 600;
    line-height: var(--lh-tight);
  }

  pre {
    margin: 0;
    padding: var(--sp-3);
    font-family: var(--font-ui);
    font-size: var(--fs-micro);
    line-height: 1.7;
    color: var(--fg-dim);
    white-space: pre-wrap;
    word-break: break-word;
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .note {
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  .links {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-2);
    margin-top: var(--sp-3);
  }

  .links a {
    display: inline-flex;
    align-items: center;
    gap: var(--sp-1);
    height: var(--h-control);
    padding: 0 var(--sp-3);
    font-size: var(--fs-micro);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  .links a:hover {
    background: var(--bg-hover);
  }
</style>
