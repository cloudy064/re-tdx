# 原生 C++ 异常波动与停牌风险链路模块化

日期：2026-08-11

## 目标

拆解 635 行的 `anomaly_risk.cpp`。原文件混合两种真实 view、六个 view 别名、四种预警状态、
八个预警别名、请求参数、三类证据投影、两类归一化、过滤汇总、缓存重试、响应和 CLI。

## 落地结果

原根文件已移除：

| 实现单元 | 行数 | 职责 |
|---|---:|---|
| `anomaly_risk_catalog.cpp` | 117 | view、别名、预警状态及请求策略目录 |
| `anomaly_risk_support.cpp` | 151 | 字段、市场身份、日期、数值和公共转换 |
| `anomaly_risk_evidence.cpp` | 99 | 预警文本、周期窗口、结果集与来源证据 |
| `anomaly_risk_normalize_statistics.cpp` | 69 | 3/10/30 日异常统计归一化 |
| `anomaly_risk_normalize_suspension.cpp` | 56 | 停复牌日期、偏离率和预警码归一化 |
| `anomaly_risk_query_plan.cpp` | 43 | 参数规范化、过滤、分页覆盖和缓存键 |
| `anomaly_risk_response.cpp` | 103 | 过滤、分组计数、边界声明和响应组合 |
| `anomaly_risk_service.cpp` | 65 | 缓存、三次重试、陈旧回退和计划执行 |
| `anomaly_risk_command.cpp` | 58 | CLI 参数、板块数据装载和输出 |

`ViewSpec` 直接绑定请求号、XML、DLL 模块、分页参数、未解释字段边界和归一化函数；
`WarningSpec` 统一绑定数值码、公开状态、中文标签和活跃标记。view id、请求号、全部别名、
预警码及状态均有编译期唯一性检查。`QueryPlan` 决定请求覆盖、过滤及缓存语义，服务不再用
`statistics` 布尔值分别选择请求、归一化、标题和字段保留策略。

## 增量验证

- `tdx-anomaly-risk-tests` 与 `tdx-tool` 构建通过；
- 专项测试覆盖两类归一化、3/10/30 日窗口、实时偏离核对、结构化预警文本、未解释原值、
  停复牌空日期、偏离率换算及证券名称解析；
- 真实 `statistics` 全分页请求 `2044` 返回 837 行，其中 5 行带预警，结果集声明与解码行列
  一致，抽取 3 行且 availability 为 `live`；
- 样本保存在 `output/anomaly-risk-refactor-sample.json`，schema 保持
  `tdx-anomaly-risk-native-v1`；
- 未修改共享 TQLEX 解析、传输、HTTP 路由或 schema，按增量验证规则未运行完整 CTest。

## 后续候选

`level2.cpp` 继续搁置；共享 `jsn.cpp`、解释器序列函数及 TPool XML/公式兼容解析需要更宽
回归。下一低风险候选为 634 行的 `native/src/threshold_stocks.cpp`。
