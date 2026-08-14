# TCalc 公式与指标引擎

> 2026-07-31，基于当前安装版 `TCalc.dll` 及其 IDA 数据库。本文覆盖公式
> 枚举、内置记录、分类树、来源标志、只读导出边界，以及已经落地的原生
> 兼容计算子集。

## 版本基线

```text
TCalc.dll
SHA-256 13FACAA52DAC552C5BE1F63331781219DE9BF798C443AF8F5E4AFC193A02E7F5
PE32 / VC++ 2010 / MFC 10
```

`TCalc.dll` 导出完整的 `CMainCalcInterface` C++ 接口。与列表相关的方法为：

```cpp
long GetIndexNum(unsigned char kind);
tag_INDEXINFO *GetIndexInfo(unsigned char kind, long index);
tag_INDEXINFO *GetIndexInfo(unsigned char kind, char *code);
long GetIndexNo(unsigned char kind, char *code);
char *GetIndexCode(unsigned char kind, long id);
long GetTypeNum();
char *GetTypeName(long category);
long GetTreeInfo(void *buffer, int mode, int argument);
```

`GetIndexNum` 本身不检查 `kind` 范围；调用方必须限制为 `0..4`。
`GetIndexInfo(kind,index)` 会检查 `kind <= 4` 和下标范围。

## 公式集合

TCalc 为五个集合分配独立数组。前四个是公开公式类型，第五个是内部保留
集合：

| kind | 类型 | 最大容量 | DLL 内置数量 |
|---:|---|---:|---:|
| 0 | 技术指标公式 | 5000 | 222 |
| 1 | 条件选股公式 | 2000 | 107 |
| 2 | 专家系统公式 | 500 | 15 |
| 3 | 五彩 K 线公式 | 500 | 35 |
| 4 | 内部保留集合 | 500 | 0 |

公开类型映射同时由四个公式编辑器标题、内置记录内容和公式管理器统计文本
交叉确认，不再是按调用位置推断。

当前版本的内置表 RVA 为：

| kind | 记录 RVA | 数量 RVA |
|---:|---:|---:|
| 0 | `0x00133888` | `0x002466E8` |
| 1 | `0x002466F0` | `0x002466EC` |
| 2 | `0x002CAEE0` | `0x002DD810` |
| 3 | `0x002DD818` | `0x002DD814` |

这些地址只适用于上述 SHA-256。离线提取器先校验文件指纹，再通过 PE 节表
把 RVA 换算为文件偏移，不直接写死 raw offset。

## `tag_INDEXINFO`

每条公式记录为 5072 字节。列表导出需要的字段为：

| 偏移 | 宽度 | 含义 |
|---:|---:|---|
| `0` | 2 | 初始化后的集合内序号 |
| `2` | 1 | 公式 kind |
| `3` | 14 | 公式代码，GB18030/ASCII、NUL 结尾 |
| `17` | 50 | 显示名称，GB18030、NUL 结尾 |
| `67` | 1 | 分类编号 |
| `68` | 2 | 主图/副图及显示属性 |
| `72` | 1 | 参数槽数量，最多 16 |
| `73` | `16 × 132` | 参数槽数组 |
| `2185` | 1 | 输出槽数量 |
| `2186` | `N × 28` | 输出槽数组，槽首为显示名；允许匿名内部槽 |
| `5052` | 4 | 公式正文首地址；受保护公式为 0 |
| `5056` | 4 | 动态文本指针之一 |
| `5060` | 4 | 动态文本指针之一 |
| `5064` | 4 | 动态公式内容指针之一 |
| `5068` | 4 | 来源和其他属性标志 |

内置静态记录的 `+0/+2` 会在 `ResetSystem` 装载时重新写入顺序号和 kind；
离线提取应采用所在表的 kind 和数组下标。

参数槽的已验证布局为：名称 `+0/16`，最小值 `+16/f32`，最大值
`+20/f32`，步长 `+24/f32`，默认值 `+28/f32`，当前值 `+32/f32`。
统一工具会把这些签名和输出槽一起写入公式 JSON；匿名参数/输出槽按 DLL
原样保留，避免把内部绘图槽误判为损坏记录。

`+5052` 已进一步通过 361 条记录交叉验证：它保存首选映像基址下的公式正文
绝对地址。离线工具在校验 DLL SHA-256 后，以 PE `ImageBase` 将它换算成 RVA，
逐字节限制在已映射节内读取 NUL 结尾的 GB18030 文本。四类 379 条公式中，
361 条能直接恢复完整正文；18 条受保护技术公式的字段为 0，包括 `ASI/SAR`
和部分筹码公式。工具不会用附近字符串猜配，而是只在对应原生处理函数、辅助
求值器和真实行情都完成验证后，以 `source_text_origin=native-recovered` 暴露
恢复体。当前 18 个原生入口均已完成，因而 379 条记录全部具有可解析正文或
恢复体；公式接口仍可按正文中的函数名搜索，例如 `FINANCE`。

来源显示的判断顺序已经从公式管理器汇编确认：

```text
flags & 0x001  -> system
flags & 0x010  -> temporary
flags & 0x800  -> default
otherwise      -> user
```

公式管理器统计“自定义公式”时使用 `flags & 0x002`。

## 分类树

技术指标有 16 个分类：

```text
大势型、超买超卖型、趋势型、能量型、成交量型、均线型、图表型、路径型、
停损型、交易型、神系、龙系、鬼系、其他系、特色型、其他类型
```

条件选股有 6 个分类：

```text
指标条件、基本面、即时盘中、走势特征、形态特征、其他类型
```

分类节点每项 44 字节：`id`、`parent`、32 字节名称和尾部标志。
`GetTypeNum/GetTypeName` 只返回技术指标分类；条件选股分类应使用
`GetTreeInfo(buffer, 1, ...)`。专家系统和五彩 K 线没有同类静态子分类树。

## 初始化与文件边界

`InitMain` 的装载顺序为：

1. `ResetSystem(0xFF)` 从 DLL 恢复四类内置记录；
2. 用安装目录 `index.dat` 补充元数据；
3. 可选加载用户目录 `PriDefault.dat`；
4. 加载 `PriGS.dat`；
5. 加载 `PriCS.dat` 和 `PriLoc.dat` 等辅助数据。

这不是纯读取过程。初始化会处理恢复文件、建立备份和目录；析构路径
`CloseAll` 还会保存 `PriGS.dat/PriCS.dat`。因此不能为了枚举列表而在真实
用户目录中自行构造和销毁 `CMainCalcInterface`。

安全路线分为：

- 系统内置列表：直接离线解析 DLL；
- 用户公式列表：纯 C++ 只读解析 `PriGS.dat`，不初始化或销毁 DLL 对象；
- 当前完整列表：附加已经初始化的 TdxW，只调用 getter；
- 独立宿主：只允许对复制到临时目录的公式库调用 `InitMain`。

### PriGS/PriCS 精确格式与只读用户库

新增 IDA 证据已把两个名字相近的文件分开：

- `PriGS.dat` 保存用户公式定义。版本 5 使用 53 字节头、每公式 10 字节动态段
  索引、每公式 5072 字节固定记录及末尾加密动态区；
- `PriCS.dat` 只保存 `flags & 1` 的系统公式参数状态。版本 5 使用 42 字节头，
  随后每公式 16 个、每个 132 字节的参数槽，即每公式 2112 字节；它不是源码库。

`PriGS.dat` 头部给出公式总数、固定记录起点、动态区起点/长度和五类计数。版本 4/5
的 10 字节索引依次记录固定结构中四个动态指针对应的字节数；第一个动态段就是
源码。整个动态区使用未经密钥扩展的 Blowfish 初始 P/S 表做 ECB 变换，32 位字按
小端解释。原生加载器先解密整个动态区，再按索引恢复指针。纯 C++ 实现复现同一
读链，并严格校验偏移、计数、50 MiB 上限、8 字节对齐和不足 8 字节的尾部填充。

只查看用户公式：

```powershell
tdx-tool formulas user-library --root C:\new_tdx `
  --output C:\tmp\tdx-user-formulas.json
```

生成可直接交给 `formulas analyze/evaluate/audit/scan/backtest` 的系统+用户合并库：

```powershell
tdx-tool formulas user-library --root C:\new_tdx `
  --scope combined --output C:\tmp\tdx-installed-formulas.json
```

不需要中间文件时，解释器工作流可显式直接装载用户公式。例如：

```powershell
tdx-tool formulas evaluate --root C:\new_tdx --include-user `
  --formula 用户公式代码 --input C:\tmp\kline.json
tdx-tool formulas scan --root C:\new_tdx --include-user `
  --formula 用户公式代码 --input C:\tmp\kline.json
```

相同开关适用于 `analyze/context-template/audit/evaluate/scan/watch/strategy/backtest`。
`watch` 沿用 `scan` 选项；组合策略清单中的 `formula` 字段也能直接引用用户条件公式。
`--include-user` 必须与安装目录库配合，不能和调用方提供的 `--library` 或
`--source-file` 混用。

### 系统/用户公式之间的输出引用

通达信源码用双引号标识同证券指标输出引用，例如：

```text
DIFF:"基础指标.FAST"-"基础指标.SLOW";
```

解释器不再只为内置源码里的 `"KDJ.J"` 写死计算式。库驱动解析会在当前装载的
系统+用户公式库中按代码选择技术指标、按名称选择输出线，在相同证券、相同周期和
相同 K 线窗口递归执行；被引用指标沿用自身参数默认值。同一个子指标被引用多条输出
时仅执行一次。上下文元数据通过 `formula_reference_resolutions`、
`formula_reference_binding_count` 和 `formula_reference_evaluation_count` 报告实际
解析结果。

安全边界如下：

- 仅接受完整的 `指标代码.输出名`，缺失指标或输出会报错，不回落为 0；
- 被引用项必须是数值安全、非未来的技术指标；
- 递归引用与 `CALCSTOCKINDEX` 共用 4 层深度限制和循环检测；
- 支持位置参数，例如 `"MACD.DIF"(12,26,9)`、`"MACD.DIF"(6+6,26,9)` 和
  `"MACD.DIF"(N,26,9)`；参数按子指标元数据顺序覆盖，未提供的尾部参数继续使用
  子指标默认值；
- 参数范围限定为有限常量、`+ - * / %` 算术以及父公式声明参数。逐 K 线变化的
  `"指标.输出"(CLOSE)` 不会取末值冒充标量，而是保持明确不可执行。

`formulas analyze` 还会附加 CLI-only 的
`formula_reference_graph`（schema `tdx-formula-reference-graph-v1`），在真正取行情和
执行公式之前列出所有引用公式与绑定，并分别统计 `resolved_binding_count`、
`missing_formula_count`、`missing_output_count`、`executable_binding_count` 和
`cycle_component_count`。循环检测按技术指标依赖图的强连通分量计算，既覆盖互相引用，
也覆盖指标直接引用自身；每条引用还会给出 `runtime_eligible` 和 `in_cycle`。该字段不
加入长期服务的 `/api/v1/formulas/coverage`，因此没有改变既有 HTTP schema。

```powershell
tdx-tool formulas analyze --root C:\new_tdx --include-user `
  --output C:\tmp\tdx-formula-analysis.json
