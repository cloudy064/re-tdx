# TCalc BUYVOL/SELLVOL 即时内外盘

> 日期：2026-08-05。实现位于 `native/src/formula_engine.cpp` 与
> `native/src/formula_context.cpp`。

## 结论

通达信官方函数表明确给出：

- `SELLVOL` 返回内盘，等价于 `DYNAINFO(22)`；
- `BUYVOL` 返回外盘，等价于 `DYNAINFO(23)`。

现有 `0x054C` 快照解析已经输出 `inside_dish`、`outer_disc` 和 `total_hand`，
三者单位均为手。平安银行盘后样本为内盘 `900,927`、外盘 `610,583`、总量
`1,511,509`，内外盘之和仅有协议取整造成的 1 手差值，字段关系闭合。

公式分析器现在把 `BUYVOL/SELLVOL` 识别为可由只读即时快照绑定的外部符号；
上下文构造器只要遇到任意一个符号，就获取一次选定股票快照，并分别绑定
`outer_disc/inside_dish`。没有把它们误接到买一/卖一挂单量。

## 收益与验证

条件选股 `B005`（盘中活跃低价股）由不可执行变为可执行，其原式中的
`SELLVOL/BUYVOL` 现在使用真实内外盘比。整体含上下文覆盖由 302 增至 303，
条件选股达到 `103/107`。

验证结果：

- 原生边界测试验证 `SELLVOL=900`、`BUYVOL=600` 时比值为 `1.5`；
- 平安银行 32 根日线的真实 B005 API 绑定了 `BUYVOL/SELLVOL` 并返回数值末值；
- 平安银行 800 根日线的全库上下文审计 `303/303` 通过、0 错误、0 空输出；
- 全量原生 CTest `21/21` 通过。

官方语义见
[通达信公式函数列表](https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html)。运行样本为
`output/formula-B005-inside-outside.json`，整库审计为
`output/tcalc-formula-context-runtime-audit-snapshot.json`。
