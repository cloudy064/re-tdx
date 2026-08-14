<script lang="ts">
  /** 通达信静态情报链：关注点击、风险模型与事件—股票成员关系。 */
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, delta, fixed, percent, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import { app } from '../../lib/store.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import type { Stat } from '../../ui/StatGrid.svelte';
  import type {
    AttentionRecord,
    IntelligenceEventRecord,
    IntelligenceHighlightRecord,
    IntelligenceMarketAnomalyRecord,
    IntelligenceRiskRecord,
    IntelligenceTimelineRecord,
    IntelligenceTopicRecord,
    IntelligenceValueAttentionCategory,
    IntelligenceValueAttentionRecord,
    MarketIntelligenceDocument,
    ValuationSecurity
  } from '../../types';

  const VIEWS = [
    { id: 'attention', label: '市场关注' },
    { id: 'value-attention', label: '价值关注' },
    { id: 'highlights', label: '安全亮点' },
    { id: 'risks', label: '风险清单' },
    { id: 'events', label: '事件列表' },
    { id: 'graph', label: '事件关系' },
    { id: 'topics', label: '专题追踪' },
    { id: 'news', label: '新闻联播' },
    { id: 'market-anomalies', label: '大盘异动复盘' }
  ];
  const RISK_CATEGORIES = [
    { id: 'all', label: '全部风险' },
    { id: 'observation', label: '风险观察' },
    { id: 'potential', label: '潜在爆雷' },
    { id: 'discredited', label: '失信被执行' }
  ];
  const EVENT_CATEGORIES = [
    { id: 'all', label: '全部事件' },
    { id: 'events', label: '事件驱动' },
    { id: 'ministries', label: '部委要闻' },
    { id: 'hotspots', label: '热点映射' }
  ];

  let view = $state('attention');
  let category = $state('all');
  let valueCategoryId = $state('');
  const resource = new Resource<MarketIntelligenceDocument>();
  const detail = new Resource<MarketIntelligenceDocument>();
  const doc = $derived(resource.data);
  const attentionRows = $derived((view === 'attention' ? doc?.records ?? [] : []) as AttentionRecord[]);
  const valueCategoryRows = $derived((view === 'value-attention' ? doc?.value_attention ?? [] : []) as IntelligenceValueAttentionCategory[]);
  const valueDetailRows = $derived((view === 'value-attention' && valueCategoryId ? doc?.records ?? [] : []) as IntelligenceValueAttentionRecord[]);
  const riskRows = $derived((view === 'risks' ? doc?.records ?? [] : []) as IntelligenceRiskRecord[]);
  const highlightRows = $derived((view === 'highlights' ? doc?.records ?? [] : []) as IntelligenceHighlightRecord[]);
  const eventRows = $derived((view === 'events' ? doc?.records ?? doc?.events ?? [] : doc?.events ?? []) as IntelligenceEventRecord[]);
  const topicRows = $derived((view === 'topics' ? doc?.records ?? [] : []) as IntelligenceTopicRecord[]);
  const newsRows = $derived((view === 'news' ? doc?.records ?? [] : []) as IntelligenceTimelineRecord[]);
  const anomalyRows = $derived((view === 'market-anomalies' ? doc?.records ?? [] : []) as IntelligenceMarketAnomalyRecord[]);
  const timelineRows = $derived((detail.data?.view === 'topic' ? detail.data.records ?? [] : []) as IntelligenceTimelineRecord[]);
  const selectedTopic = $derived(detail.data?.selected_topic ?? null);
  const selectedEvent = $derived((detail.data?.view === 'event' ? detail.data.records?.[0] : null) as IntelligenceEventRecord | null);
  const categories = $derived(view === 'risks' ? RISK_CATEGORIES : EVENT_CATEGORIES);
  const valueCategoryOptions = $derived([
    { id: '', label: '全部价值关注' },
    ...valueCategoryRows.map((item) => ({ id: item.category_id, label: item.category_name }))
  ]);

  const stats = $derived.by<Stat[]>(() => {
    if (!doc) return [];
    if (view === 'attention') {
      const first = attentionRows[0];
      return [
        { label: '覆盖证券', value: count(attentionRows.length), note: `缓存 ${count(doc.cache.age_seconds)}s` },
        { label: '统计日', value: date(first?.date) },
        { label: '榜首', value: first?.security.name || '—', note: first?.security.security_id || '' },
        { label: '榜首关注', value: compact(first?.attention_total, '次') },
        { label: '专业关注', value: compact(first?.professional_attention_total, '次') },
        { label: '看多占比', value: percent(first?.bullish_ratio_pct) }
      ];
    }
    if (view === 'value-attention') {
      const selected = doc.selected_value_attention;
      return [
        { label: '价值口径', value: count(doc.value_attention_summary?.categories) },
        { label: '股票关系', value: count(doc.value_attention_summary?.relationships) },
        { label: '去重股票', value: count(doc.value_attention_summary?.unique_securities) },
        { label: '当前口径', value: selected?.category_name || '全部' },
        { label: '主表成员', value: count(selected?.inline_member_count) },
        { label: '明细成员', value: count(selected?.dynamic_member_count), note: doc.value_attention_reconciliation?.exact_match === false ? '与主表存在差异' : '' }
      ];
    }
    if (view === 'risks') return [
      { label: '风险记录', value: count(riskRows.length), note: `缓存 ${count(doc.cache.age_seconds)}s` },
      { label: '风险观察', value: count(riskRows.filter((row) => row.category === 'observation').length) },
      { label: '潜在爆雷', value: count(riskRows.filter((row) => row.category === 'potential').length) },
      { label: '失信被执行', value: count(riskRows.filter((row) => row.category === 'discredited').length) },
      { label: '模型最低安全分', value: riskRows.some((row) => row.safety_score !== null) ? fixed(Math.min(...riskRows.flatMap((row) => row.safety_score === null ? [] : [row.safety_score])), 0) : '—' }
    ];
    if (view === 'highlights') return [
      { label: '亮点股票', value: count(doc.highlight_summary?.securities ?? highlightRows.length), note: `缓存 ${count(doc.cache.age_seconds)}s` },
      { label: '亮点类型', value: count(doc.highlight_summary?.type_count) },
      { label: '平均安全分', value: fixed(doc.highlight_summary?.average_safety_score, 1) },
      { label: '最高亮点数', value: count(highlightRows[0]?.highlight_count) },
      { label: '榜首', value: highlightRows[0]?.security.name || '—', note: highlightRows[0]?.security.security_id || '' }
    ];
    if (view === 'topics') return [
      { label: '跟踪专题', value: count(topicRows.length), note: `缓存 ${count(doc.cache.age_seconds)}s` },
      { label: '最近更新', value: date(topicRows[0]?.updated_date) },
      { label: '已展开文章', value: count(timelineRows.length) },
      { label: '当前专题', value: selectedTopic?.name || '—' }
    ];
    if (view === 'news') return [
      { label: '历史期数', value: count(newsRows.length), note: `缓存 ${count(doc.cache.age_seconds)}s` },
      { label: '最新日期', value: date(newsRows[0]?.date) },
      { label: '最新标题', value: newsRows[0]?.headline || '—' }
    ];
    if (view === 'market-anomalies') return [
      { label: '复盘记录', value: count(anomalyRows.length), note: `缓存 ${count(doc.cache.age_seconds)}s` },
      { label: '最近日期', value: date(anomalyRows[0]?.date) },
      { label: '最近异动', value: anomalyRows[0]?.anomaly_type || '—' },
      { label: '当日涨跌', value: percent(anomalyRows[0]?.same_day_change_pct, 2, true), tone: tone(anomalyRows[0]?.same_day_change_pct) }
    ];
    return [
      { label: '事件', value: count(eventRows.length), note: `缓存 ${count(doc.cache.age_seconds)}s` },
      { label: '关联证券', value: count(doc.graph?.counts.securities ?? eventRows.reduce((sum, row) => sum + row.member_count, 0)) },
      { label: '关系边', value: count(doc.graph?.counts.returned_relationships) },
      { label: '省略关系', value: count(doc.graph?.counts.omitted_relationships), tone: doc.graph?.truncated ? 'down' : '' }
    ];
  });

  const attentionColumns: Column<AttentionRecord>[] = [
    { key: 'rank', label: '排名', width: '66px', align: 'right', num: true, value: (row) => count(row.rank), sortValue: (row) => row.rank ?? 999999 },
    { key: 'security', label: '股票', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'change', label: '排名变化', align: 'right', num: true, value: (row) => delta(row.rank_change, 0), tone: (row) => tone(row.rank_change) },
    { key: 'attention', label: '关注点击', align: 'right', num: true, value: (row) => compact(row.attention_total, '次'), sub: (row) => `盘中 ${compact(row.attention_intraday, '')}`, sortValue: (row) => row.attention_total ?? 0 },
    { key: 'professional', label: 'L2 用户关注', align: 'right', num: true, value: (row) => compact(row.professional_attention_total, '次'), sortValue: (row) => row.professional_attention_total ?? 0 },
    { key: 'resonance', label: '共鸣浏览', align: 'right', num: true, value: (row) => compact(row.resonance_total, '次'), sub: (row) => `变化 ${compact(row.resonance_change, '')}`, sortValue: (row) => row.resonance_total ?? 0 },
    { key: 'sentiment', label: '舆情热度', align: 'right', num: true, value: (row) => fixed(row.sentiment_heat, 2), sortValue: (row) => row.sentiment_heat ?? 0 },
    { key: 'bullish', label: '月度看多', align: 'right', num: true, value: (row) => percent(row.bullish_ratio_pct), sub: (row) => `${count(row.bullish_votes_month)} / ${count(row.bearish_votes_month)}`, sortValue: (row) => row.bullish_ratio_pct ?? 0 }
  ];
  const valueCategoryColumns: Column<IntelligenceValueAttentionCategory>[] = [
    { key: 'name', label: '价值关注口径', width: '260px', value: (row) => row.category_name, sub: (row) => `ID ${row.category_id}` },
    { key: 'reported', label: '上报成员', align: 'right', num: true, value: (row) => count(row.reported_member_count), sortValue: (row) => row.reported_member_count ?? 0 },
    { key: 'inline', label: '主表实得', align: 'right', num: true, value: (row) => count(row.inline_member_count), sortValue: (row) => row.inline_member_count },
    { key: 'source', label: '成员来源', value: (row) => row.member_source === 'inline-master+dynamic-detail' ? '主表 + 动态明细' : '主表内嵌' },
    { key: 'resource', label: '资源', value: (row) => row.source_resource }
  ];
  const valueDetailColumns: Column<IntelligenceValueAttentionRecord>[] = [
    { key: 'security', label: '股票', width: '150px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'anchor', label: '相关价格', align: 'right', num: true, value: (row) => fixed(row.anchor_price_yuan, 3), sortValue: (row) => row.anchor_price_yuan ?? 0 },
    { key: 'adjusted-anchor', label: '复权相关价格', align: 'right', num: true, value: (row) => fixed(row.adjusted_anchor_price_yuan, 3), sortValue: (row) => row.adjusted_anchor_price_yuan ?? 0 },
    { key: 'close', label: '近三月复权收盘价', align: 'right', num: true, value: (row) => fixed(row.three_month_adjusted_close_yuan, 3), sortValue: (row) => row.three_month_adjusted_close_yuan ?? 0 },
    { key: 'breach', label: '跌破幅度', align: 'right', num: true, value: (row) => row.breach_depth_pct === null ? '需实时价' : percent(row.breach_depth_pct) },
    { key: 'resource', label: '明细资源', value: (row) => row.source_resource }
  ];
  const riskColumns: Column<IntelligenceRiskRecord>[] = [
    { key: 'security', label: '股票', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'category', label: '模型', width: '82px', value: (row) => row.category_label },
    { key: 'date', label: '公告日期', width: '92px', value: (row) => row.category === 'discredited' ? date(row.announcement_date) : '—', sortValue: (row) => row.announcement_date },
    { key: 'type', label: '预警 / 对象类型', width: '180px', value: (row) => text(row.risk_type), wrap: true },
    { key: 'detail', label: '预警详情 / 涉及对象', value: (row) => text(row.detail), wrap: true },
    { key: 'occurrences', label: '近一年次数', width: '82px', align: 'right', num: true, value: (row) => row.category === 'discredited' ? count(row.occurrences_past_year) : '—', sortValue: (row) => row.occurrences_past_year ?? 0 },
    { key: 'score', label: '安全分', width: '78px', align: 'right', num: true, value: (row) => fixed(row.safety_score, 0), tone: (row) => row.safety_score !== null && row.safety_score < 60 ? 'down' : '', sortValue: (row) => row.safety_score ?? 100 }
  ];
  const highlightColumns: Column<IntelligenceHighlightRecord>[] = [
    { key: 'security', label: '股票', width: '130px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'count', label: '亮点数', width: '72px', align: 'right', num: true, value: (row) => count(row.highlight_count), sortValue: (row) => row.highlight_count ?? 0 },
    { key: 'type', label: '主亮点', width: '150px', value: (row) => text(row.primary_highlight_type) },
    { key: 'detail', label: '亮点详情', value: (row) => text(row.highlight_detail), wrap: true },
    { key: 'safety', label: '安全分', width: '72px', align: 'right', num: true, value: (row) => fixed(row.safety_score, 0), sortValue: (row) => row.safety_score ?? 0 },
    { key: 'score', label: '亮点分', width: '72px', align: 'right', num: true, value: (row) => fixed(row.highlight_score, 0), sortValue: (row) => row.highlight_score ?? 0 }
  ];
  const eventColumns: Column<IntelligenceEventRecord>[] = [
    { key: 'date', label: '日期', width: '92px', value: (row) => date(row.date) },
    { key: 'source', label: '来源', width: '82px', value: (row) => row.source_label, sub: (row) => row.organization },
    { key: 'title', label: '事件', width: '30%', value: (row) => text(row.title), sub: (row) => row.type, wrap: true },
    { key: 'content', label: '背景 / 驱动因素', value: (row) => text(row.content), wrap: true },
    { key: 'members', label: '关联股', width: '76px', align: 'right', num: true, value: (row) => count(row.member_count), sortValue: (row) => row.member_count },
    { key: 'importance', label: '重要度', width: '66px', align: 'right', num: true, value: (row) => fixed(row.importance, 0) }
  ];
  const topicColumns: Column<IntelligenceTopicRecord>[] = [
    { key: 'updated', label: '更新日', width: '92px', value: (row) => date(row.updated_date) },
    { key: 'name', label: '专题', width: '220px', value: (row) => text(row.name), sub: (row) => `ID ${row.topic_id}` },
    { key: 'category', label: '类型', width: '96px', value: (row) => text(row.category) },
    { key: 'description', label: '专题说明', value: (row) => text(row.description), wrap: true },
    { key: 'created', label: '创建日', width: '92px', value: (row) => date(row.created_date) }
  ];
  const newsColumns: Column<IntelligenceTimelineRecord>[] = [
    { key: 'date', label: '日期', width: '96px', value: (row) => date(row.date) },
    { key: 'headline', label: '标题', width: '280px', value: (row) => text(row.headline), wrap: true },
    { key: 'content', label: '内容摘要', value: (row) => text(row.content), wrap: true },
    { key: 'source', label: '原文', width: '70px', value: (row) => row.source_url ? '可访问' : '—', slot: true }
  ];
  const anomalyColumns: Column<IntelligenceMarketAnomalyRecord>[] = [
    { key: 'date', label: '日期', width: '92px', value: (row) => date(row.date) },
    { key: 'type', label: '异动', width: '100px', value: (row) => text(row.anomaly_type) },
    { key: 'reason', label: '盘面原因', value: (row) => text(row.reason), wrap: true },
    { key: 'same-day', label: '当日', align: 'right', num: true, value: (row) => percent(row.same_day_change_pct, 2, true), tone: (row) => tone(row.same_day_change_pct) },
    { key: 'next-day', label: '次日', align: 'right', num: true, value: (row) => percent(row.next_day_change_pct, 2, true), tone: (row) => tone(row.next_day_change_pct) },
    { key: 'next-week', label: '后一周', align: 'right', num: true, value: (row) => percent(row.next_week_change_pct, 2, true), tone: (row) => tone(row.next_week_change_pct) },
    { key: 'limits', label: '涨/跌停', align: 'right', num: true, value: (row) => `${count(row.limit_up_count)} / ${count(row.limit_down_count)}` },
    { key: 'turnover', label: '两市成交', align: 'right', num: true, value: (row) => compact(row.market_turnover_yuan, '元') }
  ];

  function load(refresh = false) {
    void resource.load(`/api/v1/market/intelligence?${queryString({
      view,
      category: view === 'attention' || view === 'highlights' ? 'all' : category,
      category_id: view === 'value-attention' ? valueCategoryId : '',
      sort: view === 'highlights' ? 'highlight-count' : '',
      limit: view === 'attention' || view === 'value-attention' || view === 'risks' || view === 'highlights' ? 3000 : view === 'graph' ? 50 : 300,
      member_limit: 2000,
      refresh: refresh ? 1 : 0
    })}`);
  }
  function changeView(next: string) {
    view = next;
    category = 'all';
    valueCategoryId = '';
    detail.reset();
    load();
  }
  function openTopic(row: IntelligenceTopicRecord) {
    void detail.load(`/api/v1/market/intelligence?${queryString({ view: 'topic', topic_id: row.topic_id, limit: 1000 })}`);
  }
  function openValueCategory(row: IntelligenceValueAttentionCategory) {
    valueCategoryId = row.category_id;
    load();
  }
  function openEvent(row: IntelligenceEventRecord) {
    if (row.source !== 'events' || !row.raw_id) return;
    void detail.load(`/api/v1/market/intelligence?${queryString({ view: 'event', event_id: row.raw_id, limit: 1000 })}`);
  }
  function openSecurity(security: ValuationSecurity) {
    app.setStock({ market: security.market, code: security.code, name: security.name });
    router.go(stockPath(security.market, security.code, 'intelligence'));
  }
  onMount(() => load());
</script>

<PageHeader eyebrow="709/1721 · SCRD / JZGZ / LDPH / SXBZX / SJQD / ZTXX / XWLB / DPYD" title="市场情报与专题复盘" description="通达信关注、价值关注、亮点、风险模型、失信被执行对象、事件驱动、专题新闻链和大盘异动复盘的原生聚合；专题正文只展示清洗后的纯文本。" {stats}>
  {#snippet actions()}<Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>{/snippet}
</PageHeader>

<Panel flush={view !== 'graph' && view !== 'topics'} scroll fill title={VIEWS.find((item) => item.id === view)?.label ?? '市场情报'} busy={resource.busy} error={resource.error} empty={resource.loaded && (view === 'attention' ? attentionRows.length === 0 : view === 'value-attention' ? (valueCategoryId ? valueDetailRows.length === 0 : valueCategoryRows.length === 0) : view === 'highlights' ? highlightRows.length === 0 : view === 'risks' ? riskRows.length === 0 : view === 'topics' ? topicRows.length === 0 : view === 'news' ? newsRows.length === 0 : view === 'market-anomalies' ? anomalyRows.length === 0 : eventRows.length === 0)} emptyText="当前口径没有记录。" onRetry={() => load()}>
  {#snippet toolbar()}
    <Select options={VIEWS} value={view} width="150px" label="视图" onChange={changeView} />
    {#if view === 'risks' || view === 'events' || view === 'graph'}
      <Select options={categories} value={category} width="150px" label="口径" onChange={(next) => { category = next; load(); }} />
    {:else if view === 'value-attention'}
      <Select options={valueCategoryOptions} value={valueCategoryId} width="220px" label="价值口径" onChange={(next) => { valueCategoryId = next; load(); }} />
    {/if}
  {/snippet}
  {#if view === 'attention'}
    <DataTable columns={attentionColumns} rows={attentionRows} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} stickyFirst numbered />
  {:else if view === 'value-attention'}
    {#if valueCategoryId}
      <div class="detail-heading value-heading">
        <span>{doc?.selected_value_attention?.category_name ?? valueCategoryId}</span>
        <Badge tone={doc?.value_attention_reconciliation?.exact_match ? 'up' : 'warn'}>{doc?.value_attention_reconciliation?.exact_match ? '主表与明细一致' : '保留主表/明细差异'}</Badge>
      </div>
      <DataTable columns={valueDetailColumns} rows={valueDetailRows} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} stickyFirst numbered />
    {:else}
      <DataTable columns={valueCategoryColumns} rows={valueCategoryRows} rowKey={(row) => row.category_id} onRowClick={openValueCategory} numbered />
    {/if}
  {:else if view === 'highlights'}
    <DataTable columns={highlightColumns} rows={highlightRows} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} stickyFirst numbered />
  {:else if view === 'risks'}
    <DataTable columns={riskColumns} rows={riskRows} rowKey={(row, index) => `${row.category}-${row.security.security_id}-${row.risk_type}-${index}`} onRowClick={(row) => openSecurity(row.security)} stickyFirst numbered />
  {:else if view === 'events'}
    <DataTable columns={eventColumns} rows={eventRows} rowKey={(row) => row.event_id} onRowClick={openEvent} numbered />
    {#if selectedEvent}
      <section class="detail-stack">
        <div class="detail-heading">
          <div><span class="num">{date(selectedEvent.date)}</span><h3>{selectedEvent.title}</h3></div>
          <Badge tone={detail.data?.event_reconciliation?.exact_match ? 'up' : 'warn'}>{detail.data?.event_reconciliation?.exact_match ? '成员已核对' : '成员有差异'}</Badge>
        </div>
        <p class="content">{selectedEvent.content}</p>
        <div class="chips">
          {#each selectedEvent.members ?? [] as security (security.security_id)}
            <button onclick={() => openSecurity(security)}>{security.name || security.code}</button>
          {/each}
        </div>
      </section>
    {/if}
  {:else if view === 'topics'}
    <DataTable columns={topicColumns} rows={topicRows} rowKey={(row) => row.topic_id} onRowClick={openTopic} numbered />
    {#if selectedTopic}
      <section class="detail-stack">
        <div class="detail-heading">
          <div><span>{selectedTopic.category} · 更新于 {date(selectedTopic.updated_date)}</span><h3>{selectedTopic.name}</h3></div>
          <Badge tone="focus">{count(timelineRows.length)} 篇</Badge>
        </div>
        {#each timelineRows as item, index (`${item.date}-${item.headline}-${index}`)}
          <article class="timeline-item">
            <div class="detail-heading"><span class="num">{date(item.date)}</span>{#if item.source_url}<a href={item.source_url} target="_blank" rel="noreferrer">查看原文</a>{/if}</div>
            <h3>{item.headline}</h3>
            <p class="content">{item.content}</p>
          </article>
        {/each}
      </section>
    {/if}
  {:else if view === 'news'}
    <DataTable columns={newsColumns} rows={newsRows} rowKey={(row, index) => `${row.date}-${index}`} numbered>
      {#snippet cell({ row, column })}
        {#if column.key === 'source' && row.source_url}<a href={row.source_url} target="_blank" rel="noreferrer">查看</a>{:else}—{/if}
      {/snippet}
    </DataTable>
  {:else if view === 'market-anomalies'}
    <DataTable columns={anomalyColumns} rows={anomalyRows} rowKey={(row, index) => `${row.date}-${row.anomaly_type}-${index}`} numbered />
  {:else}
    <div class="relations">
      {#each eventRows as event (event.event_id)}
        <article class="event-card">
          <div class="event-main">
            <div class="meta"><Badge tone={event.type === '利好' ? 'up' : 'focus'}>{event.source_label}</Badge><span class="num">{date(event.date)}</span><span>{text(event.organization)}</span></div>
            <h3>{event.title}</h3>
            <p>{event.content}</p>
          </div>
          <div class="members">
            <strong>{count(event.member_count)} 只关联股</strong>
            <div class="chips">
              {#each (event.members ?? []).slice(0, 16) as security (security.security_id)}
                <button onclick={() => openSecurity(security)}>{security.name || security.code}</button>
              {/each}
              {#if (event.members?.length ?? 0) > 16}<span>+{count((event.members?.length ?? 0) - 16)}</span>{/if}
            </div>
          </div>
        </article>
      {/each}
      {#if doc?.graph?.truncated}<p class="notice">关系图为控制响应体积省略了 {count(doc.graph.counts.omitted_relationships)} 条边，可按事件口径缩小范围。</p>{/if}
    </div>
  {/if}
</Panel>

<style>
  .relations { display: grid; gap: var(--sp-2); padding: var(--sp-3); }
  .event-card { display: grid; grid-template-columns: minmax(0, 1.5fr) minmax(220px, .8fr); gap: var(--sp-3); padding: var(--sp-3); border: 1px solid var(--line); border-radius: var(--radius); background: var(--surface-2); }
  .event-main, .members { min-width: 0; }
  .meta { display: flex; align-items: center; gap: var(--sp-2); color: var(--fg-mute); font-size: var(--fs-micro); }
  h3 { margin: var(--sp-1) 0; font-size: var(--fs-body); font-weight: 600; }
  p { margin: 0; color: var(--fg-dim); font-size: var(--fs-small); line-height: 1.55; }
  .members strong { display: block; margin-bottom: var(--sp-2); color: var(--fg-dim); font-size: var(--fs-micro); }
  .chips { display: flex; flex-wrap: wrap; gap: 4px; }
  .chips button, .chips span { min-height: 22px; padding: 2px 7px; border: 1px solid var(--line); border-radius: var(--radius); color: var(--fg-dim); background: var(--surface-1); font-size: var(--fs-micro); }
  .chips button { cursor: pointer; }
  .chips button:hover { color: var(--focus); border-color: var(--focus); }
  .notice { padding: var(--sp-2); color: var(--warn); }
  .detail-stack { display: grid; gap: var(--sp-3); margin: var(--sp-3); padding: var(--sp-3); border: 1px solid var(--line-strong); border-radius: var(--radius); background: var(--surface-2); }
  .detail-heading { display: flex; align-items: flex-start; justify-content: space-between; gap: var(--sp-3); color: var(--fg-mute); font-size: var(--fs-micro); }
  .detail-heading h3 { color: var(--fg); }
  .timeline-item { padding-top: var(--sp-3); border-top: 1px solid var(--line); }
  .timeline-item:first-of-type { padding-top: 0; border-top: 0; }
  .content { white-space: pre-line; }
  .value-heading { padding: var(--sp-3); border-bottom: 1px solid var(--line); }
  a { color: var(--focus); }
  @media (max-width: 800px) { .event-card { grid-template-columns: 1fr; } }
</style>