```

该能力已贯通 `evaluate/audit/scan/watch/backtest/strategy`。基础引用证据见
[公式库输出引用记录](../99-log/2026-08-12-native-formula-library-references.md)，参数化
语义与原 DLL 编译探针见
[参数化公式引用记录](../99-log/2026-08-12-native-formula-parameterized-references.md)。

该入口标记 `private_user_data=true`，保持 CLI-only，不经 HTTP 暴露用户源码；全程
不加载 `TCalc.dll`、不写回 `PriGS/PriCS`、网络请求为 0。当前真实安装恢复 1 条
用户条件公式，源码语法支持、可执行且无外部依赖；与内置库合并为 380 条，并已在
平安银行 120 根本地日线上由 `tdx-source-interpreter-v1` 完成求值。完整证据见
[用户公式只读库](../99-log/2026-08-12-native-user-formula-library.md)。

## 可重复工具

### 纯 C++ 统一工具

当前 64 位统一工具不会加载 32 位 `TCalc.dll`。它直接复用已还原的 7709
K 线协议，并实现 26 类高频指标；输出名按 TCalc 静态槽位而不是周期值命名：

```text
MA    MA1 / MA2 / MA3 / MA4
MACD  DIF / DEA / MACD
KDJ   K / D / J
RSI   RSI1 / RSI2 / RSI3
BOLL  BOLL / UB / LB
CCI   CCI
WR    WR1 / WR2
BIAS  BIAS1 / BIAS2 / BIAS3
DMA   DIF / DIFMA
MTM   MTM / MTMMA
ROC   ROC / MAROC
TRIX  TRIX / MATRIX
ATR   MTR / ATR
VOL   VOLUME / MAVOL1 / MAVOL2
OBV   OBV / MAOBV
PSY   PSY / PSYMA
VR    VR / MAVR
BRAR  BR / AR
BBI   BBI
EXPMA EXP1 / EXP2
DMI   PDI / MDI / ADX / ADXR
WVAD  WVAD / MAWVAD
EMV   EMV / MAEMV
CHO   CHO / MACHO
ADTM  ADTM / MAADTM
DKX   DKX / MADKX
```

示例：

```powershell
tdx-tool formulas calculate --market sz --code 000001 `
  --formula MACD --period day --pages 1

tdx-tool formulas calculate --market sz --code 000001 `
  --formula KDJ --period 30m --pages 2 --date all
```

输出按日期与时间重新升序并去重，包含原始 OHLCV、每个指标输出、实际参数
以及 `next_start/has_more`。支持 `--param name=value` 覆盖默认参数。引擎名为
`tdx-native-compatible-v4`，报告明确包含 `dll_loaded=false`；这表示已验证的
标准公式兼容子集，不表示任意 TCalc 字节码执行。

7709 个股 K 线的 `volume` 是股数，而通达信公式函数 `VOL` 的单位是手；原生
计算器只在公式输入侧对非指数证券除以 100，输出点中的原始 `volume` 不变。
指数成交量保持协议原值。结果通过 `formula_volume_unit` 和
`formula_volume_divisor` 明确返回该分支，避免调用方重复换算。

只读 HTTP 接口为：

```text
/api/v1/formulas/calculate?market=sz&code=000001&formula=macd&period=day
```

Svelte 公式库页面包含同一接口的验证台和 Lightweight Charts 图表。

### 自定义源码的本地解释、扫描与回测

CLI 的 `formulas evaluate/scan/backtest` 均接受 `--source-file`，使用同一纯 C++
源码定义、语义分析和执行器；`scan/backtest` 不再要求先把公式写入 TCalc 导出的
公式库。公式库页面可切换“内置公式/自定义源码”，并在单票计算、条件扫描、
专家回测三种任务之间切换，直接粘贴通达信公式、覆盖 `NAME=NUMBER` 参数，选择
证券、周期和历史页数。Ctrl+Enter 按当前任务执行。数值点与稀疏绘图 IR 仍来自
`tdx-source-interpreter-v1`，浏览器不包含替代公式计算器。

三个接口及其确认头为：

| 任务 | 接口 | `X-TDX-Action` | 源码约束 |
| --- | --- | --- | --- |
| 单票计算 | `POST /api/v1/formulas/evaluate` | `formula-evaluate` | 技术/绘图公式；未来函数仅显式只读 |
| 条件扫描 | `POST /api/v1/formulas/scan` | `formula-scan` | 数值信号安全的条件公式，最多 50 只证券 |
| 专家回测 | `POST /api/v1/formulas/backtest` | `formula-backtest` | 必须输出 `ENTERLONG/EXITLONG`，禁止未来函数 |

#### TCalc 静态注册表与自定义公式核心

2026-08-12 的新数据库进一步闭合了 L2 订单流入口。解释器不从 L1
推导这些值，而是接受调用方拥有的授权时间序列：`L2_VOL/L2_AMO` 使用
`0..3 × 0..3` selector，`L2_VOLNUM` 使用 `0..1 × 0..1` selector；
`ACTINVOL/ACTOUTVOL/LARGEINTRDVOL/LARGEOUTTRDVOL` 与申撤单、均价、当前
买卖单字段均按 TCalc 回调 type-31 的 184 字节记录投影。`ISBUYORDER` 来自独立
type-104 byte 46，由 `level2 decode --format tcalc-order-side` 输出可直接注入的标量
上下文。`formulas context-template` 会为这些函数生成精确空键；
`level2 decode --format tcalc-order-flow` 可把合法捕获转成每条记录的 53 个公式绑定值，
包括 `TRADENUM/TRADEINNUM/TRADEOUTNUM/LARGETRDINNUM/LARGETRDOUTNUM`。
解释器接收解码文档时会按目标日 K 的真实 `date|time` 执行日期合并、裁剪和尾部继承；
截断文档、无效日期及分钟/周/月误用会明确拒绝。完整布局和 handler 地址见
[TCalc L2 订单流记录](../99-log/2026-08-12-native-tcalc-level2-order-flow.md)。

`TCalc.dll!sub_100C6870` 是函数表静态初始化器。对当前指纹
`13facaa52dac552c5be1f63331781219de9bf798c443af8f5e4afc193a02e7f5`
进行离线指令仿真，恢复出 390 个唯一的 0x47 字节注册记录；每条记录包含内联名称、
opcode 和处理函数指针。该数字只证明 DLL 注册了这些入口，不代表它们都无外部
数据依赖，也不代表纯 C++ 解释器已经实现全部入口。

早期探针曾报告 386。根因是把 IDA 的 `word_*` 源操作数误按一字节读取，导致
部分名称末字节缺失，继而把仅一至两个字符的 `TR/MA/IF/LN` 四项过滤掉。修正
操作数宽度并改用逐记录 JSONL 后，390 条记录、390 个唯一名称和 71 字节记录
尺寸可独立复核；权威清单为 `output/ida-tcalc-function-registry-jsonl-v4.log`。

当前能力清单由 `formulas analyze` 和 `/api/v1/formulas/coverage` 的
`capabilities` 返回：228 个支持函数、69 个自动符号；其中首批直接按 Hex-Rays
处理函数恢复的无授权依赖核心为：

- 自动符号：`YEAR/MONTH/DAY/WEEKDAY`、`BARSTATUS/TOTALBARSCOUNT`；
- 序列函数：`CONST`、`RANGE`；
- 数学函数：`ACOS/ASIN/ATAN/COS/SIN/TAN`、`FRACPART`、`SIGN/SGN`。

第二批继续按注册 opcode 和处理函数闭合 12 个无授权函数：

- 时序与过滤：`BARSLASTS`、`BARSSINCEN`、`FILTERX`；
- 平滑：`TMA`、`XMA`；
- 窗口排行：`FINDHIGH/FINDHIGHBARS/FINDLOW/FINDLOWBARS`；
- 统计：`COVAR`、`RELATE`、`BETAEX`。

`BARSSINCE` 也按处理函数修正为首次成立前缺失。`FILTERX/XMA` 会读取未来柱，
因此只能进入显式只读绘图，不能扫描或回测。`COVAR` 使用样本分母 `N-1`；
`BETAEX(X,Y,N)` 是两条调用方序列的协方差/`Y` 方差。详细地址、窗口边界和
固定向量见[时序搜索与统计核心记录](../99-log/2026-08-09-native-formula-sequence-statistics.md)。

第三批继续直接按处理函数闭合 15 个滚动、动态引用和方差函数：

- 滚动与动态引用：`AMA`、`REFV`、`MULAR`；
- 窗口排行：`HOD`、`LOD`、`HHVLLV`；
- 条件与有效性：`IFF`、`IFN`、`ISVALID`、`MAX6`、`MIN6`；
- 离差与方差：`DEVSQ`、`VAR`、`VARP`、`STDP`。

其中 `VAR/STD` 使用样本分母 `N-1`，`VARP/STDP` 使用总体分母 `N`，并保留
TCalc 在首个完整窗口输出 0 的边界。既有 `STD` 已由错误的总体口径修正为样本
标准差；既有 `STDDEV` 则不是普通收盘价标准差，而是对前一柱之前的 `N-1` 个
对数收益率计算总体标准差。`REFV` 的动态越界会沿用上一输出，`MULAR(X,0)` 从
首个有效值累计连乘。详细地址、容差和固定向量见
[滚动与方差核心记录](../99-log/2026-08-09-native-formula-rolling-variance.md)。

第四批闭合 `SUMBARSX` 与 `BETA(N)`，并修正原有 `SUMBARS` 的计数偏一：

- `SUMBARS` 在当前柱已达到目标时为 0；回看达到时通常返回参与柱数，但到达
  最左有效柱或历史不足时封顶为距最左柱的距离。
- `SUMBARSX` 当前柱严格超过目标时为 -1、容差内相等时为 0；需要历史柱时返回
  前向间隔数，历史不足则保持无效值，不像 `SUMBARS` 那样返回已有历史总数。
- `BETA(N)` 根据证券市场和代码选择宿主原生基准，下载相同周期/分页的基准
  K 线并按时间戳对齐 CLOSE，再将个股与基准转换为简单收益率后调用
  `BETAEX`。深市默认 `399001`、创业板 `399006`；沪市默认 `999999`、科创板
  `000688`；北交所 `899050`，港股为 `HSI`，期货类合约取非数字品种前缀加
  `L9`。响应 `context.beta_benchmark` 会明确报告实际绑定，不做静默猜测。

能力清单为这两个入口新增 `custom_formula_benchmark_cumulative_*` 精确分组；
完整证据见[市场基准与累加距离记录](../99-log/2026-08-09-native-formula-beta-sumbarsx.md)。

第五批继续闭合日历、时间、对齐和交易信号状态机：

- 自动符号 `WEEKOFYEAR` 按周日切换周序，`TIME2` 返回带秒的 `HHMMSS`；
- `DATETODAY/DATETOTODAY` 修正为以 1990-12-19 为 0 的相对日序，
  `DAYTODATE` 作为逆换算；
- `TIMETOSEC/SECTOTIME` 按原生边界在 `HHMMSS` 与日内秒数间换算；
- `ALIGNRIGHT` 将有效序列压到右侧并保持长度；
- `TFILT` 按日期/分钟闭区间过滤，`TFILTER/TTFILTER` 按原生买卖状态机消除
  重复信号并区分四种输出事件。

能力清单新增 `custom_formula_calendar_filter_functions` 7 项和
`custom_formula_calendar_filter_symbols` 2 项。`TFILTER/TTFILTER` 同柱双信号的
分支顺序、`TFILT` 的默认末日及时间钳制、日期换算的容差边界均由 32 位原 DLL
固定向量锁定，而不是按函数名推测。完整证据见
[日历、时间与信号过滤记录](../99-log/2026-08-09-native-formula-calendar-filters.md)。

第六批闭合证券元数据、字符串判断和涨跌方向：

- `NAMELIKE/CODELIKE` 分别对证券名、证券代码做原生字节前缀判断；
- `NAMEINCLUDE/FINDSTR` 使用原生子串判断；
- `STR2CON` 保留 CRT `atof` 行为，非法文本为 0；
- `UPDOWN` 首根有效柱缺失，随后按前一柱和原生相对加绝对容差输出 -1/0/1；
- `NOT` 只把精确 `+0/-0` 判真，不再使用解释器通用近似真值；
- 自动字符串符号 `STKNAME` 从证券元数据绑定并可直接进入文字绘图表达式。

能力清单新增 `custom_formula_security_string_functions` 7 项和
`custom_formula_security_string_symbols` 1 项。真实 7709 K 线若不携带名称，服务
会从启动时已加载的本地 TNF 证券主表补齐，并标记
`name_source=local-tnf-security-master`；单次执行、审计、扫描、回测和组合策略共用
同一规则。完整证据见
[证券字符串与涨跌方向记录](../99-log/2026-08-09-native-formula-security-strings.md)。

第七批继续闭合 GBK 字符串构造和逐柱/非序列分层：

- `STRLEN` 和 `SUBSTR` 按原 DLL 的 GBK 字节而非 UTF-8 码点计数/切片；
- `STRSPACE` 追加一个 ASCII 空格，`STRCAT6` 拼接六个非序列字符串；
- `VAR2STR/VARCAT/VARCAT6` 对每根 K 线分别计算；
- 既有 `CON2STR/STRCAT` 修正为只读末柱后广播，数字精度钳制为 0..4。

能力清单的 `custom_formula_security_string_functions` 由 7 项增至 14 项。
32 位原 DLL 直调同时对照字符串池输入输出，确认 `VAR*` 逐柱与 `STR*/CON2STR`
末柱广播；完整证据见
[GBK 字符串构造记录](../99-log/2026-08-09-native-formula-string-builders.md)。

第八批闭合四个由 TdxW 宿主填充的无括号证券状态入口：

- `IST0CODE` 精确合并深/沪可转债固定代码规则、`#北证可转债` 与
  `spblock.dat/#T+0基金`；
