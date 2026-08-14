# TBigData 计算列解释器

## 本轮结果

从 `TBigData.dll 1.0.2.40` 恢复 `calc/calcref/calctype/calcflag` 描述结构、
36 项固定函数注册表、普通表达式入口、缺值策略和逐行分派。新增纯 C++
`formulas cloud-calc`，支持：

- 审计完整 `cloud_cfg` 或单个 CFG；
- 输出注册函数 ID、参数个数、处理函数地址及实现状态；
- 检查表达式语法、标识符与 `calcref` 一致性、内建参数数和依赖环；
- 从平面 JSON、对象数组或 `colheader/data` JSN 读取指定行；
- 按 unit 拓扑复算派生列，支持显式宿主字段覆盖和固定当前日期；
- 明确区分成功、输入不可用与求值错误。

当前安装的实测审计为 660 个 CFG、335 个含计算列 CFG、1370 个计算列；
987 条表达式和 383 次内建调用全部有效，依赖环和解析错误均为 0。当前配置
使用的 7 个内建函数均已实现，因此当前配置执行覆盖为 1370/1370。随后继续
反编译底层日期和 YTM 辅助函数，把其余 29 项也按处理体接入；36/36 项均通过
固定日期/现金流向量，PV→YTM 的五个分支完成回算。报告仍动态区分当前 CFG
实际调用的 7 项与未调用的 29 项；后者尚无原版 UI 真实行差分，不冒充已完成
产品样本验证。

真实可转债资源 `gxjty_zq_kzzsy101_1.jsn` 首行在补入四个宿主即时字段后，
15/15 个派生列复算成功。审计和行结果分别保存在：

- `output/tdx-tbigdata-cloud-calc-audit.json`
- `output/tdx-tbigdata-cloud-calc-row0.json`
- `output/ida-tbigdata-date-ytm-helpers.json`

随后闭合 `syscol/refzqdm` 宿主层：TdxW `0x41D140` 注册三个回调，TBigData
`0x10024E30` 保存回调并用类型 135 探测行情宿主。命令新增 `--quotes` 和
`--snapshot/--finance-snapshot`。进一步导出 `0x101016B0` 的 44 项系统列总表，
并恢复 `<unit refunit="N">` 对列级 `refzqdm` 的继承；严格 XML 曾漏掉的 5 个
不规范 CFG 也由容错解析器覆盖。公开 `0x054C/0x0547` 可补 OHLC、涨跌/振幅、
量额、内外盘和买卖一价量，公开 `0x0010` 补股本/市值/换手，本地层级补通达信
行业，并从债券行派生 `$BONDAI` 和 `DQLL2`。

静态处理体同时纠正了两个易混点：`$ZS` 是 ID 19“涨速”，不是昨收；
`$NOW/$NOW3` 只取非零现价，真正回退昨收的是 `$NOW2`，市值分支另有同样的
回退。随后从 `TBigData.dll:0x100BCAC0` 恢复 `$S_AVGZF/$S_JQZF/$S_LTG/
$S_MAXZF/$S_NUM/$S_UPRATE` 六项板块聚合，并从 TdxW `sub_60E580` 证明加权字段
为总股本而非流通股本。继续沿 `$ZS` 反向追踪到 TdxW `sub_697E40` 默认消息
1342（公开 `0x053E`）和 `sub_85EB00`：响应包含五档、状态、扩展字段及
`int16 / 100` 的服务端涨速。纯 C++ 已新增 `market speed`、HTTP `/api/v1/market/speed`
并让 `cloud-calc --quotes` 自动选择该请求。继续沿主机类型 102 闭合 ETF IOPV：
TBigData 的 `$JJJZ` 读取 `Destination+57`，TdxW 从公开行情核心第七个价格差分
构造辅助价，并按 `sub_598CC0` 的 ETF 代码表缩放为 IOPV。纯 C++ 现已原样保留
该差分且不增加网络请求。后续闭环见
[封单字段、动态 PE 与宿主列全覆盖](2026-08-08-native-seal-pe-host-closure.md)：
`$FCAMO/$FCB` 经 selector 4、主机类型 163 读取
`Destination+384/+388`，`sub_9B37A0` 对封死涨停计算买一价乘买一手数乘交易
单位及买一手数/总手数，跌停使用卖一侧并取负；完整涨跌停、状态和集合竞价
分支均已移植。`$PE` 也已证明为 `DYNAINFO(39)` 的动态 PE。当前 2570 次声明
已有 2570 次自动来源（100%）。
真实可转债
首行无需任何 `--set` 即 15/15 成功且宿主未解析数为 0；结果保存在
`output/tdx-tbigdata-cloud-calc-row0-host.json`。`func_zq_ssfxr201_1` 还用 TCL 科技
实盘验证 `refunit="1"` 和 `0x0547` 买卖一价量，结果在
`output/tdx-tbigdata-cloud-calc-ssfxr-row0-host.json`。

## 代码与验证

- `native/include/tdx/cloud_calc.hpp`
- `native/src/cloud_calc.cpp`
- `native/tests/cloud_calc_tests.cpp`
- `output/ida-tbigdata-system-columns.json`
- `output/ida-tbigdata-functions-b0000-bffff.json`
- `output/ida-tdxw-functions-60d000-60efff.json`
- `output/ida-tdxw-rise-speed-request-chain.json`
- `output/ida-tdxw-remaining-cloud-helpers.json`
- `output/ida-tdxw-fund-iopv-helpers.json`
- `output/ida-tdxw-seal-order-limit-helpers.json`
- `output/tdx-market-speed-etf-iopv-live.json`
- `output/tdx-tbigdata-cloud-calc-etf-row1-iopv.json`
- `output/tdx-tbigdata-cloud-calc-audit.json`
- `output/ida-tdxw-rise-speed-public-request.json`
- `output/ida-tdxw-rise-speed-decoders.json`
- `output/tdx-tbigdata-cloud-calc-5g-row8-group.json`
- `output/tdx-market-speed-live.json`
- `output/tdx-tbigdata-cloud-calc-kzz-row1-speed.json`
- 完整纯 C++ 测试：96/96 通过。
- 正式 HTTP 契约：132/132 通过，包含 `market-speed-live` 与
  `market-speed-etf-iopv-live`；
- 正式功能目录：143 项，包含 `formulas cloud-calc` 和 `market speed`；
- 发行 SHA-256：
  `68431AA4194D3B5B52572E258443A2BCCF63A3642FC90FA55331F025B0618AE4`；
- 正式服务 PID `9476`，监听 `127.0.0.1:8765`；
- 完整 HTTP 契约报告：
  `output/api-contracts-tbigdata-etf-iopv-full.json`；
- ETF 单项正式契约：`output/api-contract-etf-iopv-formal.json`。

完整技术说明见
[TBigData 计算列与债券函数](../02-engine/10-tbigdata-cloud-calculations.md)。
