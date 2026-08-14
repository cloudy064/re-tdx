# TCalc ZSTJJ/QHJSJ 共享 K 线辅助字段闭环

## 结果

纯 C++ 公式解释器现支持裸符号 `ZSTJJ` 和 `QHJSJ`。前者在分时图周期表示累计
均价线，后者在期货/期权周期表示结算价。它们不是两个不同的原生数据槽：TCalc
注册了两个 opcode，但两个处理函数都逐柱复制同一个 35 字节宿主 K 线记录偏移
31 的 float。

公开行情输入按来源保持真实口径：

- 普通 7709 `time` 周期没有直接携带宿主均价字段，按每个交易日累计
  `amount / volume` 重建分时均价；
- 扩展市场 7727 K 线记录偏移 28 已解析为 `auxiliary_price`，期货/期权日线中
  它是 `settlement_price`；
- 7727 `0x240B/0x240C` 的 18 字节分时记录偏移 6 本来就提供
  `average_price`，调用方转换为公式 K 线时可直接保留；
- 对不提供辅助值的普通非分时周期使用宿主记录的零初始化语义，不猜测价格。

## 原 DLL 证据

静态注册表记录：

| 符号 | opcode | 处理函数 |
| --- | ---: | --- |
| `ZSTJJ` | 1329 | `sub_100155F0` |
| `QHJSJ` | 1008 | `sub_10015680` |

两段反编译都以步长 35 遍历 `this[15003]`，执行
`output[i] = *(float *)(bar + 31)`；历史/变换缓冲分支也读取同一偏移。证据在
`output/ida_probe_tcalc_daily_fields.py` 和
`output/ida-tcalc-daily-fields.log`。

新增 32 位原 DLL 固定向量探针：

- `output/native_probe_tcalc_auxiliary_fields.cpp`；
- `output/native_probe_tcalc_auxiliary_fields.exe`；
- `output/native-probe-tcalc-auxiliary-fields.json`。

向偏移 31 注入普通值、精度边界、负零、NaN、正负 Inf 和 TCalc 缺失哨兵后，
两个入口都按位输出同一结果。这证明处理函数自身没有名称、市场或周期门控；业务
含义由宿主写入共享字段时决定。

## 真实行情验证

真实 7727 `47:IFL9` 日线最近 8 根中，`auxiliary_field=settlement_price`，两个
符号逐日完全相等；2026-08-07 最新值均为 4611.2。中金所期权
`7:HO8W03UX` 的同字段也有非零日结算价。

真实 7709 平安银行 `sz000001` 分时共 240 根。按逐日累计成交额/成交量重建后，
09:31 均价约 11.18，15:00 约 11.17；`ZSTJJ/QHJSJ` 在每根柱上仍相等，符合原
DLL 的共享字段行为。输入与公式结果保存在：

- `output/native-kline-aux-ifl9-day.json`；
- `output/native-timeline-aux-ifl9.json`；
- `output/native-kline-aux-sz000001-time.json`；
- `output/native-cli-kline-auxiliary-ifl9.json`；
- `output/native-cli-kline-auxiliary-sz-time.json`。

## 能力、契约与发布

能力清单新增 `custom_formula_kline_auxiliary_symbols=[QHJSJ,ZSTJJ]`，自动符号由
63 增至 65，支持函数仍为 218。两者只支持有注册证据的裸符号形式，不伪造零参数
函数。静态注册识别并集由 263 增至 265，390 条中剩余 125。

新增 `formula-kline-auxiliary-fields-live` 契约，要求真实 `IFL9` 日线返回
`auxiliary_field=settlement_price`、两个最新值为同一正数；错误分叉值会被拒绝。

- CTest 101/101；
- 临时服务专项 4/4；
- full API 216/216，共 217 次网络请求；
- 正式服务健康、首页、能力清单、`MULTIPLIER`、辅助字段 5/5；
- 报告为 `output/native-auxiliary-targeted-api.json`、
  `output/native-auxiliary-full-api.json`、
  `output/native-auxiliary-formal-final-api.json`。

正式 EXE 为 42,810,754 字节，build/dist SHA-256 均为
`CD8CDC64A17FD28C927DBD5BC7422D3D91C34C0893A345CF2DF1727C4D8D1EF6`；正式服务
PID 25796，仅监听 `127.0.0.1:8765`，健康检查保持 `native_cpp=true`、
`python_runtime=false`。回滚副本为上一版 42,810,242 字节，SHA-256
`AFD19A1AD3A97B848C5AA26E795E3122C6485CEDBFCA5BAEA02B490301811140`。首页仍加载
`assets/index-ByEK1oAy.js`，HTTP 200；网页资源未改动，临时 8875 已关闭。

## 下一步

下一批转向不依赖 L2 的板块/证券文字入口，优先比较
`MOREHYBLOCK/LEVEL1HYBLOCK/MAINBUSINESS/ZHBLOCK/ZDBLOCK/SIMIBLOCK` 与现有本地
行业、概念、组合、自定义板块索引的可证明映射。五个单点专业数据入口
`FINONE/GPJYONE/BKJYONE/SCJYONE/GPONEDAT` 排在其后；继续排除账户交易状态、
券商私有信号和插件回调。更新后的结构化差分队列在
`output/native-formula-registry-next-audit-v2.json`。