- `ISSTCODE` 只对原生 A/B/科创/北证股票类别检查名称中的 ASCII `ST`；
- `ISQUITCODE` 复现 `infoharbor_spec.cfg` 的状态 0 和第四字段启用日期规则；
- `ISQHQQCODE` 只接受原证券记录类别 3/12，A 股来自 TNF 偏移 282，扩展市场
  来自 7727 品种目录，不按市场号推测。

四项分别对应 TdxW type 167/120/105 回调，解释器将其识别为注册函数及无括号
外部符号，自动填入当前证券上下文并报告来源元数据。证券/字符串/状态分组由
14 项增至 18 项；完整证据见
[证券状态宿主字段记录](../99-log/2026-08-09-native-formula-security-status.md)。

第九批闭合无需授权会话的本地板块元数据簇：

- `INBLOCK('板块名')` 对当前证券的本地直接/继承归属做完整名称匹配；
- `FGBLOCK/FGBLOCKNUM` 与 `ZSBLOCK/ZSBLOCKNUM` 分别物化风格、指数板块名称
  和直接归属数量，`GNBLOCKNUM` 返回概念直接归属数量；
- `HYZSCODE` 复用 type-120 行业层级选择，返回当前通达信行业指数代码。

自动上下文仅在公式实际依赖这些入口时读取 `infoharbor_block.dat`、`tdxzs3.cfg`
和 `tdxhy.cfg`。平安银行真实样本得到 `INBLOCK('银行')=1`、不存在块名为 0、
三类数量合计 29、`HYZSCODE=880471`。组合板块、自定义板块和需要原客户端账户/
用户配置的 `ZHBLOCK/ZDBLOCK` 仍保持未实现，不以空字符串冒充成功。

第十批继续闭合三个完全不依赖行情扩展、登录或宿主回调的纯序列入口：

- `CONSTA(X,N)` 读取参数末柱，将 N 截断并钳制到 `0..总柱数-1`，再把倒数第
  `N+1` 柱的 X 广播到全序列；它不是 `CONST(X)` 的别名；
- `ROUND2(X,N)` 读取精度参数首柱并钳制到 0..4，按原 DLL 的 `±0.50300002`
  偏置做十进制舍入，负数、半值和缺失值均由固定向量锁定；
- `EXISTR(X,N,M)` 判断从 N 柱前到 M 柱前的反向区间，`N=0` 时从首个有效柱
  累计；当前柱缺失则输出缺失，而窗口内部的原生缺失哨兵按 DLL 数值比较参与
  真值判断，这个反直觉边界也被保留。

三项使 `custom_formula_core_functions` 由 11 增至 14，解释器支持函数由 200
增至 203。处理函数、候选排除和 32 位原 DLL 对照见
[纯序列反向工具记录](../99-log/2026-08-09-native-formula-reverse-utilities.md)。

第十一批闭合两个需要独立算法向量的原生变换入口：

- `NEWSAR(N,S)` 读取 OHLC 和收盘方向，预热 `N-1` 柱；初始转向价取首窗高点或
  低点，加速步长为 `S/1000`，连续创新极值时递增，按收盘越过投影值反转并以
  前 N 柱极值重置；没有传统 SAR 的独立最大加速参数；
- `FFTRANS(X,N)` 将 X 同时填入复数实部和虚部，按最多 1024 点分段执行原生
  位反转与基 2 蝶形并返回实部。2 次幂段等价于离散 Hartley 变换；非 2 次幂段
  保留原 DLL 的不完整蝶形，尾段按剩余长度缩短，不能改成标准 FFT 库调用。

两项进入 `custom_formula_transform_functions`，使支持函数由 203 增至 205。
`FFTRANS` 会读取当前分段内的未来点，因此标记为未来函数，只能在显式
`allow_future=true` 的只读执行/绘图中使用，扫描和回测继续拒绝。处理函数、
辅助链及 32 位原 DLL 固定向量见
[NEWSAR/FFTRANS 记录](../99-log/2026-08-09-native-formula-newsar-fftrans.md)。

第十二批闭合两个依赖 TdxW 宿主回调、但不依赖登录或 Level2 的当前日历入口：

- `ISJYDATE` 对应 TCalc opcode 1378 和 TdxW type 122，比较宿主当前交易日
  `YYYYMMDD` 与机器本地日期，相等时广播 1，否则广播 0；它不是带日期参数的
  任意节假日判断；
- `LOCALDAYNUM` 对应 opcode 1367 和 type 168。沪深北读取
  `vipdoc/<sz|sh|bj>/lday/<前缀><代码>.day`，扩展市场读取
  `vipdoc/ds/lday/<市场号>#<代码>.day`，返回文件长度除以 32；沪深北本地最后
  一条记录日期落后当前交易日时再加 1。

解释器将两项同时支持为无括号宿主符号和零参数调用，新增
`custom_formula_host_calendar_functions` 两项，使支持函数由 205 增至 207；
自动符号仍为 54，因为两项需要自动市场上下文。API 元数据会返回当前交易日、
机器日期、本地文件、原始记录数、最后日期和是否补当前交易日。处理函数、TdxW
文件路径及原 DLL 伪宿主固定向量见
[宿主日历记录](../99-log/2026-08-09-native-formula-host-calendar.md)。

第十三批闭合依赖 TdxW type 103 历史流通股本、但不依赖登录或 Level2 的
`LFS()`：

- TCalc opcode 1197 的处理函数 `sub_1002CC30` 用 35 字节 K 线偏移 `+27`
  的原始成交量除以 type-103 每柱第二个 float（流通股本）；
- 首个有效点同时初始化快慢状态；后续快状态按保留系数 `4/5`、慢状态按
  `12/13` 与 `(1-换手率)` 衰减，输出 `(1-快/慢)*100`，并非普通 EMA；
- 前导无效股本保持缺失，序列内部无效股本继承上一有效值；首个有效柱成交量
  为零时输出缺失，但后续正成交量可以恢复；
- 深市 `39*`、沪市 `8*`/`<=000999`/`>=990000`、北市 `899000..899999`
  保留原生指数类证券门控并返回缺失。

自动上下文直接复用现有 0x000f 股本变更记录生成逐日 `CAPITAL` 序列；公式环境
的 `CAPITAL` 为手，执行前还原为股，以保留原 DLL 的 `<1 股` 缺失阈值。能力
清单新增 `custom_formula_capital_turnover_functions=["LFS"]`，支持函数由 207
增至 208，自动符号仍为 54。六组 32 位原 DLL 固定向量、type-103 分派和边界
见[LFS 历史股本换手记录](../99-log/2026-08-09-native-formula-lfs.md)。

第十四批闭合三个只依赖 CRT 本地时间、无需行情宿主回调的机器时钟入口：

- `MACHINEDATE`（opcode 1330）捕获本机年月日，广播
  `(YEAR-1900)*10000+MONTH*100+DAY`；
- `MACHINETIME`（opcode 1331）广播 `HHMMSS`，午夜后第一秒为 1；
- `MACHINEWEEK`（opcode 1332）复用与 K 线 `WEEKDAY` 相同的原生日期帮助函数，
  周日为 0、周六为 6。

三项分别对应 `sub_10034FD0/sub_100350A0/sub_10035160`。32 位探针通过 IAT
替换 TCalc 的 `_time64`，固定闰日与周日两个时间点，排除了执行时钟漂移：
`2024-02-29 23:59:58 -> 1240229/235958/4`，
`2025-01-05 00:00:01 -> 1250105/1/0`。解释器同时支持裸符号和零参数调用，
新增 `custom_formula_machine_clock_functions` 三项，使支持函数由 208 增至 211、
自动符号由 54 增至 57。分析结果显式返回
`has_machine_clock_dependency=true` 和 `pure_ohlcv=false`，但无需自动市场上下文。
完整证据见[机器时钟记录](../99-log/2026-08-09-native-formula-machine-clock.md)。

第十五批闭合 `DATETOCUR`（opcode 1394，`sub_1000F6A0`）。它不是日期相减：
处理函数只读取参数序列末柱，截断为整数日期；随后对每个输出柱扫描全部 35 字节
K 线记录，统计 `目标日期 < 柱日期 <= 当前柱日期` 的记录数。因比较只到日期，
分钟线同一天的第一根柱会直接计入该日所有后续分钟柱；它还会忽略参数序列前面
各柱的值。因此解释器将其加入未来函数集合，只允许 `allow_future=true` 的只读
绘图执行，扫描和回测始终拒绝。

32 位原 DLL 固定向量确认：日线目标 2024-01-02 输出 `0,0,1,2`；分钟线在
2024-01-03 有三根柱时三根均输出 3；缺失目标等同原生整数转换下的极小日期并
累计全部柱。纯 C++ 使用按日期聚合的前缀计数得到相同结果，避免照搬原 DLL
O(n²) 扫描。日历/过滤函数组由 7 增至 8，支持函数由 211 增至 212，自动符号
保持 57。完整证据见
[DATETOCUR 记录](../99-log/2026-08-09-native-formula-datetocur.md)。

第十六批闭合五个收盘方向段入口（opcode 1040..1044）：
`DHIGH/DOPEN/DLOW/DCLOSE/DVOL`。共享处理函数 `sub_10037640` 逐柱比较相邻
收盘价，并用 `abs(current)*1e-7+1e-5` 判断上升、下降或近似持平；同向和持平柱
留在当前段，方向直接反转时才开启新段。每段保留段首开盘，聚合最高、最低、
段末收盘及成交量，再把最终 OHLCV 回填到段内全部柱。

原 DLL 固定向量确认连续上涨四柱会从第一柱起直接得到整段末值，因此五项统一
列入只读未来函数。解释器支持裸符号和空括号调用，分析器补齐
`OPEN/HIGH/LOW/CLOSE/VOL` 依赖；扫描和回测仍拒绝。能力清单增至 217/62，
新增五项 `custom_formula_directional_bar_functions/symbols`。完整证据见
[方向段记录](../99-log/2026-08-09-native-formula-directional-bars.md)。

第十七批闭合 `TQFLAG`（opcode 1253，`sub_1000E670`）。处理函数逐柱读取求值器
对象偏移 `0xEC82` 的有符号 32 位值；该字段由 K 线求值初始化参数原样写入，不是
从价格反推。32 位原 DLL 固定向量确认 0、1、2、-1、257 都会完整广播，空序列
保持为空，因此不能把它缩窄成字节或布尔量。

纯 C++ 解释器从 K 线文档 `adjustment_mode` 绑定相同语义：缺失、`none/raw` 为 0，
`qfq/front/forward/fixed_qfq` 为 1，`hfq/back/backward/fixed_hfq` 为 2，也接受
数值 0..2。裸 `TQFLAG` 和 `TQFLAG()` 均可用。分析器将其标记为
`has_adjustment_mode_dependency=true`，不误报为纯 OHLCV，也不属于未来或外部
依赖。能力清单增至 218/63，并新增单项
`custom_formula_adjustment_functions/symbols`。完整证据见
[TQFLAG 记录](../99-log/2026-08-09-native-formula-tqflag.md)。

