# TBigData 计算列与债券函数

## 定位

当前安装的 `TBigData.dll` 版本为 `1.0.2.40`，SHA-256 为
`0D4DA553E81BF9B901886A06EB9B0F33D60390A13BED211D243A72DD98913969`。
它不是另一套 TCalc 时序公式引擎，而是 `T0002/cloud_cfg/*.cfg` 表格配置的
计算列运行器：每个 `<item>` 可声明 `calc`、`calcref`、`calctype` 和
`calcflag`，运行时从同一行的原始列及已经算出的列取值。

## C++ 模块边界

解释器实现按稳定职责拆分，不再集中于一个源文件：

- `cloud_calc_config.cpp`：容错 CFG/XML 解析和配置文件发现；
- `cloud_calc_interpreter.cpp`：算术表达式、`ABS`、列依赖图、环检测与按 unit 求值；
- `cloud_calc_builtins.cpp`：36 项强类型注册表、日期算法及债券 AI/PV/YTM 求值；
- `cloud_calc_host.cpp`：公开 L1/财务、本地行业、封单与组合证券宿主字段；
- `cloud_calc.cpp`：审计报告、最小模板、共享请求计划、单行/批量编排和 CLI。

四个 `*_internal.hpp` 只在原生库内部共享 `Config/Unit/Column`、注册表和组合入口，
不会扩大 `tdx/cloud_calc.hpp` 的公共 API。固定内建函数通过注册表查找，解释器不再
包含日期/债券 dispatcher；宿主解析器也不依赖 CLI 或 HTTP 请求模型。

## 宿主字段链

`TdxW.exe:0x41CF50` 动态加载 `TBigData.dll`，`0x41D140` 调用
`BigData_RegisterCallBack(sub_61B630, sub_62B8D0, sub_630950)`。DLL 导出
`0x10024E30` 把三者保存到 `0x10139724/28/2C`，并立即以类型 `135` 调用第一个
回调探测宿主行情状态。因此 CFG 的 `syscol/refzqdm` 是明确的 TdxW 宿主注入
契约，不是 JSN 服务端原始列。

当前 660 个 CFG 有 248 个显式 `syscol` 声明；列级 `refzqdm=N` 和
`<unit refunit="N">` 继承合并后，248 个均有引用证券。严格 XML 解析曾漏掉
5 个不规范 CFG；当前容错解析器可读取全部 660 个，同时忽略 XML 注释里的
停用 `<item>`。纯 C++ 解析器按同行 `$SCN/$ZQDMN` 定位关联证券，并保留每次
补值的证券、系统列和来源。

DLL 的 `0x101016B0` 是 44 项、每项 19 字节的系统列总表，ID 6..49 已完整导出
到 `output/ida-tbigdata-system-columns.json`。其中可由公开 L1、公开财务、本地
行业层级或同行债券日程严格恢复的映射为：

| CFG 系统列 | 原生来源 |
|---|---|
| `$CLOSE/$OPEN/$MAX/$MIN` | 昨收、今开、最高、最低 |
| `$NOW/$NOW3` | 非零现价；原生分支不回退昨收 |
| `$NOW2` | 非零现价，缺失时回退昨收 |
| `$NOWV/$QRSD/$ZEF` | 现量、现价减昨收、`(最高-最低)/昨收*100` |
| `$ZAF` | 涨跌幅 |
| `$ZQJC` | 本地证券目录名称 |
| `$ZCJJE` | 成交额 |
| `$CJL` | 成交手数 |
| `$INP/$OUTP` | 内盘、外盘 |
| `$BP1/$SP1/$BPV1/$SPV1` | 公开 `0x0547` 买卖一价/量 |
| `$J_LTGB/$J_ZGB` | 公开 `0x0010` 流通/总股本，单位为股 |
| `$J_LTSZ` | 可用现价（缺失回退昨收）乘流通股本，单位为元 |
| `$J_ZSZ` | 无 B 股歧义时，A 股价乘 `(总股本-H 股)`，单位为元 |
| `$HSL` | `成交手数*10000/流通股本`，单位为百分比 |
| `$TDXHY/$TDXHYCODE` | 本地通达信行业层级；研究行业仅作缺省回退 |
| `$BONDAI` | 同行面值、当前票息和付息区间，按 `B*i*t/365` 派生 |
| `DQLL2` | 同行 `SYFXLLXL` 的当前票息 |

