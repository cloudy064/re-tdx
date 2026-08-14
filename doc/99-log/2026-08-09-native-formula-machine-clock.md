# TCalc MACHINEDATE / MACHINETIME / MACHINEWEEK 机器时钟闭环

## 目标与结论

本轮对 TCalc 390 条静态注册表和当前 208 个函数、54 个自动符号重新做差集，
排除已接线外部行情符号、L2/券商私有字段、交易系统状态和用户 DLL 后，选择三个
无需宿主回调、登录或行情数据的机器时钟入口：

- `MACHINEDATE`：通达信日期编码 `(YEAR-1900)*10000+MONTH*100+DAY`；
- `MACHINETIME`：本地时间 `HHMMSS`；
- `MACHINEWEEK`：周日为 0、周六为 6 的本地周序。

三项捕获一次 CRT 本地时间后把结果广播到全部 K 线。纯 C++ 运行时不加载
`TCalc.dll`，也不启动 Python。

## 注册与处理函数

当前 DLL 指纹下的静态记录为：

| 名称 | opcode | 处理函数 |
| --- | ---: | --- |
| `MACHINEDATE` | 1330 | `sub_10034FD0` / `0x10034FD0` |
| `MACHINETIME` | 1331 | `sub_100350A0` / `0x100350A0` |
| `MACHINEWEEK` | 1332 | `sub_10035160` / `0x10035160` |

注册证据来自 `output/ida-tcalc-function-registry-jsonl-v4.log`；处理函数保存在
`output/ida-tcalc-next-unlicensed.log`。日期和时间处理函数先调用 `_time64(0)`，
随后用 `_localtime64_s` 拆字段。周序函数把年月日交给 `sub_10001D70`；该帮助
函数从 1980 年累计闰年和月日，最终返回 `(累计天数+DAY+1)%7`，与既有 K 线
`WEEKDAY` 同为周日 0。

## 原 DLL 确定性时钟向量

新增 `output/native_probe_tcalc_machine_clock.cpp` 和 32 位
`output/native_probe_tcalc_machine_clock.exe`。探针遍历 PE 导入表，将 TCalc 的
`_time64` IAT 槽临时替换为固定时间函数；因此测试不是依赖运行瞬间的松散截图。
结果保存在 `output/native-tcalc-machine-clock-probe.json`：

```text
2024-02-29 23:59:58
MACHINEDATE = [1240229,1240229,1240229,1240229]
MACHINETIME = [235958,235958,235958,235958]
MACHINEWEEK = [4,4,4,4]

2025-01-05 00:00:01
MACHINEDATE = [1250105,1250105,1250105,1250105]
MACHINETIME = [1,1,1,1]
MACHINEWEEK = [0,0,0,0]
```

闰日用例锁定日期和 `HHMMSS`；周日午夜用例锁定无前导零的数值表示、日期编码和
周序起点。三项四柱完全相同，确认全柱广播。

## 纯 C++ 接入与语义审计

解释器同时支持原生裸符号和零参数调用，例如：

```text
D:MACHINEDATE;
T:MACHINETIME();
W:MACHINEWEEK;
```

能力清单新增：

```json
{
  "custom_formula_machine_clock_function_count": 3,
  "custom_formula_machine_clock_functions": [
    "MACHINEDATE",
    "MACHINETIME",
    "MACHINEWEEK"
  ]
}
```

支持函数由 208 增至 211，自动符号由 54 增至 57，静态注册证据保持 390。
三项可以直接执行，不需要 `external_dependencies` 或自动市场上下文；但结果不是
由 K 线推导，因此分析器显式返回：

```text
executable=true
has_machine_clock_dependency=true
pure_ohlcv=false
has_external_dependency=false
```

既有 `formula-custom-core-inline-post` 契约追加三项，动态核对当前日期、合法
`HHMMSS`、周序、首末柱广播以及跨午夜两分钟容差，没有增加 API 契约总数。

## 验证与发布

- `tdx-formula-engine-tests`：裸符号/零参数、全柱广播、当前本地时钟包络和分析
  分类通过；
- `tdx-recon-contract-tests`：能力清单与动态 API 契约通过；
- CTest：101/101；
- 临时服务专项：3/3；
- 临时服务 full API：211/211；
- 正式服务专项：3/3。

报告为 `output/native-machine-clock-targeted-api.json`、
`output/native-machine-clock-full-api.json` 和
`output/native-machine-clock-formal-api.json`；正式真实响应摘要保存在
`output/native-machine-clock-real-api.json`。

正式 EXE SHA-256 为
`CFA414F5B19D194F43185F431C52329C079EC158B5EE429333B0CF3E510935FA`；服务 PID
25760，仅监听 `127.0.0.1:8765`，健康检查为 `native_cpp=true`、
`python_runtime=false`。网页资源没有变化。

## 边界

三项描述执行时机器的本地时钟，不是 K 线时间，也不是 TdxW 当前交易日。公式
扫描或历史回测若引用它们，会像原 TCalc 一样把同一个“执行时快照”广播到全部
历史柱；响应通过 `has_machine_clock_dependency` 明确暴露这一非历史可重放依赖。
时区和系统时钟由运行机器负责，解释器不擅自改成 UTC 或交易所时区。
