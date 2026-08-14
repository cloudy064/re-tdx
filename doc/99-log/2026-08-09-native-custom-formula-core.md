# TCalc 静态函数注册表与自定义公式核心

## 目标

内置 379 条公式已达到 379/379 数值安全和 90/90 绘图 IR 物化，但这只能证明
当前系统公式库完整，不能证明用户粘贴的任意通达信公式都能执行。本轮从
`TCalc.dll` 的函数注册入口出发，建立可审计的解释器能力边界，并优先补齐无需
L2、券商私有序列或其他授权数据的高复用函数。

## 静态注册表证据

目标 DLL：

- profile：`tdx-2025-11-14`；
- SHA-256：`13facaa52dac552c5be1f63331781219de9bf798c443af8f5e4afc193a02e7f5`；
- 初始化器：`TCalc.dll!sub_100C6870`；
- 初始化范围：`0x100C6870..0x100E6B59`。

离线 IDAPython 探针仿真该初始化器的寄存器、栈和静态内存写入。最初版本报告
386 条 0x47 字节注册记录；后续复核发现它把 IDA 的 `word_*` 源操作数误按
一字节读取，导致若干名称末字节缺失，并把 `TR/MA/IF/LN` 四个短名称过滤掉。
修正操作数宽度和寄存器相对寻址后，权威结果为 390 条记录、390 个唯一名称，
记录尺寸仍为 0x47。记录中的处理函数位于内联名称后 `+0x1F`，并同时恢复
opcode。主要证据文件为：

- `output/ida_probe_tcalc_function_registry.py`；
- `output/ida-tcalc-function-registry-jsonl-v4.log`；
- `output/ida_probe_tcalc_custom_formula_core.py`；
- `output/ida-tcalc-custom-formula-core.log`；
- `output/ida_probe_tcalc_registry_windows.py` 与窗口日志用于复核记录步长。

探针只在 IDA 离线分析阶段使用 Python；产品命令、服务和公式执行仍全部为 C++，
没有 Python 嵌入或运行时转发。

## 首批处理函数

| 名称 | opcode | 处理函数 | 恢复语义 |
| --- | ---: | --- | --- |
| `YEAR` | 1067 | `sub_10006380` | 每根 35 字节 bar 的 u16 年 |
| `MONTH` | 1068 | `sub_100063C0` | bar 偏移 2 的月 |
| `DAY` | 1071 | `sub_10006400` | bar 偏移 3 的日 |
| `WEEKDAY` | 1070 | `sub_10006640` | 周日 0 至周六 6 |
| `BARSTATUS` | 1120 | `sub_10006820` | 首/中/末柱 1/0/2 |
| `TOTALBARSCOUNT` | 1122 | `sub_10006960` | 全序列总柱数 |
| `CONST` | 1022/1385 | `sub_1000F3B0`/`sub_1000F360` | 末值填充全序列 |
| `RANGE` | 1021 | `sub_10016900` | 带原生容差的严格 `B < A < C` |
| `ACOS/ASIN` | 1161/1162 | `sub_1001C7C0`/`sub_1001C9C0` | 越界沿用上一有效值 |
| `ATAN/COS/SIN/TAN` | 1163—1166 | `sub_1001CBF0` 等 | CRT 数学函数；TAN 奇点沿用前值 |
| `FRACPART` | 1177 | `sub_1001E040` | `x-trunc(x±0.0001)` |
| `SIGN/SGN` | 1179/1256 | `sub_1000C8F0` | ±1e-5 外为 ±1，内部为 0 |

`RANGE` 的容差为 `abs(A)*1e-7 + 1e-5`。`ACOS/ASIN/TAN` 的无效点不被
猜测性替换为 0；首点无前值时保持缺失。

## C++ 接入

`formula_engine.cpp` 新增 11 个核心函数和 6 个自动符号，并把 bar 日期、
`BARSTATUS`、`TOTALBARSCOUNT` 注入统一求值环境。`CONST` 和 `RANGE` 作为序列
函数执行，三角/符号函数按处理函数的容差和缺失传播规则执行。

公式覆盖文档新增 `tdx-formula-interpreter-capabilities-v1`：

- `supported_function_count=145`；
- `automatic_symbol_count=48`；
- `custom_formula_core_function_count=11`；
- `custom_formula_core_symbol_count=6`；
- `tcalc_registry_evidence.static_registry_entry_count=390`；
- `tcalc_registry_evidence.static_registry_unique_name_count=390`。

Svelte 公式库只展示后端发布的解释器函数数和注册证据数，不在浏览器中实现第二
套公式计算器。

## 验证

- 公式引擎单测：日期/星期、首中末柱、总柱数、CONST、RANGE、FRACPART、
  SIGN/SGN、三角/反三角组合全部通过；
- API 契约求值器单测：新能力清单和 120 点自定义公式固定响应通过；
- CTest：101/101；
- Svelte：0 错误、0 警告，生产构建成功；
- 临时服务专项：2/2；
- 临时服务 full：203/203，204 次网络请求；
- 正式服务专项：健康、首页、coverage、自定义核心 4/4。

正式发布信息：

- EXE SHA-256：`B66EF9E20C548F1F824806280A5ADC7204AD583A2C52D05F4AAF2BFF34FEC4CD`；
- JS：`assets/index-BzTNWR-2.js`；
- CSS：`assets/index-BBbdHCcI.css`；
- PID：26056；
- 监听：仅 `127.0.0.1:8765`；
- 健康：`native_cpp=true`、`python_runtime=false`、379 条公式。

## 边界与后续

390 是静态注册记录数，不是当前支持函数数。注册表中仍包含外部行情、交易状态、
账户、券商私有和未来型函数；在没有处理函数证据、合法数据来源和真实调用样本时
不会仅凭名称实现。下一批高收益、无授权候选是 `BARSLASTS/BARSSINCEN`、
`FILTERX/TMA/XMA`、`FINDHIGH/FINDLOW` 以及 `COVAR/RELATE/BETA`，需先逐个核对
处理函数和系统/用户公式中的真实参数形态。
