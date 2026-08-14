<script lang="ts">
  import { onMount } from 'svelte';
  import { queryString } from '../../api';
  import { compact, count, date, fixed, percent, text, tone } from '../../lib/fmt';
  import { router, stockPath } from '../../lib/router.svelte';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    BondIssuerRecord,
    FuturesContract,
    FuturesIssuanceDocument,
    FuturesMonthlyRecord,
    FuturesPositionPoint,
    FuturesRelatedStock,
    IpoIndustryRecord,
    IpoMonthlyRecord,
    IpoSecurityRecord,
    PrivatePlacementRecord,
    PreferredShareRecord,
    RightsOfferingRecord,
    ValuationSecurity
  } from '../../types';

  const SECTIONS = [
    { id: 'futures', label: '期货统计' },
    { id: 'ipo', label: 'IPO 与债券' },
    { id: 'placements', label: '定向增发' },
    { id: 'rights', label: '配股募资' },
    { id: 'preferred-shares', label: '优先股' }
  ];
  const PLACEMENT_STATUSES = [
    { id: 'all', label: '全部' },
    { id: 'active', label: '方案推进' },
    { id: 'registered', label: '注册生效' },
    { id: 'locked', label: '锁定中' },
    { id: 'unlocked', label: '已解锁' },
    { id: 'stopped', label: '终止/延期' }
  ];
  const IPO_MODES = [
    { id: 'industries', label: 'IPO 行业' },
    { id: 'listed', label: '上市发行人' },
    { id: 'unlisted', label: '未上市发行人' }
  ];

  type Section = 'futures' | 'ipo' | 'placements' | 'rights' | 'preferred-shares';
  let section = $state<Section>('futures');
  let ipoMode = $state<'industries' | 'listed' | 'unlisted'>('industries');
  let year = $state(String(new Date().getFullYear()));
  let contractKey = $state('');
  let industryKey = $state('');
  let placementStatus = $state('all');
  let placementSearch = $state('');
  let selectedPlacementId = $state('');
  let selectedRightsId = $state('');
  let selectedPreferredId = $state('');
  const resource = new Resource<FuturesIssuanceDocument>();
  const doc = $derived(resource.data);
  const commodity = $derived(doc?.commodity_futures ?? []);
  const indices = $derived(doc?.index_futures ?? []);
  const placements = $derived(doc?.private_placements ?? []);
  const rights = $derived(doc?.rights_offerings ?? []);
  const preferredShares = $derived(doc?.preferred_shares ?? []);
  const selectedPlacement = $derived(
    placements.find((row) => row.event_id === selectedPlacementId) ?? placements[0]
  );
  const selectedRights = $derived(
    rights.find((row) => row.event_id === selectedRightsId) ?? rights[0]
  );
  const selectedPreferred = $derived(
    preferredShares.find((row) => row.record_id === selectedPreferredId) ?? preferredShares[0]
  );

  const stats = $derived.by(() => section === 'futures'
    ? [
        { label: '商品期货', value: count(doc?.counts.commodity_futures) },
        { label: '股指期货', value: count(doc?.counts.index_futures) },
        { label: '月度统计', value: count(doc?.counts.monthly_futures) },
        { label: '关联股票', value: count(doc?.counts.related_stocks) }
      ]
    : section === 'ipo' ? [
        { label: 'IPO 年度', value: count(doc?.counts.ipo_years) },
        { label: `${year} 行业`, value: count(doc?.counts.ipo_industries) },
        { label: '上市发行人', value: count(doc?.counts.listed_bond_issuers) },
        { label: '未上市发行人', value: count(doc?.counts.unlisted_bond_issuers) }
      ] : section === 'rights' ? [
        { label: '配股记录', value: count(doc?.rights_summary.records) },
        { label: '覆盖股票', value: count(doc?.rights_summary.unique_securities) },
        { label: '已实施 / 审议中', value: `${count(doc?.rights_summary.phase_counts.implemented)} / ${count(doc?.rights_summary.phase_counts.deliberating)}` },
        { label: '异常进度', value: count(doc?.rights_summary.phase_counts.abnormal) },
        { label: '实际募资', value: compact(doc?.rights_summary.implemented_raised_yuan, '元') },
        { label: '计划募资', value: compact(doc?.rights_summary.planned_raised_yuan, '元') }
      ] : section === 'preferred-shares' ? [
        { label: '优先股记录', value: count(doc?.preferred_share_summary.records) },
        { label: '基础股票', value: count(doc?.preferred_share_summary.unique_underlying_securities) },
        { label: '发行规模', value: compact(doc?.preferred_share_summary.total_issue_size_yuan, '元') },
        { label: '累计股息', value: count(doc?.preferred_share_summary.cumulative_dividend_count) },
        { label: '可调股息', value: count(doc?.preferred_share_summary.adjustable_dividend_count) }
      ] : [
        { label: '增发记录', value: count(doc?.placement_summary.records) },
        { label: '覆盖股票', value: count(doc?.placement_summary.unique_securities) },
        { label: '实际募资', value: compact(Number(doc?.placement_summary.actual_gross_10k_yuan ?? 0) * 10000, '元') },
        { label: '预计募资', value: compact(Number(doc?.placement_summary.expected_raise_10k_yuan ?? 0) * 10000, '元') }
      ]);

  function load(refresh = false) {
    const validYear = /^\d{4}$/.test(year) ? year : String(new Date().getFullYear());
    if (validYear !== year) year = validYear;
    void resource.load(`/api/v1/market/futures-issuance?${queryString({
      section,
      contract_key: section === 'futures' ? contractKey : '',
      year: section === 'ipo' ? validYear : '',
      industry_key: section === 'ipo' ? industryKey : '',
      placement_status: section === 'placements' ? placementStatus : '',
      q: section === 'placements' || section === 'rights' || section === 'preferred-shares' ? placementSearch : '',
      limit: section === 'placements' || section === 'rights' || section === 'preferred-shares' ? 2000 : 500,
      refresh: refresh ? 1 : 0
    })}`);
  }

  function switchSection(next: string) {
    section = next as Section;
    contractKey = '';
    industryKey = '';
    selectedPlacementId = '';
    selectedRightsId = '';
    selectedPreferredId = '';
    load();
  }

  function selectContract(row: FuturesContract) {
    contractKey = row.contract_key;
    load();
  }

  function selectIndustry(row: IpoIndustryRecord) {
    industryKey = row.industry_key;
    load();
  }

  function openSecurity(security?: ValuationSecurity) {
    if (security?.market && security.code) router.go(stockPath(security.market, security.code));
  }

  const commodityColumns: Column<FuturesContract>[] = [
    { key: 'name', label: '品种', value: (row) => row.name, sub: (row) => row.contract_key, sortValue: (row) => row.name },
    { key: 'spot', label: '现货价', align: 'right', num: true, value: (row) => fixed(row.spot_price, 2), sub: (row) => text(row.basis_state), sortValue: (row) => Number(row.spot_price) || 0 },
    { key: 'days5', label: '5 日', align: 'right', num: true, value: (row) => percent(row.return_5d_pct, 2, true), tone: (row) => tone(row.return_5d_pct), sortValue: (row) => Number(row.return_5d_pct) || 0 },
    { key: 'days10', label: '10 日', align: 'right', num: true, value: (row) => percent(row.return_10d_pct, 2, true), tone: (row) => tone(row.return_10d_pct), sortValue: (row) => Number(row.return_10d_pct) || 0 },
    { key: 'open', label: '持仓量', align: 'right', num: true, value: (row) => compact(row.open_interest), sortValue: (row) => Number(row.open_interest) || 0 }
  ];
  const indexColumns: Column<FuturesContract>[] = [
    { key: 'name', label: '股指', value: (row) => row.name, sub: (row) => row.contract_key },
    { key: 'position', label: '净持仓', align: 'right', num: true, value: (row) => count(row.net_position), tone: (row) => tone(row.net_position), sortValue: (row) => Number(row.net_position) || 0 },
    { key: 'date', label: '日期', num: true, value: (row) => date(row.date) }
  ];
  const relatedColumns: Column<FuturesRelatedStock>[] = [
    { key: 'name', label: '股票', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'days5', label: '5 日', align: 'right', num: true, value: (row) => percent(row.return_5d_pct, 2, true), tone: (row) => tone(row.return_5d_pct), sortValue: (row) => Number(row.return_5d_pct) || 0 },
    { key: 'days10', label: '10 日', align: 'right', num: true, value: (row) => percent(row.return_10d_pct, 2, true), tone: (row) => tone(row.return_10d_pct), sortValue: (row) => Number(row.return_10d_pct) || 0 }
  ];
  const positionColumns: Column<FuturesPositionPoint>[] = [
    { key: 'date', label: '日期', num: true, value: (row) => date(row.date), sortValue: (row) => row.date },
    { key: 'position', label: '净持仓', align: 'right', num: true, value: (row) => count(row.net_position), tone: (row) => tone(row.net_position), sortValue: (row) => Number(row.net_position) || 0 }
  ];
  const monthlyColumns: Column<FuturesMonthlyRecord>[] = [
    { key: 'product', label: '品种', value: (row) => row.product, sub: (row) => row.year_month, sortValue: (row) => row.product },
    { key: 'volume', label: '月成交量', align: 'right', num: true, value: (row) => compact(row.monthly_volume), sub: (row) => percent(row.monthly_volume_yoy_pct), sortValue: (row) => Number(row.monthly_volume) || 0 },
    { key: 'amount', label: '月成交额', align: 'right', num: true, value: (row) => compact(row.monthly_turnover_yuan, '元'), sub: (row) => percent(row.monthly_turnover_yoy_pct), sortValue: (row) => Number(row.monthly_turnover_yuan) || 0 }
  ];
  const industryColumns: Column<IpoIndustryRecord>[] = [
    { key: 'name', label: '行业', value: (row) => row.industry_name, sub: (row) => row.industry_code, sortValue: (row) => row.industry_name },
    { key: 'count', label: '上市家数', align: 'right', num: true, value: (row) => count(row.listed_count), sortValue: (row) => Number(row.listed_count) || 0 },
    { key: 'raised', label: '募资', align: 'right', num: true, value: (row) => compact(Number(row.raised_10k_yuan || 0) * 10000, '元'), sortValue: (row) => Number(row.raised_10k_yuan) || 0 },
    { key: 'gain', label: '上市涨幅', align: 'right', num: true, value: (row) => percent(row.average_listing_gain_pct), tone: (row) => tone(row.average_listing_gain_pct), sortValue: (row) => Number(row.average_listing_gain_pct) || 0 }
  ];
  const ipoStockColumns: Column<IpoSecurityRecord>[] = [
    { key: 'name', label: '股票', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id },
    { key: 'date', label: '上市日', num: true, value: (row) => date(row.listing_date), sortValue: (row) => row.listing_date },
    { key: 'price', label: '发行价', align: 'right', num: true, value: (row) => fixed(row.issue_price, 2), sortValue: (row) => Number(row.issue_price) || 0 },
    { key: 'raised', label: '募资', align: 'right', num: true, value: (row) => compact(Number(row.raised_10k_yuan || 0) * 10000, '元'), sortValue: (row) => Number(row.raised_10k_yuan) || 0 }
  ];
  const monthColumns: Column<IpoMonthlyRecord>[] = [
    { key: 'month', label: '月份', num: true, value: (row) => row.month, sortValue: (row) => row.month },
    { key: 'count', label: '家数', align: 'right', num: true, value: (row) => count(row.listed_count) },
    { key: 'raised', label: '募资', align: 'right', num: true, value: (row) => `${fixed(row.raised_100m_yuan, 2)} 亿` }
  ];
  const issuerColumns: Column<BondIssuerRecord>[] = [
    { key: 'name', label: '发行人', wrap: true, value: (row) => row.issuer_name, sub: (row) => row.security?.security_id ?? row.issuer_id, sortValue: (row) => row.issuer_name },
    { key: 'id', label: '内部键', num: true, value: (row) => row.issuer_id }
  ];
  const placementColumns: Column<PrivatePlacementRecord>[] = [
    {
      key: 'security', label: '股票',
      value: (row) => row.security.name || row.security.code,
      sub: (row) => row.security.security_id,
      sortValue: (row) => row.security.name || row.security.code
    },
    { key: 'stage', label: '阶段', value: (row) => row.stage, sub: (row) => row.lifecycle_label, sortValue: (row) => row.stage },
    { key: 'date', label: '关键日期', num: true, value: (row) => date(row.sort_date), sortValue: (row) => row.sort_date },
    {
      key: 'raised', label: '募资规模', align: 'right', num: true,
      value: (row) => compact(Number(row.actual_gross_10k_yuan ?? row.expected_raise_10k_yuan ?? 0) * 10000, '元'),
      sub: (row) => row.actual_gross_10k_yuan !== null ? '实际总额' : '预计金额',
      sortValue: (row) => Number(row.actual_gross_10k_yuan ?? row.expected_raise_10k_yuan ?? 0)
    },
    { key: 'shares', label: '发行股数', align: 'right', num: true, value: (row) => compact(Number(row.issue_shares_10k ?? 0) * 10000, '股'), sortValue: (row) => Number(row.issue_shares_10k ?? 0) },
    { key: 'price', label: '发行价', align: 'right', num: true, value: (row) => fixed(row.issue_price, 2), sortValue: (row) => Number(row.issue_price ?? 0) },
    { key: 'industry', label: '行业', value: (row) => text(row.industry), sortValue: (row) => row.industry }
  ];

  const rightsColumns: Column<RightsOfferingRecord>[] = [
    { key: 'security', label: '股票', width: '145px', value: (row) => row.security.name || row.security.code, sub: (row) => row.security.security_id, sortValue: (row) => row.security.name || row.security.code },
    { key: 'phase', label: '阶段 / 进度', width: '135px', value: (row) => row.stage, sub: (row) => row.phase_label, sortValue: (row) => row.phase },
    { key: 'date', label: '更新日期', width: '92px', num: true, value: (row) => date(row.sort_date), sub: (row) => `公告 ${date(row.announcement_date)}`, sortValue: (row) => row.sort_date },
    { key: 'ratio', label: '每 10 股配售', width: '95px', align: 'right', num: true, value: (row) => fixed(row.rights_per_10_shares, 2), sub: (row) => row.rights_code || row.rights_name },
    { key: 'price', label: '配股价格', width: '85px', align: 'right', num: true, value: (row) => fixed(row.rights_price_yuan, 2) },
    { key: 'shares', label: '配售股数', width: '105px', align: 'right', num: true, value: (row) => compact(row.offered_shares, '股'), sortValue: (row) => row.offered_shares ?? 0 },
    { key: 'raised', label: '募集资金', width: '110px', align: 'right', num: true, value: (row) => compact(row.raised_yuan, '元'), sub: (row) => row.amount_semantics === 'actual' ? '实际口径' : '计划口径', sortValue: (row) => row.raised_yuan ?? 0 }
  ];

  const preferredColumns: Column<PreferredShareRecord>[] = [
    { key: 'underlying', label: '基础股票', width: '145px', value: (row) => row.underlying_security.name || row.underlying_security.code, sub: (row) => row.underlying_security.security_id, sortValue: (row) => row.underlying_security.name || row.underlying_security.code },
    { key: 'preferred', label: '优先股', width: '125px', value: (row) => row.preferred_name || row.preferred_code, sub: (row) => row.preferred_code, sortValue: (row) => row.preferred_code },
    { key: 'date', label: '上市日期', width: '92px', num: true, value: (row) => date(row.listing_date), sortValue: (row) => row.listing_date },
    { key: 'size', label: '发行规模', width: '110px', align: 'right', num: true, value: (row) => compact(row.issue_size_yuan, '元'), sub: (row) => `${fixed(row.issue_shares_10k, 2)} 万股`, sortValue: (row) => row.issue_size_yuan ?? 0 },
    { key: 'yield', label: '初始股息率', width: '90px', align: 'right', num: true, value: (row) => percent(row.initial_dividend_yield_pct), sortValue: (row) => row.initial_dividend_yield_pct ?? 0 },
    { key: 'terms', label: '股息条款', width: '105px', value: (row) => `${row.cumulative_dividend === '是' ? '累积' : '非累积'} / ${row.adjustable_dividend === '是' ? '可调' : '固定'}` },
    { key: 'method', label: '发行方式', wrap: true, value: (row) => text(row.issue_method), sortValue: (row) => row.issue_method }
  ];

  onMount(() => load());
