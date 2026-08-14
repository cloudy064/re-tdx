# TCalc 最新财报同比上下文

> 日期：2026-08-05。运行时实现位于 `native/src/professional_data.cpp`、
> `formula_context.cpp` 和 `formula_engine.cpp`，全程为纯 C++。

## 字段闭合

通达信官方函数表定义 `FINANCE(43)` 为最近一期净利润同比增长率，
`FINANCE(44)` 为最近一期营业收入同比增长率；官方专业财务字段表同时定义：

- `FN183`：营业收入增长率（%）；
- `FN184`：净利润增长率（%）。

因此实现直接采用 `FINANCE(43) -> FN184`、`FINANCE(44) -> FN183`，没有自行
套同比公式。这样能保留数据生产端对负基数、零基数和异常报告的原始口径。
非有限值仍保持不可用，不会伪造成 0。

平安银行 `SZ000001` 的 `gpcw20260331` 实测为：

| 项目 | 2026Q1 | 2025Q1 | 官方同比 |
| --- | ---: | ---: | ---: |
| 营业收入 `FN230` | 35,277,000,704 | 33,709,000,704 | `FN183=4.65%` |
| 归母净利润 `FN232` | 14,522,999,808 | 14,096,000,000 | `FN184=3.03%` |

当前 `0x0010` 同时给出收入 35,277,000,000 元、归母净利润 14,523,000,000 元，
差异仅来自专业包的 float32 表示，报告口径和字段映射一致。

## 实现与边界

公式分析器把 `43/44` 纳入可绑定 `FINANCE` ID。上下文构造器在需要任一字段时
读取一次包含证券的最新官方季度包，并输出报告期、字段值和
`tdx-professional-fn183-fn184-latest-report-constant` 模式元数据；与 `FINVALUE`
同时出现时共用同一次读取。

这是当前截面的“最新报告期常量”，适合实时选股和接口验证，不是按公告日回放
的历史财务序列，不能直接用于无前视偏差的历史回测。

## 收益与验证

- 解锁 `A006` PEG 选股和 `A010` 轻资产小型成长股；
- 条件选股含上下文覆盖由 `103/107` 升至 `105/107`；
- 全库含上下文覆盖由 303 升至 305；
- 平安银行 800 根日线的 305 条候选全部通过，0 错误、0 空输出、305 条均有
  数值末值；
- 单元测试覆盖负增长、零增长、非有限值及错误 ID，验证不在本地重算或替换。

产物：

- `output/tcalc-formula-coverage-growth.json`；
- `output/tcalc-formula-context-runtime-audit-growth.json`；
- `output/formula-A006-finance-growth.json`；
- `output/formula-A010-finance-growth.json`。

官方定义见[专业财务数据字段表](https://help.tdx.com.cn/quant/docs/markdown/TdxQuant.md/mindoc-1h10m001ic888.html)
和[公式函数列表](https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html)。
