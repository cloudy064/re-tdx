# TCalc 本地信号序列与新增 DLL 审计

日期：2026-08-12

## 结论

这批新增 IDA 数据库把 TCalc 三类信号入口的宿主边界闭合了：

- `SIGNALS_SYS` 是安装目录中的公开本地系统信号，可自动读取；
- `SIGNALS_USER` 是用户在客户端中维护的本地信号，可自动读取；
- `SIGNALS_QS` 仍来自券商/下载宿主，不存在可等价替代的本地公开文件，继续只接受
  调用方合法持有的精确序列。

纯 C++ 工具新增 `formulas local-signals`，公式解释器也会按当前证券和 K 线日期自动
装载前两类序列。当前真实安装 `C:\new_tdx` 没有 `T0002\signals\datacfg.sys` 或
`datacfg.dat`，因此实测是明确的空目录状态，不伪造示例信号。

本轮首次复核 `ida` 目录时，26 个 DLL 中已有 25 个对应的 `.dll.i64`；随后
`tc.dll.i64` 也已于 2026-08-12 生成。当前这 26 个 DLL 已全部处理。静态审计确认
`tc.dll` 是交易客户端核心及 L2 凭据宿主桥接，而不是新的公开行情解码器，详见
[tc 交易与 L2 边界](2026-08-12-tc-trading-l2-boundary.md)。现阶段没有需要继续交给
IDA 的高收益 DLL。

文件级复核结果为：`ida` 中 26 个现存原始 `.dll` 均有同名 `.dll.i64`；另有
`Viewthem.dll.i64`，但当前目录没有对应的原始 `Viewthem.dll`，因此只把它视为既有
数据库证据，不把不存在的 DLL 列入待处理清单。

## 精确逆向证据

### TCalc 注册记录

`TCalc.dll!sub_100C6870` 使用固定 0x47 字节注册记录。此前仅按相邻常量推测的三类
信号边界，现已由处理函数地址和宿主 callback selector 精确确认：

| 公式入口 | TCalc 处理函数 | 宿主 selector |
|---|---|---:|
| `SIGNALS_SYS` | `sub_10010FA0` | 34 |
| `SIGNALS_QS` | `sub_100111B0` | 35 |
| `SIGNALS_USER` | `sub_100113C0` | 36 |
| `EXTERNVALUE` | `sub_100115D0` | 37 |
| `EXTDATA_USER` | `sub_100116B0` | 38 |

TdxW 的第一公式 callback `sub_61B630` 转发到 selector dispatcher
`sub_60FFF0`。三类信号返回相同的日期/float 值记录；TCalc 随后只按交易日匹配：
`TYPE=1` 向前填充、`TYPE=2` 补零，其他值保持 `DRAWNULL`。

### 系统信号

selector 34 由 `TdxW.exe!sub_4FB2F0 -> sub_4FA9B0` 提供，读取：

```text
T0002\signals\signals_sys_<id>.dat
```

文件是 GBK 管道文本，每行至少四列：

```text
market|numeric_code|YYYYMMDD|value
```

同一个系统信号文件可以包含多只证券，宿主按 16 位市场号、数值代码和日期筛选。
系统目录 `datacfg.sys` 同样为管道文本，至少包含
`id|kind|catalog_value|name`；ID 从 10001 开始，`kind` 为 0/1。

### 用户信号

selector 36 由 `TdxW.exe!sub_4DAAB0 -> sub_4DA810` 提供，读取：

```text
T0002\signals\signals_user_<id>\<market>_<code>.dat
```

证券文件是连续的 8 字节小端记录：

```text
int32 YYYYMMDD
float32 value
```

用户目录 `datacfg.dat` 是连续的 60 字节记录：偏移 0 为 `int32 id`，偏移 4 为
`uint32 kind`，偏移 8..55 为 NUL 结尾 GBK 名称，偏移 56..59 是运行期指针槽，
离线解析时忽略。TDXDeep 的写入链会整理用户记录；单次宿主读取最多 30,000 点。

## 原生实现

新增的独立模块为：

- `native/include/tdx/local_signals.hpp`：目录、序列和 K 线对齐类型；
- `native/src/data/local_signals.cpp`：严格解析、查询、对齐和 JSON schema；
- `native/src/data/local_signals_command.cpp`：CLI 参数编排；
- `native/src/formula/formula_context_host.cpp` 和公式函数模块：自动上下文绑定。

目录查询：

```powershell
tdx-tool formulas local-signals --root C:\new_tdx
```

读取指定用户序列：