## 板块成员聚合

`TBigData.dll:0x100BCAC0` 是 ID 44..49 的集中处理体。它读取同行
`$S_ZQDM` 成员向量，等待 TdxW 的 4080 异步行情批次完成，再按下列规则写回：

| 系统列 | 原生规则 |
|---|---|
| `$S_AVGZF`（ID 44） | 有效成员涨幅之和 / 原始成员总数；缺行情成员贡献 0，不缩小分母 |
| `$S_JQZF`（ID 45） | `sum(总股本 * 涨幅) / sum(总股本)` |
| `$S_LTG`（ID 46） | 涨幅最大成员的证券简称；相同涨幅保留成员串中先出现者 |
| `$S_MAXZF`（ID 47） | 上述龙头成员的涨幅 |
| `$S_NUM`（ID 48） | `$S_ZQDM` 解析成员数；重复成员保留计数 |
| `$S_UPRATE`（ID 49） | `现价 > 昨收` 的成员数 / 原始成员总数 × 100 |

加权口径不是按字段名推测的流通股本。TdxW `sub_60E580` 的宿主类型 105
填充体把目标 `+49` 映射为流通股本、`+61` 映射为总股本；聚合处理体明确读取
成员记录 `+61`。纯 C++ 因此只在 `$S_JQZF` 出现时批量请求公开 `0x0010`
总股本，其他五项只需 `$S_ZQDM` 和公开 L1。原始成员向量保留重复项参与分母和
求和，网络请求则按证券去重。

真实 `func_5G101` 第 9 行三成员板块已验证：3 条 L1、3 条财务记录全部映射，
得到平均涨幅 -3.54%、总股本加权涨幅 -4.53%、龙头“汇金通”、最大涨幅
0.20%、上涨比例 33.33%，宿主未解析数为 0。结果保存在
`output/tdx-tbigdata-cloud-calc-5g-row8-group.json`。

`$ZS` 在静态表中的名称是“涨速”，ID 19，不是昨收。TdxW 的默认行情批次
调用 `sub_697E40`，其消息类型为十进制 1342，即公开 `0x053E`；请求体与
`0x054C` 相同。响应由 `sub_85EB00` 解码：普通快照字段之后依次是五档盘口、
状态字、扩展游标、三个变长整数、`int16 / 100` 的服务端涨速和尾部字。
纯 C++ `market speed` 与 `/api/v1/market/speed` 已严格复现该布局，并在
`cloud-calc --quotes` 遇到 `$ZS` 时自动选择它。真实 `func_kzz_kzzsy201`
双证券行已一次请求返回转债 `$ZS=0.25`、正股 `ZGZS=0.17`，13 个宿主字段
全部绑定、未解析数为 0；结果见
`output/tdx-tbigdata-cloud-calc-kzz-row1-speed.json`。

ETF IOPV `$JJJZ` 已精确闭合：TBigData 的主机类型 102 最终读取
`Destination+57`，TdxW 对基金类证券把公开行情核心第七个价格差分还原为辅助价，
再按 `sub_598CC0` 的沪深 ETF 代码判定缩放为 IOPV。纯 C++ 解码器现在保留该
差分并输出 `fund_iopv`，不需要新增请求或私有权限。当前 2570 次有效系统列
声明（248 次显式、2322 次静态表内隐式列）已全部具有自动来源，覆盖 100%。

