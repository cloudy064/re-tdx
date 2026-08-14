# TCalc 个股形态与市场汇总宿主数据闭环

日期：2026-08-10

## 结论

本批闭合六个普通非 L2 注册函数：`BETAVALUE`、`SHAPE_SHORT`、`SHAPE_MID`、
`SHAPE_LONG`、`MAINZSHQ` 和 `TOTALHQINFO`。它们已经进入纯 C++ 公式解释器、
CLI、HTTP 自动上下文和固定 API 契约；没有 Python 运行时，也没有请求或绕过 L2。

`BETAVALUE/SHAPE_*` 来自 TdxW type 163 与本地 `tdxstat.cfg`；`MAINZSHQ` 和
`TOTALHQINFO` 来自 TCalc type 102 回调及公开 7709 L1 行情。六项都按宿主规则
取最终参数并广播到当前公式的全部 K 线。

## 个股 beta 与形态

TCalc opcode `1333` 读取 type-163 记录偏移 91 的 beta；opcode
`1345/1346/1347` 读取偏移 140 的打包整数，并分别执行 `/10000`、
`(%10000)/100`、`%100`。TdxW `sub_5A5B00` 证明 beta 来自 `tdxstat.cfg` 第 3 列，
打包形态来自第 23 列，不是文件末尾几个字节字段。

当前平安银行记录为：

```text
stats_date=20260807
beta_60d=-0.2025
shape_packed=50601
shape_short=5
shape_mid=6
shape_long=1
```

形态编号由 TCalc 内置帮助恢复：1 倒 V 型反转、2 V 型反转、3 W 底、4 M 顶、
5 盘整、6 盘整后上行、7 盘整后下跌、8 上升通道、9 下降通道、10 拐头下跌、
11 拐头上升、12 上行盘整、13 下跌盘整、14 其它形态。四个函数同时支持裸符号
和零参数调用；标准市场找不到记录时按原宿主零初始化返回 0。

`market stats` 的 JSON 也新增 `shape_packed/shape_short/shape_mid/shape_long`，使
页面或外部调用方无需再次拆位。

## 主要指数行情

`MAINZSHQ(M,N)` 对应 opcode `1368`。指数 selector 为：

- 0：按当前证券选择默认主要指数；深市代码 `399006` 或 `3` 开头且非 `39`
  使用创业板，沪市 `000688` 或 `688/689` 开头使用科创 50，其余使用市场宽基；
- 1：`SH999999`；2：`SZ399001`；3：`SZ399006`；
- 4：`SH000688`；5：`BJ899050`；其他非零值沿用宿主默认宽基。

字段 0—9 依次为现价（缺失时回退昨收）、昨收、开盘、最高、最低、成交额、
上涨家数、下跌家数、当日涨幅和三日涨幅。上涨/下跌家数对应指数 type-102
记录的买一/卖一量；三日涨幅对应 TdxW selector 313，以当前交易日及此前日线
复合计算，盘前没有有效即时价时从最近三个已收盘交易日计算。

实现一次批量请求六个指数的公开 `0x0547`；只有使用字段 9 或动态字段参数时才
额外下载日线。常量参数会生成精确上下文键，动态参数则预绑定所有选择组合。

## 全市场汇总

`TOTALHQINFO(N)` 对应 opcode `1384`，固定读取 `SH880005`：

- 1 上涨家数；2 下跌家数；3 涨停家数；4 跌停家数；
- 5 昨日上涨家数；6 总成交金额（亿元）。

TdxW 使用同一 type-102 结构：现价、偏移 65 浮点、偏移 93/97 无符号整数、
昨收和成交额。公开 `0x0547` 的五档投影与收盘样本证明这些槽位分别承载对应
家数；成交额按 `1e8` 换算为亿元。盘前服务端可能返回当前值 0、0.01 哨兵或
空成交额，解释器保留宿主即时状态，不以昨日榜单伪造当前市场数据。

## 验证与覆盖

