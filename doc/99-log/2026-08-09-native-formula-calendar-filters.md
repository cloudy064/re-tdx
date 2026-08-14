# TCalc 日历、时间、右对齐与交易信号过滤状态机

日期：2026-08-09

## 目标与边界

在 379 条内置公式已无普通解释器缺口、只剩 6 条依赖私有外部
`SIGNALS_QS` 的前提下，本轮选择自定义公式中无需授权数据且复用价值较高的
日历、时间、序列对齐和交易信号过滤函数。运行时仍为纯 C++；Python 只用于
离线驱动 IDA 导出伪代码，不进入发行包或服务进程。

通达信官方函数列表给出了这些函数的公开名称和用途：
<https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html>。实现没有只按
文档描述猜测边界，而是继续用当前 `TCalc.dll` 的处理函数和原 DLL 输出向量
锁定行为。

## 静态恢复

离线脚本 [ida_probe_tcalc_calendar_filters.py](../../output/ida_probe_tcalc_calendar_filters.py)
导出的完整日志为
[ida-tcalc-calendar-filters.log](../../output/ida-tcalc-calendar-filters.log)。定位结果：

| 入口 | TCalc 处理函数 |
| --- | --- |
| `WEEKOFYEAR` | `sub_10006570` |
| `TIME2` | `sub_100066D0` |
| `DATETODAY` | `sub_1000F760` |
| `DAYTODATE` | `sub_10034DE0` |
| `TIMETOSEC` | `sub_1000FA60` |
| `SECTOTIME` | `sub_1000FB50` |
| `ALIGNRIGHT` | `sub_10006750` |
| `TFILT` | `sub_1000F520` |
| `TFILTER` | `sub_1001A950` |
| `TTFILTER` | `sub_1001A6C0` |

脚本同时导出入口的直接调用者/被调函数，避免只看到优化后的薄包装而误判状态机。

## 32 位原 DLL 固定向量

探针源码为
[native_probe_tcalc_calendar_filters.cpp](../../output/native_probe_tcalc_calendar_filters.cpp)，
原始 JSON 为
[native-tcalc-calendar-filters-probe.json](../../output/native-tcalc-calendar-filters-probe.json)。
探针由 32 位 MSVC 编译，直接执行当前原 DLL；核心结果如下：

```text
DATETODAY(901218,901219,901220,991231,1000101,1341231,1341232)
        -> (null,0,1,3299,3300,16083,null)
DAYTODATE(-1,0,1,12,13,365,366,3653)
        -> (null,901219,901220,901231,910101,911219,911220,1001219)
TIMETOSEC(-1,0,1,59,100,93000,235959,236000)
        -> (0,0,1,59,60,34200,86399,null)
SECTOTIME(-1,0,59,60,3599,34200,86399,86400)
        -> (0,0,59,100,5959,93000,235959,null)
ALIGNRIGHT(null,1,null,2,3,null)
        -> (null,null,null,1,2,3)
```

`WEEKOFYEAR` 探针跨年和周边界结果为 `1,1,2,1,1,2,9,53`；`TIME2` 对
`00:00:01/09:30:05/23:59:59` 返回 `1/93005/235959`。

信号状态机使用同一组买卖序列锁定全部模式：

```text
TFILTER mode 0 -> 1,0,2,0,1,2,1,0,2,0
TFILTER mode 1 -> 1,0,0,0,1,0,1,0,0,0
TFILTER mode 2 -> 0,0,1,0,1,1,0,0,1,0

TTFILTER mode 0 -> 1,0,2,4,1,2,0,4,1,0
TTFILTER mode 1 -> 1,0,0,0,1,0,0,0,1,0
TTFILTER mode 2 -> 0,0,1,0,0,1,0,0,1,0
TTFILTER mode 3 -> 0,0,1,0,0,1,0,0,0,0
TTFILTER mode 4 -> 0,0,0,1,0,0,0,1,0,0
```

这些向量确认了几个非直觉边界：

- `DATETODAY` 的纪元精确为 1990-12-19，旧实现返回内部绝对日序是错误的；
- `TIMETOSEC(-1)` 和 `SECTOTIME(-1)` 因原生 `+0.503` 后向零截断而返回 0；
- `WEEKOFYEAR` 以周日作为新周起点，并带 1 月 1 日星期偏移；
- `TFILT` 的日期和分钟边界均包含，日期参数小于 700000 时使用最新柱日期；
- `TFILTER/TTFILTER` 的同柱双信号受原生分支和状态修改顺序影响，不能用简单
  去重规则替代。

## 纯 C++ 实现

`native/src/formula_engine.cpp` 新增：

- 自动符号：`TIME2`、`WEEKOFYEAR`；
- 函数：`ALIGNRIGHT`、`DAYTODATE`、`SECTOTIME`、`TFILT`、`TFILTER`、
  `TIMETOSEC`、`TTFILTER`；
- 日期逆换算 `calendar_date_from_day_number`；
- `DATETODAY/DATETOTODAY` 的 1990-12-19 相对纪元、输入范围、日期合法性和
  原生舍入边界修正；
- 秒级 K 线时间解析，日线继续使用宿主默认的 `15:00:00`；
- `custom_formula_calendar_filter_functions` 7 项和
  `custom_formula_calendar_filter_symbols` 2 项能力分组。

解释器能力由 174 个函数、48 个自动符号增至 181 个函数、50 个自动符号。
Svelte 公式库新增“日历/信号核心”能力标签，展示这 9 项能力，但没有把它们误作
行业或板块数据。

## 回归与发布

- CTest：101/101；
- Svelte 检查：0 错误、0 警告；生产构建成功；
- 临时服务全量 API 契约：207/207；
- 正式服务专项契约：4/4，报告见
  [api-contracts-calendar-filter-formal-selected.json](../../output/api-contracts-calendar-filter-formal-selected.json)；
- 公式覆盖快照见
  [formula-coverage-current.json](../../output/formula-coverage-current.json)。

正式发行文件 SHA-256 为
`B8969524C66DB650C24F439F88A3992ABBFF1D3338BBC81DA08ED55FDEAD317A`；前端资源为
`index-DzVsk5Wg.js`、`index-BBbdHCcI.css`。正式服务 PID 40296，仅监听
`127.0.0.1:8765`，健康检查为 `native_cpp=true`、`python_runtime=false`。
