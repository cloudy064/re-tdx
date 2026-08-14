# native C++ 可维护性重构：formula_environment.cpp 拆分

日期：2026-08-11
范围：`native/src/formula/formula_environment*`、`native/CMakeLists.txt`、`native/tests/formula_engine_context_library_tests.cpp`

## 动机

`formula_environment.cpp` 共 474 行，其中 `FormulaEnvironmentBuilder::build()` 一个函数占 406 行，
单函数集中度 86%，是当时 `native/` 内集中度最高的文件之一。

该函数把公式求值所需的**全部**符号表绑定塞在一处：价格字段、复权标志、方向性 K 线、证券身份、
外部指标、参数、上下文（文本符号/外部信号/目录覆盖/数值组/证券序列/换手率/隐含波动率）、
K 线位置序列、日历序列、盘中序列、会话常量、衍生序列、RAND 种子协商。

重复代码：

- ISO 日期转日号的 8 行 `substr` + `stoi` + `try/catch` 片段出现 2 次（expiry 与 history 各一份）
- 从 K 线文档读取字符串字段的 3 行判空片段出现 3 次
- `#ifdef _WIN32 / localtime_s / localtime_r` 分支散落在日历区
- `if (context && context->is_object())` 守卫在上下文各分组内反复出现

## 拆分策略

`FormulaEnvironmentBuilder` 本身已把全部输入持有为成员（`kline_document_`、`program_`、`bars_`、
`parameters_`、`context_`），因此每个阶段都能直接落成私有成员函数 `void bind_xxx(Environment&) const`，
无需引入参数结构体。公有 API（构造函数 + `build()`）保持逐字节不变。

406 行拆成 21 个单元，分布在 4 个翻译单元：

| 文件 | 行数 | 内容 |
| --- | --- | --- |
| `formula_environment.cpp` | 80 | `build()` 阶段编排 |
| `formula_environment_fields.cpp` | 179 | 价格字段、复权标志、方向性 K 线、证券身份、外部指标、参数 |
| `formula_environment_context.cpp` | 222 | `bind_context` 分派 + 7 个上下文子阶段 |
| `formula_environment_calendar.cpp` | 192 | K 线位置、日历、盘中、会话常量、衍生序列、RAND 种子 |
| `formula_environment_internal.hpp` | 106 | 18 个私有方法声明 + `FormulaRandomSeed` + 顺序依赖注释 |

## 引入的共享抽象

- `iso_date_day_number(const std::string&)` —— 合并 2 处日期解析。返回 `missing` 而非抛异常，
  与原行为一致（上下文日期来自调用方，畸形值跳过而不报错）。
- `document_text(const Json&, std::string_view)` —— 合并 3 处文档字符串读取。
- `local_calendar(std::time_t)` —— 收拢 `#ifdef _WIN32` 分支；时钟由 `build()` 采样一次。
- `bind_context` 分派器 —— `if (!context_ || !context_->is_object()) return names;` 只保留一处。
- `FormulaRandomSeed` —— 让 `resolve_random_seed` 能一次返回 `uses_random`/`seed`/`mode` 三元组。

## 保持不变的语义

`Environment` 是 `std::map<std::string, Series, std::less<>>`（`formula_engine_internal.hpp:14`），
因此**向 env 插入的顺序对查找无影响**——这是允许常量跨阶段边界移动的前提（MINDIFF/AUTOFILTER/
HQCRBK 从日历区移入 `bind_session_constants`）。

但有三处顺序是**承重的**，已写入头文件注释：

1. `bind_external_indicators` 针对价格字段求值，不能观察到其后的参数/上下文/日历绑定；
2. `bind_context_turnover`（HSL）读取 CAPITAL，必须排在任何可能绑定 CAPITAL 的阶段之后；
3. `context_bindings` 是 `Json::array`，追加顺序可观测，阶段调用顺序必须精确为
   EXTERNVALUE# → EXTERNSTR# → finance/finvalue/dynainfo → 标量 → 证券 → 序列 → HSL → IVOLAT。

