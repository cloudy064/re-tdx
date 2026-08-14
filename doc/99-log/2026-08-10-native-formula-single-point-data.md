# TCalc 单点财务与经营数据闭环

## 结果与边界

纯 C++ 公式解释器新增五个非 Level2 注册函数：

- `FINONE(FIELD,YEAR,MMDD)`：读取当前证券单个财务字段；
- `GPJYONE(ID,FIELD,YEAR,MMDD)`：读取当前证券单个经营统计值；
- `BKJYONE(ID,FIELD,YEAR,MMDD)`：读取当前证券所属行业板块的经营统计值；
- `SCJYONE(ID,FIELD,YEAR,MMDD)`：读取固定市场指数的经营统计值；
- `GPONEDAT(ID)`：读取通达信本地证券单点缓存。

五项结果都按 TCalc 行为广播到公式的全部柱。运行时不加载 DLL 或 Python，也不读取
账号、Level2 或券商私有状态；本批新增的 IDA Python 只是离线证据探针。

真实 `SZ000001 平安银行` 最新样本为：

- `FINONE(183,0,0)=4.650000095367432`；
- `GPJYONE(1,1,0,0)=457610`；
- `BKJYONE(5,1,0,0)=6.840000152587891`，目标为 `SH880471 银行`；
- `SCJYONE(1,1,0,0)=261556640`；
- `GPONEDAT(7)=0`，因为当前安装没有对应的本地 `gp*one.dat` 文件。

## TCalc 包装层证据

TCalc 的静态注册和五个处理函数证明：

| 函数 | opcode | 处理函数 | 宿主类型 | 行为 |
| --- | ---: | --- | ---: | --- |
| `FINONE` | 1326 | `sub_10011C40` | 172 | 一个财务字段值，广播 |
| `GPJYONE` | 1341 | `sub_10011FC0` | 175 | 当前证券两字段记录取其一，广播 |
| `SCJYONE` | 1343 | `sub_10012850` | 175 | 固定市场指数两字段记录取其一，广播 |
| `BKJYONE` | 1350 | `sub_10012410` | 175 | 所属行业指数两字段记录取其一，广播 |
| `GPONEDAT` | 1351 | `sub_10012990` | 170 | 本地单点缓存值，广播 |

`GPJYONE/BKJYONE/SCJYONE` 只接受 `FIELD=1/2`。包装层仅在 `YEAR>0`、
`MMDD>0` 且 `YEAR<1900` 时补齐两位年份：`YEAR<=90` 加 2000，否则加 1900。
这条规则已在分析器参数校验、上下文键和单元测试中保持一致。

## TdxW 精确选择语义

TdxW type-175 的 `sub_4DB360` 处理每个 ID 的 12 字节记录：日期、字段一和字段二。
日期大于等于 10000 时必须精确命中 `YYYYMMDD`；小于 10000 时表示倒序序号，
`0` 是最新记录，`1` 是上一条，以此类推。没有匹配项时返回 0。

type-172 的 `sub_4DADE0` 处理每字段 8 字节的日期和值，并区分五种模式：

- 完整日期：在该日期所属季度起点到指定日期之间选择记录；
- `(0,0)`：最新记录；
- `(N,0)`：从当前报告期向前 N 年；
- `(0,N)` 且 `N<=300`：向前 N 个季度；
- `(0,MMDD)` 且 `MMDD>300`：最近一次早于当前时点的该月日。

辅助函数 `sub_4D1170` 进一步确认了相对年份、季度和独立月日的计算边界。

`BKJYONE` 先通过 type-120 获取当前证券行业指数；宽基指数还保留 TdxW
`sub_4D19B0` 的固定归并，例如 `999999/999001/999888 -> 880092`、
`399001 -> 880093`、`399006 -> 880096`、`399300/300 -> 880097`、
`688 -> 880098`、`899050 -> 880099`。