</script>

<PageHeader
  eyebrow="TDX JSN · 期货与发行"
  title="期货与发行统计"
  description="通达信商品/股指期货、关联 A 股、IPO/债券发行人、定向增发、配股生命周期，以及优先股发行与股息条款。"
  {stats}
>
  {#snippet actions()}
    <Segmented options={SECTIONS} value={section} onChange={switchSection} ariaLabel="数据分区" />
    <Button icon="refresh" busy={resource.busy} onclick={() => load(true)}>刷新源数据</Button>
  {/snippet}
</PageHeader>

{#if section === 'futures'}
  <Split asideWidth="360px">
    {#snippet main()}
      <div class="stage">
        <div class="pair">
          <Panel flush scroll title="商品期货" subtitle={`${count(commodity.length)} 个品种`} busy={resource.busy} error={resource.error} onRetry={() => load()}>
            <DataTable columns={commodityColumns} rows={commodity} rowKey={(row) => row.contract_key} onRowClick={selectContract} isActive={(row) => row.contract_key === contractKey} stickyFirst />
          </Panel>
          <Panel flush scroll title="股指期货" subtitle={`${count(indices.length)} 个合约`} busy={resource.busy}>
            <DataTable columns={indexColumns} rows={indices} rowKey={(row) => row.contract_key} onRowClick={selectContract} isActive={(row) => row.contract_key === contractKey} />
          </Panel>
        </div>
        <Panel flush scroll title="月度期货成交" subtitle={`${count(doc?.monthly_futures.length)} 条`} busy={resource.busy}>
          <DataTable columns={monthlyColumns} rows={doc?.monthly_futures ?? []} rowKey={(row, index) => `${row.year_month}:${row.product}:${index}`} stickyFirst />
        </Panel>
      </div>
    {/snippet}
    {#snippet aside()}
      <Panel flush scroll eyebrow="CONTRACT DETAIL" title={doc?.selected_contract?.name ?? '选择期货合约'} subtitle={doc?.selected_contract?.contract_key ?? '点击左侧品种'} busy={resource.busy} empty={!doc?.selected_contract && !resource.busy} emptyText="点击商品期货查看关联股票；点击股指期货查看净持仓历史。">
        {#if doc?.related_stocks.length}
          <DataTable columns={relatedColumns} rows={doc.related_stocks} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} />
        {:else if doc?.position_history.length}
          <DataTable columns={positionColumns} rows={doc.position_history} rowKey={(row) => row.date} sortKey="date" />
        {/if}
      </Panel>
    {/snippet}
  </Split>
{:else if section === 'ipo'}
  <Split asideWidth="380px">
    {#snippet main()}
      <Panel flush scroll title={IPO_MODES.find((item) => item.id === ipoMode)?.label ?? '发行统计'} busy={resource.busy} error={resource.error} onRetry={() => load()}>
        {#snippet toolbar()}
          <Segmented options={IPO_MODES} value={ipoMode} onChange={(next) => (ipoMode = next as typeof ipoMode)} ariaLabel="发行数据类型" />
          <TextInput bind:value={year} width="82px" label="年份" placeholder="2026" onEnter={() => { industryKey = ''; load(); }} />
          <Button icon="search" onclick={() => { industryKey = ''; load(); }}>查询年份</Button>
        {/snippet}
        {#if ipoMode === 'industries'}
          <DataTable columns={industryColumns} rows={doc?.ipo_industries ?? []} rowKey={(row) => row.industry_key} onRowClick={selectIndustry} isActive={(row) => row.industry_key === industryKey} stickyFirst />
        {:else if ipoMode === 'listed'}
          <DataTable columns={issuerColumns} rows={doc?.listed_bond_issuers ?? []} rowKey={(row) => row.issuer_id} onRowClick={(row) => openSecurity(row.security)} stickyFirst />
        {:else}
          <DataTable columns={issuerColumns} rows={doc?.unlisted_bond_issuers ?? []} rowKey={(row) => row.issuer_id} stickyFirst />
        {/if}
      </Panel>
    {/snippet}
    {#snippet aside()}
      <div class="stage">
        <Panel flush scroll title={`${year} 月度 IPO`} subtitle={`${count(doc?.ipo_monthly.length)} 个月`} busy={resource.busy}>
          <DataTable columns={monthColumns} rows={doc?.ipo_monthly ?? []} rowKey={(row) => row.month} sortKey="month" />
        </Panel>
        <Panel flush scroll title="行业 IPO 股票" subtitle={industryKey || '选择左侧行业'} busy={resource.busy} empty={!industryKey && !resource.busy} emptyText="点击 IPO 行业，展开该年该行业的上市股票。">
          <DataTable columns={ipoStockColumns} rows={doc?.ipo_securities ?? []} rowKey={(row) => row.security.security_id} onRowClick={(row) => openSecurity(row.security)} />
        </Panel>
      </div>
    {/snippet}
  </Split>
{:else if section === 'placements'}
  <Split asideWidth="420px">
    {#snippet main()}
      <Panel flush scroll title="定向增发全景" subtitle={`${count(placements.length)} 条 · 来源生命周期不混并`} busy={resource.busy} error={resource.error} onRetry={() => load()}>
        {#snippet toolbar()}
          <Segmented options={PLACEMENT_STATUSES} value={placementStatus} onChange={(next) => { placementStatus = next; selectedPlacementId = ''; load(); }} ariaLabel="定增生命周期" />
          <TextInput bind:value={placementSearch} width="190px" label="检索" placeholder="代码 / 名称 / 行业 / 详情" onEnter={() => load()} />
          <Button icon="search" onclick={() => load()}>查询</Button>
        {/snippet}
        <DataTable
          columns={placementColumns}
          rows={placements}
          rowKey={(row) => row.event_id}
          onRowClick={(row) => (selectedPlacementId = row.event_id)}
          isActive={(row) => row.event_id === selectedPlacement?.event_id}
          sortKey="date"
          stickyFirst
        />
      </Panel>
    {/snippet}
    {#snippet aside()}
      <Panel
        scroll
        eyebrow="PRIVATE PLACEMENT"
        title={selectedPlacement ? `${selectedPlacement.security.name || selectedPlacement.security.code} · ${selectedPlacement.stage}` : '选择定增记录'}
        subtitle={selectedPlacement?.security.security_id ?? '点击左侧记录查看完整口径'}
        busy={resource.busy}
        empty={!selectedPlacement && !resource.busy}
        emptyText="当前筛选条件下没有定向增发记录"
      >
        {#if selectedPlacement}
          <div class="placement-detail">
            <div class="detail-grid">
              <span>生命周期<strong>{selectedPlacement.lifecycle_label}</strong></span>
              <span>关键日期<strong>{date(selectedPlacement.sort_date)}</strong></span>
              <span>发行价格<strong>{fixed(selectedPlacement.issue_price, 2)}</strong></span>
              <span>发行股数<strong>{compact(Number(selectedPlacement.issue_shares_10k ?? 0) * 10000, '股')}</strong></span>
              <span>实际募资<strong>{compact(Number(selectedPlacement.actual_gross_10k_yuan ?? 0) * 10000, '元')}</strong></span>
              <span>预计募资<strong>{compact(Number(selectedPlacement.expected_raise_10k_yuan ?? 0) * 10000, '元')}</strong></span>
              <span>获准→实施<strong>{percent(selectedPlacement.approval_to_implementation_return_pct, 2, true)}</strong></span>
              <span>锁定期表现<strong>{percent(selectedPlacement.lock_period_return_pct, 2, true)}</strong></span>
            </div>
            <div class="date-line">
              董事会 {date(selectedPlacement.dates.board_approved)} · 股东大会 {date(selectedPlacement.dates.shareholders_approved)} ·
              注册/获准 {date(selectedPlacement.dates.registered || selectedPlacement.dates.regulator_approved)} ·
              实施 {date(selectedPlacement.dates.implemented)} · 上市 {date(selectedPlacement.dates.listed)} · 解锁 {date(selectedPlacement.dates.unlock)}
            </div>
            {#if selectedPlacement.change_notes}<p class="note-block">{selectedPlacement.change_notes}</p>{/if}
            {#if selectedPlacement.issue_details}<p class="detail-copy">{selectedPlacement.issue_details}</p>{/if}
            <div class="detail-actions">
              <Button icon="market" onclick={() => openSecurity(selectedPlacement.security)}>打开个股工作台</Button>
              {#if selectedPlacement.registration_announcement}
                <a href={selectedPlacement.registration_announcement} target="_blank" rel="noreferrer">注册公告 PDF ↗</a>
              {/if}
            </div>
            <p class="source-note">来源：{selectedPlacement.source_resource}</p>
          </div>
        {/if}
      </Panel>
    {/snippet}
  </Split>
{:else if section === 'rights'}
  <Split asideWidth="430px">
    {#snippet main()}
      <Panel flush scroll title="配股募资生命周期" subtitle={`${count(rights.length)} 条 · 实施、审议与异常阶段分开保留`} busy={resource.busy} error={resource.error} onRetry={() => load()}>
        {#snippet toolbar()}
          <TextInput bind:value={placementSearch} width="230px" label="检索" placeholder="代码 / 名称 / 进度 / 详情" onEnter={() => load()} />
          <Button icon="search" onclick={() => { selectedRightsId = ''; load(); }}>查询</Button>
        {/snippet}
        <DataTable columns={rightsColumns} rows={rights} rowKey={(row) => row.event_id} onRowClick={(row) => (selectedRightsId = row.event_id)} isActive={(row) => row.event_id === selectedRights?.event_id} sortKey="date" stickyFirst />
      </Panel>
    {/snippet}
    {#snippet aside()}
      <Panel scroll eyebrow="QXFA107 / 108 / 109" title={selectedRights ? `${selectedRights.security.name || selectedRights.security.code} · ${selectedRights.stage}` : '选择配股记录'} subtitle={selectedRights?.security.security_id ?? '点击左侧记录查看完整口径'} busy={resource.busy} empty={!selectedRights && !resource.busy} emptyText="当前筛选条件下没有配股记录">
        {#if selectedRights}
          <div class="placement-detail">
            <div class="detail-grid">
              <span>阶段<strong>{selectedRights.phase_label}</strong></span>
              <span>更新日期<strong>{date(selectedRights.sort_date)}</strong></span>
              <span>配售比例<strong>每 10 股 {fixed(selectedRights.rights_per_10_shares, 2)} 股</strong></span>
              <span>配股价格<strong>{fixed(selectedRights.rights_price_yuan, 2)}</strong></span>
              <span>配售股数<strong>{compact(selectedRights.offered_shares, '股')}</strong></span>
              <span>{selectedRights.amount_semantics === 'actual' ? '实际募资' : '计划募资'}<strong>{compact(selectedRights.raised_yuan, '元')}</strong></span>
            </div>
            <div class="date-line">公告 {date(selectedRights.announcement_date)} · 股权登记 {date(selectedRights.registration_date)} · 缴款 {date(selectedRights.payment_start_date)} — {date(selectedRights.payment_end_date)} · 除权 {date(selectedRights.ex_rights_date)}</div>
            {#if selectedRights.major_shareholder_subscription}<p class="note-block">大股东认购：{selectedRights.major_shareholder_subscription}</p>{/if}
            {#if selectedRights.issue_details}<p class="detail-copy">{selectedRights.issue_details}</p>{/if}
            <div class="detail-actions"><Button icon="market" onclick={() => openSecurity(selectedRights.security)}>打开个股工作台</Button></div>
            <p class="source-note">来源：{selectedRights.source_resource}</p>
          </div>
        {/if}
      </Panel>
    {/snippet}
  </Split>
{:else}
  <Split asideWidth="420px">
    {#snippet main()}
      <Panel flush scroll title="优先股发行与股息条款" subtitle={`${count(preferredShares.length)} 条 · 原始万股/亿元口径同时换算为股/元`} busy={resource.busy} error={resource.error} onRetry={() => load()}>
        {#snippet toolbar()}
          <TextInput bind:value={placementSearch} width="245px" label="检索" placeholder="基础股票 / 优先股代码 / 发行方式" onEnter={() => load()} />
          <Button icon="search" onclick={() => { selectedPreferredId = ''; load(); }}>查询</Button>
        {/snippet}
        <DataTable columns={preferredColumns} rows={preferredShares} rowKey={(row) => row.record_id} onRowClick={(row) => (selectedPreferredId = row.record_id)} isActive={(row) => row.record_id === selectedPreferred?.record_id} sortKey="date" stickyFirst />
      </Panel>
    {/snippet}
    {#snippet aside()}
      <Panel scroll eyebrow="YXG101 · PREFERRED SHARE" title={selectedPreferred ? `${selectedPreferred.preferred_name || selectedPreferred.preferred_code} · ${selectedPreferred.underlying_security.name || selectedPreferred.underlying_security.code}` : '选择优先股'} subtitle={selectedPreferred?.underlying_security.security_id ?? '点击左侧记录查看完整条款'} busy={resource.busy} empty={!selectedPreferred && !resource.busy} emptyText="当前筛选条件下没有优先股记录">
        {#if selectedPreferred}
          <div class="placement-detail">
            <div class="detail-grid">
              <span>优先股代码<strong>{selectedPreferred.preferred_code}</strong></span>
              <span>上市日期<strong>{date(selectedPreferred.listing_date)}</strong></span>
              <span>每股面值<strong>{fixed(selectedPreferred.par_value_yuan, 2)} 元</strong></span>
              <span>发行价格<strong>{fixed(selectedPreferred.issue_price_yuan, 2)} 元</strong></span>
              <span>发行数量<strong>{compact(selectedPreferred.issue_shares, '股')}</strong></span>
              <span>发行规模<strong>{compact(selectedPreferred.issue_size_yuan, '元')}</strong></span>
              <span>初始股息率<strong>{percent(selectedPreferred.initial_dividend_yield_pct)}</strong></span>
              <span>每年付息<strong>{count(selectedPreferred.annual_payment_count)} 次 · {selectedPreferred.payment_method || '—'}</strong></span>
              <span>股息累积<strong>{selectedPreferred.cumulative_dividend || '—'}</strong></span>
              <span>股息可调<strong>{selectedPreferred.adjustable_dividend || '—'}</strong></span>
            </div>
            {#if selectedPreferred.issue_method}<p class="note-block">发行方式：{selectedPreferred.issue_method}</p>{/if}
            <div class="detail-actions"><Button icon="market" onclick={() => openSecurity(selectedPreferred.underlying_security)}>打开基础股票工作台</Button></div>
            <p class="source-note">来源：{selectedPreferred.source_resource} · 原始发行数量 {fixed(selectedPreferred.issue_shares_10k, 4)} 万股 · 原始发行规模 {fixed(selectedPreferred.issue_size_100m_yuan, 4)} 亿元</p>
          </div>
        {/if}
      </Panel>
    {/snippet}
  </Split>
{/if}

<style>
  .stage {
    display: grid;
    min-height: 0;
    gap: var(--sp-3);
    grid-template-rows: minmax(230px, 1fr) minmax(220px, 0.8fr);
  }

  .pair {
    display: grid;
    min-height: 0;
    gap: var(--sp-3);
    grid-template-columns: minmax(0, 1fr) minmax(260px, 0.42fr);
  }

  .placement-detail { display: grid; gap: var(--sp-4); }
  .detail-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: var(--sp-2); }
  .detail-grid span { display: grid; gap: 3px; color: var(--text-muted); font-size: var(--fs-xs); }
  .detail-grid strong { color: var(--text); font-size: var(--fs-sm); font-weight: 650; }
  .date-line, .source-note { color: var(--text-muted); font-size: var(--fs-xs); line-height: 1.7; }
  .detail-copy, .note-block { margin: 0; white-space: pre-wrap; line-height: 1.72; font-size: var(--fs-sm); }
  .note-block { padding: var(--sp-3); border-radius: var(--radius-sm); background: var(--surface-raised); color: var(--text-secondary); }
  .detail-actions { display: flex; flex-wrap: wrap; align-items: center; gap: var(--sp-3); }
  .detail-actions a { color: var(--accent); font-size: var(--fs-sm); text-decoration: none; }

  @media (max-width: 1100px) {
    .pair { grid-template-columns: minmax(0, 1fr); }
  }
</style>