其他细节：

- 原来单个 per-bar 循环写入三组互不相交的字段（位置/日期/时间），且各有独立 `try/catch`，
  因此拆成三个循环语义等价。
- `std::time(nullptr)` 仍只调用一次，由 `build()` 采样后同时传给 `bind_calendar_series` 与
  `resolve_random_seed`。
- MTM 周期选择器读的是原始 `parameters_` map，大小写敏感的 `"N"`，而非 `bind_parameters`
  写入 env 的大写键。

## 测试

在 `formula_engine_context_library_tests.cpp` 已有的 IVOLAT 用例之后补了两条断言，覆盖
`iso_date_day_number` 合并后唯一的真实风险——两个 history 序列的下标对齐：

- 在 4 个有效行之间交错插入 5 个被拒行（`"bad"`、`"2026/01/02"`、`"20260103"`、非字符串 date、
  非数值 close），断言 HV 与干净 history 逐位相等，证明 day 与 close 总是成对丢弃；
- 用无法解析的 expiry（`"2026/06/30"`）断言历史分支不受影响。

一个认识修正：`calendar_day_number`（`formula_function_support.cpp:10-18`）是纯 Hinnant 历法算术，
**不做范围校验**。因此 `"2026-13-99"` 和 `"2026-01-0X"`（`stoi("0X")` → 0）都会得到有限日号并
正常贡献数据点，只有长度/分隔符守卫才真正拒绝。最初按"月份 13 会被跳过"写的断言因此是错的，
已按实际行为重写并在注释中记录该宽松性。

变异测试：把 `if (!std::isfinite(day)) continue;` 改成丢弃 day 但保留 close，新断言立即失败——
失败信息来自 `formula_functions_host.cpp:172-174` 的 `history_days->second.size() !=
history_closes->second.size()` 防御检查。该不变量因此有双重保护。已还原。

## 验证

- 4 个翻译单元 `-fsyntax-only` 通过
- `libtdx_native.a` 及全部目标编译无错误无警告
- **ctest 108/108 通过（32.93s）**
- CLI env-check 覆盖 13 个绑定组，逐值独立核对：DT=1260102（2026-01-02）、WD=5（周五）、
  DTT=221、TM2=93507、FO=5、TQ=1、PD=1、SC=1、TRV=1.2、MTM(N=2)=None/None/2/1.2、
  ZJ==QJ、OI==CL
- 循环拆分的关键证据：故意畸形的第 4 根 K 线（`date:"bad-date"`、`time:"xx:yy"`）日期与时间
  双双解析失败，但位置字段仍正常填充（CB=1、UD=4、LB=1、BS=2），日历字段为 None、时间字段为 0
  —— 与原实现各自独立 `try/catch` 的行为完全一致
- 6 条校验错误信息逐字复现；10 个 `adjustment_mode` 取值中 8 个经 CLI 正确映射，另 2 个
  （`bogus`/`forward`）由 CLI 层既有白名单在到达 builder 前拦下，非本次回归——builder 的分支
  仍可从 HTTP/库调用方到达

## 集中度结果

| | 拆分前 | 拆分后 |
| --- | --- | --- |
| 最大函数 | 406 行 | 42 行 |
| 最高集中度 | 86% | 45% |

拆分后各文件：`formula_environment.cpp` 45%（36 行的 `build()`，纯阶段调用序列，无嵌套）、
`formula_environment_calendar.cpp` 22%、`formula_environment_context.cpp` 18%、
`formula_environment_fields.cpp` 15%。

## 后续候选

`formula_engine.cpp`：512 行，最大函数 247 行（第 29 行起），集中度 48%。

仍不在机械拆分范围内（需专门规划）：`jsn.cpp`、`formula_functions_series.cpp`、
`formula_context_dynamic.cpp`、`tpool.cpp`、`tpool_evaluate.cpp`、`pbrpc.cpp`。
