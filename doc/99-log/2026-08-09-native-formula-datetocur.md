# TCalc DATETOCUR 完整柱日期计数闭环

## 结论

本轮从 TCalc 390 条静态注册表与解释器能力差集中闭合 `DATETOCUR`。它无需
登录、Level2、宿主回调或额外数据文件，但不是普通日期差函数：参数只取末柱，
输出按完整输入缓冲区中的 K 线记录数计算，分钟周期会读取同日后续柱。因此实现
保持纯 C++，同时明确归入只读未来函数，不能用于扫描或回测。

能力清单由 211 个支持函数、57 个自动符号增至 212/57；
`custom_formula_calendar_filter_functions` 由 7 项增至 8 项。

## 注册与静态处理链

静态注册表记录为：

| 名称 | opcode | 处理函数 |
| --- | ---: | --- |
| `DATETOCUR` | 1394 | `sub_1000F6A0` / RVA `0xF6A0` |

`sub_1000F6A0(this, output, input)` 的行为为：

1. 只读取 `input[count-1]`，按 CRT 浮点到整数规则截断，再加 `19000000`；
2. 对每个输出柱 `i`，扫描 `this+0xEA6C` 指向的全部 35 字节 K 线记录；
3. 将每条记录的年月日组合为 `YYYYMMDD`；
4. 统计 `target < record_date && record_date <= current_bar_date` 的记录数。

第二个上界只有日期，没有时间。因而同一日期的第一根分钟柱也会把该日尚未到达
的后续分钟柱计入。这是原处理函数的确定行为，不是解释器推测。

## 32 位原 DLL 固定向量

新增 `output/native_probe_tcalc_datetocur.cpp` 和对应 32 位 EXE，直接调用当前
`TCalc.dll + 0xF6A0`。结果保存在
`output/native-tcalc-datetocur-probe.json`：

```text
日线 2023-12-29, 2024-01-02, 2024-01-03, 2024-01-05
目标 1240102                    -> 0,0,1,2
参数前柱不同、末柱 1240102      -> 0,0,1,2
目标 0                          -> 1,2,3,4
目标 1240105                    -> 0,0,0,0
目标 1240102.9                  -> 0,0,1,2
末柱为缺失哨兵                  -> 1,2,3,4

分钟线：1 月 2 日两柱、1 月 3 日三柱、1 月 5 日一柱
目标 1240102                    -> 0,0,3,3,3,4
目标 1240103                    -> 0,0,0,0,0,1
```

这些向量同时锁定严格下界、末柱参数、截断、缺失参数、按柱而非按唯一交易日计数，
以及同日未来柱可见性。

## 纯 C++ 实现与隔离

解释器使用 DATE 频数和有序前缀计数复现结果，把原 DLL 的 O(n²) 双循环降为
O(n log n)，不改变任意当前日期的计数。分析器返回：

```text
syntax_supported=true
executable=false
has_future_function=true
read_only_future_executable=true
has_external_dependency=false
pure_ohlcv=false
future_functions=["DATETOCUR"]
```

只有显式 `allow_future=true` 的单次只读绘图可以执行；公式扫描、组合策略扫描及
回测不会因为函数已实现而放宽未来数据门禁。

## 验证与发布

- 原 DLL 固定向量：7 组通过；
- `tdx-formula-engine-tests` 与 `tdx-recon-contract-tests`：通过；
- CTest：101/101；
- 临时服务最终专项：5/5；
- 临时服务 full API：211/211；
- 正式服务专项：5/5。

报告为：

- `output/native-datetocur-targeted-api.json`；
- `output/native-datetocur-full-api.json`；
- `output/native-datetocur-formal-api.json`；
- `output/native-datetocur-real-api.json`。

正式 EXE SHA-256 为
`7A813474A1A691B64B5AE65D5939A448C12F10514637438FD9922B6312876620`；正式服务
PID 31560，仅监听 `127.0.0.1:8765`，`native_cpp=true`、
`python_runtime=false`。

## 边界

`DATETOCUR` 的结果依赖请求实际装入的 K 线窗口，而不是证券的全历史总柱数。
增加更早或更晚的数据都可能改变结果；分钟周期还存在同日未来可见性。调用方若
需要无未来数据的“截至当前分钟柱计数”，应使用普通条件累计表达式，不能把
`DATETOCUR` 当作该语义的别名。
