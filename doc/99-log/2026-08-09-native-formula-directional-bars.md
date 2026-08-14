# TCalc DHIGH/DOPEN/DLOW/DCLOSE/DVOL 方向段闭环

## 结论

本轮从 TCalc 390 条静态注册表与解释器能力差集中闭合
`DHIGH/DOPEN/DLOW/DCLOSE/DVOL`。名称中的 `D` 不是“日线”：五项会按相邻
收盘价方向把连续 K 线合并成转折段，再把完整段的 OHLCV 回填到段内每根柱。
因此它们只依赖当前 OHLCV，无需登录、Level2 或宿主回调，但会读取段内后续柱，
必须归入显式只读未来函数，不能进入扫描或回测。

能力清单由 212 个支持函数、57 个自动符号增至 217/62；新增独立的
`custom_formula_directional_bar_*` 五项能力组。裸符号和零参数调用两种写法均可用。

## 注册与处理链

| 名称 | opcode | 包装处理函数 | 共享选择器 |
| --- | ---: | --- | ---: |
| `DHIGH` | 1040 | `sub_10037A40` | 0 |
| `DOPEN` | 1041 | `sub_10037A60` | 1 |
| `DLOW` | 1042 | `sub_10037A80` | 2 |
| `DCLOSE` | 1043 | `sub_10037AA0` | 3 |
| `DVOL` | 1044 | `sub_10037AC0` | 4 |

五个包装函数都调用 `sub_10037640`（RVA `0x37640`）。在普通 K 线缓冲区路径中，
共享函数复制完整 35 字节记录，然后比较相邻原始收盘价：

```text
tolerance = abs(current_close) * 1e-7 + 1e-5
previous_close <= current_close - tolerance  -> 上升
previous_close >= current_close + tolerance  -> 下降
否则                                         -> 近似持平
```

同向和持平柱继续合并；只有从上升直接转下降，或从下降直接转上升时才开始新段。
合并时保留段首开盘价，取段内最高/最低价、段末收盘价并累计成交量。持平不会改变
当前方向。随后原函数用段末时间戳把同一组结果回填到从段首到段末的全部原始柱。

## 32 位原 DLL 固定向量

新增：

- `output/native_probe_tcalc_directional_bars.cpp`；
- `output/native_probe_tcalc_directional_bars.exe`；
- `output/native-tcalc-directional-bars-probe.json`。

探针直接调用当前 `TCalc.dll + 0x37A40..0x37AC0`。关键结果为：

```text
连续上涨 4 柱：
DHIGH=17,17,17,17   DOPEN=10,10,10,10
DLOW = 9, 9, 9, 9  DCLOSE=16,16,16,16  DVOL=100,100,100,100

收盘 10,12,11,13,14：
DHIGH =13,13,13,15,15   DOPEN =10,10,12,11,11
DLOW  = 9, 9,10,10,10   DCLOSE=12,12,11,14,14
DVOL  =30,30,30,90,90

收盘 10,10,12,12,11：
前四柱合为同一段，最后一柱在下降反转后另起一段。
```

`10.000005` 相对 `10.0` 落入原生容差并继续合并，进一步锁定了近似持平边界。
这些结果证明首柱会看到同段末柱数据，不能按普通无未来 OHLC 派生字段处理。

## 纯 C++ 实现与安全边界

解释器以一次线性分段和一次回填完成，时间复杂度 O(n)。输入和每次成交量累加均
经过 32 位 float 落地，与原 DLL 的 35 字节记录和 float 数组保持一致。真实行情
中 `DVOL` 与页面保留 double 的原始 `VOL` 在百万手量级可能相差不足 0.1 手，
这是 float 舍入而非丢量；API 契约按 1 手以内的显示容差验证。

分析器对裸符号和空调用都返回：

```text
executable=false
has_future_function=true
read_only_future_executable=true
has_external_dependency=false
future_functions=[DCLOSE,DHIGH,DLOW,DOPEN,DVOL]
market_dependencies=[CLOSE,HIGH,LOW,OPEN,VOL]
```

只有 `allow_future=true` 的单次只读公式执行可运行；所有扫描与回测入口继续拒绝。

## 验证与发布

- 原 DLL：连续上涨、连续下跌、反转、持平后反转、近似持平 5 组固定向量通过；
- `tdx-formula-engine-tests`、`tdx-recon-contract-tests`：通过；
- CTest：101/101；
- 临时服务专项：3/3；
- 临时服务 full API：212/212；
- 正式服务专项：3/3。

报告为：

- `output/native-directional-bars-targeted-api.json`；
- `output/native-directional-bars-full-api.json`；
- `output/native-directional-bars-formal-api.json`。

正式 EXE 为 42,407,529 字节，SHA-256
`4C581E03A0A01DF59181C0FE5FC35F1D5EF4798E6DBC3DEBDBC4D4383A0863E7`；正式服务
PID 30652，仅监听 `127.0.0.1:8765`，`native_cpp=true`、
`python_runtime=false`。临时 8875 已关闭。

## 边界

结果依赖请求实际装入的 K 线窗口。若窗口从某个既有方向段中间开始，首段的开、
高、低、量自然只覆盖窗口内部分；追加新柱也可能扩展末段或确认反转，从而改写
此前同段的全部 `D*` 值。这正是原生语义，也是它们不能用于历史信号验证的原因。
