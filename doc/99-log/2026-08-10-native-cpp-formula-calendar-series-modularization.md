# 原生 C++ 公式日历与序列统计函数族拆分

日期：2026-08-10

## 拆分结果

`formula_functions.cpp` 由上一轮的 3,353 行继续降到 2,463 行：

- `formula_functions_calendar.cpp`（347 行）负责 `DATETOCUR`、日期/秒数互转、
  `ALIGNRIGHT`、`TFILT/TFILTER/TTFILTER`；
- `formula_functions_series.cpp`（645 行）负责从 `COUNT/EVERY/EXIST` 到 `TR`
  的条件序列、滚动离散度、协方差/相关性、极值排名、回归和常用技术统计；
- `calendar_day_number()` 迁入 `formula_function_support.cpp`，供日期函数和期权
  到期日计算共同使用；
- 两组函数继续通过 `unordered_map<string_view, FunctionHandler>` 注册，并由
  `evaluate_call()` 组合调用。

主函数现在保留基础数学、引用/未来路径、专用 TDX 指标及少量递归平均实现，
不再直接承载日期状态机和大段滚动统计条件链。

## 行为保持

- `DATETOCUR` 仍使用最后一个参数、float32 收窄并计算完整同日 K 线数量；
- `TFILT` 日期零值仍解析为最后一根 K 线日期，起止边界均包含；
- `TFILTER/TTFILTER` 的原生持仓状态与同柱信号处理顺序未变；
- `COVAR/RELATE/BETAEX` 的样本协方差、float32 累积和退化窗口回退未变；
- `VAR/VARP/STDP`、`FINDHIGH/FINDLOW`、`SUMBARS/SUMBARSX` 的暖机、排名
  与左边界语义保持不变；
- 公共公式 API、上下文 schema 和函数名集合没有变化。

## 增量验证

- `tdx-formula-engine-tests` 构建并通过；测试中直接覆盖上述日期、交易过滤和
  统计函数；
- `tdx-tool` 增量链接通过；
- 公开 `sz:000001` 20 根日线执行
  `COVAR(CLOSE,CLOSE*2+1,5)+DATETODAY(DATE)` 成功，输出 20 个点；
- 输入：`output/probes/function-series-calendar.tdx`；
- 证据：`output/native-formula-series-calendar-v27.json`；
- 未运行完整 CTest 或 API 合约巡检。

## 后续边界

下一轮优先把 `TDXSSRP/TDXPAV/TDXNDB/TDXSC/TDXMCST/TDXKDJ/TDXSAR` 等专用
指标及其私有辅助算法迁到策略模块；之后处理 `formula_context.cpp` 的横向统计和
行业指数序列，使解释器核心最终只保留基本表达式运算和领域分派组合。