公式 HTTP 复权已进一步闭合。`/api/v1/formulas/evaluate` 的内置 GET、内联源码
POST 和无源码内置公式 POST，以及 `/api/v1/formulas/audit`，都复用
`/api/v1/kline` 已验证的 0x000F 股本变更与本地除权因子链。请求可传
`adjust=none|qfq|hfq`；固定锚点 API 还可传 `fixed_qfq/fixed_hfq + anchor_date`。
求值前先完成 K 线价格复权，再把规范化后的 `adjustment_mode` 装入公式环境，响应
同时保留 `adjustment` 元数据。因此同一次请求中的 OHLC、指标结果和 `TQFLAG`
不会出现一个已复权、另一个仍为 0 的口径分裂。Svelte 公式工作台也已加入三态
选择，并把复权值纳入 GET URL 或 POST body。详细验证见
[公式 HTTP 复权记录](../99-log/2026-08-09-native-formula-http-adjustment.md)。

扫描、单票回测和组合策略也已完成独立安全验证并接受同一显式复权参数。复权发生在
公式上下文和信号计算之前；专家回测的下一开盘成交价与组合策略共同时间轴均来自
已复权序列。单票结果保留完整事件元数据，多证券结果只返回
`security_scope=per-security` 的公共摘要，避免混淆各证券事件。未知模式在批量循环
前拒绝，默认 none 不增加请求。网页扫描监控签名也包含复权选择。详见
[公式工作流复权记录](../99-log/2026-08-09-native-formula-workflow-adjustment.md)。

同日后续又完成 CLI 和批量性能闭环。`formulas evaluate/audit/scan/watch/strategy/backtest`
均接受 none/qfq/hfq/fixed 模式、锚点、TTL 与显式复权输入刷新。进程级
缓存只保存日线和 0x000F 原始输入，以 market/code/root/hosts 为键，默认 900 秒、
严格最多 512 项，并用 singleflight 合并同时 miss；目标周期价格、公式输出和扫描
结果不入该缓存。HTTP 扫描与策略可用 `workers=1..16` 并发抓取，但公式外部上下文
仍按证券顺序构建。详细边界见
[公式 CLI 与缓存记录](../99-log/2026-08-09-native-formula-cli-adjustment-cache.md)。

第十八批闭合裸符号 `MULTIPLIER`（opcode 1252，处理函数 RVA `0x26A40`）。原
TCalc 每次求值只请求一次宿主 type 105，读取回调结构偏移 42 的 signed int16，
再原值广播到全部 K 线；32 位固定向量已覆盖零、正数、负数和两端边界。公开 7727
`0x23F5` 合约目录记录偏移 56 的 u32 低字与该值对应：真实 `IFL9=300`、
`IC2608=200`、`HO8W03UX=100`、`LC2608=1`。

纯 C++ 上下文只为扩展目录类别 3/12 读取精确合约并按 signed-low-word 绑定；标准
A 股及非衍生品返回 0。结果公开 raw/category/mode/source 元数据，能力清单新增
`custom_formula_contract_metadata_symbols`。它是宿主外部裸符号，不支持没有
原生证据的零参数调用。完整证据见
[MULTIPLIER 记录](../99-log/2026-08-09-native-formula-multiplier.md)。

第十九批闭合两个裸符号 `ZSTJJ/QHJSJ`。`ZSTJJ` opcode 1329 的
`sub_100155F0` 与 `QHJSJ` opcode 1008 的 `sub_10015680` 都按 35 字节步长读取
每根宿主 K 线偏移 31 的 float；固定向量进一步证明普通值、精度值、负零、
NaN/Inf 和缺失哨兵的输出均按位一致。处理函数自身没有市场或周期判断。

宿主字段在分时图周期表示累计均价，在期货/期权 K 线中表示结算价。纯 C++ 对
7727 直接复用 `auxiliary_price/settlement_price`；普通 7709 `time` 帧没有预计算
字段，因此按交易日累计成交额/累计成交量重建。两者加入
`custom_formula_kline_auxiliary_symbols`，自动符号由 63 增至 65，只支持裸符号。
完整证据见
[K 线辅助字段记录](../99-log/2026-08-09-native-formula-kline-auxiliary.md)。

第二十批闭合 type-167 的两个证券文字裸符号。`LEVEL1HYBLOCK`（opcode 1372）
读取回调结构偏移 78；TdxW 先取证券研究行业代码的前三字节，再通过
`tdxzs3.cfg` 映射一级研究行业名称。`MAINBUSINESS`（opcode 1355）读取偏移 0；
TdxW 延迟加载 `T0002/hq_cache/specgpext.txt`，按数值市场/代码匹配后返回第三个
竖线分隔字段。纯 C++ 复用既有研究行业树，并增加带文件时间/大小失效的主营构成
只读缓存；缺文件和缺记录都保持原宿主空字符串语义。

能力清单新增
`custom_formula_type167_text_symbols=[LEVEL1HYBLOCK,MAINBUSINESS]`，自动符号由
65 增至 67，支持函数仍为 218。平安银行真实结果分别为“银行”和“零售金融业务”，
文字绘图可直接消费二者。完整证据见
[type-167 证券文字记录](../99-log/2026-08-10-native-formula-type167-text.md)。

第二十一批又闭合 `MOREHYBLOCK/ZDBLOCK/ZDBLOCKNUM`。前者按
`T0002/user.ini` 的 `[Other] UseTdxL3HY` 精确选择普通或研究行业叶子；后两项
复现 command-8 category 2 的固定 `zxg/tjg`、现代 `blocknew.cfg`、旧版
`block.cfg` 与 `.blk` 成员解析，并保留 TCalc 将竖线转换为空格后的尾随空格。
能力清单自动符号由 67 增至 69。证据见
[行业模式与自定义板块记录](../99-log/2026-08-10-native-formula-morehy-zdblock.md)。

第二十二批闭合 `GNBKZSCODE/FGBKZSCODE/GETNAMEOFCODE`。TdxW type-167 只收集
当前证券的直接概念（catalog type 4）和风格（type 5）成员，分别按 DWORD 数值
升序排列，概念在前、风格在后，总数组最多 60 项；TCalc 再以一基序号返回对应
十进制代码字符串。`GETNAMEOFCODE` 把 uint16 市场和代码交给 type-120，取 TNF
证券记录偏移 31 的名称。纯 C++ 使用按三份 TNF 文件时间/大小失效的共享缓存，
同时允许已加载板块目录覆盖板块名称；越界或未命中保持空字符串。能力清单增至
221 个支持函数、69 个自动符号，静态注册表已识别并集为 273，剩余 117。
平安银行当前 `GNBKZSCODE(1)=880609`、`FGBKZSCODE(1)=880679`，名称分别为
“跨境支付”和“周期股”。完整证据见
[板块代码与名称记录](../99-log/2026-08-10-native-formula-block-code-name.md)。

第二十三批闭合五个广播式单点数据函数。`FINONE(FIELD,YEAR,MMDD)` 的
type-172 宿主分支不是普通“最近报告”别名：完整日期只在所属季度窗口内选值，
`(0,0)` 取最新，`(N,0)` 向前 N 年，`(0,N<=300)` 向前 N 个季度，独立 MMDD
则取最近一次该月日。纯 C++ 会按绑定参数缩小官方 `gpcw` 报告包范围；最新值
只校验一个报告期，不再默认扫描 80 个包。

`GPJYONE/BKJYONE/SCJYONE(ID,FIELD,YEAR,MMDD)` 复现 type-175：完整日期必须
精确命中，解码日期小于 10000 时按同一字段的倒序记录号选择，`FIELD=1/2`
选择 12 字节宿主记录中的两个 float。数据来自校验过的官方 `tdxgp`；板块函数
先按 type-120 将股票映射到直接行业，并保留 TdxW `sub_4D19B0` 的宽基指数固定
映射。`GPONEDAT(ID)` 则读取 `T0002/hq_cache/gp{sz,sh,bj}one.dat` 的
10 字节代码/字段/float 记录，文件或记录缺失时保持宿主的 0。

能力清单因此增至 226 个支持函数、69 个自动符号；静态注册表识别并集为 278，
剩余 112。真实平安银行最新样本返回 `FN183=4.65`、股东人数 `457610`、银行板块
市盈率 `6.84` 和市场融资余额 `261556640`；本机未生成 `gpszone.dat`，对应
`GPONEDAT(7)=0`。证据和部署记录见
[单点财务与经营数据记录](../99-log/2026-08-10-native-formula-single-point-data.md)。

第二十四批闭合 `BLOCKSETNUM('板块名')`。TCalc opcode 1244 的处理函数向宿主
发送 command 8、type 5 请求，并把返回区偏移 1004 的 DWORD 数量广播。TdxW
先按 `HY./GN./MY.` 强制目录，或按当前行业、概念、风格、指数、自定义板块顺序
解析无前缀名称，再枚举成员并只返回数量。

纯 C++ 自动上下文复用本地板块索引和 `UseTdxL3HY` 行业模式；静态字符串参数
会生成精确标量绑定，动态名称明确拒绝。API 元数据逐项公开 family、板块代码、
来源、数量和命中状态。真实样本为 `HY.银行=42`、`GN.跨境支付=75`、不存在板块
为 0。能力清单增至 227 个支持函数、69 个自动符号；静态注册表识别并集为 279，
剩余 111。完整证据见
[BLOCKSETNUM 板块成分数量记录](../99-log/2026-08-10-native-formula-blocksetnum.md)。

第二十五批闭合 `HORCALC('板块名',数据项,计算方式,权重)`。TCalc opcode 1245
先由 command 8、type 7 取得目标板块成员，再逐证券请求行情、K 线和必要的财务
字段。32 位原 DLL 探针确认数据项 100 至 106 依次为最高、开盘、最低、收盘、
成交量、涨跌幅和成交额；计算方式 0/1/2 依次为求和、当前证券排名和平均；权重
0 至 4 依次为总股本、流通股本、等权、流通市值和总市值。

纯 C++ 自动上下文复用 `BLOCKSETNUM` 的 `HY./GN./MY.` 与无前缀目录解析，读取
`vipdoc/{sh,sz,bj}/lday` 的 32 字节日线记录，并按目标 K 线日期对齐每只成分股。
缺失交易日沿用该证券上一条有效值，首条有效值之前保持 0；求和、排名和加权平均
保留原生 float 累加与容差。只有非等权平均才批量读取股本，只有市值权重才再取
最新行情；响应元数据公开每个绑定的目录、板块代码、成员数、成功日线数和覆盖率。

当前自动上下文只开放日线。分钟、周线和月线会明确返回“不支持”，不会用日线
重采样冒充原生结果；动态板块名或数据项/计算方式/权重越界也会拒绝。真实研究行业
`HY.银行` 有 42 个成员、42 份本地日线，2026-08-07 的等权收盘横截面为总和
368.3199768066406、平均 8.769523620605469，平安银行排名 10。能力清单增至
228 个支持函数、69 个自动符号；静态注册表识别并集为 280，剩余 110。完整证据见
[HORCALC 横向统计记录](../99-log/2026-08-10-native-formula-horcalc.md)。

第二十六批闭合 `INSORT/INSUM`。opcode 1246/1247 都先从活动技术公式目录按名
选择另一个指标和一基输出线，再对 command 8、type 7 返回的每个板块成员调用
嵌套求值器。32 位原 DLL 探针确认 `INSORT` 的 0/1 为降序/升序，成员值在首个
有效点后向前沿用，并以 `1e-5` 判定并列；`INSUM` 不沿用缺日，0..5 依次为
求和、平均、最大、最小、最大成员序号、最小成员序号，平均除以完整板块成员数，
成员序号保持原始目录的一基顺序。

