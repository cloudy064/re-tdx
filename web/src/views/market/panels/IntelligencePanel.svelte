<script lang="ts">
  /** 单票关注、风险与事件焦点子图；服务端已避免返回同事件的全部其它成员。 */
  import { queryString } from '../../../api';
  import { compact, count, date, delta, fixed, percent, text, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import StatGrid, { type Stat } from '../../../ui/StatGrid.svelte';
  import type { ExchangeSupervisionRecord, IntelligenceEventRecord, IntelligenceHighlightRecord, IntelligenceRiskRecord, IntelligenceValueAttentionCategory, MarketExchangeSupervisionDocument, MarketIntelligenceDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';

  const { market, code }: PanelProps = $props();
  const resource = new Resource<MarketIntelligenceDocument>();
  const supervisionResource = new Resource<MarketExchangeSupervisionDocument>();
  const attention = $derived(resource.data?.attention ?? null);
  const valueAttention = $derived(resource.data?.value_attention ?? []);
  const risks = $derived(resource.data?.risks ?? []);
  const highlights = $derived(resource.data?.highlights ?? []);
  const events = $derived(resource.data?.events ?? []);
  const graph = $derived(resource.data?.graph ?? null);
  const supervision = $derived(supervisionResource.data?.records ?? []);

  const stats = $derived.by<Stat[]>(() => attention ? [
    { label: '关注排名', value: count(attention.rank), note: `变化 ${delta(attention.rank_change, 0)}`, tone: tone(attention.rank_change) },
    { label: '关注点击', value: compact(attention.attention_total, '次'), note: `盘中 ${compact(attention.attention_intraday, '')}` },
    { label: 'L2 用户关注', value: compact(attention.professional_attention_total, '次') },
    { label: '共鸣浏览', value: compact(attention.resonance_total, '次'), note: `变化 ${compact(attention.resonance_change, '')}` },
    { label: '月度看多', value: percent(attention.bullish_ratio_pct), note: `${count(attention.bullish_votes_month)} 多 / ${count(attention.bearish_votes_month)} 空` },
    { label: '舆情热度', value: fixed(attention.sentiment_heat, 2), note: date(attention.date) }
  ] : []);
  const riskColumns: Column<IntelligenceRiskRecord>[] = [
    { key: 'category', label: '模型', width: '84px', value: (row) => row.category_label },
    { key: 'type', label: '预警类型', width: '190px', value: (row) => text(row.risk_type), wrap: true },
    { key: 'detail', label: '详情', value: (row) => text(row.detail), wrap: true },
    { key: 'score', label: '安全分', width: '70px', align: 'right', num: true, value: (row) => fixed(row.safety_score, 0), tone: (row) => row.safety_score !== null && row.safety_score < 60 ? 'down' : '' }
  ];
  const valueAttentionColumns: Column<IntelligenceValueAttentionCategory>[] = [
    { key: 'name', label: '价值关注口径', value: (row) => row.category_name, sub: (row) => `ID ${row.category_id}` },
    { key: 'reported', label: '上报成员', width: '86px', align: 'right', num: true, value: (row) => count(row.reported_member_count) },
    { key: 'inline', label: '主表实得', width: '86px', align: 'right', num: true, value: (row) => count(row.inline_member_count) }
  ];
  const highlightColumns: Column<IntelligenceHighlightRecord>[] = [
    { key: 'count', label: '亮点数', width: '70px', align: 'right', num: true, value: (row) => count(row.highlight_count) },
    { key: 'type', label: '主亮点', width: '150px', value: (row) => text(row.primary_highlight_type) },
    { key: 'detail', label: '亮点详情', value: (row) => text(row.highlight_detail), wrap: true },
    { key: 'safety', label: '安全分', width: '70px', align: 'right', num: true, value: (row) => fixed(row.safety_score, 0) },
    { key: 'score', label: '亮点分', width: '70px', align: 'right', num: true, value: (row) => fixed(row.highlight_score, 0) }
  ];
  const eventColumns: Column<IntelligenceEventRecord>[] = [
    { key: 'date', label: '日期', width: '92px', value: (row) => date(row.date) },
    { key: 'source', label: '来源', width: '82px', value: (row) => row.source_label, sub: (row) => row.organization },
    { key: 'title', label: '事件', width: '32%', value: (row) => text(row.title), sub: (row) => row.type, wrap: true },
    { key: 'content', label: '背景 / 驱动', value: (row) => text(row.content), wrap: true },
    { key: 'members', label: '全事件关联股', width: '88px', align: 'right', num: true, value: (row) => count(row.member_count) }
  ];
  const supervisionColumns: Column<ExchangeSupervisionRecord>[] = [
    { key: 'kind', label: '状态', width: '76px', value: (row) => row.record_kind === 'current' ? '当前监管' : '历史记录' },
    { key: 'start', label: '开始', width: '90px', num: true, value: (row) => date(row.start_date) },
    { key: 'end', label: '结束', width: '90px', num: true, value: (row) => date(row.end_date) },
    { key: 'prices', label: '起始 → 当前/结束', width: '145px', align: 'right', num: true, value: (row) => `${fixed(row.start_price, 3)} → ${fixed(row.record_kind === 'current' ? row.last_price : row.end_price, 3)}` },
    { key: 'return', label: '区间表现', width: '84px', align: 'right', num: true, value: (row) => percent(row.record_kind === 'current' ? row.since_start_return_pct : row.period_return_pct), tone: (row) => tone(row.record_kind === 'current' ? row.since_start_return_pct : row.period_return_pct) },
    { key: 'pdf', label: '公告', value: (row) => row.announcement_url ? '有异动公告 PDF' : '未附公告链接' }
  ];

  function load(refresh = false) {
    void resource.load(`/api/v1/market/intelligence?${queryString({
      view: 'security', market, code, limit: 200, member_limit: 1, refresh: refresh ? 1 : 0
    })}`);
    void supervisionResource.load(`/api/v1/market/exchange-supervision?${queryString({
      view: 'security', market, code, limit: 100, quote_cache_ttl_seconds: 5,
      refresh: refresh ? 1 : 0
    })}`);
  }
  $effect(() => { void market; void code; load(); });
</script>

<Panel title="市场关注" eyebrow="SCRD · 点击 / 浏览 / 眼光数据" subtitle="关注度是客户端点击次数，共鸣度是浏览次数；L2 用户关注仅是统计口径，不表示本机拥有 L2 权限。" busy={resource.busy} error={resource.error} onRetry={load} empty={resource.loaded && !attention} emptyText="该证券不在当前关注度全市场表中。">
  <StatGrid {stats} columns={3} />
</Panel>

<Panel title="价值关注" eyebrow="JZGZ · 跌破相关价格 / 价值挖掘" subtitle="按通达信价值关注主表反查该证券命中的口径；全市场页可继续展开动态明细及主表对账。" empty={resource.loaded && valueAttention.length === 0} emptyText="该证券当前没有命中价值关注口径。" scroll>
  <DataTable columns={valueAttentionColumns} rows={valueAttention} rowKey={(row) => row.category_id} />
</Panel>

<Panel title="安全亮点" eyebrow="LDPH · 正向质量榜" subtitle="亮点数、安全分和亮点分均为通达信服务端静态榜字段；详情保留客户端原文。" empty={resource.loaded && highlights.length === 0} emptyText="该证券当前不在安全亮点前 300 名中。" scroll>
  <DataTable columns={highlightColumns} rows={highlights} rowKey={(row) => row.security.security_id} />
</Panel>

<Panel title="风险模型" eyebrow="BXGC + QZBL" subtitle="风险观察与潜在爆雷是通达信服务端模型结果；安全分越低，提示越强。" empty={resource.loaded && risks.length === 0} emptyText="该证券当前没有命中风险观察或潜在爆雷清单。" scroll>
  <DataTable columns={riskColumns} rows={risks} rowKey={(row, index) => `${row.category}-${row.risk_type}-${index}`} />
</Panel>

<Panel title="交易所监管观察期" eyebrow="JYSJK · 当前 + 历史" subtitle="这里是交易所设置的监管起止区间，不等同于问询处分或异常波动阈值；当前区间接入公开 L1 现价。" busy={supervisionResource.busy} error={supervisionResource.error} onRetry={load} empty={supervisionResource.loaded && supervision.length === 0} emptyText="该证券没有命中当前或近期历史监管观察期。" scroll>
  <DataTable columns={supervisionColumns} rows={supervision} rowKey={(row) => row.supervision_id} />
</Panel>

<Panel title="关联事件" eyebrow="SJQD + BWYQ + RDYC" subtitle={`焦点子图：${count(graph?.counts.events)} 个事件 → 当前证券；事件总成员数保留在末列。`} empty={resource.loaded && events.length === 0} emptyText="该证券当前没有命中事件驱动、部委要闻或热点映射。" scroll>
  <DataTable columns={eventColumns} rows={events} rowKey={(row) => row.event_id} />
</Panel>
