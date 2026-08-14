# 公式库驱动的系统/用户指标输出引用

日期：2026-08-12

## 缺口与结论

真实系统+用户库重新审计为 380/380 条源码语法支持。剩余 40 条运行边界是 20 条
只读未来公式、14 条授权 L2 公式和 6 条券商私有信号公式，公开非 L2 函数缺口为 0。
因此没有用推测值继续扩充函数表，而是补齐用户公式增长后更有价值的组合能力：

```text
X:"指标代码.输出名";
```

过去只有真实内置库用到的 `"KDJ.J"` 与 `SAR.SAR` 两条内建捷径。现在双引号引用由
当前装载的公式库解析，可指向系统或 `PriGS.dat` 恢复的用户技术指标。

## 原生实现

- `formula_engine_support` 识别不含证券 `$` 的 `EXTERNAL#指标.输出` 为公式引用，
  并将其纳入自动上下文，而不是普通外部证券序列；
- `formula_catalog_internal` 按技术指标代码与输出名进行不区分 ASCII 大小写的严格
  选择，检查源码、数值安全、未来函数和嵌套外部依赖；
- `formula_reference` 在相同证券、相同周期和调用方 K 线窗口执行子指标，继承被引用
  公式的参数默认值，按精确 `DATE|TIME` 绑定输出；
- 同一个子指标的多输出共享一次求值；
- 与 `CALCSTOCKINDEX` 共用最大 4 层深度和循环检测，缺失指标、缺失输出、循环或
  不安全子指标均明确报错；
- 分析、求值、扫描、监控、审计、专家回测与组合策略均把当前公式库继续传入递归
  上下文，因此 `--include-user` 装载的用户库不会在子引用中丢失。

随后补充了不执行公式的静态引用图。`formulas analyze` 的 CLI 文档会附加
`tdx-formula-reference-graph-v1`：逐绑定区分 `resolved`、`missing-formula` 和
`missing-output`，复用运行期安全条件标记目标是否可执行，并用 Tarjan 强连通分量
报告直接自引用或多指标循环。它是独立报告，没有写入服务端持有的 coverage 对象，
因此 `/api/v1/formulas/coverage` 的既有 schema 保持不变。

后续已用原 TCalc 编译器确认并补齐带位置参数的
`"指标.输出"(参数...)`；详见
[参数化公式引用记录](2026-08-12-native-formula-parameterized-references.md)。

## 增量验证

- 公式引擎与组合策略专项 CTest：2/2 通过，约 2.0 秒；
- 合成兼容库包含一个双输出技术指标、一个引用它的条件公式和一个引用它的专家公式；
- 平安银行本地 120 根日线求值成功：2 个输出绑定、子指标只求值 1 次；
- 扫描 1 条证券，错误 0；
- 全库审计 3/3 通过，错误 0；
- 专家回测与条件公式组合策略回测均完成；
- 静态图合成库验证 4 个引用全部解析；缺失指标、缺失输出和两指标循环由单元夹具
  分别命中，循环中的 2 个绑定不会计入 `executable_binding_count`；
- 真实系统+`PriGS.dat` 合并库为 380 条、222 个技术指标，当前目录源码没有跨公式
  引用，静态图为 0 个引用、0 个缺失、0 个循环，没有误报；
- 正式 `127.0.0.1:8765` 未重启、未替换。

机器可读证据：

- `output/verify-formula-reference-library-20260812.json`
- `output/verify-formula-reference-evaluate-20260812.json`
- `output/verify-formula-reference-scan-20260812.json`
- `output/verify-formula-reference-audit-20260812.json`
- `output/verify-formula-reference-backtest-20260812.json`
- `output/verify-formula-reference-strategy-backtest-20260812.json`
- `output/verify-formula-reference-graph-20260812.json`
- `output/verify-formula-reference-graph-real-20260812.json`
- `output/ida-tcalc-formula-reference-20260812.json`