纯 C++ 自动上下文从已恢复的公式库选择可执行、数值安全且只依赖 OHLCV 的技术
指标，加载本地 `.day` 并加 100 根预热。跨 8 个调用共享指标/输出/证券缓存，
真实 42 成员银行板块只执行 42 次 KDJ.J。2026-06-10 的平安银行降/升序排名为
`6/37`，总和/平均为 `3371.558349609375/80.27519989013672`，最大/最小成员序号
为 `18/20`。分钟、非日线、复权、动态字符串、外部依赖指标以及 L2/云特殊集合
明确拒绝。能力清单增至 230 个支持函数、69 个自动符号；静态注册表识别并集为
282，剩余 108。完整证据见
[INSORT/INSUM 指标横向聚合记录](../99-log/2026-08-10-native-formula-insort-insum.md)。

第二十七批闭合 `ZHBLOCK/ZHBLOCKNUM/SIMIBLOCK`。前两项沿 command-8 category 3
读取 `T0002/lc/lcidx.lii` 的 320 字节组合目录和同目录 `<键>.cis` 的 16 字节
证券成员，按目录升序返回全部命中名称、尾随空格及数量。目录最多 600 条，成员
最多 25,000 条；截断、损坏、空文件或不存在均按未命中处理。当前安装的 `lc`
目录为空，因此正式结果是单空格和 0。

`SIMIBLOCK` 的 category 0 在当前 TdxW 构建中没有查找、枚举或缓存生产分支，
精确结果同样是经 `| -> 空格` 转换后的单空格。实现没有用行业/概念相似度替代。
`zhb.zip` 已证明与此目录无关。能力清单增至 230 个支持函数、71 个自动符号；
静态注册表识别并集为 285，剩余 105。完整证据见
[组合与相似板块记录](../99-log/2026-08-10-native-formula-combination-blocks.md)。

第二十八批已闭合 `EXTERNVALUE/EXTERNSTR`。TdxW 的第一个公式回调在 selector 37
读取 `T0002/signals/extern_user.txt` 和 `extern_sys.txt`，每条记录为
`market|code|external_id|text|float_value`。第一个参数按 float32 转整数后取低字节：
0 选择 user，非 0 选择 system；第二个参数是 external id。普通市场精确匹配，
扩展市场 31/71 互相兼容，重复键按文件顺序首条命中。缺失值严格为数值 0 和文本
单空格。当前安装的两份文件均不存在，所以正式结果正是这两个哨兵。

纯 C++ 工具新增 `formulas extern-signals` 用于检查文件、记录和指定证券/id；公式
CLI/API 在检测到依赖时自动加载并把来源、记录数、匹配数与缺失语义写入上下文
元数据。能力清单现为 232 个支持函数、71 个自动符号；390 条静态注册名已识别
287、剩余 103。完整证据见
[EXTERNVALUE/EXTERNSTR 外部信号记录](../99-log/2026-08-10-native-formula-external-signals.md)。

第二十九批闭合 `EXTDATA_USER`。TCalc opcode 1298 通过 TdxW selector 38 读取
`T0002/extdata/extdata_<dataset>.idx/.dat`。idx 是 29 字节记录：uint16 市场、
23 字节 NUL 结尾代码和 int32 点数；dat 按 idx 顺序拼接 12 字节的 int32 日期、
int32 时间、float32 数值。宿主按精确市场/代码线性匹配，以前置记录点数之和定位
数据段，按 K 线首末日期闭区间过滤后最多返回 30,000 点。

公式两个参数均按 float32 转 int32，并使用末柱值。精确时间戳返回原值；mode 1
前向填充、mode 2 缺失置零、mode 3 反向填充，其他 mode 缺失为 DRAWNULL。
纯 C++ 新增 `formulas extdata-user` 诊断命令和自动公式上下文；长文件采用完整段
校验、流式日期过滤、过滤后限额，损坏文件安全拒绝。当前安装没有 `T0002/extdata`
目录。能力清单现为 233 个支持函数、71 个自动符号；静态注册表识别并集为 288，
剩余 102。完整证据见
[EXTDATA_USER 本地扩展序列记录](../99-log/2026-08-10-native-formula-extdata-user.md)。

第三十批闭合四个剩余通用绘图函数。`DRAWSL` 以 bar-price 斜率生成左右线段，
`SLOPE=10000` 时切换为像素高度的垂直线；`DRAWBMP` 在 bar/price 锚点显示
`T0002/signals/*.bmp`；`DRAWGBK` 在整窗格渐变与 BMP→PNG 背景图之间切换；
`DRAWRECTREL` 使用窗格千分比坐标，并保留零色不填充和 `NOFRAME`。四者的
TCalc opcode、参数处理及 TdxW 类型 `20/9/10/11` 和最终 renderer 均有 IDA
证据。纯 C++ API 新增 basename 限定的只读 `formulas/signal-image` 资源端点，
网页按 TradingView 当前缩放和平移实时重算覆盖层。当前能力清单为 237 个支持
函数、71 个自动符号；静态注册表识别并集为 292，剩余 98。完整证据见
[剩余绘图原语记录](../99-log/2026-08-10-native-formula-remaining-draw.md)。

原生细节也被保留：`WEEKDAY` 以周日为 0，`BARSTATUS` 首/中/末为 1/0/2，
`CONST` 将输入末值填满全序列，`RANGE(A,B,C)` 使用 TCalc 容差做严格
`B < A < C`；反三角函数的越界点沿用上一有效输出，`FRACPART` 使用原生
正负 0.0001 截断修正，`SIGN/SGN` 使用 1e-5 零阈值。固定契约在 120 根真实
日线上同时核对这些结果。

正文上限 64 KiB、源码上限 16 KiB、参数最多 64 个。请求正文和源码不写盘，响应明确返回
`formula_source_mode=inline-post`、`source_bytes` 和
`request_body_retained=false`。语法错误保留解释器字节位置，例如缺少表达式会
报告 `expected formula expression at byte 12`。

自动 `FINANCE/DYNAINFO` 上下文、严格财务时点和调用方显式 `context.series`
仍使用原解释器语义。未来函数只有 `allow_future=true` 时可做只读绘图，响应继续
标记 `scan_allowed=false/backtest_allowed=false`；扫描和回测始终拒绝未来函数。
历史财务回测继续强制显式严格财务时点，避免把当前报告常量带回历史。未携带
对应确认头的 POST 返回 HTTP 405；跨源预检没有放行这些确认头，因此浏览器中的
第三方页面不能直接触发。

```json
{
  "market": "sz",
  "code": "000001",
  "formula": "CUSTOM_MACD",
  "source": "FAST:EMA(CLOSE,12)-EMA(CLOSE,26);SIGNAL:EMA(FAST,9);HIST:(FAST-SIGNAL)*2,COLORSTICK;",
  "parameters": {},
  "period": "day",
  "pages": 1,
  "page_size": 120,
  "allow_future": false,
  "point_in_time_finance": false
}
```

条件扫描和专家回测的最小源码分别为：

```text
RESULT:CLOSE>MA(CLOSE,N);

MID:=MA(CLOSE,N);
ENTERLONG:CROSS(CLOSE,MID);
EXITLONG:CROSS(MID,CLOSE);
```

对应 CLI 可直接运行：

```powershell
tdx-tool formulas scan --source-file selection.tdx --formula CUSTOM_SELECT `
  --param N=5 --securities sz000001,sh600000 --lookback 10 --period day

tdx-tool formulas backtest --source-file expert.tdx --formula CUSTOM_EXPERT `
  --param N=10 --market sz --code 000001 --period day --pages 1
```

### 条件公式策略池与持续告警

`formulas watch` 复用 `formulas scan` 的公式、证券范围、周期、历史页数、参数、
缓存和严格财务时点选项。每轮完整扫描按 `security_id` 比较上一轮活跃成员，区分：

- `entered`：新进入策略池；
- `exited`：离开策略池；
- `updated`：仍在池内，但触发 K 线或信号载荷变化；
- `resume/heartbeat`：从相同配置状态恢复或成员不变；
- `degraded`：存在取数/执行错误，本轮不改写状态，不制造虚假退出。

状态文件只保存配置摘要和活跃成员，不保存公式源码；源码内容变化会改变配置
摘要并显式重建快照。`--block-output` 只有在用户给出路径时才写入，按通达信
`blocknew` 的七位市场前缀格式原子替换，不会默认写入真实通达信目录。

```powershell
tdx-tool formulas watch --source-file selection.tdx --formula CUSTOM_SELECT `
  --param N=5 --securities sz000001,sh600000 --lookback 10 --period day `
  --interval-seconds 60 --state-file output\formula-watch-state.json `
  --block-output output\formula-watch.blk --output output\formula-watch.jsonl
```

Svelte 公式工作台的条件扫描任务提供“会话监控”：采用请求完成后再计时的方式，
不会重叠扫描；扫描条件变化会重建快照；不完整响应同样保留上一轮成员。浏览器
关闭后该会话停止，需要后台运行、断点续跑或 `.blk` 输出时使用上述 CLI。

### 多公式同周期组合与组合回测

`formulas strategy` 接受一个 JSON 策略清单。规则可直接携带条件选股源码，也可用
`formula` 引用 TCalc 条件选股库；组合符支持 `all`、`any` 和 `at-least`：

```json
{
  "code": "TREND_CONFIRM",
  "operator": "all",
  "rules": [
    { "id": "trend", "source": "RESULT:CLOSE>MA(CLOSE,N);", "parameters": { "N": 5 } },
    { "id": "momentum", "source": "RESULT:CLOSE>REF(CLOSE,M);", "parameters": { "M": 3 } }
  ]
}
```

组合不是把各证券最后一次扫描结果相与。解释器先对同一份 K 线执行每条规则，
逐点核对数量及 `date|time`，再在同一证券的同一根 K 线上计算命中数；展示型输出
不参与真假判定。策略限制为 1—16 条、规则 ID 唯一、单条内联源码不超过 16 KiB，
并拒绝未来函数、语法不完整和数值信号不安全公式。

组合回测先取固定股票池的共同 `date|time` 交集。本根收盘只确认信号，目标等权
到下一根共同 K 线开盘才生效；持仓按开盘到开盘漂移，调仓计入佣金和滑点，末端
强制平仓。结果除权益、回撤、成本和换手外，还按证券返回毛贡献、分摊成本、净
贡献、持有区间和进出次数；逐股净贡献之和必须与最终权益变化对账。历史回测仅
接受已证明披露时点安全的 `FINANCE/FINVALUE` 外部依赖，并要求显式严格财务时点；
其他外部序列保持拒绝，不以当前值冒充历史值。

```powershell
tdx-tool formulas strategy --strategy output\probes\custom-strategy.json `
  --mode scan --securities sz000001,sh600000 --period day --lookback 10

tdx-tool formulas strategy --strategy output\probes\custom-strategy.json `
  --mode backtest --securities sz000001,sh600000 --period day --pages 2
```

对应 HTTP 接口为 `/api/v1/formulas/strategy/scan` 与
`/api/v1/formulas/strategy/backtest`，只接受带确认头的 JSON POST。响应仅保留
规则摘要、参数和源码 MD5，不回显策略源码或内部公式定义。Svelte 公式页提供同一
清单编辑、扫描、组合回测及逐股归因表。

### 财务历史时点

`gpcw` 专业财务包按报告期组织，包本身没有逐股公告日。统一工具因此保留两种
明确不同的执行语义：默认模式把当前最新报告绑定为常量；显式
`--point-in-time-finance` 模式则读取持续积累的实际披露日归档，把
`FINANCE(43/44)` 和 `FINVALUE(id)` 转成逐 K 线序列。

严格模式不会把报告期末日冒充公告日，也不会在最早已知披露日以前使用当前
值。由于公开资源没有公告时分，数值从实际披露日后的第一根 K 线生效。业绩
快报只记录为 `express_available_from`，不能解锁完整 `gpcw` 字段。归档需要
定期显式运行：

```powershell
tdx-tool market disclosures --root C:\new_tdx --view all --archive
```

单只证券还可从通达信静态 F10 的官方公告缓存回补最近一年正式财报日期：

```powershell
tdx-tool market disclosures --root C:\new_tdx --market sz --code 300503 `
  --backfill-announcements --archive
```

