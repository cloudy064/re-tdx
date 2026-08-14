<script lang="ts">
  /**
   * 机构调研与市场活动。
   *
   * 九张客户端主表的列结构差异只在「取哪几个字段、怎么格式化」，所以这里用一张
   * 视图配置表（CATEGORIES）驱动同一套列生成，而不是写九段几乎相同的模板。
   * 主表切换分两级：Segmented 选主题，Select 在主题内选具体表——九个选项平铺
   * 会把工具条撑爆。
   */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, num, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import Icon from '../../ui/Icon.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    MarketResearchDocument,
    ResearchEntity,
    ResearchRecord,
    ResearchSecurityRow
  } from '../../types';

  /** 字段格式化口径。count/percent 走右对齐等宽列，text 允许换行。 */
  type FieldKind = 'count' | 'percent' | 'date' | 'text';

  interface Field {
    key: string;
    label: string;
    kind: FieldKind;
    width?: string;
    /** 涨跌着色，只给收益率这类有方向的字段。 */
    toned?: boolean;
  }

  interface CategorySpec {
    id: string;
    label: string;
    group: string;
    entity: 'security' | 'industry' | 'institution';
    entityLabel: string;
    /** 空态与检索框的提示语。 */
    hint: string;
    placeholder: string;
    fields: Field[];
  }

  const GROUPS = [
    { id: 'research', label: '机构调研', hint: '最新调研与摘帽后被调研' },
    { id: 'interaction', label: '互动问答', hint: '投资者互动平台问答热度' },
    { id: 'regulatory', label: '问询监管', hint: '交易所问询、监管措施与市场禁入' },
    { id: 'coverage', label: '行业机构', hint: '行业调研热度与知名调研机构' }
  ];

  const CATEGORIES: CategorySpec[] = [
    {
      id: 'institution-research',
      label: '最新机构调研',
      group: 'research',
      entity: 'security',
      entityLabel: '股票',
      hint: '被调研个股的调研次数、接待机构数与区间涨幅。',
      placeholder: '股票代码、名称或行业',
      fields: [
        { key: 'research_count_1w', label: '近一周', kind: 'count', width: '62px' },
        { key: 'research_count_1m', label: '近一月', kind: 'count', width: '62px' },
        { key: 'research_count_3m', label: '近三月', kind: 'count', width: '62px' },
        { key: 'research_count_6m', label: '近半年', kind: 'count', width: '62px' },
        { key: 'latest_institution_count', label: '最近一次机构数', kind: 'count' },
        { key: 'institution_count_1m', label: '近一月机构数', kind: 'count' },
        { key: 'return_pct_1m', label: '近一月涨幅', kind: 'percent', toned: true },
        { key: 'industry', label: '所属行业', kind: 'text' }
      ]
    },
    {
      id: 'post-st',
      label: '摘帽后被调研',
      group: 'research',
      entity: 'security',
      entityLabel: '股票',
      hint: '撤销风险警示后重新获得机构关注的个股。',
      placeholder: '股票代码、名称或行业',
      fields: [
        { key: 'post_st_date', label: '摘帽日', kind: 'date', width: '84px' },
        { key: 'research_count_1m', label: '近一月', kind: 'count', width: '62px' },
        { key: 'research_count_3m', label: '近三月', kind: 'count', width: '62px' },
        { key: 'research_count_6m', label: '近半年', kind: 'count', width: '62px' },
        { key: 'institution_count_1m', label: '近一月机构数', kind: 'count' },
        { key: 'return_pct_1m', label: '近一月涨幅', kind: 'percent', toned: true },
        { key: 'industry', label: '所属行业', kind: 'text' }
      ]
    },
    {
      id: 'interaction',
      label: '最新互动问答',
      group: 'interaction',
      entity: 'security',
      entityLabel: '股票',
      hint: '互动平台提问与回复的活跃个股。',
      placeholder: '股票代码、名称或行业',
      fields: [
        { key: 'interaction_count_1m', label: '近一月互动', kind: 'count' },
        { key: 'interaction_count_3m', label: '近三月互动', kind: 'count' },
        { key: 'return_pct_1m', label: '近一月涨幅', kind: 'percent', toned: true },
        { key: 'industry', label: '所属行业', kind: 'text' }
      ]
    },
    {
      id: 'featured-interaction',
      label: '精选互动问答',
      group: 'interaction',
      entity: 'security',
      entityLabel: '股票',
      hint: '客户端精选的互动问答条目。',
      placeholder: '股票代码、名称或行业',
      fields: [
        { key: 'interaction_count_1m', label: '近一月互动', kind: 'count' },
        { key: 'interaction_count_3m', label: '近三月互动', kind: 'count' },
        { key: 'return_pct_1m', label: '近一月涨幅', kind: 'percent', toned: true },
        { key: 'industry', label: '所属行业', kind: 'text' }
      ]
    },
    {
      id: 'exchange-inquiry',
      label: '交易所问询',
      group: 'regulatory',
      entity: 'security',
      entityLabel: '股票',
      hint: '收到问询函的个股与问询类别。',
      placeholder: '股票代码、名称或问询类别',
      fields: [
        { key: 'count_10y', label: '近十年次数', kind: 'count' },
        { key: 'event_category', label: '问询类别', kind: 'text' },
        { key: 'subject', label: '涉及对象', kind: 'text' },
        { key: 'industry', label: '所属行业', kind: 'text' }
      ]
    },
    {
      id: 'regulatory',
      label: '交易所监管',
      group: 'regulatory',
      entity: 'security',
      entityLabel: '股票',
      hint: '监管措施、处分对象与责任人类型。',
      placeholder: '股票代码、名称或监管措施',
      fields: [
        { key: 'count_10y', label: '近十年次数', kind: 'count' },
        { key: 'regulatory_measure', label: '监管措施', kind: 'text' },
        { key: 'subject', label: '涉及人员', kind: 'text' },
        { key: 'subject_type', label: '人物类型', kind: 'text' }
      ]
    },
    {
      id: 'market-ban',
      label: '市场禁入',
      group: 'regulatory',
      entity: 'security',
      entityLabel: '股票',
      hint: '被采取市场禁入措施的责任人与年限。',
      placeholder: '股票代码、名称或禁入对象',
      fields: [
        { key: 'count_10y', label: '近十年次数', kind: 'count' },
        { key: 'ban_years', label: '禁入年限', kind: 'text', width: '72px' },
        { key: 'permanent', label: '是否终身', kind: 'text', width: '72px' },
        { key: 'subject', label: '涉及人员', kind: 'text' },
        { key: 'subject_type', label: '人物类型', kind: 'text' }
      ]
    },
    {
      id: 'industry',
      label: '行业调研热度',
      group: 'coverage',
      entity: 'industry',
      entityLabel: '行业',
      hint: '一级研究行业的调研次数与机构覆盖率。',
      placeholder: '行业代码或名称',
      fields: [
        { key: 'research_count_1m', label: '近一月', kind: 'count', width: '62px' },
        { key: 'research_count_3m', label: '近三月', kind: 'count', width: '62px' },
        { key: 'research_count_6m', label: '近半年', kind: 'count', width: '62px' },
        { key: 'industry_coverage_pct_1m', label: '近一月覆盖率', kind: 'percent' },
        { key: 'industry_company_count', label: '行业公司数', kind: 'count' },
        { key: 'return_pct_1m', label: '近一月涨幅', kind: 'percent', toned: true }
      ]
    },
    {
      id: 'notable-institution',
      label: '知名调研机构',
      group: 'coverage',
      entity: 'institution',
      entityLabel: '机构',
      hint: '知名机构的调研频次与覆盖股票数。',
      placeholder: '机构名称或类型',
      fields: [
        { key: 'institution_type', label: '机构类型', kind: 'text', width: '84px' },
        { key: 'institution_research_count_1m', label: '近一月调研', kind: 'count' },
        { key: 'institution_research_count_3m', label: '近三月调研', kind: 'count' },
        { key: 'institution_research_count_6m', label: '近半年调研', kind: 'count' },
        { key: 'institution_stock_count_1m', label: '近一月股数', kind: 'count' },
        { key: 'institution_stock_count_6m', label: '近半年股数', kind: 'count' },
        { key: 'researchers', label: '调研人员', kind: 'text' }
      ]
    }
  ];

  let group = $state('research');
  let category = $state('institution-research');
  let query = $state('');

  const catalog = new Resource<MarketResearchDocument>();
  const detail = new Resource<MarketResearchDocument>();

  const doc = $derived(catalog.data);
  const spec = $derived(CATEGORIES.find((item) => item.id === category) ?? CATEGORIES[0]);
  const rows = $derived(doc?.records ?? []);

  const categoryOptions = $derived(
    CATEGORIES.filter((item) => item.group === group).map((item) => {
      const meta = doc?.categories.find((entry) => entry.id === item.id);
      return { id: item.id, label: meta ? `${item.label} · ${count(meta.record_count)}` : item.label };
    })
  );

  const stats = $derived.by(() => {
    if (!doc) return [];
    return [
      { label: '主表数量', value: count(doc.counts.categories) },
      { label: '九表总行数', value: count(doc.counts.total_rows) },
      { label: '覆盖实体', value: count(doc.counts.unique_entities) },
      { label: '当前主表行数', value: count(doc.counts.category_rows) },
      { label: '本次返回', value: count(doc.counts.returned_records) },
      { label: '实体口径', value: spec.entityLabel }
    ];
  });

  function pick(record: ResearchRecord | ResearchSecurityRow, key: string) {
    return record.data[key];
  }

  function render(value: unknown, kind: FieldKind): string {
    if (kind === 'count') return count(value);
    if (kind === 'percent') return percent(value);
    if (kind === 'date') return date(value);
    return text(value);
  }

  function fieldColumn(field: Field): Column<ResearchRecord> {
    const numeric = field.kind === 'count' || field.kind === 'percent';
    return {
      key: field.key,
      label: field.label,
      width: field.width,
      align: numeric ? 'right' : undefined,
      num: numeric || field.kind === 'date',
      wrap: field.kind === 'text',
      value: (row) => render(pick(row, field.key), field.kind),
      tone: field.toned ? (row) => tone(num(pick(row, field.key))) : undefined,
      sortValue: numeric ? (row) => num(pick(row, field.key)) ?? 0 : undefined
    };
  }

  const columns = $derived.by<Column<ResearchRecord>[]>(() => [
    {
      key: 'entity',
      label: spec.entityLabel,
      width: spec.entity === 'institution' ? '180px' : '132px',
      wrap: spec.entity === 'institution',
      value: (row) => row.entity.name || row.entity.code || row.entity.id,
      sub: (row) => (row.entity.type === 'security' ? (row.entity.security_id ?? '') : row.entity.code)
    },
    {
      key: 'latest_date',
      label: '最新日期',
      width: '86px',
      num: true,
      value: (row) => date(row.latest_date),
      sortValue: (row) => row.latest_date ?? ''
    },
    ...spec.fields.map(fieldColumn)
  ]);

  function load(refresh = false) {
    void catalog.load(
      `/api/v1/market/research?${queryString({
        category,
        q: query.trim(),
        limit: 2000,
        include_text: 0,
        refresh: refresh ? 1 : 0
      })}`
    );
    detail.reset();
  }

  function switchGroup(next: string) {
    group = next;
    category = CATEGORIES.find((item) => item.group === next)?.id ?? category;
    query = '';
    load();
  }

  function switchCategory(next: string) {
    category = next;
    query = '';
    load();
  }

  /** 股票按 market/code 取动态键，行业与机构只能按 detail_id 取。 */
  function openRecord(record: ResearchRecord) {
    const selection: Record<string, string | number> = {
      category,
      include_details: 1,
      include_text: 1,
      detail_limit: record.entity.type === 'security' ? 100 : 300
    };
    if (record.entity.type === 'security' && record.entity.market) {
      selection.market = record.entity.market;
      selection.code = record.entity.code;
    } else {
      selection.detail_id = record.detail_id;
    }
    void detail.load(`/api/v1/market/research?${queryString(selection)}`);
  }

  function gotoWorkbench(entity: ResearchEntity) {
    if (!entity.market) return;
    app.setStock({ market: entity.market, code: entity.code, name: entity.name });
    router.go(stockPath(entity.market, entity.code));
  }

  const selectedEntity = $derived(detail.data?.selected_entity ?? null);
  const selectedSecurity = $derived(
    selectedEntity && selectedEntity.type === 'security' && selectedEntity.market
      ? selectedEntity
      : null
  );
  const activities = $derived(detail.data?.activities ?? []);
  const events = $derived(detail.data?.regulatory_events ?? []);
  const related = $derived(detail.data?.related_securities ?? []);
  const failures = $derived(detail.data?.detail_errors ?? []);

  // 空态不能吞掉 detail_errors——动态键取不到时那条提示就是唯一有效信息
  const asideEmpty = $derived(
    detail.busy
      ? false
      : !detail.loaded ||
        (activities.length === 0 &&
          events.length === 0 &&
          related.length === 0 &&
          failures.length === 0)
  );

  onMount(() => load());
