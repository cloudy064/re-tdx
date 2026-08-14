# 公式解释器剩余执行边界复核

日期：2026-08-11

## 结论

基于重新生成的 379 条公式 analysis schema v7 结果，339 条已经可在普通或自动上下文中
执行。剩余 40 条全部落入明确的运行边界，没有新的公开、非 L2、尚未实现的函数：

| 边界 | 数量 | 处理方式 |
|---|---:|---|
| 只读未来函数 | 20 | 可用 `--allow-future` 做图表渲染；禁止进入扫描和回测 |
| 授权 L2 序列 | 14 | 只能由调用方提供精确、已授权的序列 |
| 券商私有 `SIGNALS_QS` | 6 | 只能由调用方提供原始信号序列 |
| 未分类公开非 L2 缺口 | 0 | 无 |

## 公式范围

只读未来公式：

`XT, SQJZ, ZXNH, ICHIMOKU, CYX, WAVE, XLPL, NXTS, WAVEKX, WYGD, SGCJ,
SZTAI, PINGDING, PINGDI, HYFG, TKQK, SFWY, SSSBQ, XDSBQ, FENLI`

授权 L2 公式：

`ZJLX, ZJQDL, ZJBY, DDX, DDY, DDZ, DDF, SUPL, SUPV, SUPH, SUPAMO,
DDE排序, SUP排序, DDE力度`

券商私有信号公式：

`红绿波段, 撑压信号, R标记数, G标记数, 形态大师, 主力密码`

## 证据口径

- 379/379 公式语法支持；
- 339 条 `executable_with_context`；
- 剩余公式逐条检查 `has_future_function`、`explicit_context_bindable`、
  `external_dependencies` 和 `context_bindings_unavailable`；
- TCalc 390 项静态注册证据继续完全分类；
- 未发现需要通过猜测、伪造或绕过授权补齐的数据。

机器可读结果位于 `output/formula-interpreter-boundary-audit-v1.json`。这份结论区分了
“解释器未实现”和“运行权限/未来引用策略限制”，后续新增公式库时可用同一口径重新审计。

## 2026-08-12 信号注册边界复核

后续对 TCalc 0x47 字节注册记录、TdxW callback selector 和文件提供者的精确追踪，
已经把 `SIGNALS_SYS` 与 `SIGNALS_USER` 从静态边界重新分类为可自动读取的本地
非 L2 函数；只有 selector 35 的 `SIGNALS_QS` 仍是券商私有输入。当前能力清单为
390 条注册名识别 319、明确边界 71、完全分类。

这项重新分类不改变上面的 379 条内置公式结论：六条私有信号公式都只调用
`SIGNALS_QS`，没有内置公式依赖 `SIGNALS_SYS/SIGNALS_USER`，因此剩余仍为 40 条、
公开非 L2 缺口仍为 0。完整证据见
[本地信号与 DLL 审计](2026-08-12-native-local-signals-and-dll-audit.md)。