```powershell
tdx-tool formulas local-signals --root C:\new_tdx `
  --namespace user --id 73 --market sz --code 000001
```

响应 schema 为 `tdx-local-signals-v1`，同时报告精确来源路径、文件是否存在、原始和
匹配记录数、30,000 点宿主上限及数据格式。该能力当前是 CLI-only；公式现有
CLI/API 入口会自动使用它，没有增加重复的 HTTP 数据路由。

公式分析器现识别常量二参数调用并生成本地绑定。能力清单的相关状态为：

- 支持函数 264 个，新增 `SIGNALS_SYS/SIGNALS_USER`；
- 本地外部序列函数为 `EXTDATA_USER/SIGNALS_SYS/SIGNALS_USER`；
- 390 条静态注册名识别 319、明确边界 71、完全分类；
- 券商私有信号仅剩 `SIGNALS_QS` 一项；
- 379/379 内置公式继续语法支持且数值安全，`executable_with_context` 仍为 339。

最后一项没有提高，是因为当前内置公式的六条私有信号公式只使用
`SIGNALS_QS`，并不使用新闭合的系统/用户本地信号。剩余 40 条公式仍严格是
20 条只读未来函数、14 条授权 L2 和 6 条券商私有信号，没有公开非 L2 缺口。

## 其他新增 DLL 的收益审计

- `TEvalExp.dll` 不是第二套公式语法解释器。TDXDeep 动态加载它并转发
  `Func/Data`，它的职责是 TCalc 外部数据、板块及 UI 宿主适配。
- `TDXDeep.dll` 提供 `signals/datacfg/tdxfin` 等本地配置和宿主桥接；本轮有价值
  的信号目录读写链已经落实为原生模块。进一步审计又恢复了
  `T0002/shapematch.dat` 的本地形态匹配库和加权 Pearson 评分；现已实现为纯 C++
  `market shape-match`，详见独立归档。
- `Calcer.dll` 是 MFC 计算器界面，不是 TCalc 计算核心。
- `BlockMap.dll` 对应的 `.dat` 是 ZIP 资源/UI 包（包括 `TdxZLSelStock.dat`），不是
  可直接导出的板块股票数据库。
- `TQQCalc.dll` 的期权波动率和定价路径与工具现有实现一致；`TQQAnaly.dll` 主要是
  分析界面和 3D 展示，没有新的底层行情协议。
- `invest.dll` 的后续静态处理现已完成：`pinfo.dat` 的固定 Blowfish 模式、
  192 字节组合记录、`.da0` 的 200 字节记录和 `trdpara.dat` 六项费率公式均已
  迁入纯 C++ `market investment`。进一步结合 12 个写入 handler 和 DLL 菜单资源，
  `.da0` 的基础交易字段及 12 类业务也已分页解码，持仓数量、移动平均成本、已实现
  盈亏和现金流已可重放；真实网格只用于保留区及依赖行情的市值/浮盈对照，而不是
  基础流水或另一只 DLL。
- `TGear/Dbf` 是低收益通用组件；当前无需继续投入。
- `tc.dll` 导出交易登录、交易窗口和账户信息接口；`TC_GetL2Info` 只从已经建立的
  交易客户端 XML `Summary` 读取 `L2USER/L2PASS`，`TC_SetL2UserInfo` 也只保存调用方
  传入的三个字符串。它不提供免订阅的 L2 行情入口，当前不迁入纯 C++ 公共数据主线。

## 验证

- 受影响目标 `tdx-formula-engine-tests`、`tdx-recon-contract-tests`、`tdx-tool`
  增量构建通过；
- 公式引擎与 recon 专项 CTest 为 2/2；
- 真实 `C:\new_tdx` 的目录查询和指定用户序列查询均正确返回文件不存在的空状态；
- 候选服务快速 API 契约 9/9；按能力 schema 变更规则执行 full 契约为 211/225，
  14 项失败与变更前基线逐项一致；61 个公式相关契约只有既有港股全库审计失败；
- 候选端口 18884 已关闭；正式 8765 始终为 PID 24096，未重启、未替换。

机器可读产物：

- `output/local-signals-live-20260812.json`
- `output/formula-local-signals-coverage-20260812.json`
- `output/local-signals-api-contracts-quick-20260812.json`
- `output/local-signals-api-contracts-full-valid-20260812.json`

IDA 证据：

- `output/ida-tcalc-signal-registry-exact-20260812.log`
- `output/ida-tcalc-signal-layout-20260812.log`
- `output/ida-tdxw-formula-signal-callbacks-20260812.log`
- `output/ida-tdxw-signal-providers-20260812.log`