需要扩展严格时点归档时，可用纯 C++ 批处理读取自选股、板块或证券清单。下面先
预览解析结果，再执行可断点续传的限速任务：

```powershell
tdx-tool market disclosures --root C:\new_tdx `
  --backfill-announcements --backfill-watchlist --dry-run

tdx-tool market disclosures --root C:\new_tdx `
  --backfill-announcements --backfill-watchlist --archive
```

默认状态文件位于 `T0002\tdx-tool`；已完成证券会跳过，失败项可在下次命令中
继续。父级 `--backfill-block` 会递归纳入子板块，`--max-securities` 则在请求前
阻止意外的大范围任务。详见[批量回补记录](../99-log/2026-08-06-disclosure-announcement-batch-backfill.md)。

回补前后还可离线审计覆盖率。审计会排除 ETF/指数，将尚未到预约日的报告标记
为等待状态，只把预约过期、快报未转正式报告或归档完全缺失的 A 股放进顶层
`items` 队列：

```powershell
tdx-tool market disclosures --root C:\new_tdx `
  --audit-coverage --backfill-watchlist
```

该输出可直接作为 `--backfill-input`；`--audit-as-of` 支持按指定日期重放行动性。
详见[覆盖率审计验收](../99-log/2026-08-06-disclosure-coverage-audit.md)。

审计会自动读取 `T0002\hq_cache\base.dbf` 的 `SSDATE`，将上市前季度从要求覆盖的
组合中排除。日常维护可压缩为一个命令：

```powershell
tdx-tool market disclosures --root C:\new_tdx `
  --maintain --backfill-watchlist
```

它按“刷新日历归档→上市日感知审计→回补到期队列→再次审计”执行，并用默认
24 小时 TTL 避免对上游暂未发布的报告反复请求。详见
[增量维护验收](../99-log/2026-08-06-disclosure-listing-maintenance.md)。

摘要、审计报告和说明会公告不会解锁专业财务包；同一报告期保留公告证据并选择
最早完整报告日。这里保证的是披露时点，历史包数值仍可能包含后续追溯修订。

当前严格模式只接受专业包直接提供的 `FINANCE(43/44)` 和全部
`FINVALUE(0..584)`；公式若还要求其他 `FINANCE(id)`，执行会拒绝而不是混入
当前财务快照。详见[真实验收记录](../99-log/2026-08-06-point-in-time-finance.md)。

条件选股与专家回测也接受同一显式开关：

```powershell
tdx-tool formulas scan --root C:\new_tdx --formula A006 `
  --securities sz300503,sz000001 --point-in-time-finance

tdx-tool formulas backtest --root C:\new_tdx --library formulas.json `
  --formula PITEXPERT --market sz --code 300503 --point-in-time-finance
```

财务回测不带开关时会拒绝当前报告常量；带开关但使用不可历史重建的
`FINANCE(id)` 或混入未证明时点安全的外部快照依赖时同样拒绝。扫描命中和回测
结果都会返回财务归档事件数、报告加载数与严格模式元数据。详见
[扫描与回测验收](../99-log/2026-08-06-point-in-time-finance-scan-backtest.md)。

### 源码解释器语义真实性与绘图 IR

`formulas analyze` 的 `analysis_schema_version=7` 不再用单一“支持/不支持”掩盖
展示占位或外部数据依赖。报告会分别给出 `numeric_signal_safe`、
`presentation_semantics_faithful`、`render_semantics_materialized`、
`semantic_surrogate_scope`、`presentation_return_surrogates`、
`unsupported_presentation_directives/functions` 和
`degraded_numeric_output_causes`。当前系统库结果为 375/379 数值安全，另有 4 条
ZTPRICE/DTPRICE 因宿主 type-120 上下文未闭合而明确进入 fidelity gate；当前加载一条用户公式后为
376/380 数值安全。90/90 条绘图公式已物化 IR、380/380 条展示语义可信、0 个未知展示指令；56 条
`presentation-return-only` 只说明绘图调用在数值解释器中保留了兼容返回占位，
不代表缺少 renderer，也未传播到数值输出。扫描与回测只接受数值安全公式；一旦
字符串句柄或绘图返回占位经变量传播进入数值输出，会在执行前拒绝。

`formulas evaluate` 和 `/api/v1/formulas/evaluate` 还会返回
`tdx-formula-render-ir-v1`。它不复制 K 线，而是让普通指标线引用
`points.values.<输出名>`，让绘图函数只保存命中位置的 `index` 和当根已求值参数。
当前 IR 覆盖 `STICKLINE/DRAWTEXT/DRAWTEXT_FIX/DRAWICON/DRAWKLINE/`
`DRAWNUMBER/DRAWNUMBER_FIX/DRAWNUMBER_DIF/DRAWBAND/DRAWGBK/DRAWGBK_DIV/`
`DRAWBMP/DRAWRECTREL/DRAWSL/PARTLINE/DRAWLINE/PLOYLINE`，并逐语句、按原顺序保留
`COLOR*/RGBX*/LINETHICK*/NODRAW/DRAWABOVE/DRAWCFRAME` 以及
`VOLSTICK/COLORSTICK/STICK/LINESTICK/CIRCLEDOT/CROSSDOT/POINTDOT/DOTLINE`。
同一语句出现多个 series renderer 指令时按源码顺序处理，最后一个指令生效；颜色、
线宽和其他非 renderer 指令仍独立保留。
`STRCAT/CON2STR` 赋值链会物化为逐根字符串。样式会直接发布 `color_ref`、来源及
编码说明；16 个命名色取自 `TCalc.dll` 的真实表，`COLORrrggbb` 按 DLL 原样保存
为 COLORREF，`RGBXrrggbb` 则交换红蓝。函数 `RGB(r,g,b)` 同样使用 Windows
`COLORREF` 字节序 `r | g<<8 | b<<16`。

该结构是前端渲染协议，不声称像素级复刻通达信画布；结果中的
`pixel_renderer_equivalent=false` 和原有展示真实性标志继续明确这一边界。
绘图 IR 只随只读求值结果返回，扫描与回测仍只读取数值输出，不把图标、文字或
颜色作为交易信号。

CLI 与 GET/POST 求值结果另返回 `tdx-formula-render-environment-v1`。TdxW 的
`DRAWTEXT/DRAWNUMBER/DRAWTEXT_FIX/DRAWNUMBER_FIX` 都经 `sub_68F020` 选择字体表
index 1；`DRAWNUMBER_DIF` 在当前绘制窗口首根 bar 的样式参数精确等于 2 时，
整层切到 index 13。C++ 只读解析
`T0002/user.ini` 的显示键并发布 face、GDI logical height、weight、charset、
quality 和 `ElderStyle` 调整。当前安装解析为 Arial、cell height 15、weight 400、
`DEFAULT_CHARSET`、`ANTIALIASED_QUALITY`。五类原生路径均为透明背景；普通标注
使用 `TextOutA`，连续数字使用 `DrawTextA`，统一以
`GetTextExtentPoint32A` 测量。缺配置时回退到已恢复的字体默认表，不读取或返回
其他用户配置。

每个 IR 图元还携带完整源码 `statement_index` 和仅对图元连续编号的
`render_order`；顶层以 `primitive_order=source-statement-order`、
`primitive_order_contiguous=true` 和 `source_statement_count` 固定契约。网页先按
该顺序稳定排列 TradingView series；混合 series 与 HTML/SVG 的公式会把普通线、
柱图、`LINESTICK`、`PARTLINE`、`DRAWKLINE`、`DRAWBAND` 边界和连续数字提升到
同一组有序覆盖层，再与背景、柱体、线段、文字和图标按 `render_order` 叠放。
这保证语句级先画/后画关系。`DRAWBAND` 的不透明合成、标注字体选择与 GDI 文本
API 均已有独立原生证据；浏览器字体栅格化、其他图元的 Alpha 混合与 GDI 像素
取整仍不声明为等价。

Svelte 公式工作台现已消费这份 IR，而不再只显示图元计数：普通输出按
`COLOR*/LINETHICK*/DOTLINE/NODRAW` 呈现。`COLORSTICK` 不再近似为 72% 宽
实心直方图：IR 按 `TdxW.exe!sub_9555B0` 逐 bar 物化正/负颜色角色，网页画从
零轴到指标值的一像素竖线。`VOLSTICK` 按 `sub_957030/sub_956F40` 同时物化
“开收比较、平盘回退前收”和“只比较前收”两套角色，再由
`T0002/user.ini` 的 `Other/VolKUseZT` 选择；`Other/RealUPK=0` 的上涨柱为空心，
设为 1 才实心，下跌柱始终实心。柱宽按 `sub_987080` 的间距公式计算，不再使用
固定 0.72 比例。当前安装两个开关均为 0。`PARTLINE` 支持逐点 `COLORREF`，并按
`DIRECT=0/1` 区分当前到下一根与上一根到
当前的线段颜色；`DRAWKLINE` 映射为蜡烛价格图元；`STICKLINE` 不再借用蜡烛近似，
IR 逐事件物化 `WIDTH/4` 的 bar 间距比例以及
`solid/dashed-hollow/solid-hollow/center-full/center-half` 五类柱体。网页用随缩放、
平移和尺寸变化重算的 SVG 柱体绘制宽度、实心/空心/虚线边框；中轴模式忽略
`PRICE1`，`EMPTY=3` 使用半占宽度。小数等其他非零 `EMPTY` 保留普通实线空心语义。
`DRAWICON` 已改用 `TCalc.dll` 的 `RT_BITMAP/2060` 原生图集：纯 C++ 离线
PE 解析器把 `1800×18` DIB 切成 100 个 `18×18` 单元，并生成白色透明键 PNG。
IR 逐事件保存价格、类型、横向精灵偏移以及 `DRAWABOVE` 上/下对齐；Svelte 通过
TradingView 的 bar-price 坐标放置位图。官方函数类型仍只接受 1—51，资源剩余
单元只作为可审计容量公开，不猜测其调用语义。`DRAWTEXT/DRAWNUMBER` 已改为真正的 bar-price
文字覆盖层，逐事件携带价格、实际文字、按 `&` 拆分的多行、最多 250 个 UTF-8
码点以及上下对齐。`DRAWTEXT_FIX/DRAWNUMBER_FIX` 使用 pane-fraction 坐标，
精确保留 `X/Y` 和 `TYPE=0/1` 的左右对齐。`DRAWCFRAME` 只在 `DRAWTEXT` 的
`sub_961290` 中生效：固定文字虽收到参数但 renderer 不读取，数字 renderer 则不
接收该参数。有效 frame 忽略公式 PRICE 与 `DRAWABOVE`，按当前 bar 的 HIGH/LOW
及窗格剩余空间选择上/下，绘制 20px 点状 leader、4px 圆角、同色 Alpha `0x50`
填充和同色不透明 1px 边框；宽高比文字度量多 `5/4`px，文字 inset 为 `(3,3)`。
网页会随缩放重新计算 HIGH/LOW 屏幕坐标，不再把固定文字或数字套进主题 panel。
网页从 `render_environment` 取 face、逻辑高度和 weight；当前安装近似为
`Arial 15px/400`，不再使用统一 11px monospace、连续数字粗体或通用文字阴影。
这些 HTML 覆盖层会随 TradingView 的时间轴、价格轴、缩放、平移和尺寸变化重算
坐标，不再退化成方形 marker。`DRAWNUMBER_DIF` 会从触发 bar
展开连续数字/字母并锚定 K 线高低点。其原生条件为
`abs(value-1)<0.0001`，同一图元只维护一段活动序列，期间的新触发会被忽略；
START/NUM 在源 bar 锁定，STYLE 在每个绘制 bar 重新求值。STYLE 0 无偏移，
STYLE 1 有 10px 点状引线，STYLE 2 再加同色 Alpha `0x50` 的直角矩形和不透明
1px 闭合边框，其他非零 STYLE 只有 10px 偏移。数字盒为 8×14px/右对齐，
字母盒为 14×14px/居中，并按窗格边缘在 HIGH/LOW 之间翻转。IR 发布逐 bar
STYLE 序列，Svelte 在缩放或平移后按首个可见 bar 为整个图元选择 index 1/13；
`DRAWBAND` 在两条动态边界间按上下关系选择颜色并填充，交叉处拆分。原生
`TdxW.exe!sub_959960` 使用两个实心 GDI 画刷和 `StrokeAndFillPath`，没有
`AlphaBlend`，因此 IR 固定发布 `band_fill_opacity=1` 和
`band_fill_compositing=opaque-gdi-stroke-and-fill-path`，网页也采用不透明填充。
浏览器同时显示“TDX IR 预览”，不会把 TradingView
近似呈现冒充通达信原画布；`STICKLINE` 的宽度会跟随 TradingView 当前 bar 间距，
但取整、覆盖顺序与原生 GDI 像素仍不声称逐像素等价。