封单两列已经严格接入。TBigData 的 `$FCAMO/$FCB` 通过 selector 4
请求 TdxW 主机类型 163，分别读取 `Destination+384/+388`；`sub_9B37A0` 在确认
封死涨停后计算 `买一价 × 买一手数 × 交易单位` 和 `买一手数 / 总手数`，封死
跌停则对卖一侧使用同样口径并返回负值。纯 C++ 现已移植 `sub_5AA080` 的主板、
ST 生效日期、创业板、科创板、北交所、N 股和特殊证券规则，优先采用公开
`0x0452` 的逐证券当日边界；交易单位来自 360 字节 TNF 的 `+78`。类型 163 的
状态拒绝、连续竞价、集合竞价净委托和第二档承接分支也已覆盖。真实 SH603228
与公开封板榜逐字段交叉验证得到 `$FCAMO=651186090`、
`$FCB=0.2008569411943874`，误差为零。

`$PE` 虽不属于 TBigData 的 44 项静态系统列，但 TdxZdView 描述表证明它是
“动态市盈率”（`$PES` 才是静态市盈率，`$PETTM` 为 TTM）。TCalc 的
`DYNAINFO(39)` 调用 TdxW 类型 105，并以现价/昨收回退价除以 `Destination+185`；
后者为 `净利润 × 12 / 报告期月份 / 总股本`。统一工具现从公开 `0x0010` 和 L1
按该式绑定，SH603468 实盘得到 `33.61823777538364`。公式解释器也共享同一语义，
不再将原始 EPS 当作分母。

全配置还发现两个真正的外部公式引用：
`DQLL2` 出现 18 次，已按票息日程补齐。`price2` 的两次外部引用只存在于
`func_qxfa201` 的隐藏计算列：这里是尚未到解锁日的定增记录，解锁日前收盘价
客观上尚未定义；事件到期后迁移到 `func_qxfa202`，其 CFG 与 JSN 都会显式下发
`price2`。审计器因此标记为 `upstream-lifecycle-conditional`，不再建议手工输入，
也绝不以当前价伪造未来端点。

`sub_100C4140` 解析上述四个属性；计算描述结构为 11 字节：

| 偏移 | 字段 |
|---:|---|
| `+0` | `calc` 字符串指针 |
| `+4` | `calctype` 字节 |
| `+5` | `calcflag` 字节 |
| `+6` | `calcref` 数量字节 |
| `+7` | 引用数组指针 |

`sub_1008A810` 初始化计算列：固定函数从
`0x100FF470..0x100FF620` 的 36 项 `{name,id,argc}` 表查找；普通表达式则把
引用列替换为表达式变量，进入 `sub_100EB2E0/sub_100EBD70`。`sub_1008AA80`
负责逐行求值。引用值缺失时，`calcflag=1` 按零参与计算；其他值保持不可用，
而不是静默伪造零。

## 当前配置的完整语法面

对当前 `cloud_cfg` 的原生解析结果为：

- 660 个 CFG，335 个包含计算列；
- 1370 个计算列，其中 987 个普通表达式、383 个固定函数调用；
- 普通表达式只使用 `+ - * /`、括号、一元正负号和大小写不敏感的 `ABS`；
- 1370/1370 通过语法、`calcref` 声明和函数参数个数检查；
- 计算列依赖环为 0。

36 个注册函数中，当前配置只引用 7 个：

| 函数 | ID | 当前调用数 | 原生执行 |
|---|---:|---:|---|
| `$B_GetDateDiff_d$` | 100 | 114 | 是 |
| `$B_CalcRemainTime$` | 110 | 57 | 是 |
| `$B_CalcAITime_All$` | 112 | 48 | 是 |
| `$B_CalcAI_All$` | 118 | 41 | 是 |
| `$B_CalcPV_All$` | 122 | 9 | 是 |
| `$B_CalcYTM_All$` | 129 | 57 | 是 |
| `$SF_CurrDate$` | 1000 | 57 | 是 |

