<script lang="ts">
  /** Authorized Level2 research boundary and local read-only session preflight. */
  import { onMount } from 'svelte';
  import { postJson, queryString } from '../../api';
  import { stageLevel2FormulaContext } from '../../lib/formulaContextBridge';
  import { count, text } from '../../lib/fmt';
  import { router } from '../../lib/router.svelte';
  import { Resource } from '../../lib/resource.svelte';
  import Badge from '../../ui/Badge.svelte';
  import Button from '../../ui/Button.svelte';
  import DataTable, { type Column } from '../../ui/DataTable.svelte';
  import PageHeader from '../../ui/PageHeader.svelte';
  import Panel from '../../ui/Panel.svelte';
  import Select from '../../ui/Select.svelte';
  import TextInput from '../../ui/TextInput.svelte';

  interface ModuleRecord {
    name: string;
    path: string;
    sha256?: string;
    recognized?: boolean;
  }

  interface SessionStatus {
    schema: string;
    read_only: boolean;
    attached: boolean;
    subscription_sent: boolean;
    token_accessed: boolean;
    entitlement_bypass: boolean;
    running: boolean;
    ambiguous?: boolean;
    candidate_pids?: number[];
    pid?: number;
    image_path?: string;
    tdxw_sha256?: string;
    tdxw_recognized?: boolean;
    tpbus_loaded?: boolean;
    tpbus_recognized?: boolean;
    ready_for_passive_capture: boolean;
    reason: string;
    modules?: ModuleRecord[];
  }

  interface Decoder {
    format: string;
    source: string;
    boundary: string;
  }

  interface Level2BuildResult {
    schema: string;
    format: string;
    size?: number;
    payload_hex?: string;
    offline: boolean;
    network_requests: number;
    subscription_sent: boolean;
    [key: string]: unknown;
  }

  type Level2DecodeResult = Record<string, unknown> & {
    schema: string;
    offline: boolean;
    input_size: number;
    input_retained: boolean;
    file_accessed: boolean;
  };

  type Level2ProjectResult = Record<string, unknown> & {
    schema: string;
    data_type: number;
    projected_state: Record<string, unknown>;
    offline: boolean;
    input_size: number;
    input_retained: boolean;
    file_accessed: boolean;
  };

  type Level2SnapshotTransitionResult = Record<string, unknown> & {
    schema: string;
    format: string;
    projected_state?: Record<string, unknown>;
    projected_state_candidate?: Record<string, unknown>;
    offline: boolean;
    input_size: number;
    input_retained: boolean;
    file_accessed: boolean;
  };

  type ResultDetailMode = 'summary' | 'preview' | 'full';

  interface TruncatedArrayMetadata {
    path: string;
    total_items: number;
    shown_items: number;
    omitted_items: number;
  }

  const resultPreviewArrayLimit = 100;

  function boundedResultPreview(value: unknown): Record<string, unknown> {
    const truncatedArrays: TruncatedArrayMetadata[] = [];
    let omittedArrayItems = 0;
    const ancestors = new WeakSet<object>();

    function visit(current: unknown, path: string): unknown {
      if (Array.isArray(current)) {
        const shown = Math.min(current.length, resultPreviewArrayLimit);
        if (shown < current.length) {
          const omitted = current.length - shown;
          omittedArrayItems += omitted;
          truncatedArrays.push({
            path,
            total_items: current.length,
            shown_items: shown,
            omitted_items: omitted
          });
        }
        return current.slice(0, shown).map((item, index) =>
          visit(item, `${path}[${index}]`));
      }
      if (current && typeof current === 'object') {
        if (ancestors.has(current)) return '[circular]';
        ancestors.add(current);
        const result: Record<string, unknown> = {};
        for (const [key, child] of Object.entries(current as Record<string, unknown>))
          result[key] = visit(child, `${path}.${key}`);
        ancestors.delete(current);
        return result;
      }
      return current;
    }

    const result = visit(value, '$');
    return {
      preview_metadata: {
        preview: true,
        array_item_limit: resultPreviewArrayLimit,
        truncated_array_count: truncatedArrays.length,
        omitted_array_item_count: omittedArrayItems,
        truncated_arrays: truncatedArrays
      },
      result
    };
  }

  function resultJsonText(value: unknown, mode: ResultDetailMode): string {
    if (mode === 'summary')
      throw new Error('summary result must not be serialized');
    return JSON.stringify(
      mode === 'preview' ? boundedResultPreview(value) : value, null, 2);
  }

  const neutralLevels = () => Array.from({ length: 10 }, (_, index) => ({
    level: index + 1,
    price: 0,
    quantity_raw: 0
  }));
  const previousStateSchemaTemplate = {
    schema: 'tdx-level2-sdk-host-quote-state-v1',
    base_quote: {
      pre_close_price: 0,
      open_price: 0,
      high_price: 0,
      low_price: 0,
      last_price: 0,
      time_hhmmss_raw: 0,
      special_volume_projected_raw: 0,
      cumulative_volume_raw: 0,
      last_positive_volume_delta_raw: 0,
      amount_raw: 0,
      first_volume_bucket_raw: 0,
      second_volume_bucket_raw: 0,
      host_status_flags_raw: 0,
      host_auxiliary_142_raw: 0
    },
    ask_levels: neutralLevels(),
    bid_levels: neutralLevels(),
    aggregate: {
      average_bid_price: 0,
      total_bid_quantity_raw: 0,
      average_ask_price: 0,
      total_ask_quantity_raw: 0
    }
  };
  const contextSchemaTemplate = {
    security_class_raw: null,
    price_transition_guard_raw: null,
    small_last_price_fallback_predicate_raw: null,
    special_volume_multiplier_raw: null,
    security_auxiliary_dword_73_raw: null,
    host_word_280_raw: null
  };

  const status = new Resource<SessionStatus>();
  const buildResult = new Resource<Level2BuildResult>();
  const decodeResult = new Resource<Level2DecodeResult>();
  const projectResult = new Resource<Level2ProjectResult>();
  const hostProjectionResult = new Resource<Level2ProjectResult>();
  const snapshotTransitionResult = new Resource<Level2SnapshotTransitionResult>();
  let buildDetailMode = $state<ResultDetailMode>('summary');
  let decodeDetailMode = $state<ResultDetailMode>('summary');
  let projectDetailMode = $state<ResultDetailMode>('summary');
  let hostProjectDetailMode = $state<ResultDetailMode>('summary');
  let snapshotDetailMode = $state<ResultDetailMode>('summary');
  let selectedPid = $state(0);
  let buildFormat = $state('direct');
  let buildMarket = $state('0');
  let buildCode = $state('000001');
  let buildKind = $state('transaction');
  let buildInitial = $state(false);
  let buildCursor = $state('0');
  let buildCount = $state('1500');
  let buildWantNumber = $state('80');
  let buildDepth = $state('10');
  let buildAttachInfo = $state(false);
  let buildRepurchaseTime = $state(false);
  let buildPlanHex = $state('00303030303031');
  let build1369Count = $state('11');
  let build1371SideModeRaw = $state('1');
  let build1371SelectedPrice = $state('10.5');
  let build1371Cursor = $state('-1');
  let build1371Count = $state('5000');
  let buildSubscribeDataType = $state('1801');
  let buildSubscribeSymbols = $state('SZ000001\nSH600000');
  let buildFnReqDataType = $state('1801');
  let buildFnReqDataSideModeRaw = $state('1');
  let buildFnReqDataSelectedPrice = $state('10.25');
  let buildCallbackDataType = $state('1801');
  let buildCallbackRegistryModeRaw = $state('9');
  let buildCallbackHostTimeAdvanced = $state<'auto' | 'false' | 'true'>('auto');
  let decodeFormat = $state('sdk-1801');
  let payloadHex = $state('');
  let decodeLimit = $state('20');
  let decodeXorKey = $state('-1');
  let decodeMaxFields = $state('100');
  let decodeSampleBytes = $state('64');
  let decodeSdkJson = $state('');
  let decodeSdkDepth = $state('10');
  let decodeCallbackDataType = $state('1801');
  let decodeCallbackArg5Raw = $state('1');
  let decodeCallbackArg6Raw = $state('0');
  let decodeCallbackRegistryModeRaw = $state('9');
  let projectDataType = $state('1807');
  let projectPayloadHex = $state('');
  let projectPreviousJson = $state(JSON.stringify(previousStateSchemaTemplate, null, 2));
  let projectContextJson = $state(JSON.stringify(contextSchemaTemplate, null, 2));
  let hostProjectDataType = $state('1801');
  let hostProjectMarket = $state('0');
  let hostProjectCode = $state('000001');
  let hostProjectPayloadHex = $state('');
  let snapshotTransitionFormat = $state('sdk-4654-dual-snapshot-transition');
  let snapshotFirstHex = $state('');
  let snapshotRawHex = $state('');
  let snapshotPreviousJson = $state('');

  const modules = $derived(status.data?.modules ?? []);
  const candidates = $derived(status.data?.candidate_pids ?? []);
  const stats = $derived.by(() => {
    const doc = status.data;
    if (!doc) return [];
    return [
      { label: '运行实例', value: doc.ambiguous ? count(candidates.length) : doc.running ? '1' : '0' },
      { label: '当前 PID', value: doc.pid ? String(doc.pid) : '—' },
      { label: '主程序版本', value: doc.tdxw_recognized ? '已识别' : doc.ambiguous ? '待选择' : '未确认' },
      { label: 'tpbus', value: doc.tpbus_recognized ? '已识别' : doc.ambiguous ? '待选择' : '未确认' },
      {
        label: '被动捕获前提',
        value: doc.ready_for_passive_capture ? '具备' : '未具备',
        tone: (doc.ready_for_passive_capture ? 'up' : 'down') as 'up' | 'down'
      }
    ];
  });

  const decoders: Decoder[] = [
    { format: 'direct-transaction', source: '1363/1364', boundary: '秒级逐笔成交；支持 XOR 后捕获体' },
    { format: 'direct-order', source: '1373/1374', boundary: '秒级逐笔委托与撤单状态' },
    { format: 'sdk-1801', source: 'SDK 52 B 固定体', boundary: '逐笔成交、价格、成交量和买卖方向' },
    { format: 'sdk-1802', source: 'SDK 40 B 固定体', boundary: '逐笔委托以及买撤、卖撤状态' },
    { format: 'sdk-1803', source: 'SDK 固定体', boundary: '双边最多 1000 档盘口' },
    { format: 'sdk-18031', source: 'SDK 固定体', boundary: '选中价位最多 5000 笔委托量队列' },
    { format: 'sdk-1804', source: 'SDK 432 B 固定体', boundary: '双价格与两侧各最多 50 个数量槽；不猜买卖侧' },
    { format: 'sdk-1807', source: 'SDK 380 B 固定体', boundary: 'OHLC、量额、买卖十档及总量/均价' },
    { format: 'sdk-18071', source: 'SDK 380 B 固定体', boundary: '与 1807 共布局、保持独立回调 schema' },
    { format: 'sdk-correlation', source: 'TdxW 25 B 本地记录', boundary: '把 SDK 回调键、data type 关联到证券；不是网络包、会话或令牌' },
    { format: 'sdk-callback-invocation', source: 'TdxW sub_68C750 ABI', boundary: '校验 arg5/arg6、精确 SDK body、关联与宿主路由；只离线归一化，不执行回调或投递消息' },
    { format: 'sdk-json-4653', source: 'tpbus SDK JSON', boundary: '已授权分时响应；源序保留，价格按 /1000 的 float32 边界缩放；时间变换只保留证据命名' },
    { format: 'sdk-json-4655', source: 'tpbus SDK JSON', boundary: '已授权逐笔成交响应；反序并归一化时间与方向' },
    { format: 'sdk-json-4671', source: 'tpbus SDK JSON', boundary: '已授权买一/卖一委托队列；选取首买末卖并换算手数' },
    { format: 'sdk-json-4680', source: 'tpbus SDK JSON', boundary: '已授权五/十档快照；归一化档位方向与数量单位' },
    { format: 'tpbus-111', source: 'FastHQ push', boundary: '即时行情与成对多档价量/席位数' },
    { format: 'tpbus-112', source: 'FastHQ push', boundary: '买一/卖一委托队列' },
    { format: 'tpbus-115', source: 'FastHQ 批容器', boundary: '按原始顺序解包 111/112 子体；只显示长度、SHA-256 与有界摘要，不回显 body' },
    { format: 'tcalc-order-flow', source: 'TCalc type 31 · 184 B', boundary: '日级订单流及 53 个精确公式绑定；可直接作为日线公式上下文' },
    { format: 'tcalc-order-side', source: 'TCalc type 104 · 104 B', boundary: 'byte 46 原生方向判断与 ISBUYORDER 标量上下文' },
    { format: 'protobuf', source: '113/114/116', boundary: '仅检查字段号与 wire type，不猜业务 schema' }
  ];

  const buildFormats = [
    { id: 'direct', label: '内置逐笔请求' },
    { id: 'sdk-4653', label: 'SDK 4653' },
    { id: 'sdk-4655', label: 'SDK 4655' },
    { id: 'sdk-4680', label: 'SDK 4680' },
    { id: 'sdk-1807-plan', label: 'SDK 1807 订阅计划' },
    { id: 'sdk-fnreqdata-plan', label: 'SDK fnReqData 单证券计划' },
    { id: 'sdk-fnreqdata-18031-plan', label: 'SDK fnReqData 18031 计划' },
    { id: 'sdk-fnsubscribe-batch-plan', label: 'SDK 批量订阅计划' },
    { id: 'sdk-callback-route-plan', label: 'SDK 回调路由计划' },
    { id: 'tdxw-1369', label: 'TdxW 1369 · 多档盘口' },
    { id: 'tdxw-1371', label: 'TdxW 1371 · 价位委托队列' }
  ];
  const decodeFormats = decoders.map((item) => ({
    id: item.format,
    label: item.format
  }));

  const moduleColumns: Column<ModuleRecord>[] = [
    { key: 'name', label: '模块', width: '150px', value: (row) => row.name },
    { key: 'path', label: '路径', wrap: true, num: true, value: (row) => row.path },
    {
      key: 'recognized', label: '版本', width: '72px',
      value: (row) => row.recognized === undefined ? '已加载' : row.recognized ? '已识别' : '未知'
    }
  ];

  const decoderColumns: Column<Decoder>[] = [
    { key: 'format', label: '--format', width: '160px', num: true, value: (row) => row.format },
    { key: 'source', label: '协议入口', width: '120px', value: (row) => row.source },
    { key: 'boundary', label: '解析边界', wrap: true, value: (row) => row.boundary }
  ];

  function load(pid = selectedPid) {
    selectedPid = pid;
    void status.load(`/api/v1/level2/status?${queryString({ pid: pid || '' })}`);
  }

  function boundedInteger(raw: string, label: string, minimum: number, maximum: number): number {
    const value = Number(raw);
    if (!Number.isSafeInteger(value) || value < minimum || value > maximum)
      throw new Error(`${label} 必须是 ${minimum}..${maximum} 的整数`);
    return value;
  }

  function positiveFloat32(raw: string, label: string): number {
    if (!raw.trim()) throw new Error(`${label}不能为空`);
    const value = Number(raw);
    const converted = Math.fround(value);
    if (!Number.isFinite(value) || value <= 0 || value > 3.4028234663852886e38 ||
        !Number.isFinite(converted) || converted <= 0)
      throw new Error(`${label}必须是可表示为 32 位浮点数的正数`);
    return value;
  }

  function finiteFloat32(raw: string, label: string): number {
    if (!raw.trim()) throw new Error(`${label}不能为空`);
    const value = Number(raw);
    const converted = Math.fround(value);
    if (!Number.isFinite(value) || !Number.isFinite(converted))
      throw new Error(`${label}必须是可表示为 32 位浮点数的有限数`);
    return value;
  }

  function inlineHexBytes(raw: string): number {
    const compact = raw.replace(/[ \t\r\n\f\v]/g, '');
    if (!compact) throw new Error('请粘贴合法会话捕获的十六进制数据');
    if (/[^0-9a-fA-F]/.test(compact))
      throw new Error('十六进制捕获只能包含十六进制数字和 ASCII 空白');
    if (compact.length % 2 !== 0)
      throw new Error('十六进制捕获必须包含偶数个数字');
    const bytes = compact.length / 2;
    if (bytes > 384 * 1024)
      throw new Error('十六进制捕获的原始数据必须不超过 384 KiB');
    return bytes;
  }

  function callbackExpectedBodyBytes(dataType: number, arg5: number): number {
    if (dataType === 1801) return arg5 * 52;
    if (dataType === 1802) return arg5 * 40;
    const fixedSizes: Record<number, number> = {
      1803: 32016, 1804: 432, 1807: 380, 18031: 20012, 18071: 380
    };
    return fixedSizes[dataType] ?? 0;
  }

  function resultScaleLabel(value: Record<string, unknown>): string {
    const byteSize = [value.input_size, value.body_size, value.source_body_size,
      value.size].find((item) => typeof item === 'number' && Number.isFinite(item));
    const recordCount = [value.record_count, value.source_record_count]
      .find((item) => typeof item === 'number' && Number.isFinite(item));
    const parts: string[] = [];
    if (typeof byteSize === 'number') parts.push(`${count(byteSize)} B`);
    if (typeof recordCount === 'number') parts.push(`${count(recordCount)} 条`);
    return parts.length ? parts.join(' · ') : '当前完整响应';
  }

  function switchBuildFormat(next: string) {
    buildFormat = next;
    buildDetailMode = 'summary';
    buildResult.reset();
  }

  function switchBuildCallbackDataType(next: string) {
    buildCallbackDataType = next;
    buildDetailMode = 'summary';
    buildResult.reset();
  }

  function switchDecodeFormat(next: string) {
    decodeFormat = next;
    decodeDetailMode = 'summary';
    decodeResult.reset();
    payloadHex = '';
    decodeSdkJson = '';
  }

  function switchDecodeCallbackDataType(next: string) {
    decodeCallbackDataType = next;
    decodeDetailMode = 'summary';
    decodeResult.reset();
    payloadHex = '';
  }

  function switchProjectDataType(next: string) {
    projectDataType = next;
    projectDetailMode = 'summary';
    projectResult.reset();
    projectPayloadHex = '';
  }

  function switchHostProjectDataType(next: string) {
    hostProjectDataType = next;
    hostProjectDetailMode = 'summary';
    hostProjectionResult.reset();
    hostProjectPayloadHex = '';
  }

  function switchSnapshotTransitionFormat(next: string) {
    snapshotTransitionFormat = next;
    snapshotDetailMode = 'summary';
    snapshotTransitionResult.reset();
    snapshotFirstHex = '';
    snapshotRawHex = '';
  }

  function clearDecodeCapture() {
    payloadHex = '';
    decodeSdkJson = '';
  }

  function clearQuoteCapture() {
    projectPayloadHex = '';
  }

  function clearHostProjectionCapture() {
    hostProjectPayloadHex = '';
  }

  function clearSnapshotCaptures() {
    snapshotFirstHex = '';
    snapshotRawHex = '';
  }

  const decodeCallbackExpectedBytes = $derived.by(() => {
    const dataType = Number(decodeCallbackDataType);
    const arg5 = Number(decodeCallbackArg5Raw);
    return Number.isSafeInteger(arg5) && arg5 > 0
      ? callbackExpectedBodyBytes(dataType, arg5)
      : callbackExpectedBodyBytes(dataType, 0);
  });

  function buildOfflineRequest() {
    buildDetailMode = 'summary';
    buildResult.reset();
    void buildResult.loadWith(async () => {
      const direct = buildFormat === 'direct';
      const tdxwIpc = buildFormat === 'tdxw-1369' || buildFormat === 'tdxw-1371';
      const body: Record<string, unknown> = { format: buildFormat };
      if (buildFormat === 'sdk-callback-route-plan') {
        const dataType = boundedInteger(buildCallbackDataType, '回调数据类型', 1801, 18071);
        if (![1801, 1802, 1803, 1804, 1807, 18031, 18071].includes(dataType))
          throw new Error('回调数据类型必须是 1801、1802、1803、1804、1807、18031 或 18071');
        body.data_type = dataType;
        if (dataType !== 18031)
          body.registry_mode_raw = boundedInteger(
            buildCallbackRegistryModeRaw, '关联表模式原值', 0, 4294967295);
        if (dataType === 1807 && buildCallbackHostTimeAdvanced !== 'auto')
          body.host_time_advanced = buildCallbackHostTimeAdvanced === 'true';
      } else if (buildFormat === 'sdk-fnsubscribe-batch-plan') {
        const symbols = buildSubscribeSymbols.split(/\r?\n/)
          .map((value) => value.trim()).filter(Boolean);
        if (!symbols.length || symbols.length > 100)
          throw new Error('证券列表必须包含 1..100 行');
        for (const symbol of symbols)
          if (!/^(SZ|SH|BJ)\d{6}$/.test(symbol))
            throw new Error(`证券 ${symbol} 必须是大写 SZ/SH/BJ 加六位数字`);
        const document = {
          data_type: boundedInteger(buildSubscribeDataType, '数据类型', 1801, 1803),
          symbols
        };
        if (![1801, 1802, 1803].includes(document.data_type))
          throw new Error('数据类型必须是 1801、1802 或 1803');
        if (new TextEncoder().encode(JSON.stringify(document)).byteLength > 16 * 1024)
          throw new Error('批量订阅紧凑 JSON 必须不超过 16 KiB（UTF-8）');
        body.document = document;
      } else if (buildFormat === 'sdk-1807-plan') {
        if (!buildPlanHex.trim()) throw new Error('请提供 7 字节证券身份记录的十六进制数据');
        body.payload_hex = buildPlanHex;
      } else {
        body.market = boundedInteger(buildMarket, '市场 ID', 0, direct || tdxwIpc ? 2 : 65535);
        body.code = buildCode;
      }
      if (direct) {
        body.kind = buildKind;
        body.initial = buildInitial;
        body.cursor = boundedInteger(buildCursor, '游标', 0, 4294967295);
        body.count = boundedInteger(buildCount, '数量', 1, 1500);
      } else if (buildFormat === 'sdk-fnreqdata-plan') {
        const dataType = boundedInteger(buildFnReqDataType, '数据类型', 1801, 18071);
        if (![1801, 1802, 1803, 1804, 18071].includes(dataType))
          throw new Error('数据类型必须是 1801、1802、1803、1804 或 18071');
        body.data_type = dataType;
        if (dataType === 1801 || dataType === 1802) {
          body.cursor_raw = boundedInteger(buildCursor, '游标原值', 0, 4294967295);
          body.count = boundedInteger(buildCount, '数量', 1, 1500);
        }
      } else if (buildFormat === 'sdk-fnreqdata-18031-plan') {
        body.side_mode_raw = boundedInteger(
          buildFnReqDataSideModeRaw, '方向/模式原值', 0, 255);
        body.selected_price = finiteFloat32(
          buildFnReqDataSelectedPrice, '选中价格');
      } else if (buildFormat === 'sdk-4653') {
        body.attach_info = buildAttachInfo;
        body.repurchase_time = buildRepurchaseTime;
      } else if (buildFormat === 'sdk-4655') {
        body.want_number = boundedInteger(buildWantNumber, '委托数量', 1, 65535);
        body.attach_info = buildAttachInfo;
      } else if (buildFormat === 'sdk-4680') {
        body.depth = boundedInteger(buildDepth, '深度', 5, 10);
      } else if (buildFormat === 'tdxw-1369') {
        body.count = boundedInteger(build1369Count, '档数上限', 1, 1000);
      } else if (buildFormat === 'tdxw-1371') {
        body.side_mode_raw = boundedInteger(build1371SideModeRaw, '方向/模式原值', 0, 255);
        body.selected_price = positiveFloat32(build1371SelectedPrice, '选中价格');
        body.cursor = boundedInteger(build1371Cursor, '续取游标', -2147483648, 2147483647);
        body.count = boundedInteger(build1371Count, '数量上限', 1, 5000);
      }
      return postJson<Level2BuildResult>(
        '/api/v1/level2/build', body, 'level2-build');
    });
  }

  function decodeInlineInput() {
    decodeDetailMode = 'summary';
    decodeResult.reset();
    void decodeResult.loadWith(async () => {
      const submittedPayloadHex = payloadHex;
      const submittedSdkJson = decodeSdkJson;
      const sdkJson = decodeFormat === 'sdk-json-4653' || decodeFormat === 'sdk-json-4655' ||
        decodeFormat === 'sdk-json-4671' || decodeFormat === 'sdk-json-4680';
      const callbackInvocation = decodeFormat === 'sdk-callback-invocation';
      const body: Record<string, unknown> = { format: decodeFormat };
      if (callbackInvocation) {
        const dataType = boundedInteger(
          decodeCallbackDataType, '回调数据类型', 1801, 18071);
        if (![1801, 1802, 1803, 1804, 1807, 18031, 18071].includes(dataType))
          throw new Error('回调数据类型必须是 1801、1802、1803、1804、1807、18031 或 18071');
        const arg5 = boundedInteger(
          decodeCallbackArg5Raw, 'callback arg5 原值', -2147483648, 2147483647);
        if (dataType === 1801 && (arg5 < 1 || arg5 > 7561))
          throw new Error('1801 的 callback arg5 必须是 1..7561 的记录数');
        if (dataType === 1802 && (arg5 < 1 || arg5 > 9830))
          throw new Error('1802 的 callback arg5 必须是 1..9830 的记录数');
        const inputBytes = inlineHexBytes(payloadHex);
        const expectedBytes = callbackExpectedBodyBytes(dataType, arg5);
        if (inputBytes !== expectedBytes)
          throw new Error(`${dataType} 回调体必须恰好是 ${expectedBytes} B，当前为 ${inputBytes} B`);
        body.data_type = dataType;
        body.callback_arg5_raw = arg5;
        body.callback_arg6_raw = boundedInteger(
          decodeCallbackArg6Raw, 'callback arg6 原值', 0, 4294967295);
        if (dataType !== 18031)
          body.registry_mode_raw = boundedInteger(
            decodeCallbackRegistryModeRaw, '关联表模式原值', 0, 4294967295);
        body.payload_hex = payloadHex;
        body.limit = boundedInteger(decodeLimit, '记录上限', 0, 10000);
      } else if (sdkJson) {
        if (!decodeSdkJson.trim()) throw new Error('请粘贴已授权 SDK 返回的 JSON 对象');
        const inputBytes = new TextEncoder().encode(decodeSdkJson).byteLength;
        if (inputBytes > 384 * 1024)
          throw new Error('SDK JSON 必须不超过 384 KiB（UTF-8）');
        let document: unknown;
        try {
          document = JSON.parse(decodeSdkJson);
        } catch {
          throw new Error('SDK JSON 不是合法 JSON');
        }
        if (!document || typeof document !== 'object' || Array.isArray(document))
          throw new Error('SDK JSON 顶层必须是对象');
        body.document = document;
        if (decodeFormat === 'sdk-json-4680') {
          const depth = boundedInteger(decodeSdkDepth, '盘口深度', 5, 10);
          if (depth !== 5 && depth !== 10) throw new Error('盘口深度必须是 5 或 10');
          body.depth = depth;
        } else
          body.limit = boundedInteger(decodeLimit, '记录上限', 0, 10000);
      } else if (decodeFormat === 'protobuf') {
        if (!payloadHex.trim()) throw new Error('请粘贴合法会话捕获的十六进制数据');
        body.payload_hex = payloadHex;
        body.max_fields = boundedInteger(decodeMaxFields, '最大字段数', 1, 10000);
        body.sample_bytes = boundedInteger(decodeSampleBytes, '样本字节', 0, 4096);
      } else {
        if (!payloadHex.trim()) throw new Error('请粘贴合法会话捕获的十六进制数据');
        body.payload_hex = payloadHex;
        if (decodeFormat !== 'tcalc-order-side') {
          // A type-31 document can enter the interpreter only when every record
          // from the caller-owned payload was decoded. Other viewers keep their
          // small display limit so a large tick capture remains responsive.
          body.limit = decodeFormat === 'tcalc-order-flow'
            ? 10000
            : boundedInteger(decodeLimit, '记录上限', 0, 10000);
          if (decodeFormat === 'direct-transaction' || decodeFormat === 'direct-order')
            body.xor_key = boundedInteger(decodeXorKey, 'XOR key', -1, 255);
        }
      }
      const result = await postJson<Level2DecodeResult>(
        '/api/v1/level2/decode', body, 'level2-decode');
      if (sdkJson && decodeSdkJson === submittedSdkJson) decodeSdkJson = '';
      if (!sdkJson && payloadHex === submittedPayloadHex) payloadHex = '';
      return result;
    });
  }

  function parseProjectObject(raw: string, label: string): Record<string, unknown> {
    if (!raw.trim()) throw new Error(`${label}不能为空`);
    let value: unknown;
    try {
      value = JSON.parse(raw);
    } catch {
      throw new Error(`${label}不是合法 JSON`);
    }
    if (!value || typeof value !== 'object' || Array.isArray(value))
      throw new Error(`${label}顶层必须是对象`);
    return value as Record<string, unknown>;
  }

  function validatePreviousState(previous: Record<string, unknown>) {
    if (previous.schema !== 'tdx-level2-sdk-host-quote-state-v1')
      throw new Error('上一状态 schema 必须是 tdx-level2-sdk-host-quote-state-v1');
    if (!previous.base_quote || typeof previous.base_quote !== 'object' ||
        Array.isArray(previous.base_quote))
      throw new Error('上一状态必须包含 base_quote 对象');
    for (const side of ['ask_levels', 'bid_levels']) {
      const levels = previous[side];
      if (!Array.isArray(levels) || levels.length !== 10)
        throw new Error(`上一状态 ${side} 必须恰好包含 10 档`);
    }
    if (!previous.aggregate || typeof previous.aggregate !== 'object' ||
        Array.isArray(previous.aggregate))
      throw new Error('上一状态必须包含 aggregate 对象');
  }

  function validateProjectionContext(context: Record<string, unknown>) {
    const integer = (name: string, minimum: number, maximum: number) => {
      const value = context[name];
      if (typeof value !== 'number' || !Number.isSafeInteger(value) ||
          value < minimum || value > maximum)
        throw new Error(`显式上下文 ${name} 必须是 ${minimum}..${maximum} 的整数`);
    };
    integer('security_class_raw', -2147483648, 2147483647);
    integer('price_transition_guard_raw', -2147483648, 2147483647);
    integer('security_auxiliary_dword_73_raw', 0, 4294967295);
    integer('host_word_280_raw', 0, 65535);
    if (typeof context.small_last_price_fallback_predicate_raw !== 'boolean')
      throw new Error('显式上下文 small_last_price_fallback_predicate_raw 必须是布尔值');
    if (typeof context.special_volume_multiplier_raw !== 'number' ||
        !Number.isFinite(context.special_volume_multiplier_raw) ||
        Math.abs(context.special_volume_multiplier_raw) > 3.4028234663852886e38)
      throw new Error('显式上下文 special_volume_multiplier_raw 必须是有限 float32');
  }

  function projectQuoteTransition() {
    projectDetailMode = 'summary';
    projectResult.reset();
    void projectResult.loadWith(async () => {
      const submittedPayloadHex = projectPayloadHex;
      const dataType = boundedInteger(projectDataType, '投影数据类型', 1807, 18071);
      if (dataType !== 1807 && dataType !== 18071)
        throw new Error('投影数据类型必须是 1807 或 18071');
      const inputBytes = inlineHexBytes(projectPayloadHex);
      if (inputBytes !== 380)
        throw new Error(`1807/18071 回调体必须恰好是 380 B，当前为 ${inputBytes} B`);
      const previous = parseProjectObject(projectPreviousJson, '上一宿主行情状态');
      const context = parseProjectObject(projectContextJson, '显式证据上下文');
      validatePreviousState(previous);
      validateProjectionContext(context);
      const body = {
        format: 'sdk-quote-transition',
        data_type: dataType,
        payload_hex: projectPayloadHex,
        previous,
        context
      };
      if (new TextEncoder().encode(JSON.stringify(body)).byteLength > 384 * 1024)
        throw new Error('投影请求总体积必须不超过 384 KiB（UTF-8）');
      const result = await postJson<Level2ProjectResult>(
        '/api/v1/level2/project', body, 'level2-project');
      if (projectPayloadHex === submittedPayloadHex) projectPayloadHex = '';
      return result;
    });
  }

  function projectHostRecords() {
    hostProjectDetailMode = 'summary';
    hostProjectionResult.reset();
    void hostProjectionResult.loadWith(async () => {
      const submittedPayloadHex = hostProjectPayloadHex;
      const dataType = boundedInteger(hostProjectDataType, '投影数据类型', 1801, 18031);
      if (dataType !== 1801 && dataType !== 1802 && dataType !== 1803 &&
          dataType !== 1804 && dataType !== 18031)
        throw new Error('宿主投影数据类型必须是 1801、1802、1803、1804 或 18031');
      const inputBytes = inlineHexBytes(hostProjectPayloadHex);
      const exactSize = dataType === 1803 ? 32016
        : dataType === 1804 ? 432
        : dataType === 18031 ? 20012 : 0;
      const recordSize = dataType === 1801 ? 52 : 40;
      if (exactSize ? inputBytes !== exactSize
          : !inputBytes || inputBytes % recordSize !== 0)
        throw new Error(exactSize
          ? `${dataType} 回调体必须恰好为 ${exactSize} B，当前为 ${inputBytes} B`
          : `${dataType} 回调体必须是非空且为 ${recordSize} B 的整数倍，当前为 ${inputBytes} B`);
      const format = dataType === 1803
        ? 'sdk-1803-depth-record-projection'
        : dataType === 18031
          ? 'sdk-18031-queue-record-projection'
          : `sdk-${dataType}-host-projection`;
      const body: Record<string, unknown> = {
        format,
        payload_hex: hostProjectPayloadHex
      };
      if (dataType === 1801 || dataType === 1802 || dataType === 18031) {
        const market = boundedInteger(hostProjectMarket, '市场 ID', 0, 2);
        const code = hostProjectCode.trim();
        if (!/^\d{6}$/.test(code)) throw new Error('证券代码必须恰好是 6 位数字');
        body.market = market;
        body.code = code;
      }
      if (new TextEncoder().encode(JSON.stringify(body)).byteLength > 384 * 1024)
        throw new Error('投影请求总体积必须不超过 384 KiB（UTF-8）');
      const result = await postJson<Level2ProjectResult>(
        '/api/v1/level2/project', body, 'level2-project');
      if (hostProjectPayloadHex === submittedPayloadHex)
        hostProjectPayloadHex = '';
      return result;
    });
  }

  function validate4651Previous(previous: Record<string, unknown>) {
    const allowed = [
      'schema', 'decoded_byte_size', 'decoded_sha256', 'raw_byte_size',
      'raw_sha256', 'ready', 'source', 'metadata_complete', 'bodies_emitted'
    ];
    const keys = Object.keys(previous).sort();
    if (keys.length !== allowed.length ||
        keys.some((key) => !allowed.includes(key)))
      throw new Error('4651 previous 必须是 projected_state 的严格 9 字段对象');
    if (previous.schema !== 'tdx-level2-sdk-4651-dual-snapshot-state-v1')
      throw new Error('4651 previous schema 不匹配');
    for (const field of ['decoded_byte_size', 'raw_byte_size']) {
      const value = previous[field];
      if (typeof value !== 'number' || !Number.isSafeInteger(value) ||
          value < 0 || value > 2147483647)
        throw new Error(`4651 previous.${field} 必须是 0..INT32_MAX 的整数`);
    }
    for (const field of ['decoded_sha256', 'raw_sha256']) {
      const value = previous[field];
      if (typeof value !== 'string' || !/^[0-9a-fA-F]{64}$/.test(value))
        throw new Error(`4651 previous.${field} 必须是 64 位十六进制 SHA-256`);
    }
    if (typeof previous.ready !== 'boolean' || typeof previous.source !== 'string')
      throw new Error('4651 previous.ready/source 类型不正确');
    if (previous.metadata_complete !== true || previous.bodies_emitted !== false)
      throw new Error('4651 previous 必须 metadata_complete=true 且 bodies_emitted=false');
  }

  function projectSdkSnapshots() {
    snapshotDetailMode = 'summary';
    snapshotTransitionResult.reset();
    void snapshotTransitionResult.loadWith(async () => {
      const submittedFirstHex = snapshotFirstHex;
      const submittedRawHex = snapshotRawHex;
      const format = snapshotTransitionFormat;
      if (![
        'sdk-4654-dual-snapshot-transition',
        'sdk-4651-dual-snapshot-transition',
        'sdk-4655-companion-raw-transition'
      ].includes(format)) throw new Error('SDK 双快照投影格式不受支持');
      const firstBytes = inlineHexBytes(snapshotFirstHex);
      const rawBytes = inlineHexBytes(snapshotRawHex);
      if (format === 'sdk-4654-dual-snapshot-transition' && firstBytes !== 48)
        throw new Error(`4654 decoded 快照必须恰好为 48 B，当前为 ${firstBytes} B`);
      if (format === 'sdk-4655-companion-raw-transition' && firstBytes !== 46)
        throw new Error(`4655 companion 快照必须恰好为 46 B，当前为 ${firstBytes} B`);
      if (format === 'sdk-4651-dual-snapshot-transition' && firstBytes < 6)
        throw new Error(`4651 decoded 快照至少需要 6 B caller gate，当前为 ${firstBytes} B`);
      if (format === 'sdk-4655-companion-raw-transition' && rawBytes < 57)
        throw new Error(`4655 raw 快照至少需要 57 B dispatcher shape，当前为 ${rawBytes} B`);

      const firstField = format === 'sdk-4655-companion-raw-transition'
        ? 'companion_hex' : 'decoded_hex';
      const body: Record<string, unknown> = {
        format,
        [firstField]: snapshotFirstHex,
        raw_hex: snapshotRawHex
      };
      if (format === 'sdk-4651-dual-snapshot-transition' &&
          snapshotPreviousJson.trim()) {
        const previous = parseProjectObject(
          snapshotPreviousJson, '4651 上一投影状态');
        validate4651Previous(previous);
        body.previous = previous;
      }
      if (new TextEncoder().encode(JSON.stringify(body)).byteLength > 384 * 1024)
        throw new Error('双快照投影请求总体积必须不超过 384 KiB（UTF-8）');
      const result = await postJson<Level2SnapshotTransitionResult>(
        '/api/v1/level2/project', body, 'level2-project');
      if (snapshotFirstHex === submittedFirstHex) snapshotFirstHex = '';
      if (snapshotRawHex === submittedRawHex) snapshotRawHex = '';
      return result;
    });
  }

  function use4651ProjectedState() {
    const projected = snapshotTransitionResult.data?.projected_state;
    if (!projected || typeof projected !== 'object' || Array.isArray(projected))
      throw new Error('当前 4651 结果没有可继续的 projected_state');
    snapshotPreviousJson = JSON.stringify(projected, null, 2);
  }

  function queueProjectedRecordCount(result: Level2ProjectResult): number {
    const first = result.projected_state.first_raw;
    if (!first || typeof first !== 'object' || Array.isArray(first)) return 0;
    return Number((first as Record<string, unknown>).projected_record_count ?? 0);
  }

  function resetPreviousStateTemplate() {
    projectPreviousJson = JSON.stringify(previousStateSchemaTemplate, null, 2);
  }

  function useProjectedStateAsPrevious() {
    const projected = projectResult.data?.projected_state;
    if (!projected || typeof projected !== 'object' || Array.isArray(projected))
      throw new Error('当前没有可继续的 projected_state');
    projectPreviousJson = JSON.stringify(projected, null, 2);
  }

  function useDecodedFormulaContext() {
    const document = decodeResult.data;
    if (!document || (document.schema !== 'tdx-level2-tcalc-order-flow-v1' &&
        document.schema !== 'tdx-level2-tcalc-order-side-v1'))
      throw new Error('当前解码结果不能作为公式上下文');
    stageLevel2FormulaContext(document);
    router.go('/protocol/formulas');
  }

  onMount(() => load(0));