`PLOYLINE(COND,PRICE)` 现在只连接相邻的真实条件顶点，不再因稀疏点被普通折线
错误跨越；事件显式携带前后顶点的 bar/price。`DRAWLINE(COND1,PRICE1,COND2,PRICE2,EXPAND)`
会保留起止锚点、斜率和 `EXPAND=1` 的向右延长端点；两者由网页 SVG 覆盖层随
TradingView 缩放、平移、尺寸和价格刻度变化重算。`DRAWKLINE(HIGH,OPEN,LOW,CLOSE)`
发布固定 H/O/L/C 参数次序与阴阳线规则。`DRAWGBK_DIV` 的原生类型 21 由
`sub_95A6C0` 将 `abs(COND-1)<0.0001` 的连续区间交给 `sub_95A330`。IR 仍逐根
保留双 `COLORREF`，但会标出唯一区域 leader、起止索引和聚合价格；网页因而只画
一块连续区域。官方模式 `0/1/2/3` 分别为不透明纵向渐变、横向渐变、边框、
`COLOR2` 填充加 `COLOR1` 边框。私有模式 `10..20` 通过 GDI+ 只用 `COLOR1`
实心填充，Alpha 字节为 `255*(mode-10)/10`；系统模式 `17` 精确为 `178/255`。
范围 1 聚合整段高低极值，范围 2 使用首根 OPEN 到末根 CLOSE。网页已移除背景层
统一 `opacity:0.24`，未知模式不再猜成渐变。

普通 series 现另发布 TCalc 原生绘图类型 0/4/5/6/7/8/9。普通线与 `DOTLINE`
共用 `TdxW.exe!sub_957620`，但分别使用 `PS_SOLID/PS_DOT`；两者都按缺失值中断
连续 run，孤立 run 画 `x-3..x` 的短横线。`Other/BoldZBLine` 只在默认线宽 1
时把有效宽度切为 2，当前安装值为 0。`STICK/LINESTICK` 分别由
`sub_957D70/sub_957F70` 绘制零轴 stem，后者再连接连续有限值；IR 保留
`zero-baseline-stick`、`indicator-line` 和 `sticks-then-line` 兼容字段。网页使用
透明 series 保持 autoscale，再由 SVG 画细 stem，不再生成 72% 宽 histogram。
`CIRCLEDOT/CROSSDOT/POINTDOT` 分别按当前 bar spacing 和 LINETHICK 切换四方向
像素/空心圆、三档半径对角叉、单像素/实心椭圆；缩放和平移都会重算。普通
LineSeries 还显式写入 WhitespaceData，避免底层跨 `DRAWNULL` 连接。当前系统
`LINESTICK` 用例 `SLZT/青龙` 已由加强后的 type 5 契约固定。证据见
[series 样式记录](../99-log/2026-08-09-native-series-styles.md)。图集可由下列命令
导出，固定 API 同时提供
`/api/v1/formulas/icons`、`/api/v1/formulas/drawicon-strip.png` 和 BMP 端点：

```powershell
tdx-tool formulas icons --root C:\new_tdx `
  --output drawicon.png --bmp-output drawicon.bmp --manifest drawicon.json
```

当前 379 条公式全部数值安全，0 条存在污染输出。平安银行真实日线配合完整
上下文及未来只读模式时，当前品种/周期可适用的 354 条全部通过且都有最新数值；
期权隐波和金融期货分钟结算公式分别标为市场、周期不适用。精确统计、
`SIGNALS_QS` 私有能力标志证据和剩余 L2 边界见
[语义真实性审计](../99-log/2026-08-06-formula-semantic-fidelity-audit.md)。
真实 `TJCJL` 指标的绘图验收见
[公式绘图 IR 记录](../99-log/2026-08-06-formula-render-ir.md)。
展示返回占位与 renderer 物化状态的全库复核见
[公式展示真实性 schema 7 记录](../99-log/2026-08-09-formula-render-fidelity-schema7.md)。
价格文字、固定文字、数字标签、换行/对齐/边框及显式行情上下文的恢复证据见
[公式标注语义记录](../99-log/2026-08-09-native-formula-annotations.md)。
字体表、`user.ini` 配置、透明背景、测量与 Win32 文本 API 的证据见
[公式字体/GDI 记录](../99-log/2026-08-09-native-formula-font-gdi.md)。
`DRAWCFRAME` 的适用范围、HIGH/LOW 锚点、几何和 Alpha 证据见
[DRAWCFRAME 记录](../99-log/2026-08-09-native-formula-drawcframe.md)。
条件顶点折线、双条件连线、K 线参数及分区背景的证据见
[公式线段与背景语义记录](../99-log/2026-08-09-native-formula-lines-background.md)。
原生图标资源与柱线复合输出证据见
[DRAWICON/LINESTICK 记录](../99-log/2026-08-09-native-drawicon-linestick.md)。
混合图元顺序与公式工作台深链接证据见
[源码绘制顺序记录](../99-log/2026-08-09-native-formula-render-order-router.md)。
精确命名色表、`COLOR/RGBX` 字节序和 IDA 地址见
[颜色语义记录](../99-log/2026-08-09-native-formula-colorref.md)。

`PEAKBARS(K,N,M)` 与 `TROUGHBARS(K,N,M)` 已补齐为真实转折点距离语义：先复用
同一条 `ZIG(K,N)` 转折序列，从当前 K 线回看最近第 `M` 个峰/谷，再返回距该点
的 K 线根数。它们继续被标记为未来函数，只允许显式开启的只读求值，不能进入
扫描或回测。Svelte 公式工作台也会显示本次结果实际生成的绘图 IR 图元数和事件数，
不再把后端已有 IR 隐藏在响应中。

### 扩展市场全库审计

公式单次执行、全库审计和 HTTP 工作台现在共用同一套市场语义。市场参数接受
`sz/sh/bj`、`qz/qd/qs/cz/qg` 或 TDX 扩展市场 ID `3..255`。审计扩展市场时，
A 股独占的 `DYNAINFO/板块` 上下文仍会逐公式归入 `market_inapplicable`。
`FINANCE` 不再被整类排除：IDA 已确认 `TCalc!sub_10026B20` 的 `1/7` 分支分别读取
宿主 type 103 的第一、第二个 float；其余已闭合字段来自 type 105，且
`TdxW!sub_60E580 case 0x69` 对市场 `71/31/48` 填充港股财务结构。TdxW 对扩展市场
不会读取 A 股 29 字节历史股本文件；证券类别为 2 时，type 103 的两个槽均回退到
`security+152`。当前 7727 目录中市场 31 的 2582 条、市场 48 的 339 条记录全部为
类别 2，因此解释器从本地 `hkcwdata.dat` 自动绑定
`1/2/3/6/7/9/10/16/19/20/30/31/32/33/34/37/38/42/53` 共 19 个 selector。
其中港股 `1/7` 均为当前 H 股股本；这保留了原 DLL 的扩展市场回退语义，并不伪造
历史序列。兼容市场 71 在当前目录中无证券类别样本，因此只保留 17 个已证实的
type-105 selector，不开放 `1/7`。其他 selector 仍逐公式标记 `market_inapplicable` 并发布
`unsupported_expansion_bindings`。港股 selector 的宿主槽语义按原 DLL 保留，例如
`16` 对应港股少数股权，不套用 A 股字段名称。

```powershell
# 港股日 K 自带 HKSHORTVOL；SHORTVOL 应通过
tdx-tool formulas audit --root C:\new_tdx --market 31 --code 00700 `
  --period day --with-context --output hk-audit.json

# 期货日 K 自带 open_interest；CCL/持仓量应通过
tdx-tool formulas audit --root C:\new_tdx --market 47 --code IFL9 `
  --period day --with-context --output futures-audit.json

# IVOLAT 会按期权显示名解析标的、模型和精确到期日
tdx-tool formulas audit --root C:\new_tdx --market 7 --code HO8W03UX `
  --option-name HO2608-C-2500 --period day --with-context `
  --output option-audit.json
```

同一能力开放只读接口：

```text
/api/v1/formulas/audit?market=31&code=00700&period=day&with_context=1
/api/v1/formulas/evaluate?market=47&code=IFL9&formula=CCL&period=day
/api/v1/formulas/evaluate?market=7&code=HO8W03UX&formula=VOLATILITY&option_name=HO2608-C-2500&period=day
```

2026-08-08 真实样本中，港股 `SHORTVOL`、期货 `CCL` 和期权
`VOLATILITY` 均通过；三份全库报告都逐条返回 379 条记录且解释器错误为 0。

### 本地系统/用户信号

`SIGNALS_SYS(id,mode)` 与 `SIGNALS_USER(id,mode)` 已由 TCalc 固定注册记录、TdxW
selector 34/36 和文件提供者三层证据闭合。前者读取共享系统文本文件并按市场、
数值代码、日期筛选；后者读取按 signal ID 和证券分目录的 8 字节日期/float32
记录。两者都只按 K 线交易日匹配，`mode=1` 向前填充、`mode=2` 补零，其他值
保持 `DRAWNULL`。

公式解释器会对常量二参数调用自动构造本地上下文；也可以用
`formulas local-signals` 独立查看目录和序列。当前安装没有对应文件时返回明确空
状态，不用零值伪装数据。selector 35 的 `SIGNALS_QS` 不属于这两套本地文件，
仍保持券商私有边界。文件布局、命令和验证见
[本地信号与 DLL 审计](../99-log/2026-08-12-native-local-signals-and-dll-audit.md)。

### 显式宿主与授权 L2 序列

`SIGNALS_QS(id,mode)` 是券商宿主提供的私有信号序列；`L2_AMO(i,j)`、
`LARGEINTRDVOL/LARGEOUTTRDVOL/LARGETRDINNUM/LARGETRDOUTNUM/`
`TRADEINNUM/TRADEOUTNUM/TRADENUM` 则依赖调用方合法取得的 L2 逐笔上下文，
都不能从普通 OHLCV 推导。解释器会把带参数调用规范化为精确键，例如
`SIGNALS_QS#102#0` 和 `L2_AMO#0#1`，并在分析结果中输出
`explicit_context_bindable=true` 与完整 `explicit_context_bindings_required`。
当前 379 条系统公式中有 20 条属于此类：14 条需要授权 L2，6 条需要券商私有
序列，共形成 36 个精确绑定键；动态 ID 仍拒绝执行。

调用方取得合法真实序列后，可使用同一格式注入求值或全库审计：

```json
{
  "schema": "tdx-explicit-formula-context-v1",
  "series": {
    "SIGNALS_QS#102#0": {
      "2023-04-18|15:00": 1,
      "2026-08-05|15:00": -1
    }
  }
}
```