当前安装配置先达到 1370/1370 可执行。随后又从每个处理函数本体恢复剩余
29 项，而不是按名称猜公式：包括年/日拆分、日期前后移、星期、付息次数、
非 `All` 应计利息、PV 和 YTM 族。36/36 项现均有原生 C++ 分派，并以一组
固定日期和现金流向量覆盖；普通/固定/到期单次付息/复利/零息的 PV→YTM
回算都回到输入收益率。命令的 `builtin_catalog` 会分别报告
`used_by_audited_configs` 与 `audited_config_calls`，因此不会把“已按处理体实现”
混同为“当前配置已经实际调用”。当前真实 CFG 仍只调用表中的 7 项。

## 纯 C++ 命令

全目录审计：

```powershell
tdx-tool formulas cloud-calc --root C:\new_tdx `
  --output output\tdx-tbigdata-cloud-calc-audit.json
```

从一个 CFG 的依赖图生成最小调用方输入骨架：

```powershell
tdx-tool formulas cloud-calc --root C:\new_tdx `
  --cfg func_kzz_kzzsy101 --template `
  --output output\tdx-tbigdata-cloud-calc-template-kzz.json
```

模板不会把宿主可自动绑定字段、同行可派生字段或待计算字段混入
`row_template`。真实 `func_kzz_kzzsy101` 得到 19 个调用方字段、3 个宿主字段、
1 个派生字段和 15 个计算字段；所有输入键默认值均为 `null`，便于调用方填值或
让网页保留已有值后只补缺失键。

对一个真实 JSN 行复算：

```powershell
tdx-tool formulas cloud-calc --root C:\new_tdx `
  --cfg func_kzz_kzzsy101 `
  --jsn output\tdx-jsn\list\gxjty_zq_kzzsy101_1.jsn `
  --row-index 0 --as-of 20260808 --quotes `
  --output output\tdx-tbigdata-cloud-calc-row0-host.json
```

直接批量复算 JSN 前 128 行，并共享去重后的行情计划：

```powershell
tdx-tool formulas cloud-calc --root C:\new_tdx `
  --cfg func_kzz_kzzsy101 `
  --jsn output\tdx-jsn\list\gxjty_zq_kzzsy101_1.jsn `
  --all-rows --max-rows 128 --as-of 20260808 --quotes `
  --output output\tdx-tbigdata-cloud-calc-batch.json
