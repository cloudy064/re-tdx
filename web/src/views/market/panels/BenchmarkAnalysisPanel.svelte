<script lang="ts">
  import { queryString } from '../../../api';
  import { count, date, delta, tone } from '../../../lib/fmt';
  import { Resource } from '../../../lib/resource.svelte';
  import DataTable, { type Column } from '../../../ui/DataTable.svelte';
  import Panel from '../../../ui/Panel.svelte';
  import type { BenchmarkAnalysisRecord, MarketBenchmarkAnalysisDocument } from '../../../types';
  import type { PanelProps } from '../StockWorkbench.svelte';
  const {market,code,name}:PanelProps=$props(); const resource=new Resource<MarketBenchmarkAnalysisDocument>(); const rows=$derived(resource.data?.records??[]);
  function load(refresh=false){void resource.load(`/api/v1/market/benchmark-analysis?${queryString({view:'stocks',market,code,limit:100,include_raw:0,refresh:refresh?1:0})}`)}
  const columns:Column<BenchmarkAnalysisRecord>[]=[
    {key:'period',label:'指数地标阶段',width:'220px',value:(r)=>`${date(r.stage_start_date)} — ${r.stage_open?'至今':date(r.stage_end_date)}`,sub:(r)=>`${r.stage_start_anchor} → ${r.stage_open?'当前':r.stage_end_anchor}`},
    {key:'stock',label:'个股涨幅',align:'right',num:true,value:(r)=>delta(r.security_return_pct),tone:(r)=>tone(r.security_return_pct)},
    {key:'market',label:'市场涨幅 / 超额',align:'right',num:true,value:(r)=>delta(r.market_return_pct),sub:(r)=>delta(r.excess_market_pct)},
    {key:'industry',label:'行业涨幅 / 超额',align:'right',num:true,value:(r)=>delta(r.industry_return_pct),sub:(r)=>delta(r.excess_industry_pct)},
    {key:'recent',label:'近1月 / 3月',align:'right',num:true,value:(r)=>delta(r.recent_1m_pct),sub:(r)=>delta(r.recent_3m_pct)}
  ];
  $effect(()=>{void market;void code;load()});
</script>
<Panel title={`${name} · 牛熊阶段相对表现`} eyebrow="JZFX · 13 LANDMARK STAGES" subtitle={`${count(rows.length)} 段个股 / 市场 / 行业对照`} busy={resource.busy} error={resource.error} onRetry={()=>load()} empty={resource.loaded&&rows.length===0} emptyText="该证券暂无阶段基准数据。" flush scroll><DataTable {columns} {rows} rowKey={(r)=>r.event_id} minWidth="920px"/></Panel>
