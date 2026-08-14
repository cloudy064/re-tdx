# TCalc 文本符号与 DYNAINFO(88) 闭合

本轮继续处理非 L2 公式依赖，目标是系统技术指标 `上榜标注` 和 `SKSD`
（时空隧道）。实现仍为纯 C++；Python 只用于离线 IDA 数据库探针，不进入
运行时。

## 文本函数与宿主显示状态

TCalc 注册表和求值器确认：

| 符号/函数 | TCalc 求值器 | 原生行为 |
| --- | --- | --- |
| `HQCRBK` | `sub_1000FC40` | 请求宿主 type 122，读取返回结构字节 `+57`；0/1 选择深/浅背景绘图颜色 |
| `DYBLOCK` | `sub_10045500` | 请求宿主 type 120，读取 `+155` 地域字符串 |
| `GNBLOCK` | `sub_10044890` → `sub_10044720` | 调用宿主 command 8、类别 1，取概念板块并把 `|` 替换为空格 |
| `CON2STR` | `sub_10046350` | 按 0—4 位小数格式化数值并返回 TCalc 字符串池句柄 |
| `STRCAT` | `sub_100458A0` | 取两个字符串池句柄，拼接后生成新句柄 |

TdxW type-120 case 从证券结构读取 `province_id`，再查固定 32 项、每项 10 字节
的 GBK 地域表并写入 `+155`。离线导出确认了完整映射；例如 `province_id=18`
是“深圳板块”，`27` 是“云南板块”。实现直接固化这张已恢复的表，不按公司
名称或注册地址猜测。

公式数值运行时不渲染 `DRAWTEXT_FIX/STICKLINE`，因此字符串池句柄本身没有
数值意义。解释器对 `CON2STR/STRCAT` 校验真实的二参数签名后返回无害数值
占位；`HYBLOCK/DYBLOCK/GNBLOCK` 的真实 UTF-8 文本保存在
`context_metadata.formula_text_symbols`，来源保存在
`formula_text_symbol_sources`。`GNBLOCK` 复用本地 `infoharbor_block.dat` 概念
归属。`HQCRBK` 仅在已恢复公式中选择绘图颜色，headless 运行采用深色背景值 0。

只有 `HYBLOCK` 文本而不使用 `HY_INDEX*` 时，现在不再额外下载行业指数 K 线，
减少了“上榜标注”的一次网络请求。

## DYNAINFO(88) 的实际返回

帮助文字把 88 描述为“实时封单额”，但当前 DLL/EXE 组合的真实机器码更精确：

1. `TCalc!sub_10028670` 的 case 88 请求宿主 type 163（`0xA3`），读取返回结构
   `+384`。
2. `TdxW!sub_60FFF0` 的 case 163 先由 `sub_684240` 计算涨跌幅，再调用
   `sub_9B37A0(...,1,1,...)` 判断盘口是否处于封死涨停方向。
3. 只有方向成立时才把涨停幅度写入 `+384`；否则保持 0。跌停方向写入
   `+388`，并不被 case 88 读取。

因此这个版本的 `DYNAINFO(88)` 对普通 A 股实际是“封死涨停时返回涨停幅度，
否则返回 0”，不是买一金额。系统“上榜标注”只判断 `DYNAINFO(88)>0`，与这一
实际语义完全一致。

纯 C++ 上下文复用 `0x0547` 五档盘口及 `0x0452` 特殊涨跌停价表：现价和买一
等于涨停价、买一有量且卖一价量均为 0 时，按宿主单精度口径返回
`(涨停价-昨收)*100/昨收`。同时把买一金额作为诊断元数据保留，但不冒充
`DYNAINFO(88)` 返回值。

## 真实验证

- `SZ002428 云南锗业`：昨收 75.19、现价/买一 82.71、卖一价量为 0，
  `DYNAINFO(88)=10.0013303757`；买一金额约 9.85 亿元只出现在诊断元数据。
- 同一证券文本为“`小金属 云南板块 三代半导 光伏 光通信 …`”，三类来源均
  已物化。
- `SZ000001 平安银行`：文本为“`银行 深圳板块 跨境支付`”；盘口未封板，
  `DYNAINFO(88)=0`。
- 系统 `SKSD` 与“上榜标注”均通过真实 800 根日线执行。
- 全库覆盖从 216 条直接执行、317 条上下文执行提升到 **217/319**；技术指标
  从 117/171 提升到 **118/173**。
- 平安银行全库上下文审计 **319/319** 无错误，318 条具有数值末值；唯一日线
  全空项仍是只适用于分钟周期的期货参考结算价。
- MinGW CTest **24/24** 通过。

覆盖报告中剩余 42 条正文也已重新分类：18 条明确含
`BACKSET/REFX/XMA/ZIG` 等未来函数，继续隔离于扫描和回测；其余 24 条全部是
L2 大单/逐笔、券商 `SIGNALS_QS`，或港股沽空、期货持仓、期权隐波等专用品种
依赖。也就是说，当前没有再发现一条遗漏的普通 A 股、公开非 L2、非未来函数
公式。后续公式方向应转向“未来函数只读绘图模式”或特定衍生品数据，而不是
继续把不可得字段填零。

## 产物

- TCalc 文本探针：`output/ida_probe_tcalc_text_symbols.py`；
- TCalc 文本反编译：`output/ida-probe-tcalc-text-symbols.log`；
- TdxW 地域表探针：`output/ida_dump_tdxw_region_table.py`；
- 地域表日志：`output/ida-dump-tdxw-region-table.log`；
- DYNAINFO(88) 探针：`output/ida_probe_tdxw_dynainfo88.py`；
- 宿主反编译：`output/ida-probe-tdxw-dynainfo88.log`；
- 封板实测：`output/formula-upper-list-label-002428.json`；
- 普通盘口实测：`output/formula-upper-list-label-000001.json`；
- 最新覆盖与审计：`output/tcalc-formula-coverage-professional.json`、
  `output/tcalc-formula-context-runtime-audit-professional.json`。

## 边界

当前解释器保留文本符号和来源，但不复刻 TCalc 进程内字符串池，也不输出
绘图指令的像素结果。`HQCRBK=0` 是 headless 的确定性绘图分支；未来若前端要
逐指令还原通达信配色，应在绘图层传入主题状态，而不是改变数值公式结果。
