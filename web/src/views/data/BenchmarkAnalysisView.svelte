<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { count, date, delta, fixed, price, text, tone } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import { router, stockPath } from '../../lib/router.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import StatGrid, { type Stat } from '../../ui/StatGrid.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type { BenchmarkAnalysisRecord, MarketBenchmarkAnalysisDocument } from '../../types';

  type View = MarketBenchmarkAnalysisDocument['view'];
  const VIEWS = [
    { id: 'stocks', label: '个股牛熊' }, { id: 'industries', label: '行业牛熊' },
    { id: 'suspensions', label: '停复牌' }, { id: 'new-stocks', label: '次新股' }
  ];
  const STAGES = [
    ['201','2009-08—2013-06'],['202','2013-06—2015-06'],['203','2015-06—2016-01'],
    ['204','2016-01—2018-01'],['208','2018-01—2019-01'],['209','2019-01—2019-04'],
    ['210','2019-04—2020-03'],['211','2020-03—2021-02'],['212','2021-02—2022-04'],
    ['213','2022-04—2022-07'],['214','2022-07—2024-02'],['215','2024-02—2024-09'],['216','2024-09—至今']
  ].map(([id,label]) => ({ id, label }));
  let view = $state<View>('stocks');
  let stage = $state('216');
  let query = $state('');
  let selectedId = $state('');
  const resource = new Resource<MarketBenchmarkAnalysisDocument>();
  const doc = $derived(resource.data); const rows = $derived(doc?.records ?? []);
  const selected = $derived(rows.find((row) => row.event_id === selectedId) ?? rows[0] ?? null);
  const staged = $derived(view === 'stocks' || view === 'industries');
  const stats = $derived.by<Stat[]>(() => doc ? [
    { label: '命中记录', value: count(doc.match_count) }, { label: '历史阶段', value: count(doc.summary.stages) },
    { label: '覆盖实体', value: count(doc.summary.unique_entities) }, { label: '客户端源', value: count(doc.sources.length) }
  ] : []);
  function load(refresh=false) {
    selectedId=''; void resource.load(`/api/v1/market/benchmark-analysis?${queryString({ view, stage: staged ? stage : '', q: query.trim(), limit: 10000, include_raw: 0, refresh: refresh ? 1 : 0 })}`);
  }
  function switchView(next:string) { view=next as View; load(); }
  function switchStage(next:string) { stage=next; load(); }
  function name(row:BenchmarkAnalysisRecord) { return row.security.name || row.security.code; }
  function openStock() { if(selected && selected.kind !== 'industries') router.go(stockPath(selected.security.market,selected.security.code,'benchmark-analysis')); }
  const stockColumns: Column<BenchmarkAnalysisRecord>[] = [
    {key:'security',label:'证券 / 行业',width:'160px',value:name,sub:(r)=>r.security.security_id},
    {key:'period',label:'阶段',width:'190px',value:(r)=>`${date(r.stage_start_date)} — ${r.stage_open?'至今':date(r.stage_end_date)}`},
    {key:'return',label:'阶段涨幅',align:'right',num:true,value:(r)=>delta(r.kind==='industries'?r.industry_return_pct:r.security_return_pct),tone:(r)=>tone(r.kind==='industries'?r.industry_return_pct:r.security_return_pct)},
    {key:'market',label:'市场涨幅',align:'right',num:true,value:(r)=>delta(r.market_return_pct),tone:(r)=>tone(r.market_return_pct)},
    {key:'excess',label:'超额市场',align:'right',num:true,value:(r)=>delta(r.excess_market_pct),tone:(r)=>tone(r.excess_market_pct)},
    {key:'industry',label:'行业 / 超额行业',align:'right',num:true,value:(r)=>delta(r.industry_return_pct),sub:(r)=>delta(r.excess_industry_pct)},
    {key:'recent',label:'近1月 / 3月',align:'right',num:true,value:(r)=>delta(r.recent_1m_pct),sub:(r)=>delta(r.recent_3m_pct)}
  ];
  const suspensionColumns: Column<BenchmarkAnalysisRecord>[] = [
    {key:'security',label:'证券',width:'160px',value:name,sub:(r)=>r.security.security_id},
    {key:'dates',label:'停牌 — 复牌',width:'210px',value:(r)=>`${date(r.suspension_date)} — ${date(r.resumption_date)}`},
    {key:'days',label:'交易日',align:'right',num:true,value:(r)=>count(r.suspension_trading_days)},
    {key:'reason',label:'停牌原因',width:'260px',value:(r)=>text(r.reason)},
    {key:'industry',label:'期间行业',align:'right',num:true,value:(r)=>delta(r.industry_return_during_suspension_pct)},
    {key:'market',label:'期间市场',align:'right',num:true,value:(r)=>delta(r.market_return_during_suspension_pct)}
  ];
  const newColumns: Column<BenchmarkAnalysisRecord>[] = [
    {key:'security',label:'次新股',width:'160px',value:name,sub:(r)=>r.security.security_id},
    {key:'issue',label:'发行价',align:'right',num:true,value:(r)=>price(r.issue_price)},
    {key:'3m',label:'近3月 / 超额市场',align:'right',num:true,value:(r)=>delta(r.return_3m_pct),sub:(r)=>delta(r.excess_market_3m_pct),tone:(r)=>tone(r.return_3m_pct)},
    {key:'3mi',label:'近3月超额行业',align:'right',num:true,value:(r)=>delta(r.excess_industry_3m_pct)},
    {key:'1m',label:'近1月 / 超额市场',align:'right',num:true,value:(r)=>delta(r.return_1m_pct),sub:(r)=>delta(r.excess_market_1m_pct)},
    {key:'1w',label:'近1周 / 超额市场',align:'right',num:true,value:(r)=>delta(r.return_1w_pct),sub:(r)=>delta(r.excess_market_1w_pct)}
  ];
  const columns=$derived(staged?stockColumns:view==='suspensions'?suspensionColumns:newColumns);
  const detailStats=$derived.by<Stat[]>(()=>selected ? [
    {label:'阶段涨幅',value:delta(selected.security_return_pct ?? selected.industry_return_pct ?? selected.return_3m_pct)},
    {label:'超额市场',value:delta(selected.excess_market_pct ?? selected.excess_market_3m_pct)},
    {label:'超额行业',value:delta(selected.excess_industry_pct ?? selected.excess_industry_3m_pct)},
    {label:'原始字段',value:count(Object.keys(selected.raw ?? {}).length)}
  ]:[]);
  onMount(()=>load());