</script>

<PageHeader
  eyebrow="TZZHD + CFJG + ZMJG · 709/1721"
  title="机构调研与市场活动"
  description="九张客户端主表统一关联到调研纪要、互动问答正文、监管公告，以及行业成分股与机构覆盖股票。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={catalog.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

<Split asideWidth="400px">
  {#snippet main()}
    <Panel
      flush
      scroll
      busy={catalog.busy}
      error={catalog.error}
      empty={catalog.loaded && !catalog.busy && rows.length === 0}
      emptyText="当前主表在该检索条件下没有记录，换个关键词或清空检索。"
      onRetry={() => load()}
      title={doc?.category_label || spec.label}
      subtitle={spec.hint}
    >
      {#snippet toolbar()}
        <Segmented options={GROUPS} value={group} onChange={switchGroup} ariaLabel="调研主题" />
        <Select
          value={category}
          options={categoryOptions}
          label="主表"
          width="220px"
          onChange={switchCategory}
        />
        <TextInput
          bind:value={query}
          icon="search"
          width="240px"
          label="检索当前主表"
          placeholder={spec.placeholder}
          onEnter={() => load()}
        />
      {/snippet}

      <DataTable
        {columns}
        {rows}
        stickyFirst
        rowKey={(row, index) => `${row.detail_id}-${index}`}
        onRowClick={openRecord}
        isActive={(row) => row.detail_id === detail.data?.selected?.detail_id}
      />
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="LINKED DETAIL"
      title={selectedEntity?.name || selectedEntity?.code || '关联详情'}
      subtitle={detail.data
        ? `${count(detail.data.counts.activities)} 条活动 · ${count(detail.data.counts.regulatory_events)} 条监管 · ${count(detail.data.counts.related_securities)} 只关联股票`
        : '点击左侧任意行展开'}
      busy={detail.busy}
      error={detail.error}
      empty={asideEmpty}
      emptyText={detail.loaded
        ? '该动态键没有返回可展开的关联内容。'
        : spec.entity === 'security'
          ? '点击左侧股票，按动态键读取调研纪要、互动问答与监管公告正文。'
          : spec.entity === 'industry'
            ? '点击左侧行业，下钻到该行业被调研的成分股。'
            : '点击左侧机构，下钻到该机构近期覆盖的股票。'}
    >
      {#if selectedSecurity}
        <button class="jump" type="button" onclick={() => gotoWorkbench(selectedSecurity)}>
          在个股工作台打开 {selectedSecurity.name || selectedSecurity.code}
        </button>
      {/if}

      {#if detail.data?.category_memberships.length}
        <div class="tags">
          {#each detail.data.category_memberships as item (item.category)}
            <Badge tone="focus">{item.category_label}</Badge>
          {/each}
        </div>
      {/if}

      {#if activities.length}
        <h3>调研纪要与互动问答</h3>
        <ul class="events">
          {#each activities as item, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(item.date)}</time>
                <span class="num mute">{count(item.text_length)} 字节</span>
              </div>
              <strong>{text(item.title)}</strong>
              {#if item.text}
                <details>
                  <summary>展开正文</summary>
                  <pre>{item.text}</pre>
                </details>
              {/if}
              {#if item.source_urls.length}
                <div class="links">
                  {#each item.source_urls as url (url)}
                    <a href={url} target="_blank" rel="noreferrer">
                      <Icon name="external" size={10} />原始附件
                    </a>
                  {/each}
                </div>
              {/if}
            </li>
          {/each}
        </ul>
      {/if}

      {#if events.length}
        <h3>监管事件</h3>
        <ul class="events">
          {#each events as item, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(item.date)}</time>
                <span class="down">{text(item.event_type)}</span>
              </div>
              <strong>{text(item.subject)}</strong>
              {#if item.summary}
                <details>
                  <summary>展开事件描述</summary>
                  <pre>{item.summary}</pre>
                </details>
              {/if}
              <small>{text(item.progress)}</small>
              {#if item.source_urls.length}
                <div class="links">
                  {#each item.source_urls as url (url)}
                    <a href={url} target="_blank" rel="noreferrer">
                      <Icon name="external" size={10} />原始文件
                    </a>
                  {/each}
                </div>
              {/if}
            </li>
          {/each}
        </ul>
      {/if}

      {#if related.length}
        <h3>{spec.entity === 'institution' ? '机构覆盖股票' : '行业被调研成分股'}</h3>
        <ul class="events">
          {#each related as item, index (index)}
            <li>
              <div class="row">
                <time class="num">{date(item.latest_date)}</time>
                <span class={tone(num(pick(item, 'return_pct_1m')))}>
                  {percent(pick(item, 'return_pct_1m'))}
                </span>
              </div>
              <button class="link-row" type="button" onclick={() => gotoWorkbench(item.security)}>
                {item.security.name || item.security.code}
                <Icon name="chevron-right" size={10} />
              </button>
              <p class="num">
                调研 {count(pick(item, 'research_count_1m'))} / {count(pick(item, 'research_count_3m'))} /
                {count(pick(item, 'research_count_6m'))} 次（一月 / 三月 / 半年）
              </p>
              <small>
                {item.security.security_id ?? item.security.id} · 近一月机构
                {count(pick(item, 'institution_count_1m'))}
              </small>
            </li>
          {/each}
        </ul>
      {/if}

      {#each failures as failure, index (index)}
        <p class="warn-line">{failure.resource ?? '详情'}：{failure.message}</p>
      {/each}
    </Panel>
  {/snippet}
</Split>

<style>
  .jump {
    display: block;
    width: 100%;
    height: 22px;
    margin-bottom: var(--sp-3);
    font-size: var(--fs-micro);
    color: var(--focus);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  .jump:hover {
    background: var(--bg-hover);
  }

  .tags {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-1);
    margin-bottom: var(--sp-3);
  }

  h3 {
    margin: var(--sp-4) 0 var(--sp-2);
    font-size: var(--fs-micro);
    font-weight: 600;
    color: var(--fg-dim);
  }

  h3:first-child {
    margin-top: 0;
  }

  .events {
    display: flex;
    flex-direction: column;
    gap: var(--sp-2);
    margin: 0;
    padding: 0;
    list-style: none;
  }

  .events li {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: var(--sp-2) var(--sp-3);
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .row {
    display: flex;
    align-items: baseline;
    justify-content: space-between;
    gap: var(--sp-3);
    font-size: var(--fs-micro);
  }

  time {
    color: var(--fg-mute);
  }

  .events strong {
    font-size: var(--fs-micro);
    font-weight: 500;
    line-height: var(--lh-tight);
  }

  .events p {
    font-size: var(--fs-micro);
    color: var(--fg-dim);
  }

  .events small {
    font-size: 10px;
    line-height: 1.4;
    color: var(--fg-mute);
  }

  details {
    margin-top: var(--sp-1);
  }

  summary {
    font-size: 10px;
    color: var(--focus);
    cursor: pointer;
  }

  pre {
    max-height: 260px;
    margin: var(--sp-2) 0 0;
    padding: var(--sp-2);
    overflow: auto;
    font-family: var(--font-ui);
    font-size: 10px;
    line-height: 1.6;
    color: var(--fg-dim);
    white-space: pre-wrap;
    word-break: break-word;
    background: var(--bg-panel);
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .links {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-2);
    margin-top: var(--sp-1);
  }

  .links a {
    display: inline-flex;
    align-items: center;
    gap: 3px;
    font-size: 10px;
  }

  .link-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: var(--sp-2);
    width: 100%;
    padding: 0;
    font-size: var(--fs-micro);
    font-weight: 500;
    color: var(--focus);
    text-align: left;
  }

  .link-row:hover {
    text-decoration: underline;
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
