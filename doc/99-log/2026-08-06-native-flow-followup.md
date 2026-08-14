# 融资融券与北向资金后续表现纯 C++ 固定化

## 目标

把 `rzrq_zsyc1.xml / 200011` 和 `bxzj_zsyc4.xml / 500503` 从通用 PBRPC
调试入口迁为固定业务接口。两页展示的是资金信号出现后沪深300的历史表现，
不是普通融资融券/陆港通流水，也不是预测或因果模型。

## 配置语义

- 融资融券默认从 `2017-11-15` 开始，字段为融资/融券余额、活筹市值、
  融资/融券率、沪深300点位，以及后续 1/3/5/10 日累计涨幅。客户端明确提示
  最近交易日可能因披露不同步只覆盖部分市场。
- 北向默认从 `2017-01-01` 开始，字段为单日净流入、净买入、沪深300点位，
  以及后续 1/3/5 日累计涨幅。客户端说明是不同净买入规模下的历史统计。
- 余额与资金为亿元，活筹市值为百亿元，率与收益为百分点。

## 真实响应与异常

截至 2026-08-05：

| 视图 | 解码内容 | 声明 RowNum/ColNum | 分段 |
| --- | ---: | --- | ---: |
| 融资融券 | 2,117 行、11 列 | 2,117 / 11 | 5 |
| 北向资金 | 2,231 行、7 列 | 1 / 2 | 4 |

北向响应的声明元数据明显错误，但列描述与每行内容稳定为 7 列。固定接口以
通过宽度校验的解码内容为权威，同时在 `source.result_set_metadata` 保留声明
值、解码值、`consistent=false` 和 `row_count_trusted=decoded-content`。

北向从 `2024-08-19` 起连续 460 个交易日报告 `valuejlr=1040.00`、
`valuejmr=0.00`，最后一个非该组合的交易日为 `2024-08-16`。这段值仍保留在
`reported_net_*` 字段作为证据，但有效字段置空，并标记：

```json
{"signal_available":false,"usable_for_signal_analysis":false,
 "data_status":"upstream-placeholder"}
```

检测要求至少连续 20 行完全相同的 `1040/0`，不会把偶然单日相同值直接猜成
占位。`--available-only` / `available_only=1` 当前返回 1,771 行可用信号。

## 实现与验证

- 新增纯 C++ `market flow-followup` 和固定只读
  `/api/v1/market/flow-followup`，视图为 `margin`、`northbound`；
- 日期严格校验，小 `limit` 保留最新记录，空串/`-`/`--` 统一为 null；
- 响应给出可用/占位/排除/截断计数，并只在可用信号总体上计算各前瞻周期的
  观测数、平均涨幅、正收益比例和极值，明确 `computed_locally=true`；
- 方法字段固定 `association_not_causation=true`、`forecast=false`；
- 同参数缓存、三次瞬时错误重试和显式陈旧缓存降级与其他 PBRPC 业务接口一致；
- 原始探针：`output/probes/pbrpc-200011-current.json`、
  `pbrpc-500503-current.json`；规范化探针：`flow-followup-*.json`；
- 新增专项单元测试覆盖金额/比率、空前瞻值、日期排序、长/短占位段和重复日；
  全量测试目标增至 30 项。

## 部署验收

服务 PID `30140`，发布 EXE SHA-256 为
`FB23B5F50834A1BE7FA642B5107EE22090D7835EDE495ABB3055BB7A739E9C7A`；主页
HTTP 200、功能目录由 78 增至 79。融资融券全量冷请求约 2.352 秒、缓存约
200.4 ms，返回 2,117 行。北向确认解码 2,231 行、占位 460 行、元数据
`consistent=false`；`available_only=1` 的最后日期为 2024-08-16。非法日期
`20260231` 返回结构化 HTTP 400。

## v2：分档模型与当前摘要

同日继续把此前只停留在通用 TQLEX 调试入口的四个普通 JSON ReqId 并入同一
业务接口：

| 视图 | ReqId / Type | 当前分档数 | 当前值 |
| --- | --- | ---: | --- |
| `financing-model` | `200009/200010 / 1` | 11 | 融资余额 25,962.61 亿元、融资率 5.04% |
| `lending-model` | `200009/200010 / 2` | 11 | 融券余额 239.29 亿元、融券率 0.05% |
| `northbound-inflow-model` | `500501/500502 / 1` | 16 | 上游报告 1,040 亿元 |
| `northbound-purchase-model` | `500501/500502 / 2` | 16 | 上游报告 0 亿元 |

每个模型把 `rankvalue` 的客户端枚举固定为明确区间，保留样本数、当前分档，
并将沪深300后续平均涨幅和上涨概率整理为 1/3/5/10 日结构。`200010/500502`
不再只返回无法索引的中文句子：日期、余额、比率、资金额和各周期统计均被
结构化，同时保留原文用于审计。

北向两条摘要会额外互查同日值。当前 Type 1/2 精确组成 `1040/0`，因此两种
模型均返回 `signal_available=false`、`data_status=upstream-placeholder-pair`，
对应 `current_bucket.usable_for_current_signal=false`；16 档历史统计仍可使用。
这也解释了 Type 2 在当前 0 值下却把空样本 `< -200` 档标为当前档的异常，
工具没有把该档伪装成有效信号。`500501/500502` 继续以实际解码的 16/9 列内容
为权威，并在每个来源中暴露错误的 `RowNum=1/ColNum=2`。

接口 schema 升为 `tdx-flow-followup-native-v2`，原 `margin/northbound` 两视图
保持兼容，新增四个模型视图后共六视图。专项测试新增分档边界、带符号零、中文
摘要和 10 日统计解析；全量 CTest 37/37 通过。真实 CLI 证据为
`output/probes/flow-followup-*-model-current.json`，普通 JSON 原始证据为
`output/probes/tqlex-*-type[12]-current.json`。

正式 `8765` 服务已部署该版本，PID 为 `28952`，EXE SHA-256 为
`FAC669988AEE54FDC530D7DF026BB08CC6ED0214005D63B5F115BDD0B4D58C2F`。
健康检查为 `native_cpp=true`、`python_runtime=false`，功能目录仍为 86（本次是
既有命令扩展而非新增命令）。正式 API 已复验融资 11 档、融资率 5.04%、北向
占位配对和基金分析 37 行回归；主页 HTTP 200。Svelte HTML/JS/CSS 哈希仍为
`0F1FBB...5777`、`B03C28...4011`、`733085...30A`，没有覆盖用户 UI。
该部署后来已被异常波动与停牌风险正式版本取代，当前 PID 和哈希以对应日志
为准。