type-170 的 `sub_4DB520` 读取 `T0002/hq_cache/gpszone.dat`、
`gpshone.dat` 或 `gpbjone.dat`。每条记录恰为 10 字节：32 位数值代码、16 位字段
ID 和一个 float，首个匹配项生效。宿主目标先清零且缺失仍返回成功，因此文件或
记录缺失时结果必须是 0，而不是错误或远端替代值。

离线证据及 SHA-256：

- `output/ida-tdxw-single-point-values.log`：
  `14F0D6D4AFE9FDC50087AA9024C58E3898CDF4C300D4F9E1D53E7899E91C874C`；
- `output/ida-tdxw-single-point-helpers.log`：
  `8F8A3558AC5389E98AA886694C0C56B011179FC644ED3FDD04E50D6E50B47053`。

## 数据源与纯 C++ 实现

`formula_context.cpp` 按需绑定分析阶段收集的五类标量键。`FINONE` 复用官方
`gpcw` 报告包；其余三类经营数据读取官方 `tdxgp` 文件，实测包括
`gpsz000001.dat`、`gpsh880471.dat` 和 `gpsh999999.dat`。`BKJYONE` 会自动加载
本地板块映射。`GPONEDAT` 只读取通达信安装目录中的 10 字节本地缓存，不把缺失
文件偷偷替换成网络近似值。

首次实现会为一个最新值预取最多 80 个财报包，冷请求约 98 秒。修正后加载范围
由参数严格限定：完整日期只取相应季度，最新值只取一个报告期，相对年/季度只取
计算所需数量并以 80 为硬上限。候选服务缓存后的五函数联合请求约 1.54 秒，正式
服务约 1.22 秒。

`formula_engine.cpp` 对常量参数做范围校验，拒绝动态 ID/日期，并通过
`formula_scalar_bindings` 把上下文值送入求值器；响应元数据保留具体文件、选择
模式、报告期数量和板块目标，便于网页或契约区分“真实零值”和“数据缺失”。

能力清单因此增至 226 个支持函数、69 个自动符号；新增能力组
`custom_formula_single_point_functions` 精确包含
`BKJYONE/FINONE/GPJYONE/GPONEDAT/SCJYONE`。390 条静态注册表中识别并集为
278，剩余 112。

## 验证、产物与正式部署

验证结果：

- CTest `101/101`；
- 379 条内置公式源码、语法和安全数值返回均为 `379/379`，降级输出 0；
- 候选服务健康、覆盖率和单点真实数据契约 `3/3`；
- 正式 8765 同三项 `3/3`；
- 正式服务健康状态为 `native_cpp=true`、`python_runtime=false`。

本批产物及 SHA-256：

- `output/native-formula-coverage-single-point-v6.json`：
  `0AEE195D9F298B60ACCC89C3FA412661A6BD587A6409BAA70BF976B84B98DF0D`；
- `output/native-formula-registry-next-audit-v6.json`：
  `0EB1E5FBBBE0117AB97F6EA0D0AD95C75CDEA93CDC2449A78DC4749A858ABE88`；
- `output/native-single-point-v6-candidate-api.json`：
  `86E23D19215C4E0A732F0C66F3AA826F637896F2DBFDFFC90C0C9506B06723B4`；
- `output/native-single-point-v6-formal-final-api.json`：
  `1DB0F4AA2EC03369D6934C0BF85E76795CC641F1216CF6D548BED99EB1D28933`。

正式 EXE 为 43,354,706 字节，build/dist SHA-256 均为
`29F988A7F3C7FB7CA9181F2BCB42C118FC7FB83A1CFC4BE956D1DFF898FC81B3`。
服务 PID 41780，仅监听 `127.0.0.1:8765`。首次启动建立完整本地索引约需十余秒，
端口在索引完成后才进入监听；这不是进程崩溃。上一正式版已保存在
`output/tdx-tool-single-point-v6-predeploy-rollback-20260810.exe`，大小
43,194,300 字节，SHA-256
`3A9ADD51651E04A7C7A93ADB9C41ECF25E33B517BC19285FD96E87F8E2E9EF88`。
临时 8875 已关闭。