```

`--all-rows` 与 `--row-index` 互斥；对象、对象数组和 `colheader/data` JSN 均可
作为来源。报告保留源文件总行数和是否被 `--max-rows` 截断，网络仍只在显式
`--quotes` 时启用。

`--quotes` 仅请求 CFG 实际需要的主证券/关联证券；普通字段走公开 `0x054C`，
遇到买卖一价量自动切换为包含完整快照的 `0x0547`，遇到 `$ZS` 切换为同时含
服务端涨速与五档的 `0x053E`；股本/市值/换手率同时请求
公开 `0x0010` 财务包，本地行业不联网。`--snapshot FILE` 和
`--finance-snapshot FILE` 可分别提供离线 L1/深度与财务文档，完成可重复复算。
这些来源均先产生 `host_context.bindings/unresolved`，再执行计算列。`--row` 也
接受平面 JSON 对象或对象数组；`--set CODE=VALUE` 仍是最后优先级显式覆盖。
工具按每个 unit 单独建立依赖图、拓扑排序并输出
`evaluated/unavailable/error`，不会跨 unit 混用同名列。`--as-of` 同时固定
`$SF_CurrDate$` 和应计利息日期。

真实 `gxjty_zq_kzzsy101_1.jsn` 首行已在 **0 个 `--set`** 下完成 15/15 个
计算列，无不可用或错误；本次实际自动绑定 9 个宿主字段，关联正股行业也从
本地层级得到“空运”。`func_zq_ssfxr201_1` 还验证了 unit 级 `refunit="1"`
继承和 `0x0547`：TCL 科技成功得到买一/卖一 4.86/4.87 元及对应一档量，证明
详情 CFG 不会再把内部发行记录 ID 误当证券代码。

## HTTP 与网页工作台

服务现在以同一路径开放两种固定能力：

- `GET /api/v1/formulas/cloud-calc` 审计全部配置；带
  `?cfg=func_kzz_kzzsy101` 时只审计该安全文件名；
- `GET /api/v1/formulas/cloud-calc/template?cfg=func_kzz_kzzsy101` 从所选 CFG
  生成最小输入模板，同时返回 `input_fields/host_fields/derived_fields/
  calculated_fields` 的来源与分类；
- `POST /api/v1/formulas/cloud-calc` 配合
  `X-TDX-Action: formula-cloud-calc`，接收 `cfg`、扁平 `row`、`as_of`、
  `quotes`、可选离线快照和标量 `overrides`，返回按 unit 分组的计算结果与
  `host_context`；
- `POST /api/v1/formulas/cloud-calc/batch` 配合
  `X-TDX-Action: formula-cloud-calc-batch`，接收 1—128 个 `rows`。服务先对
  主证券、关联证券、财务、行业和封单需求做全批预检，再按最强实际需求选择
  `0x054C/0x0547/0x053E`，跨行去重后只建立一份共享行情/财务文档计划。

HTTP 不接受 CFG/JSN 文件路径；CFG 名只允许 1—128 个字母、数字、点、下划线
和连字符，单行最多 4096 个字段，批量总计最多 65,536 个字段，覆盖最多 256
个标量。响应不包含原始 `row/rows`，批量结果只按零基 `row_index` 关联，服务
也不会写盘。`quotes=false` 可做完全离线复算；`quotes=true` 只拉取
当前配置实际声明的公开宿主字段，不能与调用方提供的离线快照混用。

Svelte 公式库页面的“榜单计算列解释器”提供配置审计、单对象/对象数组编辑、
评价日、公开行情开关、宿主绑定/未解析列表和计算结果表。用户可显式保存最多
20 个模板到当前浏览器 `localStorage`，或下载本次结果 JSON；“补齐最小模板”
会请求 CFG 模板并合并到当前单行或每个批量对象，已有值优先，模板不会自动写盘。
默认南航转债样本在
2026-08-08 的真实服务上完成 15/15，公开 L1 与本地行业共解析 9 个绑定，
未解析和执行错误均为 0。

两行真实样本（相同转债/正股、不同面值）完成 30/30 个计算列；全批只发现 2
个唯一行情证券并执行 1 次共享 `0x054C` 文档获取，每行各 9 个宿主绑定、0 个
未解析。批量层保留逐行错误隔离，某行预检/执行失败时用该行的 `status=error`
报告，不泄漏原始输入，也不打乱其余行的索引。

## 边界

- 命令只读 CFG/JSON，不加载 `TBigData.dll`，不修改通达信目录；默认离线，
  只有显式 `--quotes` 才请求公开 L1/财务；
- JSN 不保存 `$NOW3` 等宿主即时列；工具只补有静态回调和字段语义证据的值，
  其余字段保留为 `unresolved`；
- PV/YTM 的 `All` 分派、现金流数组长度和特殊 type 4/5 分支按当前二进制恢复；
  当前未调用的 29 项有处理体级向量验证，但尚无原版 UI 真实行差分，尤其闰日
  的两种年/日拆分和特殊产品业务命名仍需样本对照；
- 36 项完整注册表见命令输出 `builtin_catalog`，静态证据见
  `output/ida-tbigdata-builtins.json`、`output/ida-tbigdata-calc-parser-decompile.json`、
  `output/ida-tbigdata-used-builtin-helpers.json`、
  `output/ida-tbigdata-system-columns.json`、
  `output/ida-tdxw-tbigdata-host-init.json`、
  `output/ida-tdxw-tbigdata-host-callbacks.json` 和
  `output/ida-tbigdata-register-callback.json`。