固定单元测试覆盖统计文件拆位、裸符号/零参数调用、常量/动态主要指数参数、
非法 selector 默认映射和全市场广播。全量 CTest 为 102/102，Svelte 检查为
0 错误、0 警告，生产构建成功。

平安银行真实 CLI 与候选 HTTP 结果均返回 beta `-0.2025`、形态 `5/6/1`；候选
固定契约 `health + coverage + formula-host-summary-inline-post` 为 3/3。新增
契约会在 120 根真实日线上验证六个依赖、20 个输出、形态拆位、数值广播和两类
来源元数据。

完整巡检还暴露并修正了三个既有 live 契约的交易时段边界。公开 L1 在盘前可
明确返回昨收有效但现价/深度归零；09:15 后 ETF 会先出现现价为 0、IOPV 与
集合竞价盘口已有效的中间态；港股扩展日线会在当日零成交时产生 `0/0 -> null`
的沽空占比。契约现在分别精确接受盘前重置、以昨收校验 IOPV 的集合竞价状态和
零成交空比率，仍拒绝任意部分字段缺失。30 分钟 K 线契约也从脆弱的“数组首尾
日期不同”改为验证 160/160 唯一日期时间戳及至少两个交易日，符合跨页追加顺序。

候选边界专项分别为 4/4、2/2，最终完整 API 契约为 220/220。完整报告 SHA-256
为 `7DA0007F46183B768B5FE2A3A3249AFC0F636623C7533BC775B250D06C97623E`。

覆盖能力从 237 增至 243 个支持函数，自动符号从 71 增至 75；390 条静态注册名
的已识别并集从 292 增至 298，剩余从 98 降至 92。379/379 条内置公式保持
语法支持与数值安全，退化数值输出为 0。

## 证据与产物

- `output/ida-tcalc-host-shape-handlers-v1.json`：六个 TCalc 参数处理函数；
- `output/ida-tdxw-host-dispatch-shape-v1.json`：type 163 目标字段复制；
- `output/ida-tdxw-stat-loader-shape-v1.json`：`tdxstat` 第 3/23 列加载链；
- `output/ida-tdxw-host-type102-v1.json`：type 102 字段生产与 selector；
- `output/ida-tcalc-host-shape-data-v1.json`：内置帮助和形态编号；
- `output/ida-tdxw-index-three-day-v1.json`：selector 313 三日涨幅；
- `output/native-stats-shape-smoke-v1.json`；
- `output/native-formula-host-summary-smoke-v1.json`；
- `output/native-formula-coverage-host-summary-v14.json`；
- `output/native-formula-registry-next-audit-v14.json`；
- `output/api-contract-host-summary-v14-candidate.json`；
- `output/api-contract-premarket-boundary-v1-candidate.json`；
- `output/api-contract-auction-boundary-v1-candidate.json`；
- `output/api-contract-full-host-summary-v14-candidate.json`；
- `output/api-contract-host-summary-v14-formal.json`。

## 正式部署

最终全目标重链接与 CTest 为 102/102，前端 `svelte-check` 为 0 错误、0 警告，
生产构建成功。正式发行 EXE 为 19,101,184 字节，SHA-256
`6D66BEF15BC8FFC18FE762A508CCFCCC7AE69FCF77E06829618BDB206C44CE0A`。
服务 PID 25932，只监听 `127.0.0.1:8765`；健康响应保持
`native_cpp=true`、`python_runtime=false`，首页返回 200。正式端健康、覆盖、
六函数宿主公式及四项时段边界专项为 7/7，报告 SHA-256
`F0DAE601329BBC9A857C0EC63110E63F97D3FE3803DA1D704FB945FD2D68ECC5`。

替换前版本保存在
`output/tdx-tool-host-summary-v14-predeploy-rollback-20260810.exe`，大小
22,188,452 字节，SHA-256
`E4D6B952FA9444EAF14860880D5F20D4216C29139601B3733EF9A36D66D983CA`。

下一项只把 `TOTALMMPAMO` 列为证据审计目标。在确认它是否完全属于公开 L1、
恢复全部字段和金额单位之前，不实现近似版本。