</script>

<PageHeader
  eyebrow="AUTHORIZED DATA · /api/v1/level2/status"
  title="Level2 协议实验室"
  description="只处理本机合法会话产生的数据。页面不会附加进程、发送订阅、读取 QSHQToken 或绕过账号权限。"
  {stats}
>
  {#snippet actions()}
    <Badge tone="neutral">只读</Badge>
    <Button icon="refresh" busy={status.busy} onclick={() => load()}>重新预检</Button>
  {/snippet}
</PageHeader>

<div class="stack">
  <Panel
    title="本机会话前提"
    subtitle={text(status.data?.reason)}
    busy={status.busy}
    error={status.error}
    onRetry={() => load()}
  >
    {#if candidates.length}
      <div class="candidates">
        <span>检测到多个实例，请选择：</span>
        {#each candidates as pid (pid)}
          <Button variant={selectedPid === pid ? 'primary' : 'default'} onclick={() => load(pid)}>PID {pid}</Button>
        {/each}
      </div>
    {/if}
    {#if status.data && !status.data.ambiguous}
      <div class="guards">
        <Badge tone={status.data.attached ? 'down' : 'up'}>未附加：{status.data.attached ? '否' : '是'}</Badge>
        <Badge tone={status.data.subscription_sent ? 'down' : 'up'}>未发送订阅：{status.data.subscription_sent ? '否' : '是'}</Badge>
        <Badge tone={status.data.token_accessed ? 'down' : 'up'}>未读取票据：{status.data.token_accessed ? '否' : '是'}</Badge>
        <Badge tone={status.data.entitlement_bypass ? 'down' : 'up'}>未绕过权限：{status.data.entitlement_bypass ? '否' : '是'}</Badge>
      </div>
      <DataTable columns={moduleColumns} rows={modules} rowKey={(row) => row.path} />
    {/if}
  </Panel>

  <div class="offline-tools">
    <Panel
      title="请求与回调离线计划"
      subtitle="POST /api/v1/level2/build · 只生成字节或宿主路由元数据，不连接、不调用 SDK、不投递消息"
      busy={buildResult.busy}
      error={buildResult.error}
    >
      <div class="tool-body">
        <div class="tool-controls">
          <Select bind:value={buildFormat} options={buildFormats} width="170px" label="请求格式" onChange={switchBuildFormat} />
          {#if buildFormat !== 'sdk-1807-plan' && buildFormat !== 'sdk-fnsubscribe-batch-plan' && buildFormat !== 'sdk-callback-route-plan'}
            <TextInput bind:value={buildMarket} width="92px" label="市场 ID" placeholder="市场 ID" onEnter={buildOfflineRequest} />
            <TextInput bind:value={buildCode} width="140px" label="证券代码" placeholder="证券代码" onEnter={buildOfflineRequest} />
          {/if}
          {#if buildFormat === 'direct'}
            <Select
              bind:value={buildKind}
              options={[{ id: 'transaction', label: '逐笔成交' }, { id: 'order', label: '逐笔委托' }]}
              width="160px"
              label="请求类型"
            />
            <TextInput bind:value={buildCursor} width="110px" label="游标" placeholder="游标" onEnter={buildOfflineRequest} />
            <TextInput bind:value={buildCount} width="100px" label="数量" placeholder="1..1500" onEnter={buildOfflineRequest} />
            <label class="toggle"><input type="checkbox" bind:checked={buildInitial} /><span>初始命令（1363/1373）</span></label>
          {:else if buildFormat === 'sdk-fnreqdata-plan'}
            <Select
              bind:value={buildFnReqDataType}
              options={[
                { id: '1801', label: '1801 · 逐笔成交' },
                { id: '1802', label: '1802 · 逐笔委托' },
                { id: '1803', label: '1803 · 多档盘口' },
                { id: '1804', label: '1804 · 价位队列' },
                { id: '18071', label: '18071 · 扩展行情' }
              ]}
              width="190px"
              label="数据类型"
            />
            {#if buildFnReqDataType === '1801' || buildFnReqDataType === '1802'}
              <TextInput bind:value={buildCursor} width="132px" label="游标原值" placeholder="0..4294967295" onEnter={buildOfflineRequest} />
              <TextInput bind:value={buildCount} width="100px" label="数量" placeholder="1..1500" onEnter={buildOfflineRequest} />
            {/if}
          {:else if buildFormat === 'sdk-fnreqdata-18031-plan'}
            <TextInput bind:value={buildFnReqDataSideModeRaw} width="132px" label="方向/模式原值" placeholder="0..255" onEnter={buildOfflineRequest} />
            <TextInput bind:value={buildFnReqDataSelectedPrice} width="116px" label="选中价格" placeholder="价格" onEnter={buildOfflineRequest} />
          {:else if buildFormat === 'sdk-4655'}
            <TextInput bind:value={buildWantNumber} width="130px" label="委托数量" placeholder="want number" onEnter={buildOfflineRequest} />
          {:else if buildFormat === 'sdk-4680'}
            <Select
              bind:value={buildDepth}
              options={[{ id: '5', label: '五档' }, { id: '10', label: '十档' }]}
              width="100px"
              label="盘口深度"
            />
          {:else if buildFormat === 'tdxw-1369'}
            <TextInput bind:value={build1369Count} width="116px" label="档数上限" placeholder="1..1000" onEnter={buildOfflineRequest} />
          {:else if buildFormat === 'tdxw-1371'}
            <TextInput bind:value={build1371SideModeRaw} width="132px" label="方向/模式原值" placeholder="0..255" onEnter={buildOfflineRequest} />
            <TextInput bind:value={build1371SelectedPrice} width="116px" label="选中价格" placeholder="价格" onEnter={buildOfflineRequest} />
            <TextInput bind:value={build1371Cursor} width="116px" label="续取游标" placeholder="-1" onEnter={buildOfflineRequest} />
            <TextInput bind:value={build1371Count} width="116px" label="数量上限" placeholder="1..5000" onEnter={buildOfflineRequest} />
          {:else if buildFormat === 'sdk-fnsubscribe-batch-plan'}
            <Select
              bind:value={buildSubscribeDataType}
              options={[
                { id: '1801', label: '1801 · 逐笔成交' },
                { id: '1802', label: '1802 · 逐笔委托' },
                { id: '1803', label: '1803 · 多档盘口' }
              ]}
              width="170px"
              label="数据类型"
            />
          {:else if buildFormat === 'sdk-callback-route-plan'}
            <Select
              bind:value={buildCallbackDataType}
              onChange={switchBuildCallbackDataType}
              options={[
                { id: '1801', label: '1801 · 逐笔成交' },
                { id: '1802', label: '1802 · 逐笔委托' },
                { id: '1803', label: '1803 · 多档盘口' },
                { id: '1804', label: '1804 · 价位队列' },
                { id: '1807', label: '1807 · 行情更新' },
                { id: '18031', label: '18031 · 指定价队列' },
                { id: '18071', label: '18071 · 扩展行情' }
              ]}
              width="190px"
              label="回调数据类型"
            />
            {#if buildCallbackDataType !== '18031'}
              <TextInput bind:value={buildCallbackRegistryModeRaw} width="132px" label="关联表模式原值" placeholder="0..4294967295" onEnter={buildOfflineRequest} />
            {/if}
            {#if buildCallbackDataType === '1807'}
              <Select
                bind:value={buildCallbackHostTimeAdvanced}
                options={[
                  { id: 'auto', label: '自动 · 保留三分支' },
                  { id: 'false', label: '否 · 普通路由' },
                  { id: 'true', label: '是 · 时间推进路由' }
                ]}
                width="190px"
                label="宿主时间已推进"
              />
            {/if}
          {/if}
          {#if buildFormat === 'sdk-4653' || buildFormat === 'sdk-4655'}
            <label class="toggle"><input type="checkbox" bind:checked={buildAttachInfo} /><span>附加信息</span></label>
          {/if}
          {#if buildFormat === 'sdk-4653'}
            <label class="toggle"><input type="checkbox" bind:checked={buildRepurchaseTime} /><span>回购时间</span></label>
          {/if}
          <Button variant="primary" icon="market" busy={buildResult.busy} onclick={buildOfflineRequest}>构造</Button>
        </div>
        {#if buildFormat === 'sdk-1807-plan'}
          <label class="payload-editor">
            <span>每只证券为 7 字节：1 字节市场原值 + 6 字节 ASCII 代码，最多 100 条。结果仅生成待解析计划，不生成或发送网络请求。</span>
            <textarea
              bind:value={buildPlanHex}
              rows="4"
              maxlength="1400"
              spellcheck="false"
              placeholder="例如深市 000001：00303030303031"
            ></textarea>
          </label>
        {:else if buildFormat === 'sdk-fnsubscribe-batch-plan'}
          <label class="payload-editor">
            <span>每行一个预解析证券身份：大写 SZ/SH/BJ + 六位数字，1..100 行。只生成六槽 SDK 逻辑调用与 mode-9 关联模板，不执行订阅。</span>
            <textarea
              bind:value={buildSubscribeSymbols}
              rows="6"
              maxlength="10000"
              spellcheck="false"
              placeholder={'SZ000001\nSH600000\nBJ430001'}
            ></textarea>
          </label>
        {:else if buildFormat === 'sdk-callback-route-plan'}
          <p class="tool-note">只投影 TdxW 宿主的回调关联、消息 ID、同步/异步分支与未解析接收方条件；不会调用 SDK、执行回调、投递窗口消息、构造网络字节或发送订阅。</p>
        {:else if buildFormat === 'sdk-fnreqdata-plan'}
          <p class="tool-note">只生成 fnReqData 的 12 个 ABI 槽与 25 B callback-correlation 模板。1801/1802 使用显式游标和数量；1803/1804/18071 固定为 cursor=0、count=1。不会调用 SDK、构造 wire 或发送请求。</p>
        {:else if buildFormat === 'sdk-fnreqdata-18031-plan'}
          <p class="tool-note">只生成指定价位 18031 的 12 个 ABI 槽；方向/模式仅按原始值映射 selector，价格先落 float32 再提升为 ABI double。关联仍需 live host-global context，不调用 SDK 或发送请求。</p>
        {:else if buildFormat === 'tdxw-1369'}
          <p class="tool-note">固定 40 字节的 TdxW 内部 IPC 请求；11/1000 是已观察到的常见档数。它不是 SDK 网络请求。</p>
        {:else if buildFormat === 'tdxw-1371'}
          <p class="tool-note">固定 48 字节的 TdxW 内部 IPC 请求；方向枚举与原客户端“模式值 +10”分支尚未标定，因此这里只接受最终原始字节。</p>
        {/if}
        {#if buildResult.data}
          <div class="result-meta">
            <Badge tone="up">离线</Badge>
            <span>{buildResult.data.format}</span>
            {#if buildResult.data.size !== undefined}
              <span>{count(buildResult.data.size)} B</span>
            {/if}
            <span>网络请求 {count(buildResult.data.network_requests)}</span>
            {#if buildDetailMode === 'summary'}
              <Button onclick={() => { buildDetailMode = 'preview'; }}>查看有界预览</Button>
            {:else}
              <Button onclick={() => { buildDetailMode = 'summary'; }}>收起详情</Button>
              {#if buildDetailMode === 'preview'}
                <Button onclick={() => { buildDetailMode = 'full'; }}>显示完整 JSON · {resultScaleLabel(buildResult.data)}</Button>
              {:else}
                <Button onclick={() => { buildDetailMode = 'preview'; }}>返回有界预览</Button>
              {/if}
            {/if}
          </div>
          {#if buildDetailMode !== 'summary'}
            <pre class="result-code">{resultJsonText(buildResult.data, buildDetailMode)}</pre>
          {/if}
        {:else if !buildResult.busy && !buildResult.error}
          <p class="tool-note">生成结果只返回请求字节和元数据；页面不会打开 Level2 会话。</p>
        {/if}
      </div>
    </Panel>

    <Panel
      title="合法捕获离线解码"
      subtitle="POST /api/v1/level2/decode · 仅接受内联捕获或 SDK JSON，不读取服务器文件"
      busy={decodeResult.busy}
      error={decodeResult.error}
    >
      <div class="tool-body">
        <div class="tool-controls">
          <Select bind:value={decodeFormat} options={decodeFormats} width="190px" label="解码格式" onChange={switchDecodeFormat} />
          {#if decodeFormat === 'protobuf'}
            <TextInput bind:value={decodeMaxFields} width="116px" label="最大字段数" placeholder="最大字段数" onEnter={decodeInlineInput} />
            <TextInput bind:value={decodeSampleBytes} width="116px" label="样本字节" placeholder="样本字节" onEnter={decodeInlineInput} />
          {:else if decodeFormat === 'sdk-json-4680'}
            <Select
              bind:value={decodeSdkDepth}
              options={[{ id: '5', label: '五档' }, { id: '10', label: '十档' }]}
              width="100px"
              label="盘口深度"
            />
          {:else if decodeFormat === 'tcalc-order-flow'}
            <span class="tool-note">完整解码全部 type-31 记录，以便公式物化</span>
          {:else if decodeFormat === 'sdk-callback-invocation'}
            <Select
              bind:value={decodeCallbackDataType}
              onChange={switchDecodeCallbackDataType}
              options={[
                { id: '1801', label: '1801 · 逐笔成交' },
                { id: '1802', label: '1802 · 逐笔委托' },
                { id: '1803', label: '1803 · 多档盘口' },
                { id: '1804', label: '1804 · 价位队列' },
                { id: '1807', label: '1807 · 行情更新' },
                { id: '18031', label: '18031 · 指定价队列' },
                { id: '18071', label: '18071 · 扩展行情' }
              ]}
              width="190px"
              label="回调数据类型"
            />
            <TextInput bind:value={decodeCallbackArg5Raw} width="132px" label="callback arg5 原值" placeholder="int32" onEnter={decodeInlineInput} />
            <TextInput bind:value={decodeCallbackArg6Raw} width="132px" label="callback arg6 原值" placeholder="u32" onEnter={decodeInlineInput} />
            {#if decodeCallbackDataType !== '18031'}
              <TextInput bind:value={decodeCallbackRegistryModeRaw} width="132px" label="关联表模式原值" placeholder="u32" onEnter={decodeInlineInput} />
            {/if}
            <TextInput bind:value={decodeLimit} width="110px" label="记录上限" placeholder="0..10000" onEnter={decodeInlineInput} />
          {:else if decodeFormat !== 'tcalc-order-side'}
            <TextInput bind:value={decodeLimit} width="110px" label="记录上限" placeholder="记录上限" onEnter={decodeInlineInput} />
            {#if decodeFormat === 'direct-transaction' || decodeFormat === 'direct-order'}
              <TextInput bind:value={decodeXorKey} width="100px" label="XOR key" placeholder="XOR key" onEnter={decodeInlineInput} />
            {/if}
          {/if}
          <Button variant="primary" icon="search" busy={decodeResult.busy} onclick={decodeInlineInput}>解码</Button>
          <Button onclick={clearDecodeCapture}>清除捕获</Button>
        </div>
        {#if decodeFormat === 'sdk-json-4653' || decodeFormat === 'sdk-json-4655' || decodeFormat === 'sdk-json-4671' || decodeFormat === 'sdk-json-4680'}
          {#if decodeFormat === 'sdk-json-4653'}
            <p class="tool-note">只接受严格的 Data 数组：首条必须含 reference_price；价格按原生 float32 /1000，datetime 只展示 raw 与证据命名的 time_u16_raw，不解释为日期、分钟或秒。结果不回显原始 document、不生成 native body（每记录 18 B 仅为恢复布局说明），不读取文件、不调用 SDK、不联网。</p>
          {/if}
          <label class="payload-editor">
            <span>已授权 SDK 返回 JSON（仅内联提交，成功后自动清空输入框；UTF-8 上限 384 KiB）</span>
            <textarea
              bind:value={decodeSdkJson}
              rows="7"
              maxlength="393216"
              spellcheck="false"
              placeholder="请粘贴包含 Data 字段的 JSON 对象"
            ></textarea>
          </label>
        {:else}
          {#if decodeFormat === 'tpbus-115'}
            <p class="tool-note">只解析首个外层长度段；内层 discriminator 0/1 映射 111，其余映射 112。零长子体跳过，截断处停止；结果不回显子体、不访问 EventBus、不调用 SDK、不投递消息、不联网。</p>
          {/if}
          <label class="payload-editor">
            {#if decodeFormat === 'sdk-callback-invocation'}
              <span>已授权 SDK 回调 body：必须恰好 {count(decodeCallbackExpectedBytes)} B。arg5 仅对 1801/1802 表示记录数；固定体只保留原值。1807 会保留三条未决宿主路由，但不会投递消息。</span>
            {:else}
              <span>十六进制捕获（允许空格和换行，成功后自动清空输入框；原始数据上限 384 KiB）</span>
            {/if}
            <textarea
              bind:value={payloadHex}
              rows="7"
              maxlength="900000"
              spellcheck="false"
              placeholder="例如：000000000000..."
            ></textarea>
          </label>
        {/if}
        {#if decodeResult.data}
          <div class="result-meta">
            <Badge tone="up">离线</Badge>
            <span>{decodeResult.data.schema}</span>
            <span>输入 {count(decodeResult.data.input_size)} B</span>
            <span>未读取文件</span>
            {#if decodeResult.data.schema === 'tdx-level2-tcalc-order-flow-v1' || decodeResult.data.schema === 'tdx-level2-tcalc-order-side-v1'}
              <Button icon="market" onclick={useDecodedFormulaContext}>用于公式解释</Button>
            {/if}
            {#if decodeDetailMode === 'summary'}
              <Button onclick={() => { decodeDetailMode = 'preview'; }}>查看有界预览</Button>
            {:else}
              <Button onclick={() => { decodeDetailMode = 'summary'; }}>收起详情</Button>
              {#if decodeDetailMode === 'preview'}
                <Button onclick={() => { decodeDetailMode = 'full'; }}>显示完整 JSON · {resultScaleLabel(decodeResult.data)}</Button>
              {:else}
                <Button onclick={() => { decodeDetailMode = 'preview'; }}>返回有界预览</Button>
              {/if}
            {/if}
          </div>
          {#if decodeDetailMode !== 'summary'}
            <pre class="result-code decode-output">{resultJsonText(decodeResult.data, decodeDetailMode)}</pre>
          {/if}
        {/if}
      </div>
    </Panel>
  </div>

  <Panel
    title="1807/18071 宿主行情状态投影"
    subtitle="POST /api/v1/level2/project · 精确 380 B 内联快照 + 调用者提供的上一状态与证据上下文"
    busy={projectResult.busy}
    error={projectResult.error}
  >
    <div class="tool-body">
      <div class="tool-controls">
        <Select
          bind:value={projectDataType}
          onChange={switchProjectDataType}
          options={[
            { id: '1807', label: '1807 · 行情更新' },
            { id: '18071', label: '18071 · 扩展行情' }
          ]}
          width="180px"
          label="回调数据类型"
        />
        <Button variant="primary" icon="market" busy={projectResult.busy} onclick={projectQuoteTransition}>投影下一状态</Button>
        <Button icon="refresh" onclick={resetPreviousStateTemplate}>恢复零值 schema 模板</Button>
        <Button onclick={clearQuoteCapture}>清除捕获</Button>
      </div>
      <p class="tool-note">
        零值模板只用于展示可复制的状态 schema，不代表任何证券。上下文中的 <code>null</code> 必须用你自有合法捕获的六个原始值替换；页面不推断证券类别。
      </p>
      <div class="projection-inputs">
        <label class="payload-editor projection-payload">
          <span>已授权的 SDK 回调 body 十六进制（只能内联，必须恰好 380 B；成功后自动清空）</span>
          <textarea
            bind:value={projectPayloadHex}
            rows="8"
            maxlength="900000"
            spellcheck="false"
            placeholder="760 个十六进制数字，可含 ASCII 空白"
          ></textarea>
        </label>
        <label class="payload-editor">
          <span>上一宿主行情状态 JSON（首次为可复制的零值 schema 模板）</span>
          <textarea
            bind:value={projectPreviousJson}
            rows="15"
            spellcheck="false"
            aria-label="上一宿主行情状态 JSON"
          ></textarea>
        </label>
        <label class="payload-editor">
          <span>显式证据上下文 JSON（不会自动填充或伪造证券上下文）</span>
          <textarea
            bind:value={projectContextJson}
            rows="15"
            spellcheck="false"
            aria-label="宿主行情投影证据上下文 JSON"
          ></textarea>
        </label>
      </div>
      {#if projectResult.data}
        <div class="result-meta">
          <Badge tone="up">离线</Badge>
          <span>{projectResult.data.schema}</span>
          <span>输入 {count(projectResult.data.input_size)} B</span>
          <span>未读取文件</span>
          <Button icon="refresh" onclick={useProjectedStateAsPrevious}>以 projected_state 继续</Button>
          {#if projectDetailMode === 'summary'}
            <Button onclick={() => { projectDetailMode = 'preview'; }}>查看有界预览</Button>
          {:else}
            <Button onclick={() => { projectDetailMode = 'summary'; }}>收起详情</Button>
            {#if projectDetailMode === 'preview'}
              <Button onclick={() => { projectDetailMode = 'full'; }}>显示完整 JSON · {resultScaleLabel(projectResult.data)}</Button>
            {:else}
              <Button onclick={() => { projectDetailMode = 'preview'; }}>返回有界预览</Button>
            {/if}
          {/if}
        </div>
        {#if projectDetailMode !== 'summary'}
          <pre class="result-code projection-output">{resultJsonText(projectResult.data, projectDetailMode)}</pre>
        {/if}
      {:else if !projectResult.busy && !projectResult.error}
        <p class="tool-note">结果只包含解析字段、转移判定与投影状态；不保留原始输入，不调用 SDK、不执行回调、不投递宿主消息、不联网、不绕过授权。</p>
      {/if}
    </div>
  </Panel>

  <Panel
    title="1801/1802/1803/18031/1804 宿主记录与槽投影"
    subtitle="POST /api/v1/level2/project · 已授权逐笔、委托、深度与价位队列回调体"
    busy={hostProjectionResult.busy}
    error={hostProjectionResult.error}
  >
    <div class="tool-body">
      <div class="tool-controls">
        <Select
          bind:value={hostProjectDataType}
          onChange={switchHostProjectDataType}
          options={[
            { id: '1801', label: '1801 · 逐笔成交（52 B/条）' },
            { id: '1802', label: '1802 · 逐笔委托（40 B/条）' },
            { id: '1803', label: '1803 · 深度记录（精确 32016 B）' },
            { id: '18031', label: '18031 · 队列记录（精确 20012 B）' },
            { id: '1804', label: '1804 · 价位队列（精确 432 B）' }
          ]}
          width="290px"
          label="回调数据类型"
        />
        {#if hostProjectDataType === '1801' || hostProjectDataType === '1802' || hostProjectDataType === '18031'}
          <TextInput bind:value={hostProjectMarket} width="92px" label="市场 ID" placeholder="0..2" onEnter={projectHostRecords} />
          <TextInput bind:value={hostProjectCode} width="140px" label="证券代码" placeholder="6 位数字" onEnter={projectHostRecords} />
        {/if}
        <Button variant="primary" icon="market" busy={hostProjectionResult.busy} onclick={projectHostRecords}>投影宿主记录</Button>
        <Button onclick={clearHostProjectionCapture}>清除捕获</Button>
      </div>
      <p class="tool-note">
        1801/1802/18031 的证券身份仅用于恢复 TdxW 品种类别与证据中的数量除数；1803 生成两组 13 B 深度记录，18031 生成 6 B 队列记录，1804 生成 105 个 raw f32 槽。字段保留 raw/first/second 命名，不推断买卖业务含义。
      </p>
      <label class="payload-editor">
        <span>已授权 SDK 回调体十六进制：1801 为 52 B 整数倍，1802 为 40 B 整数倍，1803/18031/1804 分别恰好 32016/20012/432 B；只能内联，成功后自动清空</span>
        <textarea
          bind:value={hostProjectPayloadHex}
          rows="9"
          maxlength="900000"
          spellcheck="false"
          placeholder="可含 ASCII 空白"
        ></textarea>
      </label>
      {#if hostProjectionResult.data}
        <div class="result-meta">
          <Badge tone="up">离线</Badge>
          <span>{hostProjectionResult.data.schema}</span>
          <span>输入 {count(hostProjectionResult.data.input_size)} B</span>
          {#if hostProjectionResult.data.data_type === 1804}
            <span>{count(Number(hostProjectionResult.data.projected_state.slot_count ?? 0))} 槽 / {count(Number(hostProjectionResult.data.projected_state.byte_size ?? 0))} B</span>
          {:else if hostProjectionResult.data.data_type === 1803}
            <span>13 B × {count(Number(hostProjectionResult.data.projected_state.projected_record_count ?? 0))}</span>
          {:else if hostProjectionResult.data.data_type === 18031}
            <span>6 B × {count(queueProjectedRecordCount(hostProjectionResult.data))}</span>
          {:else}
            <span>20 B × {count(Number(hostProjectionResult.data.projected_state.record_count ?? 0))}</span>
          {/if}
          <span>未读取文件</span>
          {#if hostProjectDetailMode === 'summary'}
            <Button onclick={() => { hostProjectDetailMode = 'preview'; }}>查看有界预览</Button>
          {:else}
            <Button onclick={() => { hostProjectDetailMode = 'summary'; }}>收起详情</Button>
            {#if hostProjectDetailMode === 'preview'}
              <Button onclick={() => { hostProjectDetailMode = 'full'; }}>显示完整 JSON · {resultScaleLabel(hostProjectionResult.data)}</Button>
            {:else}
              <Button onclick={() => { hostProjectDetailMode = 'preview'; }}>返回有界预览</Button>
            {/if}
          {/if}
        </div>
        {#if hostProjectDetailMode !== 'summary'}
          <pre class="result-code projection-output">{resultJsonText(hostProjectionResult.data, hostProjectDetailMode)}</pre>
        {/if}
      {:else if !hostProjectionResult.busy && !hostProjectionResult.error}
        <p class="tool-note">页面只做本地字段投影，不调用 SDK、不执行回调、不修改宿主存储、不投递消息、不联网、不绕过授权。</p>
      {/if}
    </div>
  </Panel>

  <Panel
    title="4654 / 4651 / 4655 双快照状态转换"
    subtitle="POST /api/v1/level2/project · 显式内联 decoded/companion 与 raw 快照；不从 raw 伪造另一份状态"
    busy={snapshotTransitionResult.busy}
    error={snapshotTransitionResult.error}
  >
    <div class="tool-body">
      <div class="tool-controls">
        <Select
          bind:value={snapshotTransitionFormat}
          onChange={switchSnapshotTransitionFormat}
          options={[
            { id: 'sdk-4654-dual-snapshot-transition', label: '4654 · decoded 48 B + raw' },
            { id: 'sdk-4651-dual-snapshot-transition', label: '4651 · size replace / retain' },
            { id: 'sdk-4655-companion-raw-transition', label: '4655 · companion 46 B + raw' }
          ]}
          width="270px"
          label="状态转换格式"
        />
        <Button variant="primary" icon="market" busy={snapshotTransitionResult.busy} onclick={projectSdkSnapshots}>投影状态</Button>
        <Button onclick={clearSnapshotCaptures}>清除捕获</Button>
        {#if snapshotTransitionFormat === 'sdk-4651-dual-snapshot-transition'}
          <Button icon="refresh" onclick={() => { snapshotPreviousJson = ''; }}>清空 previous</Button>
        {/if}
      </div>
      <p class="tool-note">
        {#if snapshotTransitionFormat === 'sdk-4654-dual-snapshot-transition'}
          4654 只复现证据闭合的 48 B decoded 与非空 raw 双快照无条件替换；两份输入都必须由调用者显式提供。
        {:else if snapshotTransitionFormat === 'sdk-4651-dual-snapshot-transition'}
          4651 先验证 decoded +2 的 caller gate，再按 raw 逻辑长度决定替换或保留；可把 projected_state 原样作为下一次 previous。
        {:else}
          4655 的 46 B companion 是 caller 克隆的既有本地快照，raw 需满足 dispatcher shape；宿主 gate 未知，所以只输出条件候选，不声称真正替换。
        {/if}
      </p>
      <div class="projection-inputs">
        <label class="payload-editor">
          <span>{snapshotTransitionFormat === 'sdk-4655-companion-raw-transition' ? 'companion_hex（精确 46 B）' : snapshotTransitionFormat === 'sdk-4654-dual-snapshot-transition' ? 'decoded_hex（精确 48 B）' : 'decoded_hex（非空、至少 6 B，+2 必须为 0xffffffff）'}</span>
          <textarea
            bind:value={snapshotFirstHex}
            rows="10"
            maxlength="900000"
            spellcheck="false"
            placeholder="只接受内联十六进制与 ASCII 空白"
          ></textarea>
        </label>
        <label class="payload-editor">
          <span>raw_hex（显式合法捕获；成功后自动清空输入框）</span>
          <textarea
            bind:value={snapshotRawHex}
            rows="10"
            maxlength="900000"
            spellcheck="false"
            placeholder="只接受内联十六进制与 ASCII 空白"
          ></textarea>
        </label>
        {#if snapshotTransitionFormat === 'sdk-4651-dual-snapshot-transition'}
          <label class="payload-editor">
            <span>previous JSON（可省略；只接受上一结果的严格 9 字段 projected_state）</span>
            <textarea
              bind:value={snapshotPreviousJson}
              rows="10"
              spellcheck="false"
              aria-label="4651 上一投影状态 JSON"
              placeholder="首次留空；之后可点击“以 projected_state 继续”"
            ></textarea>
          </label>
        {/if}
      </div>
      {#if snapshotTransitionResult.data}
        <div class="result-meta">
          <Badge tone="up">离线</Badge>
          <span>{snapshotTransitionResult.data.schema}</span>
          <span>总输入 {count(snapshotTransitionResult.data.input_size)} B</span>
          <span>未读取文件</span>
          {#if snapshotTransitionResult.data.format === 'sdk-4651-dual-snapshot-transition'}
            <Button icon="refresh" onclick={use4651ProjectedState}>以 projected_state 继续</Button>
          {/if}
          {#if snapshotDetailMode === 'summary'}
            <Button onclick={() => { snapshotDetailMode = 'preview'; }}>查看有界预览</Button>
          {:else}
            <Button onclick={() => { snapshotDetailMode = 'summary'; }}>收起详情</Button>
            {#if snapshotDetailMode === 'preview'}
              <Button onclick={() => { snapshotDetailMode = 'full'; }}>显示完整 JSON · {resultScaleLabel(snapshotTransitionResult.data)}</Button>
            {:else}
              <Button onclick={() => { snapshotDetailMode = 'preview'; }}>返回有界预览</Button>
            {/if}
          {/if}
        </div>
        {#if snapshotDetailMode !== 'summary'}
          <pre class="result-code projection-output">{resultJsonText(snapshotTransitionResult.data, snapshotDetailMode)}</pre>
        {/if}
      {:else if !snapshotTransitionResult.busy && !snapshotTransitionResult.error}
        <p class="tool-note">响应只保留长度、SHA-256、替换判定与投影元数据；不回显输入 body，不调用 SDK、不写宿主状态、不投递消息、不联网、不绕过授权。</p>
      {/if}
    </div>
  </Panel>

  <Panel
    flush
    title="纯 C++ 离线解码器"
    subtitle="CLI 可读取本地输入；网页只接受内联十六进制或 SDK JSON"
  >
    <DataTable columns={decoderColumns} rows={decoders} rowKey={(row) => row.format} />
  </Panel>

  <Panel title="证据边界">
    <div class="boundary">
      <p><strong>已确认：</strong>请求字节、固定体布局、tpbus 111/112 字段边界、未知 PB 的 wire 编码。</p>
      <p><strong>仍需合法样本：</strong><code>FastHQ.Subscribe.LX</code> 与逐笔/深度窗口的最终动态对应、字段单位、1371 方向枚举及模式值 +10 的业务含义。</p>
      <p><strong>不会执行：</strong>匿名直连 L2、发送上方构造结果、构造登录票据、读取账号令牌、修改客户端参数或返回值。</p>
    </div>
  </Panel>
</div>

<style>
  .stack {
    display: flex;
    flex-direction: column;
    gap: var(--sp-3);
    min-height: 0;
  }

  .candidates,
  .guards {
    display: flex;
    align-items: center;
    flex-wrap: wrap;
    gap: var(--sp-2);
    padding: 0 var(--sp-3) var(--sp-3);
    font-size: var(--fs-micro);
    color: var(--fg-mute);
  }

  .offline-tools {
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
    gap: var(--sp-3);
    align-items: start;
  }

  .tool-body {
    display: grid;
    gap: var(--sp-3);
    padding: 0 var(--sp-3) var(--sp-3);
    min-width: 0;
  }

  .projection-inputs {
    display: grid;
    grid-template-columns: minmax(0, 0.8fr) minmax(0, 1.1fr) minmax(0, 0.9fr);
    gap: var(--sp-3);
    align-items: start;
    min-width: 0;
  }

  .tool-controls,
  .result-meta {
    display: flex;
    align-items: center;
    flex-wrap: wrap;
    gap: var(--sp-2);
  }

  .toggle {
    display: inline-flex;
    align-items: center;
    gap: var(--sp-1);
    height: var(--h-control);
    padding: 0 var(--sp-2);
    color: var(--fg-dim);
    font-size: var(--fs-micro);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
    background: var(--bg-input);
  }

  .payload-editor {
    display: grid;
    gap: var(--sp-2);
    min-width: 0;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }

  .payload-editor textarea,
  .result-code {
    width: 100%;
    box-sizing: border-box;
    margin: 0;
    padding: var(--sp-3);
    color: var(--fg);
    background: var(--bg-input);
    border: 1px solid var(--line-strong);
    border-radius: var(--radius);
    font-family: var(--font-num);
    font-size: var(--fs-micro);
    line-height: 1.45;
    overflow: auto;
  }

  .payload-editor textarea {
    resize: vertical;
  }

  .payload-editor textarea:focus {
    outline: none;
    border-color: var(--focus);
  }

  .result-code {
    max-height: 220px;
    white-space: pre-wrap;
    overflow-wrap: anywhere;
  }

  .decode-output {
    max-height: 420px;
    white-space: pre;
  }

  .projection-output {
    max-height: 360px;
  }

  .result-meta,
  .tool-note {
    margin: 0;
    color: var(--fg-mute);
    font-size: var(--fs-micro);
  }

  .boundary {
    padding: 0 var(--sp-3) var(--sp-3);
    font-size: var(--fs-micro);
    color: var(--fg-dim);
  }

  .boundary p {
    margin: 0 0 var(--sp-2);
  }

  .boundary strong {
    color: var(--fg);
  }

  @media (max-width: 1100px) {
    .offline-tools { grid-template-columns: 1fr; }
    .projection-inputs { grid-template-columns: 1fr; }
  }
</style>