```powershell
tdx-tool formulas evaluate --library formulas.json --formula 主力密码 `
  --input kline.json --context-file signals-qs-context.json

tdx-tool formulas audit --library formulas.json --input kline.json `
  --context-file signals-qs-context.json

tdx-tool formulas context-template --library formulas.json `
  --stamp "2026-08-05|15:00" --output explicit-context-template.json

# 也可以直接读取真实 K 线，把该批次每根 bar 的精确时间全部写成空占位
tdx-tool formulas context-template --library formulas.json --formula ZJLX `
  --market sz --code 000001 --period day --pages 1 --page-size 20 `
  --output zjlx-kline-context-template.json
```

序列按 K 线的 `日期|时间` 对齐；缺少任一声明键时状态为
`explicit_context_unavailable`。`context-template` 会为选中公式生成全部精确键和
空占位，调用方只能填入自己合法持有的 TCalc 兼容数值；工具不会下载、推导、
伪造或绕过权限取得这些数据。显式文件不会自动启用无关的
`FINANCE/DYNAINFO` 市场上下文，也不构成 L2 授权或数据获取能力。实现与验收记录见
[私募、解禁、公式显式上下文与主题机会](../99-log/2026-08-07-native-private-fund-unlock-formula-thematic.md)。

K 线模式同时适用于只读 API
`/api/v1/formulas/context-template?formula=ZJLX&market=sz&code=000001&period=day&pages=1&page_size=20`。
返回模板的 `_template.stamp_source` 为 `kline`，并给出 `stamp_count`、
`bar_count`、市场、代码和周期。它只借用公开 K 线确定索引边界；八条 `L2_AMO`
值仍全部为 `null`，不会因为能取得 K 线而推导受限序列。

### 行业估值 HYSYL/HYSJL

`HYSYL`、`HYSJL` 是零参数标量函数，分别把当前指数或个股所属行业的市盈率、
市净率（MRQ）广播到整条序列。纯 C++ 上下文从公开
`func_gx_hyzt101_1.jsn` 的 `hyPE/hyPB` 自动绑定；解析目录按资源路径、大小和
修改时间缓存，因此通过统一下载命令原子刷新后不必重启服务。

原生宿主可选择 880 普通行业或 881 研究行业，公开 HYZT 当前只有 880 叶子行业
估值。个股缺少 881 估值时会回退到同票的 880 行业，并在 `industry_valuation`
元数据中同时返回配置代码、有效代码和回退标志；直接 881 行业指数或宽基指数不
使用替代值。完整处理链和数据边界见
[HYSYL/HYSJL 行业估值记录](../99-log/2026-08-10-native-formula-industry-valuation.md)。

### 大盘涨跌家数与动态快捷函数

`INDEXADV/INDEXDEC` 可作为裸自动符号或零参数函数使用。上下文按照当前证券选择
上证指数、深证成指、创业板指、科创 50、北证 50、恒生指数或期货品种主连，
再从公开指数 K 线记录偏移 31/33 读取上涨、下跌家数并按日期时间对齐。它们与
已有 `ADVANCE/DECLINE` 共享数据，但补齐了 TCalc opcode 1201/1202 的精确
零参数入口和创业板、科创板等代码级基准选择。

`DYNA_NOW/DYNA_ZAF/DYNA_LB/DYNA_ZAS` 分别是 `DYNAINFO(7/14/17/24)` 的
零参数别名，按 opcode 1380—1383 将现价、涨幅比例、量比和涨速比例广播到整条
序列。前三项使用公开 `0x054C` 快照及日线，涨速使用公开 `0x053E`；现价缺失
时回退昨收，涨幅和涨速都保持公式内部比例单位。完整证据、市场映射和验证见
[大盘涨跌家数与 DYNA 快捷函数记录](../99-log/2026-08-10-native-formula-market-breadth-dyna.md)。

### 所属大盘、标的证券与 DIVFACTOR

`DPZSCODE/DPZSNAME` 既可作为裸符号，也可使用零参数调用。实现逐字复现
TCalc opcode 1323/1357 的市场和代码分支：深市普通/创业板、沪市普通/科创板、
北交所和港股分别返回原生主要指数代码及名称；名称来自 DLL 中恢复的六组 GBK
常量，不从网页标签反推。

`UNDERCODE/UNDERLYC` 对股票期权和可转债等存在标的关系的品种有效。前者返回
标的六位代码；后者按目标 K 线的周期和日期时间读取标的 `CLOSE`，值保持
float32，并保留原 DLL 的空类别返回空字符串/零序列边界。期权关系来自通达信
期权目录，可转债关系来自本地 `T0002/hq_cache/speckzzdata.txt`，运行时仍为纯
C++。

`DIVFACTOR(TYPE)` 的原生语义比价格复权算法更窄：只读取公开 `0x000F`/type-164
除权记录偏移 21 的“每 10 股送转数”，单次因子为 `(送转+10)/10`。`TYPE=1`
将事件日前的每根柱除以因子，`TYPE=2` 从事件日开始乘因子，`TYPE=0` 根据
`TQFLAG` 自适应；现金分红和配股字段不参与，所有累乘除保留 float32 顺序。
固定向量、真实样本和证据见
[证券关系与 DIVFACTOR 记录](../99-log/2026-08-10-native-formula-security-relation-divfactor.md)。

港股扩展市场 `31/48` 使用本地加密 `hkqxinfo.dat/hkqxinfo2.dat` 的相邻累计
股份乘数，不请求 A 股 `0x000F`。其语义仍与 TCalc 一致：`TYPE=1` 只除事件日
之前的柱，`TYPE=2` 从事件日开始相乘，现金偏移不进入因子。真实 `31:02650`
在 `2026-02-20` 五股拆细，前一交易日得到 `0.2/1`，事件日及以后得到 `1/5`；
内联 HTTP 执行由 `tdx-source-interpreter-v1` 返回 194 个点并识别一个事件日。
完整证据见
[本地港股公司行动记录](../99-log/2026-08-12-native-local-hk-actions.md)。

### 绝对阈值 ZIGA

`ZIGA(X,N)` 与百分比阈值的 `ZIG` 不同，`N` 是绝对价格差。实现逐项复现
`TCalc.dll!sub_10021DD0`：先在首段确认方向，只有局部极值可以更新候选点，
反转越过阈值后才确认并用 float32 线性插值；未完成尾段保留原生候选点与末柱
回画规则。若 `X` 的末尾最多十点近似为常量，则其末值按整数 0/1/2/3 选择
`OPEN/HIGH/LOW/CLOSE`，否则使用显式序列；非正或过小阈值整段返回 0。

该函数会随未来柱重画历史输出，因此能力清单把它单独列入
`custom_formula_future_path_functions`。只有显式 `allow_future` 的只读计算可执行，
条件扫描和回测不接受。12 组固定向量、2,000 组原 DLL 差分和真实平安银行
120 根日线样本见
[ZIGA 记录](../99-log/2026-08-10-native-formula-ziga.md)。

### 随机序列与本地安全/亮点分

`RAND(N)` 已直接调用 32 位 `TCalc.dll+0xCA40` 对照。处理函数在每次函数调用时
以 `time64(0)` 播种 MSVC CRT：状态更新为 `state*214013+2531011`，返回
`(state>>16)&0x7fff`。每个有效柱输出
`rand()%trunc(float32(N)+0.503000020980835)+1`；`N` 缺失或不在
`1..1000000` 时返回缺失且不推进状态。正常运行默认使用当前秒，调用方可在
context 中提供 `formula_random_seed` 做确定性审计和重放。

`SAFESCORE/SHINESCORE` 的宿主链已闭合到 `TdxW.exe!sub_4F5560`。它懒加载
`T0002/hq_cache/specgpext.txt`，按 `market|code` 建立 18 字节记录，第 4/5 个
管道字段分别存为安全分和亮点分 float。type-167 偏移 51/67 读取两项值；亮点分
仍先使用安全分字段作有效性门控。平安银行当前本地记录为 `92/7`，显式随机种子
1 的前四项为 `2,8,5,1`。完整证据见
[RAND 与本地评分记录](../99-log/2026-08-10-native-formula-rand-security-scores.md)。

### TPool 流程状态

TPool 规则求值复用同一源码解释器。除无环只读投影外，`pool watch` 现按 DLL
证据推进一秒 tick、8 类首次启动和 3 类循环，并按 XML 顺序传播 cell 成员；
带环 flow 在后续 tick 中继续推进。状态可显式保存到独立文件：

```powershell
tdx-tool pool watch --input example.xml --root C:\new_tdx `
  --interval-seconds 60 --state-file output\example-state.json
```

状态文件包含来源摘要、cell 成员、flow 运行次数与首次/末次 tick，写入采用原子
替换且不会修改股票池 XML。`psatt` 的 `baimpool` 已恢复为按池、节点和日期分区的
入池 XML 日志，按 `market+code` 去重；它同时是声音、提示和板块保存的运行时前置
条件，`bclearblock` 只是保存前清空选项。证券新进入目标 cell 时，单次投影和状态机
都会返回对应的
`planned_actions`。`bdel/ndelnum/ndeltype` 则是独立的存量历史过期清理策略：
`ndeltype=0/1/2/3` 分别按天/小时/分钟/秒解释，只有 `bdel!=0`、窗口为正且单位有效
时才生效，因此不会附着到新进入的证券。`bsavehis` 也不属于 cell-entry：它仅在
池状态序列化时按 `baimpool && bsavehis` 写每日 `.dat` 全量快照。所有计划均标记为 `planned-only`、
`executed=false`，工具不会创建 TPool 提示窗口、调用宿主 UI/声音接口，也不会写
板块、历史文件或池 XML。详见
[流程状态机记录](../99-log/2026-08-06-tpool-flow-state-machine.md)和
[`psatt` 动作计划记录](../99-log/2026-08-12-native-tpool-action-plans.md)。

单次规则和流程投影也可用固定 API 执行。已有安装目录池继续使用安全路径 GET；
调用方自有 XML 使用确认 POST，不创建临时文件：

```http
POST /api/v1/pools/evaluate
Content-Type: application/json
X-TDX-Action: pool-evaluate

{"source_name":"my-pool.xml","xml":"<root>...</root>","limit":200}
```

POST 只接受最多 512 KiB 的内联 XML 和安全显示文件名，响应固定
`request_body_retained=false`、`tdx_state_mutated=false`，不回显 XML 或接受服务端
路径。Svelte TPool 实验室提供粘贴和本地文件导入，并调用同一纯 C++ 求值函数。

### 历史 Python 研究工具

离线导出 222 条技术指标：

```powershell
python doc/90-scripts/extract_tcalc_formulas.py `
  --dll ida/TCalc.dll `
  --kind technical `
  --format csv `
  --output doc/02-engine/tcalc-system-indicators.csv
```

导出四类内置公式：

```powershell
python doc/90-scripts/extract_tcalc_formulas.py `
  --dll ida/TCalc.dll `
  --kind all `
  --format json `
  --output C:\tmp\tdx-builtin-formulas.json
```

对已初始化的 TdxW 导出当前完整列表：

```powershell
python doc/90-scripts/dump_tcalc_runtime.py `
  --process TdxW.exe `
  --kind all `
  --format json `
  --output C:\tmp\tdx-runtime-formulas.json
```

运行时脚本先核验 `TdxW.exe/TCalc.dll` 哈希、对象槽和虚表，再调用
`GetIndexNum/GetIndexInfo/GetTreeInfo`。早期 PID 16216 的公式接口尚未初始化，
因此当时没有调用 getter；现在用户公式已改由上述 `formulas user-library` 静态
只读解析，不再要求先打开 K 线/公式管理器，也不依赖运行中对象。

## 产物

- [系统技术指标 CSV](tcalc-system-indicators.csv)：222 条，仅名称、分类和
  属性，不含公式源码；
- `extract_tcalc_formulas.py`：离线、无第三方依赖；
- `dump_tcalc_runtime.py`：Frida 只读运行时枚举，结果默认建议写到仓库外。
