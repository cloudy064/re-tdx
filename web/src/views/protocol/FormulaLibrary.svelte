<script lang="ts">
  /**
   * 公式库：TCalc.dll 里内置的技术指标、条件选股、专家系统与五彩 K 线。
   * 后端 kind 取值固定为 all / technical / selection / expert / color-k。
   */
  import { onDestroy, onMount } from 'svelte';
  import FormulaChart from '../../charts/FormulaChart.svelte';
  import FormulaAutofilterTrace from './FormulaAutofilterTrace.svelte';
  import { getJson, postJson, queryString } from '../../api';
  import { takeStagedLevel2FormulaContext } from '../../lib/formulaContextBridge';
  import { count, text } from '../../lib/fmt';
  import { Resource } from '../../lib/resource.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Segmented from '../../ui/Segmented.svelte';
  import Select from '../../ui/Select.svelte';
  import Split from '../../ui/Split.svelte';
  import TextInput from '../../ui/TextInput.svelte';
  import type {
    CloudCalcAuditDocument,
    CloudCalcBatchDocument,
    CloudCalcEvaluationDocument,
    CloudCalcResultRow,
    CloudCalcTemplateDocument,
    Formula,
    FormulaAuditDocument,
    FormulaAuditRow,
    FormulaBacktestDocument,
    FormulaCalculationDocument,
    FormulaCoverage,
    FormulaScanMatch,
    FormulaScanDocument,
    FormulaStrategyAttribution,
    FormulaStrategyBacktestDocument,
    FormulaStrategyScanDocument,
    FormulaResult
  } from '../../types';

  type MonitorEventKind = 'snapshot' | 'entered' | 'exited' | 'updated' | 'degraded' | 'error';
  interface MonitorEvent {
    id: number;
    kind: MonitorEventKind;
    at: string;
    securityId?: string;
    message: string;
  }

  interface CloudCalcTemplate {
    id: string;
    name: string;
    cfg: string;
    asOf: string;
    quotes: boolean;
    row: string;
    updatedAt: string;
  }

  interface CloudCalcDisplayRow extends CloudCalcResultRow {
    row_index: number;
    unit_id: string;
  }

  type CloudCalcRunDocument = CloudCalcEvaluationDocument | CloudCalcBatchDocument;

  const KINDS = [
    { id: 'all', label: '全部' },
    { id: 'technical', label: '技术指标' },
    { id: 'selection', label: '条件选股' },
    { id: 'expert', label: '专家系统' },
    { id: 'color-k', label: '五彩 K 线' }
  ];

  const ORIGINS = [
    { id: 'all', label: '全部来源' },
    { id: 'system', label: '系统 TCalc' },
    { id: 'user', label: '我的 PriGS' }
  ];

  const CALC_MARKETS = [
    { id: 'sz', label: '深市' }, { id: 'sh', label: '沪市' },
    { id: 'bj', label: '北交所' }, { id: '31', label: '港股 · 31' },
    { id: '28', label: '郑商所期货 · 28' }, { id: '29', label: '大商所期货 · 29' },
    { id: '30', label: '上期所期货 · 30' }, { id: '47', label: '中金所期货 · 47' },
    { id: '66', label: '广期所期货 · 66' }, { id: '4', label: '郑商所期权 · 4' },
    { id: '5', label: '大商所期权 · 5' }, { id: '6', label: '上期所期权 · 6' },
    { id: '7', label: '中金所期权 · 7' }, { id: '67', label: '广期所期权 · 67' }
  ];

  const CUSTOM_SOURCES = {
    technical: 'FAST:EMA(CLOSE,12)-EMA(CLOSE,26);\nSIGNAL:EMA(FAST,9);\nHIST:(FAST-SIGNAL)*2,COLORSTICK;',
    selection: 'RESULT:CLOSE>MA(CLOSE,N);',
    expert: 'MID:=MA(CLOSE,N);\nENTERLONG:CROSS(CLOSE,MID);\nEXITLONG:CROSS(MID,CLOSE);'
  } as const;

  const DEFAULT_STRATEGY = `{
  "code": "TREND_CONFIRM",
  "name": "趋势与动量同周期确认",
  "operator": "all",
  "rules": [
    {
      "id": "trend",
      "label": "收盘站上五期均线",
      "source": "RESULT:CLOSE>MA(CLOSE,N);",
      "parameters": { "N": 5 }
    },
    {
      "id": "momentum",
      "label": "三期价格动量为正",
      "source": "RESULT:CLOSE>REF(CLOSE,M);",
      "parameters": { "M": 3 }
    }
  ]
}`;

  const DEFAULT_CLOUD_ROW = `{
  "$ZQDM": "110075",
  "$SC": "1",
  "MZ": 100,
  "ZGJ": 6.17,
  "$ZQDM1": "600029",
  "$SC1": "1",
  "XXCFBL": 85,
  "HSCFBL": 70,
  "QSCFBL": 130,
  "QXRQ": "20201015",
  "DQRQ": "20261015",
  "SYNX": 0.192,
  "SYNXSYL": 0.014769,
  "LLLXBZ": 3,
  "SYFXCS": 1,
  "SYFXLLXL": "0.0650",
  "SGFXRQ": "20251015",
  "FXPL1": 12,
  "XGFXRQ": "20261015"
}`;
  const CLOUD_TEMPLATE_STORAGE_KEY = 'tdx.cloud-calc.templates.v1';
  const TRADE_EVENT_PREVIEW_LIMIT = 100;

  let kind = $state('technical');
  let origin = $state('all');
  let query = $state('');
  let selectedCode = $state('');
  let calcMarket = $state('sz');
  let calcCode = $state('000001');
  let calcFormula = $state('macd');
  let calcFormulaKind = $state('technical');
  let calcPeriod = $state('day');
  let calcAdjustment = $state('none');
  let calcPages = $state('1');
  let optionName = $state('');
  let optionExpiry = $state('');
  let optionRiskFree = $state('0.0187');
  let scanCodes = $state('sz000001,sh600000');
  let scanLookback = $state('5');
  let pointInTimeFinance = $state(false);
  let calcMode = $state<'builtin' | 'custom'>('builtin');
  let customTask = $state<keyof typeof CUSTOM_SOURCES>('technical');
  let customFormulaSource = $state<string>(CUSTOM_SOURCES.technical);
  let customParameters = $state('');
  let allowFuture = $state(false);
  let futureReplay = $state(false);
  let futureReplayObservations = $state('20');
  let futureReplayEvents = $state('1000');
  let monitorSeconds = $state('60');
  let monitorActive = $state(false);
  let monitorLastRun = $state('');
  let monitorEvents = $state<MonitorEvent[]>([]);
  let monitorTimer: number | undefined;
  let monitorGeneration = 0;
  let monitorEventId = 0;
  let monitorSnapshot = $state<Map<string, string>>(new Map());
  let monitorSignature = '';
  let monitorHasSnapshot = false;
  let strategyManifest = $state(DEFAULT_STRATEGY);
  let strategyCodes = $state('sz000001,sh600000');
  let strategyPages = $state('2');
  let strategyLookback = $state('10');
  let strategyLastMode = $state<'scan' | 'backtest'>('scan');
  let contextStamp = $state('2026-08-08|15:00');
  let contextUseKline = $state(true);
  let contextInput = $state('');
  let contextOrigin = $state<'none' | 'template' | 'capture' | 'level2' | 'manual'>('none');
  let contextSchema = $state('');
  let captureInput = $state('');
  let captureAllowPartial = $state(false);
  let captureLocalError = $state('');
  let cloudCfg = $state('func_kzz_kzzsy101');
  let cloudAsOf = $state('20260808');
  let cloudQuotes = $state(true);
  let cloudRow = $state(DEFAULT_CLOUD_ROW);
  let cloudTemplateName = $state('南航转债收益表');
  let cloudTemplateId = $state('');
  let cloudTemplates = $state<CloudCalcTemplate[]>([]);
  let cloudTemplateNotice = $state('');
  let expandedTradePrimitives = $state<Record<string, boolean>>({});

  const formulas = new Resource<FormulaResult>();
  const calculation = new Resource<FormulaCalculationDocument>();
  const audit = new Resource<FormulaAuditDocument>();
  const coverage = new Resource<FormulaCoverage>();
  const backtest = new Resource<FormulaBacktestDocument>();
  const scan = new Resource<FormulaScanDocument>();
  const strategyScan = new Resource<FormulaStrategyScanDocument>();
  const strategyBacktest = new Resource<FormulaStrategyBacktestDocument>();
  const contextTemplate = new Resource<Record<string, unknown>>();
  const contextImport = new Resource<Record<string, unknown>>();
  const cloudAudit = new Resource<CloudCalcAuditDocument>();
  const cloudInputTemplate = new Resource<CloudCalcTemplateDocument>();
  const cloudEvaluation = new Resource<CloudCalcRunDocument>();

  const strategyBusy = $derived(strategyScan.busy || strategyBacktest.busy);
  const strategyError = $derived(strategyScan.error || strategyBacktest.error);

  const optionMarket = $derived(['4', '5', '6', '7', '67'].includes(calcMarket));
  const standardMarket = $derived(['sz', 'sh', 'bj'].includes(calcMarket));
  const workbenchBusy = $derived(
    calcMode === 'custom' && customTask === 'selection'
      ? scan.busy || audit.busy
      : calcMode === 'custom' && customTask === 'expert'
        ? backtest.busy || audit.busy
        : calculation.busy || audit.busy
  );
  const workbenchError = $derived(
    calcMode === 'custom' && customTask === 'selection'
      ? scan.error || audit.error
      : calcMode === 'custom' && customTask === 'expert'
        ? backtest.error || audit.error
        : calculation.error || audit.error
  );

  const rows = $derived(formulas.data?.formulas ?? []);
  const selected = $derived(rows.find((item) => item.code === selectedCode) ?? null);
  const lastPoint = $derived.by(() => {
    const points = calculation.data?.points ?? [];
    return points.length ? points[points.length - 1] : null;
  });
  const lastValues = $derived(
    lastPoint
      ? Object.entries(lastPoint.values).map(([label, value]) => ({ label, value }))
      : []
  );
  const tradeEventIr = $derived(calculation.data?.trade_event_ir ?? null);
  const replay = $derived(calculation.data?.future_replay ?? null);
  const replayEvents = $derived(replay?.events.slice(0, 100) ?? []);
  const tradeEventRawCandidateCount = $derived.by(() =>
    tradeEventIr?.primitives.reduce(
      (total, primitive) => total + primitive.historical_signal_candidates.length,
      0) ?? 0
  );
  const auditContextRows = $derived(
    audit.data?.formulas.filter((item) =>
      item.status === 'context_unavailable' ||
      item.status === 'explicit_context_unavailable' ||
      item.status === 'dependency_unavailable') ?? []
  );
  const auditApplicabilityRows = $derived(
    audit.data?.formulas.filter((item) =>
      item.status === 'market_inapplicable' ||
      item.status === 'period_inapplicable' ||
      item.status === 'future_read_only_disabled') ?? []
  );

  function auditStatusLabel(status: FormulaAuditRow['status']): string {
    if (status === 'market_inapplicable') return '当前市场不适用';
    if (status === 'period_inapplicable') return '当前周期不适用';
    if (status === 'future_read_only_disabled') return '未来函数只读未启用';
    if (status === 'dependency_unavailable') return '外部依赖不可用';
    if (status === 'context_unavailable') return '市场上下文不可用';
    if (status === 'explicit_context_unavailable') return '需要显式上下文';
    return status;
  }

  function auditMissingBindings(item: FormulaAuditRow): string[] {
    const exact = [
      ...(item.context_bindings_unavailable ?? []),
      ...(item.explicit_context_bindings_required ?? [])
    ];
    if (exact.length) return [...new Set(exact)];
    return [...new Set(item.context_bindings_required ?? [])];
  }

  function tradePrimitiveKey(statementIndex: number, functionName: string): string {
    return `${statementIndex}:${functionName}`;
  }

  function toggleTradePrimitive(key: string, event: Event) {
    const details = event.currentTarget as HTMLDetailsElement;
    expandedTradePrimitives = {
      ...expandedTradePrimitives,
      [key]: details.open
    };
  }

  function recentTradeEvents<T>(events: T[]): T[] {
    return events.slice(-TRADE_EVENT_PREVIEW_LIMIT);
  }
  const cloudBatch = $derived(
    cloudEvaluation.data?.schema === 'tdx-tbigdata-cloud-calc-batch-v1'
      ? cloudEvaluation.data : null
  );
  const cloudSingle = $derived(
    cloudEvaluation.data?.schema === 'tdx-tbigdata-cloud-calc-evaluation-v1'
      ? cloudEvaluation.data : null
  );
  const cloudResults = $derived.by((): CloudCalcDisplayRow[] => {
    const document = cloudEvaluation.data;
    if (!document) return [];
    if (document.schema === 'tdx-tbigdata-cloud-calc-evaluation-v1')
      return document.units.flatMap((unit) => unit.results.map((result) => ({
        ...result, row_index: 0, unit_id: unit.id
      })));
    return document.rows.flatMap((entry) => entry.result
      ? entry.result.units.flatMap((unit) => unit.results.map((result) => ({
          ...result, row_index: entry.row_index, unit_id: unit.id
        })))
      : []);
  });
  const cloudTemplateOptions = $derived([
    { id: '', label: '选择本地模板' },
    ...cloudTemplates.map((template) => ({ id: template.id, label: template.name }))
  ]);

  const cloudResultColumns: Column<CloudCalcDisplayRow>[] = [
    { key: 'row', label: '输入行', width: '72px', num: true, value: (row) => String(row.row_index + 1) },
    { key: 'code', label: '列代码', width: '120px', value: (row) => row.code },
    { key: 'name', label: '显示名', width: '160px', value: (row) => row.name || '—' },
    { key: 'status', label: '状态', width: '92px', value: (row) => row.status },
    {
      key: 'value', label: '结果', align: 'right', num: true,
      value: (row) => row.value === null || row.value === undefined
        ? '—' : typeof row.value === 'number' ? row.value.toFixed(6) : row.value
    },
    { key: 'calc', label: '原始 calc', value: (row) => row.calc },
    {
      key: 'detail', label: '缺失 / 错误',
      value: (row) => row.error || row.reason || row.missing?.join(', ') || '—'
    }
  ];

  const stats = $derived.by(() => {
    const doc = formulas.data;
    if (!doc) return [];
    return [
      { label: '命中公式', value: count(doc.match_count) },
      { label: '本次返回', value: count(doc.returned) },
      { label: '可恢复源码', value: count(doc.source_text_count) },
      { label: '用户公式', value: doc.user_library_enabled ? count(doc.user_formula_count) : '未启用' },
      { label: '直接执行', value: count(coverage.data?.executable) },
      { label: '含上下文', value: count(coverage.data?.executable_with_context) },
      { label: '可外部注入', value: count(coverage.data?.explicit_context_bindable) },
      { label: '解释器函数', value: count(coverage.data?.capabilities?.supported_function_count) },
      { label: '时序/统计核心', value: count(coverage.data?.capabilities?.custom_formula_sequence_statistics_function_count) },
      { label: '滚动/方差核心', value: count(coverage.data?.capabilities?.custom_formula_rolling_variance_function_count) },
      { label: '基准/累加核心', value: count(coverage.data?.capabilities?.custom_formula_benchmark_cumulative_function_count) },
      { label: '日历/信号核心', value: count(
        (coverage.data?.capabilities?.custom_formula_calendar_filter_function_count ?? 0) +
        (coverage.data?.capabilities?.custom_formula_calendar_filter_symbol_count ?? 0)
      ) },
      { label: '证券/字符串核心', value: count(
        (coverage.data?.capabilities?.custom_formula_security_string_function_count ?? 0) +
        (coverage.data?.capabilities?.custom_formula_security_string_symbol_count ?? 0)
      ) },
      { label: 'TCalc 注册证据', value: count(coverage.data?.capabilities?.tcalc_registry_evidence.static_registry_entry_count) },
      { label: '是否截断', value: doc.truncated ? '是' : '否' },
      { label: '检索词', value: doc.query ? doc.query : '（空）' }
    ];
  });

  /** 详情面板遍历渲染：Formula 带索引签名，字段随公式类型变化，不能写死。 */
  const detailEntries = $derived.by(() => {
    if (!selected) return [];
    return Object.entries(selected).map(([key, value]) => ({
      key,
      value:
        value !== null && typeof value === 'object'
          ? JSON.stringify(value)
          : text(value as unknown)
    }));
  });

  function load() {
    void formulas.load(
      `/api/v1/formulas?${queryString({ kind, origin, q: query.trim(), limit: 1000 })}`
    );
  }

  function loadCoverage() {
    void coverage.load('/api/v1/formulas/coverage');
  }

  function switchKind(next: string) {
    kind = next;
    selectedCode = '';
    load();
  }

  function switchOrigin(next: string) {
    origin = next;
    selectedCode = '';
    load();
  }

  function parseCustomParameters(): Record<string, number> {
    const result: Record<string, number> = {};
    for (const item of customParameters.split(/[;,\n]+/).map((value) => value.trim()).filter(Boolean)) {
      const separator = item.indexOf('=');
      if (separator <= 0 || separator === item.length - 1)
        throw new Error(`参数必须使用 NAME=NUMBER：${item}`);
      const name = item.slice(0, separator).trim();
      const value = Number(item.slice(separator + 1).trim());
      if (!/^[A-Za-z_][A-Za-z0-9_]{0,63}$/.test(name) || !Number.isFinite(value))
        throw new Error(`无效公式参数：${item}`);
      result[name] = value;
    }
    return result;
  }

  function parseOptionalFormulaContext(): Record<string, unknown> | undefined {
    if (!contextInput.trim()) return undefined;
    const parsed: unknown = JSON.parse(contextInput);
    if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed))
      throw new Error('显式上下文必须是 JSON 对象');
    return parsed as Record<string, unknown>;
  }

  function validCalculationSecurity(): boolean {
    const code = calcCode.trim();
    if (standardMarket) return /^\d{6}$/.test(code);
    return /^[A-Za-z0-9](?:[A-Za-z0-9 ]{0,7}[A-Za-z0-9])?$/.test(code);
  }

  function optionRequestFields(): Record<string, string> {
    if (!optionMarket) return {};
    return {
      option_name: optionName.trim(),
      expiry: optionExpiry.trim(),
      risk_free: optionRiskFree.trim()
    };
  }

  function switchCalcMarket(next: string) {
    calcMarket = next;
    if (!['sz', 'sh', 'bj'].includes(next)) calcAdjustment = 'none';
    const presets: Record<string, { code: string; formula: string; option?: string }> = {
      sz: { code: '000001', formula: 'MACD' },
      sh: { code: '600000', formula: 'MACD' },
      bj: { code: '920002', formula: 'MACD' },
      '31': { code: '00700', formula: 'SHORTVOL' },
      '29': { code: 'A2609', formula: 'CCL' },
      '47': { code: 'IFL9', formula: 'CCL' },
      '5': { code: 'A 8X06SH', formula: 'VOLATILITY', option: 'A2609-C-4400' },
      '7': { code: 'HO8W03UX', formula: 'VOLATILITY', option: 'HO2608-C-2500' }
    };
    const preset = presets[next];
    if (preset) {
      calcCode = preset.code;
      calcFormula = preset.formula;
      calcFormulaKind = 'technical';
      optionName = preset.option ?? '';
    } else {
      calcCode = '';
      calcFormula = ['4', '5', '6', '7', '67'].includes(next) ? 'VOLATILITY' : 'CCL';
      calcFormulaKind = 'technical';
      optionName = '';
    }
    optionExpiry = '';
    calculation.reset();
    audit.reset();
    backtest.reset();
  }

  function calculate() {
    if (!validCalculationSecurity()) return;
    expandedTradePrimitives = {};
    if (calcMode === 'custom') {
      void calculation.loadWith(async () => {
        const context = parseOptionalFormulaContext();
        return postJson<FormulaCalculationDocument>(
          '/api/v1/formulas/evaluate', {
          market: calcMarket,
          code: calcCode.trim(),
          formula: calcFormula.trim() || 'CUSTOM',
          source: customFormulaSource,
          parameters: parseCustomParameters(),
          ...(context ? { context } : {}),
          period: calcPeriod,
          adjust: standardMarket ? calcAdjustment : 'none',
          pages: Math.max(1, Math.min(20, Number(calcPages) || 1)),
          page_size: 800,
          date: 'all',
          point_in_time_finance: pointInTimeFinance,
          allow_future: allowFuture,
          ...futureReplayRequestFields(),
          ...optionRequestFields()
          }, 'formula-evaluate');
      });
      return;
    }
    const explicitFormula = selected &&
      selected.code.toLowerCase() === calcFormula.trim().toLowerCase() &&
      selected.kind_key === calcFormulaKind &&
      selected.analysis?.explicit_context_bindable;
    if (explicitFormula || contextOrigin === 'level2') {
      void calculation.loadWith(async () => {
        const context = parseOptionalFormulaContext();
        if (!context)
          throw new Error('请先生成模板或从 Level2 实验室导入显式上下文');
        return postJson<FormulaCalculationDocument>(
          '/api/v1/formulas/evaluate',
          {
            market: calcMarket,
            code: calcCode.trim(),
            formula: calcFormula,
            formula_kind: calcFormulaKind,
            parameters: parseCustomParameters(),
            context,
            period: calcPeriod,
            adjust: standardMarket ? calcAdjustment : 'none',
            pages: Math.max(1, Math.min(20, Number(calcPages) || 1)),
            page_size: 800,
            date: 'all',
            point_in_time_finance: pointInTimeFinance,
            allow_future: allowFuture,
            ...futureReplayRequestFields(),
            ...optionRequestFields()
          },
          'formula-evaluate'
        );
      });
      return;
    }
    void calculation.loadWith(async () => postJson<FormulaCalculationDocument>(
      '/api/v1/formulas/evaluate',
      {
        market: calcMarket,
        code: calcCode.trim(),
        formula: calcFormula,
        formula_kind: calcFormulaKind,
        parameters: parseCustomParameters(),
        period: calcPeriod,
        adjust: standardMarket ? calcAdjustment : 'none',
        pages: Math.max(1, Math.min(20, Number(calcPages) || 1)),
        page_size: 800,
        date: 'all',
        point_in_time_finance: pointInTimeFinance,
        allow_future: allowFuture,
        ...futureReplayRequestFields(),
        ...optionRequestFields()
      },
      'formula-evaluate'
    ));
  }

  function futureReplayRequestFields(): Record<string, unknown> {
    if (!allowFuture || !futureReplay) return {};
    const observations = Number(futureReplayObservations);
    const events = Number(futureReplayEvents);
    if (!Number.isSafeInteger(observations) || observations < 1 || observations > 64)
      throw new Error('重绘观测根数必须是 1..64 的整数');
    if (!Number.isSafeInteger(events) || events < 1 || events > 10000)
      throw new Error('重绘事件上限必须是 1..10000 的整数');
    return {
      future_replay: {
        max_observations: observations,
        max_events: events
      }
    };
  }

  function runAudit() {
    if (!validCalculationSecurity()) return;
    void audit.load(
      `/api/v1/formulas/audit?${queryString({
        market: calcMarket,
        code: calcCode.trim(),
        period: calcPeriod,
        adjust: standardMarket ? calcAdjustment : 'none',
        pages: Math.max(1, Math.min(20, Number(calcPages) || 1)),
        page_size: 800,
        date: 'all',
        with_context: 1,
        ...optionRequestFields()
      })}`
    );
  }

  function parseCloudRows(): { rows: Array<Record<string, unknown>>; batch: boolean } {
    const value: unknown = JSON.parse(cloudRow);
    const batch = Array.isArray(value);
    const rows = batch ? value : [value];
    if (!rows.length || rows.length > 128)
      throw new Error('TBigData 批量输入必须包含 1–128 行');
    if (rows.some((row) => !row || typeof row !== 'object' || Array.isArray(row)))
      throw new Error('TBigData 输入必须是扁平 JSON 对象或对象数组');
    if (rows.some((row) => Object.keys(row as Record<string, unknown>).length > 4096))
      throw new Error('单行字段数不能超过 4096');
    return { rows: rows as Array<Record<string, unknown>>, batch };
  }

  function loadCloudAudit() {
    const cfg = cloudCfg.trim();
    if (!/^[A-Za-z0-9_.-]{1,128}$/.test(cfg)) return;
    void cloudAudit.load(
      `/api/v1/formulas/cloud-calc?${queryString({ cfg })}`
    );
  }

  function generateCloudInputTemplate() {
    const cfg = cloudCfg.trim();
    if (!/^[A-Za-z0-9_.-]{1,128}$/.test(cfg)) return;
    void cloudInputTemplate.loadWith(async () => {
      const document = await getJson<CloudCalcTemplateDocument>(
        `/api/v1/formulas/cloud-calc/template?${queryString({ cfg })}`
      );
      let rows: Array<Record<string, unknown>> = [{}];
      let batch = false;
      try {
        const parsed = parseCloudRows();
        rows = parsed.rows;
        batch = parsed.batch;
      } catch {
        // A broken editor should still be recoverable from the generated skeleton.
      }
      const completed = rows.map((row) => ({ ...document.row_template, ...row }));
      cloudRow = JSON.stringify(batch ? completed : completed[0], null, 2);
      cloudTemplateNotice = `已补齐 ${document.counts.input_fields} 个原始输入字段；宿主字段和计算列无需手填`;
      cloudEvaluation.reset();
      return document;
    });
  }

  function runCloudCalc() {
    const cfg = cloudCfg.trim();
    if (!/^[A-Za-z0-9_.-]{1,128}$/.test(cfg)) return;
    void cloudEvaluation.loadWith(async () => {
      const asOf = Number(cloudAsOf);
      if (!/^\d{8}$/.test(cloudAsOf.trim()) || !Number.isInteger(asOf))
        throw new Error('评价日期必须是 YYYYMMDD');
      const parsed = parseCloudRows();
      const common = {
        cfg,
        quotes: cloudQuotes,
        as_of: asOf,
        timeout_ms: 10000,
        overrides: {}
      };
      return parsed.batch
        ? postJson<CloudCalcRunDocument>(
            '/api/v1/formulas/cloud-calc/batch',
            { ...common, rows: parsed.rows },
            'formula-cloud-calc-batch'
          )
        : postJson<CloudCalcRunDocument>(
            '/api/v1/formulas/cloud-calc',
            { ...common, row: parsed.rows[0] },
            'formula-cloud-calc'
          );
    });
  }

  function loadCloudTemplates() {
    try {
      const raw = localStorage.getItem(CLOUD_TEMPLATE_STORAGE_KEY);
      if (!raw) return;
      const parsed: unknown = JSON.parse(raw);
      if (!Array.isArray(parsed)) return;
      cloudTemplates = parsed.filter((item): item is CloudCalcTemplate => Boolean(
        item && typeof item === 'object' &&
        typeof item.id === 'string' && typeof item.name === 'string' &&
        typeof item.cfg === 'string' && typeof item.asOf === 'string' &&
        typeof item.quotes === 'boolean' && typeof item.row === 'string' &&
        typeof item.updatedAt === 'string'
      )).slice(0, 20);
    } catch {
      cloudTemplates = [];
    }
  }

  function persistCloudTemplates(next: CloudCalcTemplate[]) {
    try {
      localStorage.setItem(CLOUD_TEMPLATE_STORAGE_KEY, JSON.stringify(next));
    } catch {
      throw new Error('浏览器本地存储不可写，模板未保存');
    }
  }

  function saveCloudTemplate() {
    try {
      parseCloudRows();
      const cfg = cloudCfg.trim();
      if (!/^[A-Za-z0-9_.-]{1,128}$/.test(cfg))
        throw new Error('CFG 名称无效');
      const name = cloudTemplateName.trim() || cfg;
      const id = cloudTemplateId || `cloud-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;
      const template: CloudCalcTemplate = {
        id, name, cfg, asOf: cloudAsOf.trim(), quotes: cloudQuotes,
        row: cloudRow, updatedAt: new Date().toISOString()
      };
      const next = [template, ...cloudTemplates.filter((item) => item.id !== id)].slice(0, 20);
      persistCloudTemplates(next);
      cloudTemplates = next;
      cloudTemplateId = id;
      cloudTemplateNotice = `已保存“${name}”到当前浏览器`;
    } catch (error) {
      cloudTemplateNotice = error instanceof Error ? error.message : String(error);
    }
  }

  function applyCloudTemplate(id: string) {
    cloudTemplateId = id;
    const template = cloudTemplates.find((item) => item.id === id);
    if (!template) return;
    cloudTemplateName = template.name;
    cloudCfg = template.cfg;
    cloudAsOf = template.asOf;
    cloudQuotes = template.quotes;
    cloudRow = template.row;
    cloudEvaluation.reset();
  }

  function deleteCloudTemplate() {
    if (!cloudTemplateId) return;
    const next = cloudTemplates.filter((item) => item.id !== cloudTemplateId);
    persistCloudTemplates(next);
    cloudTemplates = next;
    cloudTemplateId = '';
    cloudTemplateNotice = '本地模板已删除';
  }

  function makeCloudBatchExample() {
    try {
      const parsed = parseCloudRows();
      if (parsed.batch) {
        cloudTemplateNotice = '当前输入已经是批量数组';
        return;
      }
      const second = { ...parsed.rows[0] };
      if (typeof second.MZ === 'number') second.MZ += 10;
      cloudRow = JSON.stringify([parsed.rows[0], second], null, 2);
      cloudEvaluation.reset();
      cloudTemplateNotice = '已生成两行示例；批量请求会共享去重后的行情计划';
    } catch (error) {
      cloudTemplateNotice = error instanceof Error ? error.message : String(error);
    }
  }

  function downloadCloudResult() {
    if (!cloudEvaluation.data) return;
    const blob = new Blob([JSON.stringify(cloudEvaluation.data, null, 2)], {
      type: 'application/json;charset=utf-8'
    });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    const safeCfg = cloudCfg.trim().replace(/[^A-Za-z0-9_.-]+/g, '_') || 'cloud-calc';
    link.href = url;
    link.download = `${safeCfg}-${cloudAsOf.trim() || 'result'}.json`;
    link.click();
    URL.revokeObjectURL(url);
  }

  function runBacktest() {
    if (!validCalculationSecurity() || !standardMarket) return;
    if (calcMode === 'custom') {
      void backtest.loadWith(async () => postJson<FormulaBacktestDocument>(
        '/api/v1/formulas/backtest',
        {
          market: calcMarket,
          code: calcCode.trim(),
          formula: calcFormula.trim() || 'CUSTOM_EXPERT',
          source: customFormulaSource,
          parameters: parseCustomParameters(),
          period: calcPeriod,
          adjust: calcAdjustment,
          pages: Math.max(1, Math.min(20, Number(calcPages) || 5)),
          page_size: 800,
          point_in_time_finance: pointInTimeFinance,
          initial_capital: 100000,
          commission_bps: 2.5,
          slippage_bps: 1
        },
        'formula-backtest'
      ));
      return;
    }
    if (calcFormulaKind !== 'expert') return;
    void backtest.load(
      `/api/v1/formulas/backtest?${queryString({
        market: calcMarket,
        code: calcCode.trim(),
        formula: calcFormula,
        formula_kind: 'expert',
        period: calcPeriod,
        adjust: calcAdjustment,
        pages: Math.max(1, Math.min(20, Number(calcPages) || 5)),
        page_size: 800,
        point_in_time_finance: pointInTimeFinance ? 1 : 0,
        ...parseCustomParameters()
      })}`
    );
  }

  function requestScan(): Promise<FormulaScanDocument> {
    if (!scanCodes.trim()) return Promise.reject(new Error('请输入至少一只扫描证券'));
    if (calcMode === 'custom') {
      if (customTask !== 'selection')
        return Promise.reject(new Error('自定义源码监控仅适用于条件扫描任务'));
      return postJson<FormulaScanDocument>(
        '/api/v1/formulas/scan',
        {
          formula: calcFormula.trim() || 'CUSTOM_SELECT',
          source: customFormulaSource,
          parameters: parseCustomParameters(),
          codes: scanCodes.trim(),
          period: calcPeriod,
          adjust: calcAdjustment,
          pages: Math.max(1, Math.min(20, Number(calcPages) || 1)),
          page_size: 800,
          workers: 4,
          lookback: Math.max(1, Math.min(10000, Number(scanLookback) || 1)),
          point_in_time_finance: pointInTimeFinance
        },
        'formula-scan'
      );
    }
    if (calcFormulaKind !== 'selection')
      return Promise.reject(new Error('请选择条件选股公式'));
    return getJson<FormulaScanDocument>(
      `/api/v1/formulas/scan?${queryString({
        formula: calcFormula,
        formula_kind: 'selection',
        codes: scanCodes.trim(),
        period: calcPeriod,
        adjust: calcAdjustment,
        pages: Math.max(1, Math.min(20, Number(calcPages) || 1)),
        page_size: 800,
        workers: 4,
        lookback: Math.max(1, Math.min(10000, Number(scanLookback) || 1)),
        point_in_time_finance: pointInTimeFinance ? 1 : 0,
        ...parseCustomParameters()
      })}`
    );
  }

  function runScan() {
    void scan.loadWith(requestScan);
  }

  function scanMonitorSignature(): string {
    return JSON.stringify({
      mode: calcMode,
      task: customTask,
      source: calcMode === 'custom' ? customFormulaSource : '',
      formula: calcFormula,
      formulaKind: calcFormulaKind,
      parameters: customParameters,
      codes: scanCodes.trim(),
      period: calcPeriod,
      pages: calcPages,
      lookback: scanLookback,
      adjustment: calcAdjustment,
      pointInTimeFinance
    });
  }

  function appendMonitorEvent(kind: MonitorEventKind, message: string, securityId?: string) {
    monitorEvents = [{
      id: ++monitorEventId,
      kind,
      at: new Date().toLocaleTimeString('zh-CN', { hour12: false }),
      securityId,
      message
    }, ...monitorEvents].slice(0, 200);
  }

  function matchDescription(match: FormulaScanMatch): string {
    const name = match.name || match.security_id;
    return `${name} · ${match.trigger_date} ${match.trigger_time}`;
  }

  function scheduleMonitor(generation: number) {
    if (!monitorActive || generation !== monitorGeneration) return;
    const seconds = Math.max(1, Math.min(86400, Number(monitorSeconds) || 60));
    monitorTimer = window.setTimeout(() => void monitorTick(generation), seconds * 1000);
  }

  async function monitorTick(generation: number) {
    if (!monitorActive || generation !== monitorGeneration) return;
    const signature = scanMonitorSignature();
    if (monitorSignature && monitorSignature !== signature) {
      monitorSnapshot = new Map();
      monitorHasSnapshot = false;
      appendMonitorEvent('snapshot', '扫描条件已变化，重新建立策略池快照');
    }
    monitorSignature = signature;
    const document = await scan.loadWith(requestScan, { silent: monitorHasSnapshot });
    if (!monitorActive || generation !== monitorGeneration) return;
    monitorLastRun = new Date().toLocaleString('zh-CN', { hour12: false });
    if (!document) {
      appendMonitorEvent('error', scan.error || '扫描请求失败；上一轮成员保持不变');
      scheduleMonitor(generation);
      return;
    }
    const complete = document.requested_count > 0 &&
      document.evaluated === document.requested_count &&
      document.error_count === 0 && document.fetch_error_count === 0;
    if (!complete) {
      appendMonitorEvent(
        'degraded',
        `本轮不完整（完成 ${document.evaluated}/${document.requested_count}，错误 ${document.error_count + document.fetch_error_count}），策略池保持不变`
      );
      scheduleMonitor(generation);
      return;
    }
    const current = new Map<string, string>();
    const byId = new Map<string, FormulaScanMatch>();
    for (const match of document.matches) {
      current.set(match.security_id, JSON.stringify(match));
      byId.set(match.security_id, match);
    }
    if (!monitorHasSnapshot) {
      appendMonitorEvent('snapshot', `建立策略池快照：${document.matches.length} 只证券`);
    } else {
      for (const [securityId, payload] of current) {
        const match = byId.get(securityId)!;
        if (!monitorSnapshot.has(securityId))
          appendMonitorEvent('entered', `进入：${matchDescription(match)}`, securityId);
        else if (monitorSnapshot.get(securityId) !== payload)
          appendMonitorEvent('updated', `信号更新：${matchDescription(match)}`, securityId);
      }
      for (const securityId of monitorSnapshot.keys()) {
        if (!current.has(securityId))
          appendMonitorEvent('exited', `退出：${securityId}`, securityId);
      }
    }
    monitorSnapshot = current;
    monitorHasSnapshot = true;
    scheduleMonitor(generation);
  }

  function startScanMonitor() {
    if (!scanCodes.trim()) return;
    stopScanMonitor();
    monitorEvents = [];
    monitorSnapshot = new Map();
    monitorSignature = '';
    monitorHasSnapshot = false;
    monitorActive = true;
    const generation = ++monitorGeneration;
    void monitorTick(generation);
  }

  function stopScanMonitor() {
    monitorActive = false;
    ++monitorGeneration;
    if (monitorTimer !== undefined) window.clearTimeout(monitorTimer);
    monitorTimer = undefined;
  }

  function toggleScanMonitor() {
    if (monitorActive) stopScanMonitor();
    else startScanMonitor();
  }

  function runPrimaryAction() {
    if (calcMode === 'custom' && customTask === 'selection') {
      runScan();
      return;
    }
    if (calcMode === 'custom' && customTask === 'expert') {
      runBacktest();
      return;
    }
    calculate();
  }

  function selectFormula(row: Formula) {
    selectedCode = row.code;
    contextTemplate.reset();
    if (contextOrigin !== 'level2') {
      contextInput = '';
      contextOrigin = 'none';
      contextSchema = '';
      captureInput = '';
      captureAllowPartial = false;
      captureLocalError = '';
      contextImport.reset();
    }
    if (row.analysis?.executable_with_context || row.analysis?.explicit_context_bindable ||
        row.analysis?.read_only_future_executable) {
      calcMode = 'builtin';
      calcFormula = row.code;
      calcFormulaKind = row.kind_key;
    }
  }

  function loadContextTemplate() {
    if (!selected) return;
    void contextTemplate.loadWith(async () => {
      const params: Record<string, string | number> = { formula: selected.code };
      if (contextUseKline) {
        Object.assign(params, {
          market: calcMarket,
          code: calcCode.trim(),
          period: calcPeriod,
          pages: calcPages,
          page_size: 800
        });
      } else {
        params.stamp = contextStamp.trim();
      }
      const document = await getJson<Record<string, unknown>>(
        `/api/v1/formulas/context-template?${queryString(params)}`
      );
      contextInput = JSON.stringify(document, null, 2);
      contextOrigin = 'template';
      contextSchema = typeof document.schema === 'string' ? document.schema : '';
      captureInput = '';
      captureAllowPartial = false;
      captureLocalError = '';
      contextImport.reset();
      return document;
    });
  }

  function createCaptureScaffold() {
    try {
      const template: unknown = JSON.parse(contextInput);
      if (!template || typeof template !== 'object' || Array.isArray(template))
        throw new Error('请先生成有效的显式上下文模板');
      const root = template as Record<string, unknown>;
      if (root.schema !== 'tdx-formula-explicit-context-v1')
        throw new Error('捕获填写表只能从显式上下文模板生成');
      const scalarSource = root.formula_scalar_bindings;
      const seriesSource = root.series;
      if (!scalarSource || typeof scalarSource !== 'object' || Array.isArray(scalarSource) ||
          !seriesSource || typeof seriesSource !== 'object' || Array.isArray(seriesSource))
        throw new Error('模板缺少 scalar/series 占位');
      const scalarBindings = Object.fromEntries(
        Object.keys(scalarSource as Record<string, unknown>).map((name) => [name, null])
      );
      const series = seriesSource as Record<string, unknown>;
      const stamps = new Set<string>();
      for (const values of Object.values(series)) {
        if (!values || typeof values !== 'object' || Array.isArray(values))
          throw new Error('模板 series 占位必须是时间戳对象');
        for (const stamp of Object.keys(values as Record<string, unknown>)) stamps.add(stamp);
      }
      const names = Object.keys(series);
      const records = [...stamps].sort().map((stamp) => ({
        stamp,
        values: Object.fromEntries(names.map((name) => [name, null]))
      }));
      captureInput = JSON.stringify({
        schema: 'tdx-formula-caller-context-capture-v1',
        capture_id: `caller-capture-${new Date().toISOString().slice(0, 10)}`,
        ownership_confirmed: false,
        formula_scalar_bindings: scalarBindings,
        records
      }, null, 2);
      captureLocalError = '';
      contextImport.reset();
    } catch (error) {
      captureLocalError = error instanceof Error ? error.message : String(error);
    }
  }

  function importCallerCapture() {
    void contextImport.loadWith(async () => {
      const template: unknown = JSON.parse(contextInput);
      const submittedCapture = captureInput;
      const capture: unknown = JSON.parse(submittedCapture);
      if (!template || typeof template !== 'object' || Array.isArray(template) ||
          !capture || typeof capture !== 'object' || Array.isArray(capture))
        throw new Error('模板和捕获都必须是 JSON 对象');
      const document = await postJson<Record<string, unknown>>(
        '/api/v1/formulas/context-import',
        { template, capture, allow_partial: captureAllowPartial },
        'formula-context-import'
      );
      contextInput = JSON.stringify(document, null, 2);
      contextOrigin = 'capture';
      contextSchema = typeof document.schema === 'string' ? document.schema : '';
      captureLocalError = '';
      if (captureInput === submittedCapture) captureInput = '';
      return document;
    });
  }

  function clearFormulaContext() {
    contextInput = '';
    contextOrigin = 'none';
    contextSchema = '';
    captureInput = '';
    captureAllowPartial = false;
    captureLocalError = '';
    contextImport.reset();
  }

  function startManualFormulaContext() {
    if (!contextInput.trim()) {
      const document = {
        schema: 'tdx-formula-explicit-context-v1',
        automatic_market_context: false,
        formula_scalar_bindings: {},
        series: {}
      };
      contextInput = JSON.stringify(document, null, 2);
    }
    contextOrigin = 'manual';
    contextSchema = 'tdx-formula-explicit-context-v1';
  }

  function switchCalcMode(next: string) {
    stopScanMonitor();
    calcMode = next === 'custom' ? 'custom' : 'builtin';
    if (calcMode === 'custom' && calcFormula.toLowerCase() === 'macd') calcFormula = 'CUSTOM';
    calculation.reset();
    backtest.reset();
    scan.reset();
  }

  function switchCustomTask(next: string) {
    stopScanMonitor();
    const task: keyof typeof CUSTOM_SOURCES = next === 'selection'
      ? 'selection'
      : next === 'expert'
        ? 'expert'
        : 'technical';
    if (customFormulaSource === CUSTOM_SOURCES[customTask])
      customFormulaSource = CUSTOM_SOURCES[task];
    customTask = task;
    calcFormula = task === 'selection'
      ? 'CUSTOM_SELECT'
      : task === 'expert'
        ? 'CUSTOM_EXPERT'
        : 'CUSTOM';
    customParameters = task === 'technical' ? '' : task === 'selection' ? 'N=5' : 'N=10';
    calculation.reset();
    backtest.reset();
    scan.reset();
  }

  function parsedStrategy(): Record<string, unknown> {
    const parsed: unknown = JSON.parse(strategyManifest);
    if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed))
      throw new Error('策略清单必须是 JSON 对象');
    return parsed as Record<string, unknown>;
  }

  function strategyRequestBody(backtestMode: boolean) {
    const body: Record<string, unknown> = {
      strategy: parsedStrategy(),
      codes: strategyCodes.trim(),
      period: calcPeriod,
      adjust: calcAdjustment,
      pages: Math.max(1, Math.min(20, Number(strategyPages) || (backtestMode ? 5 : 1))),
      page_size: 800,
      workers: 4,
      point_in_time_finance: pointInTimeFinance
    };
    if (backtestMode) {
      body.initial_capital = 100000;
      body.commission_bps = 2.5;
      body.slippage_bps = 1;
    } else {
      body.lookback = Math.max(1, Math.min(10000, Number(strategyLookback) || 1));
    }
    return body;
  }

  function runStrategyScan() {
    strategyLastMode = 'scan';
    strategyBacktest.reset();
    void strategyScan.loadWith(async () => {
      if (!strategyCodes.trim()) throw new Error('请输入至少一只策略证券');
      return postJson<FormulaStrategyScanDocument>(
        '/api/v1/formulas/strategy/scan',
        strategyRequestBody(false),
        'formula-strategy-scan'
      );
    });
  }

  function runStrategyBacktest() {
    strategyLastMode = 'backtest';
    strategyScan.reset();
    void strategyBacktest.loadWith(async () => {
      if (!strategyCodes.trim()) throw new Error('请输入至少一只策略证券');
      return postJson<FormulaStrategyBacktestDocument>(
        '/api/v1/formulas/strategy/backtest',
        strategyRequestBody(true),
        'formula-strategy-backtest'
      );
    });
  }

  function retryStrategy() {
    if (strategyLastMode === 'backtest') runStrategyBacktest();
    else runStrategyScan();
  }

  const columns: Column<Formula>[] = [
    { key: 'code', label: '代码', width: '80px', num: true, value: (row) => row.code, sortValue: (row) => row.code },
    { key: 'name', label: '公式名', width: '140px', value: (row) => row.name, sortValue: (row) => row.name },
    { key: 'category', label: '分类', width: '150px', value: (row) => text(row.category_name) },
    { key: 'kind', label: '类型', width: '110px', value: (row) => text(row.kind_name) },
    {
      key: 'source', label: '来源', width: '100px',
      value: (row) => row.source_text_origin === 'user-file-decrypted'
        ? '用户 PriGS' : '系统 TCalc'
    },
    {
      key: 'status',
      label: '执行能力',
      width: '100px',
      value: (row) => row.analysis?.executable
        ? '直接执行'
        : row.analysis?.executable_with_context
          ? '自动补数据'
          : row.analysis?.explicit_context_bindable
            ? '需外部序列'
          : row.analysis?.has_future_function
            ? '未来函数'
            : '待补依赖'
    },
    { key: 'kindKey', label: 'kind_key', value: (row) => text(row.kind_key) }
  ];

  const strategyAttributionColumns: Column<FormulaStrategyAttribution>[] = [
    {
      key: 'security', label: '证券', width: '150px',
      value: (row) => row.name || row.security_id,
      sub: (row) => row.security_id,
      sortValue: (row) => row.security_id
    },
    {
      key: 'net', label: '净贡献', width: '110px', align: 'right', num: true,
      value: (row) => `${row.net_contribution.toFixed(2)}`,
      sub: (row) => `${row.net_contribution_pct.toFixed(3)}%`,
      tone: (row) => row.net_contribution > 0 ? 'up' : row.net_contribution < 0 ? 'down' : 'flat',
      sortValue: (row) => row.net_contribution
    },
    {
      key: 'gross', label: '毛贡献', width: '100px', align: 'right', num: true,
      value: (row) => row.gross_contribution.toFixed(2),
      sortValue: (row) => row.gross_contribution
    },
    {
      key: 'cost', label: '分摊成本', width: '100px', align: 'right', num: true,
      value: (row) => row.allocated_cost.toFixed(2),
      sortValue: (row) => row.allocated_cost
    },
    {
      key: 'activity', label: '持有 / 进出', width: '120px', align: 'right', num: true,
      value: (row) => `${count(row.active_intervals)} / ${count(row.entries)}·${count(row.exits)}`,
      sortValue: (row) => row.active_intervals
    }
  ];

  onMount(() => {
    const staged = takeStagedLevel2FormulaContext();
    if (staged) {
      contextInput = JSON.stringify(staged.document, null, 2);
      contextOrigin = 'level2';
      contextSchema = staged.schema;
      if (staged.schema === 'tdx-level2-tcalc-order-flow-v1')
        calcPeriod = 'day';
    }
    load();
    loadCoverage();
    loadCloudTemplates();
  });

  onDestroy(stopScanMonitor);
</script>

<PageHeader
  eyebrow="TCALC · SOURCE INTERPRETER"
  title="公式库"
  description="恢复公式源码、分析依赖并由纯 C++ 直接执行；服务可通过 --include-user-formulas 显式只读加载 PriGS 用户公式，券商私有序列仍只接受显式上下文注入。"
  {stats}
>
  {#snippet actions()}
    <Button icon="refresh" busy={formulas.busy} onclick={load}>重新检索</Button>
  {/snippet}
</PageHeader>

<Panel
  eyebrow="NATIVE C++ · /api/v1/formulas/evaluate"
  title={calcMode === 'custom'
    ? customTask === 'selection'
      ? '自定义条件扫描'
      : customTask === 'expert'
        ? '自定义专家回测'
        : '自定义公式解释'
    : '公式解释与专家回测'}
  subtitle={calcMode === 'custom'
    ? '源码由纯 C++ 解释器在本次请求内执行，不落盘、不转发 Python'
    : '列表中标记可执行的技术指标、条件选股、专家系统和五彩 K 线均可运行'}
  busy={workbenchBusy}
  error={workbenchError}
  onRetry={runPrimaryAction}
>
  {#snippet toolbar()}
    <Segmented
      options={[{ id: 'builtin', label: '内置公式' }, { id: 'custom', label: '自定义源码' }]}
      value={calcMode}
      onChange={switchCalcMode}
      ariaLabel="公式来源"
    />
    {#if calcMode === 'custom'}
      <Segmented
        options={[
          { id: 'technical', label: '单票计算' },
          { id: 'selection', label: '条件扫描' },
          { id: 'expert', label: '专家回测' }
        ]}
        value={customTask}
        onChange={switchCustomTask}
        ariaLabel="自定义公式任务"
      />
    {/if}
    <Select
      bind:value={calcMarket}
      options={CALC_MARKETS}
      onChange={switchCalcMarket}
      width="156px"
    />
    <TextInput bind:value={calcCode} width="126px" label="证券代码" placeholder={standardMarket ? '000001' : '1..9 位线码'} onEnter={runPrimaryAction} />
    <TextInput bind:value={calcFormula} width="110px" label="公式代码" placeholder="MACD" onEnter={runPrimaryAction} />
    <Select
      bind:value={calcPeriod}
      options={[
        { id: '1m', label: '1 分钟' }, { id: '5m', label: '5 分钟' },
        { id: '15m', label: '15 分钟' }, { id: '30m', label: '30 分钟' },
        { id: '60m', label: '60 分钟' }, { id: 'day', label: '日线' },
        { id: 'week', label: '周线' }, { id: 'month', label: '月线' }
      ]}
      width="110px"
    />
    {#if standardMarket}
      <Select
        bind:value={calcAdjustment}
        options={[
          { id: 'none', label: '不复权' },
          { id: 'qfq', label: '前复权' },
          { id: 'hfq', label: '后复权' }
        ]}
        width="102px"
      />
    {/if}
    <TextInput bind:value={calcPages} width="76px" label="历史页数" placeholder="1" onEnter={runPrimaryAction} />
    {#if calcMode === 'builtin'}
      <TextInput
        bind:value={customParameters}
        width="190px"
        label="参数覆盖"
        placeholder="N=20,M=2"
        onEnter={runPrimaryAction}
      />
    {/if}
    {#if optionMarket}
      <TextInput bind:value={optionName} width="168px" label="期权显示名" placeholder="A2609-C-4400" onEnter={runPrimaryAction} />
      <TextInput bind:value={optionExpiry} width="132px" label="到期日（可空）" placeholder="自动解析" onEnter={runPrimaryAction} />
      <TextInput bind:value={optionRiskFree} width="104px" label="无风险利率" placeholder="0.0187" onEnter={runPrimaryAction} />
    {/if}
    <Button icon="search" busy={audit.busy} onclick={runAudit}>全库审计</Button>
    <label
      class="strict-finance-toggle"
      class:active={pointInTimeFinance}
      title="财务字段按已归档的正式报告实际披露日生效；缺历史时明确报错，不回退当前快照"
    >
      <input type="checkbox" bind:checked={pointInTimeFinance} />
      <span>严格财务时点</span>
    </label>
    {#if calcMode === 'builtin' || customTask === 'technical'}
      <label
        class="strict-finance-toggle"
        class:active={allowFuture}
        title="未来函数只允许生成只读图形结果，不能进入扫描或回测"
      >
        <input type="checkbox" bind:checked={allowFuture} />
        <span>未来函数只读</span>
      </label>
      {#if allowFuture}
        <label
          class="strict-finance-toggle"
          class:active={futureReplay}
          title="逐步揭示最近 K 线前缀，只记录后来对既有历史点造成的改写"
        >
          <input type="checkbox" bind:checked={futureReplay} />
          <span>历史重绘时间线</span>
        </label>
        {#if futureReplay}
          <TextInput bind:value={futureReplayObservations} width="86px" label="观测根数" placeholder="20" onEnter={calculate} />
          <TextInput bind:value={futureReplayEvents} width="92px" label="事件上限" placeholder="1000" onEnter={calculate} />
        {/if}
      {/if}
    {/if}
    {#if calcMode === 'builtin' || customTask === 'technical'}
      <Button icon="market" busy={calculation.busy} onclick={calculate}>{calcMode === 'custom' ? '运行源码' : '计算'}</Button>
    {/if}
    {#if calcMode === 'custom' && customTask === 'expert' && standardMarket}
      <Button icon="clock" busy={backtest.busy} onclick={runBacktest}>回测源码</Button>
    {/if}
    {#if calcMode === 'custom' && customTask === 'selection'}
      <TextInput bind:value={scanCodes} width="210px" label="扫描证券" placeholder="sz000001,sh600000" onEnter={runScan} />
      <TextInput bind:value={scanLookback} width="70px" label="近 N 根" placeholder="5" onEnter={runScan} />
      <Button icon="search" busy={scan.busy} onclick={runScan}>扫描源码</Button>
    {/if}
    {#if calcMode === 'builtin' && calcFormulaKind === 'expert' && standardMarket}
      <Button icon="clock" busy={backtest.busy} onclick={runBacktest}>回测</Button>
    {/if}
    {#if calcMode === 'builtin' && calcFormulaKind === 'selection'}
      <TextInput bind:value={scanCodes} width="210px" label="扫描证券" placeholder="sz000001,sh600000" onEnter={runScan} />
      <TextInput bind:value={scanLookback} width="70px" label="近 N 根" placeholder="5" onEnter={runScan} />
      <Button icon="search" busy={scan.busy} onclick={runScan}>扫描</Button>
    {/if}
    {#if (calcMode === 'custom' && customTask === 'selection') || (calcMode === 'builtin' && calcFormulaKind === 'selection')}
      <TextInput bind:value={monitorSeconds} icon="clock" width="82px" label="监控间隔秒" placeholder="60 秒" />
      <Button icon={monitorActive ? 'close' : 'alert'} onclick={toggleScanMonitor}>
        {monitorActive ? '停止监控' : '会话监控'}
      </Button>
    {/if}
  {/snippet}

  {#if calcMode === 'custom'}
    <div class="source-editor">
      <label>
        <span>通达信公式源码</span>
        <textarea
          bind:value={customFormulaSource}
          rows="6"
          spellcheck="false"
          maxlength="16384"
          onkeydown={(event: KeyboardEvent) => {
            if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
              event.preventDefault();
              runPrimaryAction();
            }
          }}
        ></textarea>
      </label>
      <TextInput
        bind:value={customParameters}
        width="240px"
        label="参数覆盖"
        placeholder="N=20,M=2"
        onEnter={runPrimaryAction}
      />
      <p>Ctrl+Enter 执行当前任务。源码只在本次请求内存中执行，不写入通达信目录或本地文件。</p>
    </div>
    {#if customTask === 'technical' && contextOrigin !== 'level2'}
      <div class="external-context">
        <div>
          <strong>调用方显式上下文（可选）</strong>
          <span>只读账户/策略状态与私有序列</span>
          <p>可用 <code>formula_scalar_bindings</code> 提供 FREEMONEY、TOTALPOSITION 或模板列出的宿主 raw 标量，用 <code>series</code> 提供精确 DATE|TIME 序列；模板只给空占位，不推导这些值，也不会登录账户或执行 ORDERBUY/ORDERSELL/CLOSEALLD/CLOSEALLK。</p>
        </div>
        {#if contextInput}
          <Button icon="close" onclick={clearFormulaContext}>清除上下文</Button>
          <textarea bind:value={contextInput} rows="8" spellcheck="false" aria-label="自定义公式显式上下文 JSON"></textarea>
        {:else}
          <Button icon="market" onclick={startManualFormulaContext}>填写显式上下文</Button>
        {/if}
      </div>
    {/if}
  {/if}

  {#if contextOrigin === 'level2'}
    <div class="external-context">
      <div>
        <strong>已导入 Level2 显式上下文</strong>
        <span>{contextSchema}</span>
        <p>type-31 会按目标日 K 的真实时间戳物化；type-104 提供 ISBUYORDER 标量。上下文只随单票 evaluate 请求发送，不用于扫描或回测。</p>
      </div>
      <Button icon="close" onclick={clearFormulaContext}>清除上下文</Button>
      <textarea bind:value={contextInput} rows="8" spellcheck="false" aria-label="已导入 Level2 公式上下文 JSON"></textarea>
    </div>
  {/if}

  {#if !standardMarket && ((calcMode === 'custom' && customTask === 'expert') || (calcMode === 'builtin' && calcFormulaKind === 'expert'))}
    <div class="calc-empty compact">扩展市场当前开放公式计算与全库审计；历史回测仍只接受深沪京证券。</div>
  {/if}

  {#if calculation.data}
    <div class="calc-meta">
      <span>{calculation.data.market.toUpperCase()}{calculation.data.code}</span>
      <span>{calculation.data.period}</span>
      <span>{calculation.data.adjustment_mode === 'qfq'
        ? '前复权'
        : calculation.data.adjustment_mode === 'hfq'
          ? '后复权'
          : '不复权'}</span>
      <span>{count(calculation.data.count)} 个点</span>
      <span>{calculation.data.engine}</span>
      {#if calculation.data.render_ir}
        <strong>绘图 IR · {count(calculation.data.render_ir.primitive_count)} 图元 / {count(calculation.data.render_ir.event_count)} 事件</strong>
      {/if}
      {#if calculation.data.formula_source_mode === 'inline-post'}
        <strong>自定义源码 · {count(calculation.data.source_bytes ?? 0)} B</strong>
      {:else if calculation.data.formula_source_mode === 'library-post'}
        <strong>内置公式 · 显式上下文</strong>
      {/if}
      {#if calculation.data.context_metadata?.finance_point_in_time_mode}
        <strong>严格财务时点</strong>
      {/if}
      <span>{lastPoint ? `${lastPoint.date} ${lastPoint.time}` : '—'}</span>
      {#each lastValues as item (item.label)}
        <strong>{item.label} {item.value === null ? '—' : Number(item.value).toFixed(4)}</strong>
      {/each}
    </div>
    <FormulaChart document={calculation.data} />
    {#if replay}
      <section class="future-replay" aria-label="未来函数历史重绘时间线">
        <div class="future-replay-summary">
          <strong>历史重绘时间线</strong>
          <span>{replay.future_functions.join(', ')}</span>
          <span>基线 {count(replay.baseline_bar_count)} 根</span>
          <span>观测 {count(replay.observation_count)} 次</span>
          <span>改写 {count(replay.changed_target_count)} 个历史点 / {count(replay.event_count)} 个值</span>
          {#if replay.events_truncated}<em>仅保留前 {count(replay.stored_event_count)} 个事件</em>{/if}
        </div>
        <p>每次只向解释器揭示到当时为止的 K 线；新追加的当前柱不计为“重绘”。这是显式只读观察，不进入扫描、回测、账户或委托。</p>
        {#if replayEvents.length}
          <div class="future-replay-events">
            {#each replayEvents as event (`${event.observed_at.date}|${event.observed_at.time}|${event.target_index}|${event.output}`)}
              <div>
                <time>{event.observed_at.date} {event.observed_at.time}</time>
                <strong>{event.output}</strong>
                <span>回写 {event.target.date} {event.target.time}（早 {count(event.age_bars)} 根）</span>
                <code>{event.previous_value === null ? '—' : event.previous_value} → {event.current_value === null ? '—' : event.current_value}</code>
              </div>
            {/each}
          </div>
          {#if replay.stored_event_count > replayEvents.length}
            <p>网页仅展示前 100 个已保留事件；完整数量仍在响应摘要中。</p>
          {/if}
        {:else}
          <p>当前观测窗口内没有发现对既有历史输出点的改写。</p>
        {/if}
      </section>
    {/if}
    {#if tradeEventIr && tradeEventIr.primitive_count > 0}
      <section class="trade-event-ir" aria-label="交易信号离线 IR">
        <div class="trade-event-summary">
          <strong>交易信号离线 IR</strong>
          <span>原始候选 {count(tradeEventRawCandidateCount)}</span>
          <span>AUTOFILTER {tradeEventIr.autofilter.enabled ? '已启用' : '未启用'}</span>
          {#if tradeEventIr.autofilter.enabled}
            <span>接受 {count(tradeEventIr.autofilter.accepted_candidate_count)}</span>
            <span>过滤 {count(tradeEventIr.autofilter.filtered_out_candidate_count)}</span>
            <span>最终持仓 {tradeEventIr.autofilter.final_position}</span>
          {/if}
          <em>只读投影，不访问账户、不下单</em>
        </div>
        {#if tradeEventIr.autofilter.enabled && tradeEventIr.autofilter.decision_trace}
          <FormulaAutofilterTrace
            decisions={tradeEventIr.autofilter.decision_trace}
            positionChanges={tradeEventIr.autofilter.position_change_count ?? 0}
          />
        {/if}
        {#each tradeEventIr.primitives as primitive (tradePrimitiveKey(primitive.statement_index, primitive.function))}
          {@const primitiveKey = tradePrimitiveKey(primitive.statement_index, primitive.function)}
          <details class="trade-event-primitive" ontoggle={(event) => toggleTradePrimitive(primitiveKey, event)}>
            <summary>
              <strong>{primitive.function}</strong>
              <span>原始 {count(primitive.historical_signal_candidates.length)}</span>
              {#if tradeEventIr.autofilter.enabled}
                <span>过滤后 {count(primitive.filtered_historical_signal_candidates?.length ?? 0)}</span>
                <span>拒绝 {count(primitive.autofilter_filtered_out_candidate_count ?? 0)}</span>
              {/if}
              <span>动作位 0x{primitive.host_action_bits.toString(16)}</span>
            </summary>
            {#if expandedTradePrimitives[primitiveKey]}
              <div class="trade-event-detail">
                {#if tradeEventIr.autofilter.enabled}
                  {@const filteredCandidates = primitive.filtered_historical_signal_candidates ?? []}
                  <div>
                    <strong>过滤后候选 · 最近 {count(Math.min(filteredCandidates.length, TRADE_EVENT_PREVIEW_LIMIT))}/{count(filteredCandidates.length)}</strong>
                    {#if filteredCandidates.length > TRADE_EVENT_PREVIEW_LIMIT}
                      <span class="trade-event-truncated">已省略更早 {count(filteredCandidates.length - TRADE_EVENT_PREVIEW_LIMIT)} 条</span>
                    {/if}
                    <pre>{JSON.stringify(recentTradeEvents(filteredCandidates), null, 2)}</pre>
                  </div>
                {/if}
                <div>
                  <strong>原始候选 · 最近 {count(Math.min(primitive.historical_signal_candidates.length, TRADE_EVENT_PREVIEW_LIMIT))}/{count(primitive.historical_signal_candidates.length)}</strong>
                  {#if primitive.historical_signal_candidates.length > TRADE_EVENT_PREVIEW_LIMIT}
                    <span class="trade-event-truncated">已省略更早 {count(primitive.historical_signal_candidates.length - TRADE_EVENT_PREVIEW_LIMIT)} 条</span>
                  {/if}
                  <pre>{JSON.stringify(recentTradeEvents(primitive.historical_signal_candidates), null, 2)}</pre>
                </div>
                {#if tradeEventIr.autofilter.enabled}
                  <div>
                    <strong>过滤后最新动作</strong>
                    <pre>{JSON.stringify(primitive.filtered_latest_host_action ?? null, null, 2)}</pre>
                  </div>
                {:else}
                  <p>AUTOFILTER 未启用；这里保留原始离线候选，不代表候选被过滤。</p>
                {/if}
              </div>
            {/if}
          </details>
        {/each}
      </section>
    {/if}
  {:else if (calcMode === 'builtin' || customTask === 'technical') && !calculation.busy && !calculation.error}
    <div class="calc-empty">选择证券、周期和指标后开始计算；增加历史页数可扩展分钟线预热区间。</div>
  {/if}

  {#if audit.data}
    <div class="backtest-meta audit-meta">
      <strong>全库审计 · {audit.data.market}:{audit.data.code}</strong>
      <span>{count(audit.data.bar_count)} 根 {audit.data.period} K 线</span>
      <span>通过 {count(audit.data.audit.passed)} / {count(audit.data.audit.library_total)}</span>
      <span>市场不适用 {count(audit.data.audit.market_inapplicable)}</span>
      <span>上下文缺失 {count(audit.data.audit.context_unavailable)}</span>
      <span>周期不适用 {count(audit.data.audit.period_inapplicable)}</span>
      <strong class:error={audit.data.audit.errors > 0}>运行错误 {count(audit.data.audit.errors)}</strong>
      <span>{audit.data.audit_mode}</span>
    </div>
    {#if audit.data.audit.errors > 0}
      <div class="audit-errors">
        {#each audit.data.formulas.filter((item) => item.status === 'error').slice(0, 8) as item (item.code)}
          <span><strong>{item.code}</strong> · {item.error || '未知解释器错误'}</span>
        {/each}
      </div>
    {/if}
    {#if auditContextRows.length > 0}
      <details class="audit-gaps">
        <summary>
          <strong>显式数据缺口 {count(auditContextRows.length)}</strong>
          <span>展开查看不可伪造的依赖与序列键</span>
        </summary>
        <div class="audit-gap-list">
          {#each auditContextRows as item (`${item.kind_key}:${item.code}`)}
            {@const bindings = auditMissingBindings(item)}
            <article>
              <div>
                <strong>{item.code}</strong>
                <span>{item.name}</span>
                <em>{auditStatusLabel(item.status)}</em>
              </div>
              {#if item.external_dependencies?.length}
                <p>依赖：{item.external_dependencies.join(', ')}</p>
              {/if}
              {#if bindings.length}
                <p>缺少序列：{bindings.join(', ')}</p>
              {/if}
            </article>
          {/each}
        </div>
        <p class="audit-gap-note">这些值必须来自合法券商私有序列或调用方显式上下文；页面不会下载、推导或填零伪造。可在公式详情中生成带真实 K 线时间戳的空模板。</p>
      </details>
    {/if}
    {#if auditApplicabilityRows.length > 0}
      <details class="audit-gaps applicability">
        <summary>
          <strong>市场/周期边界 {count(auditApplicabilityRows.length)}</strong>
          <span>不计为解释器运行错误</span>
        </summary>
        <div class="audit-gap-list compact">
          {#each auditApplicabilityRows as item (`${item.kind_key}:${item.code}`)}
            <article>
              <div>
                <strong>{item.code}</strong>
                <span>{item.name}</span>
                <em>{auditStatusLabel(item.status)}</em>
              </div>
            </article>
          {/each}
        </div>
      </details>
    {/if}
  {/if}

  {#if backtest.data}
    <div class="backtest-meta">
      <strong>回测 {backtest.data.formula}</strong>
      <span>收益 {backtest.data.total_return_pct.toFixed(2)}%</span>
      <span>最大回撤 {backtest.data.max_drawdown_pct.toFixed(2)}%</span>
      <span>交易 {count(backtest.data.trade_count)} 笔</span>
      <span>胜率 {backtest.data.win_rate_pct.toFixed(1)}%</span>
      <span>{backtest.data.signal_timing}</span>
      {#if backtest.data.formula_source_mode === 'inline-post'}
        <strong>自定义源码 · {count(backtest.data.source_bytes ?? 0)} B</strong>
      {/if}
      {#if backtest.data.point_in_time_finance}
        <strong>严格财务时点</strong>
      {/if}
    </div>
  {:else if backtest.error}
    <div class="calc-empty">回测失败：{backtest.error}</div>
  {/if}

  {#if scan.data}
    <div class="scan-result">
      <div class="backtest-meta">
        <strong>扫描 {scan.data.formula}</strong>
        <span>请求 {count(scan.data.requested_count)}</span>
        <span>完成 {count(scan.data.evaluated)}</span>
        <span>命中 {count(scan.data.match_count)}</span>
        <span>取数错误 {count(scan.data.fetch_error_count)}</span>
        {#if scan.data.formula_source_mode === 'inline-post'}
          <strong>自定义源码 · {count(scan.data.source_bytes ?? 0)} B</strong>
        {/if}
        {#if scan.data.point_in_time_finance}
          <strong>严格财务时点</strong>
        {/if}
      </div>
      {#if scan.data.matches.length}
        <div class="scan-matches">
          {#each scan.data.matches as match (match.security_id)}
            <div class="scan-match">
              <strong>{match.name || match.security_id}</strong>
              <span>{match.security_id} · {match.trigger_date} {match.trigger_time}</span>
              <span>{match.signals.map((item) => `${item.output}=${item.value}`).join(' · ')}</span>
            </div>
          {/each}
        </div>
      {/if}
    </div>
  {:else if scan.error}
    <div class="calc-empty">扫描失败：{scan.error}</div>
  {/if}

  {#if monitorActive || monitorEvents.length}
    <div class="monitor-panel">
      <div class="monitor-head">
        <strong class:live={monitorActive}>{monitorActive ? '策略池监控中' : '策略池监控已停止'}</strong>
        <span>间隔 {Math.max(1, Math.min(86400, Number(monitorSeconds) || 60))} 秒</span>
        <span>活跃 {count(monitorSnapshot.size)} 只</span>
        <span>最近运行 {monitorLastRun || '等待首次扫描'}</span>
        {#if monitorEvents.length}
          <button type="button" onclick={() => monitorEvents = []}>清空事件</button>
        {/if}
      </div>
      {#if monitorEvents.length}
        <div class="monitor-events">
          {#each monitorEvents as event (event.id)}
            <div class="monitor-event {event.kind}">
              <time>{event.at}</time>
              <strong>{event.kind === 'entered'
                ? '进入'
                : event.kind === 'exited'
                  ? '退出'
                  : event.kind === 'updated'
                    ? '更新'
                    : event.kind === 'degraded'
                      ? '不完整'
                      : event.kind === 'error'
                        ? '错误'
                        : '快照'}</strong>
              <span>{event.message}</span>
            </div>
          {/each}
        </div>
      {:else}
        <p class="monitor-empty">首次完整扫描后建立快照；后续仅记录进入、退出和信号更新。</p>
      {/if}
      <p class="monitor-note">网页监控只在当前浏览器会话运行；需要后台断点续跑和通达信自选池导出时使用 <code>tdx-tool formulas watch</code>。</p>
    </div>
  {/if}
</Panel>

<Panel
  eyebrow="NATIVE C++ · TBIGDATA CALC/CALCREF"
  title="榜单计算列解释器"
  subtitle="直接复算 cloud_cfg 表格列；既支持单个对象，也支持 1–128 行对象数组，批量模式共享去重后的公开行情与财务请求。"
  busy={cloudAudit.busy || cloudInputTemplate.busy || cloudEvaluation.busy}
  error={cloudEvaluation.error || cloudInputTemplate.error || cloudAudit.error}
  onRetry={cloudEvaluation.error ? runCloudCalc : cloudInputTemplate.error ? generateCloudInputTemplate : loadCloudAudit}
>
  {#snippet toolbar()}
    <TextInput bind:value={cloudCfg} width="230px" label="CFG 名称" placeholder="func_kzz_kzzsy101" onEnter={runCloudCalc} />
    <TextInput bind:value={cloudAsOf} width="118px" label="评价日期" placeholder="20260808" onEnter={runCloudCalc} />
    <label class="strict-finance-toggle" class:active={cloudQuotes} title="只请求当前 CFG 实际声明的公开 L1、财务和关联证券字段">
      <input type="checkbox" bind:checked={cloudQuotes} />
      <span>补公开行情</span>
    </label>
    <Button icon="search" busy={cloudAudit.busy} onclick={loadCloudAudit}>审计配置</Button>
    <Button icon="copy" busy={cloudInputTemplate.busy} onclick={generateCloudInputTemplate}>补齐最小模板</Button>
    <Button icon="market" busy={cloudEvaluation.busy} onclick={runCloudCalc}>复算输入行</Button>
    <Button icon="download" disabled={!cloudEvaluation.data} onclick={downloadCloudResult}>导出结果</Button>
  {/snippet}

  <div class="source-editor cloud-row-editor">
    <label>
      <span>扁平 JSON 对象或对象数组 · 示例为南航转债及其正股关系</span>
      <textarea
        bind:value={cloudRow}
        rows="12"
        spellcheck="false"
        maxlength="524288"
        onkeydown={(event: KeyboardEvent) => {
          if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
            event.preventDefault();
            runCloudCalc();
          }
        }}
      ></textarea>
    </label>
    <div class="cloud-template-tools">
      <TextInput bind:value={cloudTemplateName} width="190px" label="模板名" placeholder="我的榜单模板" />
      <Select value={cloudTemplateId} options={cloudTemplateOptions} width="220px" onChange={applyCloudTemplate} />
      <Button icon="copy" onclick={saveCloudTemplate}>保存到浏览器</Button>
      <Button variant="ghost" disabled={!cloudTemplateId} onclick={deleteCloudTemplate}>删除模板</Button>
      <Button variant="ghost" onclick={makeCloudBatchExample}>生成两行示例</Button>
    </div>
    <p>接口只接受 CFG 文件名和内联 JSON，不接受服务器文件路径；Ctrl+Enter 复算。输入原文不会上传到其他服务，模板仅在点击保存后写入当前浏览器。</p>
    {#if cloudTemplateNotice}<p class="cloud-template-notice">{cloudTemplateNotice}</p>{/if}
  </div>

  {#if cloudAudit.data}
    <div class="backtest-meta audit-meta">
      <strong>TBigData 配置审计</strong>
      <span>CFG {count(cloudAudit.data.summary.cfg_files)}</span>
      <span>计算列 {count(cloudAudit.data.summary.calc_columns)}</span>
      <span>可执行 {count(cloudAudit.data.summary.current_config_executable)}</span>
      <span>36 项内建 {count(cloudAudit.data.summary.registered_builtin_implemented)} 项已实现</span>
      <span>宿主列 {count(cloudAudit.data.summary.auto_resolvable_host_columns)} / {count(cloudAudit.data.summary.effective_host_columns)}</span>
      <strong class:error={cloudAudit.data.summary.invalid > 0 || cloudAudit.data.summary.parse_errors > 0}>
        无效 {count(cloudAudit.data.summary.invalid)} · 解析错误 {count(cloudAudit.data.summary.parse_errors)}
      </strong>
    </div>
  {/if}

  {#if cloudInputTemplate.data}
    <div class="backtest-meta audit-meta">
      <strong>最小输入模板</strong>
      <span>原始输入 {count(cloudInputTemplate.data.counts.input_fields)}</span>
      <span>自动宿主 {count(cloudInputTemplate.data.counts.host_fields)}</span>
      <span>同行派生 {count(cloudInputTemplate.data.counts.derived_fields)}</span>
      <span>计算列 {count(cloudInputTemplate.data.counts.calculated_fields)}</span>
      <span>资源 {cloudInputTemplate.data.source_resources.join(', ') || '—'}</span>
    </div>
    <div class="cloud-host-context">
      {#each cloudInputTemplate.data.input_fields as field (field.code)}
        <span title={`${field.datatype} · ${field.reasons.join(', ')}`}>
          <strong>{field.code}</strong> {field.name || '需填写'}
        </span>
      {/each}
      {#each cloudInputTemplate.data.host_fields as field (`host:${field.code}`)}
        <span title={`${field.system_column} · ${field.resolver}`}>
          <strong>{field.code}</strong> 自动宿主
        </span>
      {/each}
      {#each cloudInputTemplate.data.derived_fields as field (`derived:${field.code}`)}
        <span title={field.reasons.join(', ')}><strong>{field.code}</strong> 同行派生</span>
      {/each}
    </div>
  {/if}

  {#if cloudSingle}
    <div class="backtest-meta">
      <strong>{cloudSingle.cfg_name}</strong>
      <span>评价日 {cloudSingle.as_of}</span>
      <span>执行 {count(cloudSingle.counts.evaluated)} / {count(cloudSingle.counts.calculated)}</span>
      <span>不可用 {count(cloudSingle.counts.unavailable)}</span>
      <span>错误 {count(cloudSingle.counts.errors)}</span>
      <span>宿主绑定 {count(cloudSingle.host_context.bindings.length)}</span>
      <span>未解析 {count(cloudSingle.host_context.unresolved.length)}</span>
      <strong>{cloudSingle.execution_mode}</strong>
    </div>
    <div class="cloud-host-context">
      {#each cloudSingle.host_context.bindings as binding, index (`${binding.code}:${binding.security || ''}:${index}`)}
        <span title={`${binding.source}${binding.security ? ` · ${binding.security}` : ''}`}>
          <strong>{binding.code}</strong> {String(binding.value ?? '—')}
        </span>
      {/each}
      {#each cloudSingle.host_context.unresolved as item, index (`unresolved:${item.code}:${index}`)}
        <span class="unresolved"><strong>{item.code}</strong> 未解析</span>
      {/each}
    </div>
  {:else if cloudBatch}
    <div class="backtest-meta">
      <strong>{cloudBatch.cfg_name}</strong>
      <span>评价日 {cloudBatch.as_of}</span>
      <span>行 {count(cloudBatch.counts.succeeded)} / {count(cloudBatch.counts.rows)}</span>
      <span>执行 {count(cloudBatch.counts.evaluated)} / {count(cloudBatch.counts.calculated)}</span>
      <span>失败行 {count(cloudBatch.counts.failed)}</span>
      <span>不可用 {count(cloudBatch.counts.unavailable)}</span>
      <span>错误 {count(cloudBatch.counts.errors)}</span>
      <strong>{cloudBatch.execution_mode}</strong>
    </div>
    <div class="cloud-host-context">
      <span><strong>行情模式</strong> {cloudBatch.fetch_plan.quote_mode}</span>
      <span><strong>去重证券</strong> {count(cloudBatch.fetch_plan.unique_quote_securities)}</span>
      <span><strong>行情文档请求</strong> {count(cloudBatch.fetch_plan.quote_document_fetches)}</span>
      <span><strong>财务证券</strong> {count(cloudBatch.fetch_plan.unique_finance_securities)}</span>
      <span><strong>财务文档请求</strong> {count(cloudBatch.fetch_plan.finance_document_fetches)}</span>
      {#each cloudBatch.rows.filter((entry) => entry.status === 'error') as entry (entry.row_index)}
        <span class="unresolved"><strong>第 {entry.row_index + 1} 行</strong> {entry.error || '执行失败'}</span>
      {/each}
    </div>
  {/if}
  {#if cloudEvaluation.data}
    <div class="cloud-results">
      <DataTable
        columns={cloudResultColumns}
        rows={cloudResults}
        rowKey={(row) => `${row.row_index}:${row.unit_id}:${row.code}`}
        maxHeight="420px"
        stickyFirst
      />
    </div>
  {/if}
</Panel>

<Panel
  eyebrow="NATIVE C++ · SAME-BAR STRATEGY"
  title="多公式组合策略"
  subtitle="每条规则在同一证券、同一日期和时间的 K 线上组合；信号在收盘确认，组合回测到下一根共同 K 线开盘才调仓。"
  busy={strategyBusy}
  error={strategyError}
  onRetry={retryStrategy}
>
  {#snippet toolbar()}
    <TextInput bind:value={strategyCodes} width="240px" label="固定股票池" placeholder="sz000001,sh600000" />
    <TextInput bind:value={strategyPages} width="76px" label="历史页数" placeholder="2" />
    <TextInput bind:value={strategyLookback} width="76px" label="扫描近 N 根" placeholder="10" />
    <Button icon="search" busy={strategyScan.busy} onclick={runStrategyScan}>组合扫描</Button>
    <Button icon="clock" busy={strategyBacktest.busy} onclick={runStrategyBacktest}>组合回测</Button>
  {/snippet}

  <div class="strategy-workbench">
    <label>
      <span>策略清单 · operator 支持 all / any / at-least</span>
      <textarea
        bind:value={strategyManifest}
        rows="14"
        spellcheck="false"
        maxlength="60000"
        onkeydown={(event: KeyboardEvent) => {
          if ((event.ctrlKey || event.metaKey) && event.key === 'Enter') {
            event.preventDefault();
            runStrategyScan();
          }
        }}
      ></textarea>
    </label>
    <div class="strategy-help">
      <strong>执行约束</strong>
      <span>1–16 条条件公式；每条可写 source，或用 formula 引用内置条件选股。</span>
      <span>未来函数、非数值安全公式直接拒绝；规则正文不写盘，也不会在响应中回显。</span>
      <span>回测股票池必须完整，按命中证券等权；佣金 2.5 bps、滑点 1 bps，末端强制平仓。</span>
      <span>当前周期沿用上方的“{calcPeriod}”，严格财务时点开关也与上方共用。</span>
    </div>
  </div>

  {#if strategyScan.data}
    <div class="backtest-meta">
      <strong>{strategyScan.data.strategy.name}</strong>
      <span>{strategyScan.data.strategy.operator} · 至少 {strategyScan.data.strategy.minimum_matches}/{strategyScan.data.strategy.rule_count}</span>
      <span>请求 {count(strategyScan.data.requested_count)}</span>
      <span>完成 {count(strategyScan.data.evaluated)}</span>
      <span>命中 {count(strategyScan.data.match_count)}</span>
      <span>错误 {count(strategyScan.data.error_count + strategyScan.data.fetch_error_count)}</span>
    </div>
    {#if strategyScan.data.matches.length}
      <div class="scan-matches strategy-matches">
        {#each strategyScan.data.matches as match (match.security_id)}
          <div class="scan-match">
            <strong>{match.name || match.security_id}</strong>
            <span>{match.security_id} · {match.trigger_date} {match.trigger_time}</span>
            <span>{match.triggers[match.triggers.length - 1]?.matched_rule_ids.join(' + ') || '—'}</span>
          </div>
        {/each}
      </div>
    {/if}
  {/if}

  {#if strategyBacktest.data}
    <div class="backtest-meta">
      <strong>{strategyBacktest.data.strategy.name}</strong>
      <span>收益 {strategyBacktest.data.total_return_pct.toFixed(2)}%</span>
      <span>最大回撤 {strategyBacktest.data.max_drawdown_pct.toFixed(2)}%</span>
      <span>最终权益 {strategyBacktest.data.final_equity.toFixed(2)}</span>
      <span>共同 K 线 {count(strategyBacktest.data.aligned_bar_count)}</span>
      <span>调仓 {count(strategyBacktest.data.rebalance_count)} 次</span>
      <span>成本 {strategyBacktest.data.total_cost.toFixed(2)}</span>
      <span>平均持仓 {strategyBacktest.data.average_holding_count.toFixed(2)} 只</span>
    </div>
    <div class="strategy-attribution">
      <DataTable
        columns={strategyAttributionColumns}
        rows={strategyBacktest.data.attribution}
        rowKey={(row) => row.security_id}
        sortKey="net"
        maxHeight="280px"
        stickyFirst
      />
    </div>
  {/if}
</Panel>

<Split asideWidth="320px">
  {#snippet main()}
    <Panel
      flush
      scroll
      title={KINDS.find((item) => item.id === kind)?.label ?? '公式'}
      subtitle={formulas.loaded ? `${count(rows.length)} 条` : ''}
      busy={formulas.busy}
      error={formulas.error}
      onRetry={load}
      empty={formulas.loaded && !formulas.busy && rows.length === 0}
      emptyText={origin === 'user' && formulas.data?.user_library_enabled === false
        ? '服务未通过 --include-user-formulas 显式启用 PriGS 用户库。'
        : '没有匹配的公式；换一个关键词、来源或公式类型。'}
    >
      {#snippet toolbar()}
        <Segmented options={KINDS} value={kind} onChange={switchKind} ariaLabel="公式类型" />
        <Segmented options={ORIGINS} value={origin} onChange={switchOrigin} ariaLabel="公式来源" />
        <TextInput
          bind:value={query}
          icon="search"
          width="240px"
          label="检索公式"
          placeholder="代码、名称、分类或正文函数，如 MACD / FINANCE"
          onEnter={load}
        />
        <Button icon="search" busy={formulas.busy} onclick={load}>检索</Button>
      {/snippet}

      <DataTable
        {columns}
        rows={rows}
        rowKey={(row) => row.code}
        onRowClick={selectFormula}
        isActive={(row) => row.code === selectedCode}
        stickyFirst
      />
    </Panel>
  {/snippet}

  {#snippet aside()}
    <Panel
      scroll
      eyebrow="FORMULA DETAIL"
      title={selected?.name ?? '公式详情'}
      subtitle={selected ? `${selected.code} · ${text(selected.kind_name)}` : '点击左侧任意公式'}
      empty={!selected}
      emptyText="点击左侧任意公式，查看它的全部字段。"
    >
      {#if selected}
        {#if selected.analysis?.explicit_context_bindable}
          <div class="context-tools">
            <label class="strict-finance-toggle" class:active={contextUseKline} title="读取上方证券、周期和历史页数，为每根真实 K 线生成 DATE|TIME 键">
              <input type="checkbox" bind:checked={contextUseKline} />
              <span>按当前 K 线批量生成</span>
            </label>
            {#if contextUseKline}
              <p>{calcMarket.toUpperCase()}{calcCode} · {calcPeriod} · {calcPages} 页；将为取回的每根 K 线生成精确空占位。</p>
            {:else}
              <TextInput bind:value={contextStamp} width="100%" label="单个序列时间戳" placeholder="YYYY-MM-DD|HH:MM" onEnter={loadContextTemplate} />
            {/if}
            <Button icon="download" busy={contextTemplate.busy} onclick={loadContextTemplate}>{contextUseKline ? '按 K 线生成外部序列模板' : '生成单点外部序列模板'}</Button>
            <p>把每个 null 替换为合法数值后即可执行。券商私有信号和授权 L2 数据不会下载、推导或伪造。</p>
            {#if contextTemplate.error}<p class="context-error">{contextTemplate.error}</p>{/if}
            {#if contextInput}
              <textarea bind:value={contextInput} rows="12" spellcheck="false" aria-label="公式显式上下文 JSON"></textarea>
              {#if contextOrigin === 'template'}
                <Button icon="download" onclick={createCaptureScaffold}>生成调用方捕获填写表</Button>
                <p>填写表只接受你合法持有的序列与宿主 raw 标量；把 <code>ownership_confirmed</code> 明确改为 true 后才能导入。窗口外时间戳会被报告但不保留，不会登录券商或发起订阅。</p>
                {#if captureInput}
                  <textarea bind:value={captureInput} rows="12" spellcheck="false" aria-label="调用方授权捕获 JSON"></textarea>
                  <label class="strict-finance-toggle" class:active={captureAllowPartial} title="仅用于查看缺失 binding/时间戳，不会把缺值伪造成零">
                    <input type="checkbox" bind:checked={captureAllowPartial} />
                    <span>允许部分捕获，仅输出缺口诊断</span>
                  </label>
                  <Button icon="market" busy={contextImport.busy} onclick={importCallerCapture}>严格校验并物化上下文</Button>
                  <p>成功后浏览器会清空原始捕获填写表，仅保留解释器所需的物化 context；失败时保留以便修正。</p>
                {/if}
                {#if captureLocalError || contextImport.error}<p class="context-error">{captureLocalError || contextImport.error}</p>{/if}
              {:else if contextOrigin === 'capture'}
                <p>调用方捕获已按模板精确对齐；上方 JSON 可直接用于本次内置公式 evaluate。原始捕获封套已从页面状态清除。</p>
              {/if}
              <Button icon="market" busy={calculation.busy} onclick={calculate}>用填写值执行内置公式</Button>
            {/if}
          </div>
        {/if}
        <dl class="fields">
          {#each detailEntries as entry (entry.key)}
            <div class="cell">
              <dt>{entry.key}</dt>
              <dd class:source-code={entry.key === 'source_text'}>{entry.value}</dd>
            </div>
          {/each}
        </dl>
      {/if}
    </Panel>
  {/snippet}
</Split>

<style>
  .strategy-workbench {
    display: grid;
    grid-template-columns: minmax(0, 2fr) minmax(240px, 1fr);
    gap: var(--sp-3);
    margin: 0 var(--sp-3) var(--sp-3);
  }

  .strategy-workbench label,
  .strategy-help {
    display: grid;
    gap: var(--sp-2);
    min-width: 0;
    padding: var(--sp-3);
    color: var(--fg-mute);
    font-size: var(--fs-micro);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    background: var(--bg-raised);
  }

  .strategy-workbench textarea {
    width: 100%;
    min-height: 270px;
    resize: vertical;
    padding: var(--sp-2) var(--sp-3);
    color: var(--fg);
    background: var(--bg-input);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
    line-height: 1.5;
  }

  .strategy-workbench textarea:focus {
    border-color: var(--focus);
    outline: none;
  }

  .strategy-help {
    align-content: start;
    line-height: 1.55;
  }

  .strategy-help strong { color: var(--fg); }
  .strategy-matches { margin-bottom: var(--sp-3); }

  .strategy-attribution {
    margin: var(--sp-2) var(--sp-3) var(--sp-3);
    overflow: hidden;
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .source-editor {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    gap: var(--sp-2) var(--sp-3);
    align-items: end;
    margin: 0 var(--sp-3) var(--sp-3);
    padding: var(--sp-3);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    background: var(--bg-raised);
  }

  .source-editor label {
    display: grid;
    gap: var(--sp-1);
    min-width: 0;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }

  .source-editor textarea {
    width: 100%;
    min-height: 116px;
    resize: vertical;
    padding: var(--sp-2) var(--sp-3);
    color: var(--fg);
    background: var(--bg-input);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
    line-height: 1.55;
  }

  .source-editor textarea:focus {
    border-color: var(--focus);
    outline: none;
  }

  .source-editor p {
    grid-column: 1 / -1;
    margin: 0;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }

  .cloud-row-editor { grid-template-columns: minmax(0, 1fr); }

  .cloud-template-tools {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: var(--sp-2);
  }

  .source-editor p.cloud-template-notice {
    color: var(--focus);
  }

  .external-context {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    gap: var(--sp-2) var(--sp-3);
    margin: 0 var(--sp-3) var(--sp-3);
    padding: var(--sp-3);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
    background: var(--bg-raised);
  }

  .external-context div { display: grid; gap: var(--sp-1); }
  .external-context strong { color: var(--focus); }
  .external-context span,
  .external-context p { margin: 0; color: var(--fg-mute); font-size: var(--fs-micro); }
  .external-context textarea {
    grid-column: 1 / -1;
    width: 100%;
    min-height: 150px;
    resize: vertical;
    padding: var(--sp-2) var(--sp-3);
    color: var(--fg);
    background: var(--bg-input);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
    font: 10px/1.5 var(--font-num);
  }

  .external-context textarea:focus { border-color: var(--focus); outline: none; }

  .cloud-host-context {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-1) var(--sp-2);
    margin: var(--sp-2) var(--sp-3) 0;
  }

  .cloud-host-context span {
    padding: 3px var(--sp-2);
    color: var(--fg-mute);
    background: var(--bg-raised);
    border: 1px solid var(--line);
    border-radius: 999px;
    font-family: var(--font-num);
    font-size: var(--fs-micro);
  }

  .cloud-host-context strong { color: var(--fg); }
  .cloud-host-context .unresolved { color: var(--negative); }

  .cloud-results {
    margin: var(--sp-2) var(--sp-3) var(--sp-3);
    overflow: hidden;
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .calc-meta {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-2) var(--sp-4);
    padding: 0 var(--sp-3) var(--sp-2);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  .calc-meta strong {
    color: var(--fg);
    font-weight: 500;
  }

  .calc-empty {
    display: grid;
    place-items: center;
    min-height: 120px;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }

  .calc-empty.compact {
    min-height: 0;
    padding: var(--sp-2) var(--sp-3);
  }

  .future-replay {
    display: grid;
    gap: var(--sp-2);
    margin: 0 var(--sp-3) var(--sp-3);
    padding: var(--sp-3);
    border: 1px solid var(--line);
    border-left: 2px solid var(--warn);
    border-radius: var(--radius);
    background: var(--bg-raised);
  }

  .future-replay > p {
    margin: 0;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }

  .future-replay-summary {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-1) var(--sp-3);
    align-items: center;
    color: var(--fg-mute);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
  }

  .future-replay-summary strong { color: var(--fg); }
  .future-replay-summary em { color: var(--warn); font-style: normal; }

  .future-replay-events {
    display: grid;
    max-height: 260px;
    overflow: auto;
    border: 1px solid var(--line);
    border-radius: var(--radius);
    background: var(--bg-input);
  }

  .future-replay-events > div {
    display: grid;
    grid-template-columns: 150px 90px minmax(220px, 1fr) minmax(160px, auto);
    gap: var(--sp-2);
    align-items: center;
    padding: var(--sp-2);
    border-bottom: 1px solid var(--line);
    font-size: var(--fs-micro);
  }

  .future-replay-events > div:last-child { border-bottom: 0; }
  .future-replay-events time,
  .future-replay-events code { font-family: var(--font-num); }
  .future-replay-events span { color: var(--fg-mute); }
  .future-replay-events code { color: var(--focus); }

  .trade-event-ir {
    display: grid;
    gap: var(--sp-2);
    margin: 0 var(--sp-3) var(--sp-3);
    padding: var(--sp-3);
    border: 1px solid var(--line);
    border-left: 2px solid var(--focus);
    border-radius: var(--radius);
    background: var(--bg-raised);
  }

  .trade-event-summary,
  .trade-event-primitive summary {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: var(--sp-1) var(--sp-3);
    color: var(--fg-mute);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
  }

  .trade-event-summary strong,
  .trade-event-primitive summary strong { color: var(--fg); }
  .trade-event-summary em { margin-left: auto; color: var(--warn); font-style: normal; }

  .trade-event-primitive {
    padding: var(--sp-2);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    background: var(--bg-input);
  }

  .trade-event-primitive summary { cursor: pointer; }
  .trade-event-detail {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
    gap: var(--sp-2);
    margin-top: var(--sp-2);
  }

  .trade-event-detail div { min-width: 0; }
  .trade-event-detail p { margin: 0; color: var(--fg-mute); font-size: var(--fs-micro); }
  .trade-event-detail strong { color: var(--fg-mute); font-size: var(--fs-micro); }
  .trade-event-truncated {
    display: block;
    margin-top: var(--sp-1);
    color: var(--warn);
    font-size: var(--fs-micro);
  }
  .trade-event-detail pre {
    max-height: 220px;
    overflow: auto;
    margin: var(--sp-1) 0 0;
    padding: var(--sp-2);
    color: var(--fg-dim);
    background: var(--bg);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    font: 10px/1.45 var(--font-num);
    white-space: pre-wrap;
    word-break: break-word;
  }

  .backtest-meta {
    display: flex;
    flex-wrap: wrap;
    gap: var(--sp-2) var(--sp-4);
    margin: var(--sp-2) var(--sp-3) 0;
    padding: var(--sp-3);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    background: var(--bg-raised);
    color: var(--fg-mute);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
  }

  .backtest-meta strong { color: var(--fg); }

  .backtest-meta strong.error { color: var(--negative); }

  .audit-meta { border-left: 2px solid var(--focus); }

  .audit-errors {
    display: grid;
    gap: 2px;
    margin: var(--sp-2) var(--sp-3) 0;
    padding: var(--sp-2) var(--sp-3);
    color: var(--negative);
    background: color-mix(in srgb, var(--negative) 7%, transparent);
    border: 1px solid color-mix(in srgb, var(--negative) 28%, var(--line));
    border-radius: var(--radius);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
  }

  .audit-gaps {
    margin: var(--sp-2) var(--sp-3) 0;
    border: 1px solid var(--line);
    border-radius: var(--radius);
    background: var(--bg-raised);
  }

  .audit-gaps > summary {
    display: flex;
    align-items: center;
    gap: var(--sp-2);
    padding: var(--sp-2) var(--sp-3);
    color: var(--fg-dim);
    cursor: pointer;
  }

  .audit-gaps > summary strong { color: var(--fg); }

  .audit-gaps > summary span {
    margin-left: auto;
    font-size: var(--fs-micro);
  }

  .audit-gap-list {
    display: grid;
    gap: 1px;
    max-height: 300px;
    overflow: auto;
    border-top: 1px solid var(--line);
    background: var(--line);
  }

  .audit-gap-list article {
    display: grid;
    gap: 3px;
    min-width: 0;
    padding: var(--sp-2) var(--sp-3);
    background: var(--bg);
  }

  .audit-gap-list article > div {
    display: flex;
    align-items: baseline;
    gap: var(--sp-2);
    min-width: 0;
  }

  .audit-gap-list strong { color: var(--fg); }
  .audit-gap-list span { color: var(--fg-dim); }

  .audit-gap-list em {
    margin-left: auto;
    color: var(--warning);
    font-size: var(--fs-micro);
    font-style: normal;
    white-space: nowrap;
  }

  .audit-gap-list p,
  .audit-gap-note {
    margin: 0;
    color: var(--fg-mute);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
    overflow-wrap: anywhere;
  }

  .audit-gap-note {
    padding: var(--sp-2) var(--sp-3);
    border-top: 1px solid var(--line);
  }

  .audit-gap-list.compact { max-height: 180px; }

  .strict-finance-toggle {
    display: inline-flex;
    flex: none;
    align-items: center;
    gap: var(--sp-1);
    height: var(--h-control);
    padding: 0 var(--sp-2);
    color: var(--fg-dim);
    font-size: var(--fs-micro);
    white-space: nowrap;
    cursor: pointer;
    background: var(--bg-raised);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
  }

  .strict-finance-toggle.active {
    color: var(--fg);
    border-color: var(--focus);
  }

  .strict-finance-toggle input {
    width: 13px;
    height: 13px;
    accent-color: var(--focus);
  }

  .scan-result { margin-bottom: var(--sp-3); }

  .scan-matches {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: var(--sp-2);
    margin: var(--sp-2) var(--sp-3) 0;
  }

  .scan-match {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: var(--sp-2) var(--sp-3);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    color: var(--fg-mute);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
  }

  .scan-match strong { color: var(--fg); }

  .monitor-panel {
    display: grid;
    gap: var(--sp-2);
    margin: var(--sp-2) var(--sp-3) var(--sp-3);
    padding: var(--sp-3);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    background: var(--bg-raised);
  }

  .monitor-head {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: var(--sp-2) var(--sp-4);
    color: var(--fg-mute);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
  }

  .monitor-head strong { color: var(--fg); }
  .monitor-head strong.live { color: var(--down); }

  .monitor-head button {
    margin-left: auto;
    color: var(--fg-dim);
    font-size: var(--fs-micro);
  }

  .monitor-events {
    display: grid;
    max-height: 220px;
    overflow: auto;
    border: 1px solid var(--line);
    border-radius: var(--radius);
  }

  .monitor-event {
    display: grid;
    grid-template-columns: 72px 48px minmax(0, 1fr);
    gap: var(--sp-2);
    align-items: baseline;
    min-height: 26px;
    padding: var(--sp-1) var(--sp-2);
    color: var(--fg-dim);
    font-size: var(--fs-micro);
    border-bottom: 1px solid var(--line);
  }

  .monitor-event:last-child { border-bottom: 0; }
  .monitor-event time { color: var(--fg-mute); font-family: var(--font-num); }
  .monitor-event.entered strong, .monitor-event.updated strong { color: var(--up); }
  .monitor-event.exited strong { color: var(--down); }
  .monitor-event.degraded strong, .monitor-event.error strong { color: var(--warn); }
  .monitor-event.snapshot strong { color: var(--focus); }

  .monitor-empty,
  .monitor-note {
    margin: 0;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }

  .monitor-note code {
    color: var(--fg-dim);
    font-family: var(--font-num);
  }

  .fields {
    display: flex;
    flex-direction: column;
    gap: var(--sp-2);
    margin: 0;
  }

  .cell {
    display: flex;
    flex-direction: column;
    gap: 1px;
    padding-bottom: var(--sp-2);
    border-bottom: 1px solid var(--line);
  }

  .cell:last-child {
    border-bottom: 0;
  }

  dt {
    font-family: var(--font-num);
    font-size: 10px;
    color: var(--fg-mute);
  }

  dd {
    margin: 0;
    font-size: var(--fs-micro);
    line-height: var(--lh-tight);
    color: var(--fg);
    word-break: break-word;
    white-space: pre-wrap;
  }

  dd.source-code {
    max-height: 280px;
    overflow: auto;
    padding: var(--sp-2);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    background: var(--bg-raised);
    font-family: var(--font-num);
    line-height: 1.55;
  }

  .context-tools {
    display: grid;
    gap: var(--sp-2);
    margin-bottom: var(--sp-3);
    padding: var(--sp-2);
    border: 1px solid var(--line);
    border-radius: var(--radius);
    background: var(--bg-raised);
  }

  .context-tools p { margin: 0; color: var(--fg-mute); font-size: 10px; line-height: 1.5; }
  .context-tools .context-error { color: var(--warn); }
  .context-tools textarea {
    width: 100%;
    min-height: 220px;
    resize: vertical;
    padding: var(--sp-2);
    color: var(--fg);
    background: var(--bg-input);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
    font: 10px/1.5 var(--font-num);
  }
  .context-tools textarea:focus { border-color: var(--focus); outline: none; }

  @media (max-width: 880px) {
    .strategy-workbench { grid-template-columns: 1fr; }
    .source-editor { grid-template-columns: 1fr; }
    .source-editor p { grid-column: 1; }
    .external-context { grid-template-columns: 1fr; }
    .trade-event-detail { grid-template-columns: 1fr; }
  }
</style>