</script>

<PageHeader eyebrow="JZFX · 28 LIVE SOURCES" title="相对基准分析" description="按十三个上证指数地标阶段比较个股、行业与市场，并还原停复牌期间及次新股的一周、一月、三月相对表现。" {stats}>
  {#snippet actions()}<Segmented options={VIEWS} value={view} onChange={switchView} ariaLabel="基准分析视图"/><Button icon="refresh" busy={resource.busy} onclick={()=>load(true)}>刷新源数据</Button>{/snippet}
</PageHeader>
{#if staged}<Panel title="牛熊阶段" subtitle="客户端固定的十三段历史地标"><Segmented options={STAGES} value={stage} onChange={switchStage} ariaLabel="牛熊阶段"/></Panel>{/if}
<Panel title="相对表现主表" subtitle={doc?`${count(doc.match_count)} 条 · ${count(doc.sources.length)} 个源`:'本地 JSN 优先'} busy={resource.busy} error={resource.error} onRetry={()=>load()} empty={resource.loaded&&rows.length===0} emptyText="当前筛选下没有记录。" flush scroll>
  {#snippet toolbar()}<TextInput bind:value={query} icon="search" width="260px" label="检索证券或行业" placeholder="代码或名称" onEnter={()=>load()}/>{/snippet}
  <DataTable {columns} {rows} rowKey={(r)=>r.event_id} onRowClick={(r)=>(selectedId=r.event_id)} isActive={(r)=>r.event_id===selected?.event_id} stickyFirst numbered minWidth="1050px"/>
</Panel>
{#if selected}<Panel eyebrow={selected.kind_label} title={name(selected)} subtitle={selected.source_resource}><StatGrid stats={detailStats} inline/>{#if selected.kind!=='industries'}<button type="button" onclick={openStock}>在个股工作台打开</button>{/if}<p>阶段锚点 {fixed(selected.stage_start_anchor,2)} → {selected.stage_open?'至今':fixed(selected.stage_end_anchor,2)}。需要当前行情才能计算的客户端宿主列没有伪造。</p></Panel>{/if}
<style>button{height:24px;padding:0 var(--sp-3);color:var(--focus);border:1px solid var(--line-strong);border-radius:var(--radius)}p{font-size:var(--fs-xs);color:var(--fg-mute)}</style>
