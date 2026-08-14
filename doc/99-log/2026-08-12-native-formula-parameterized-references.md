# TCalc 参数化公式输出引用

日期：2026-08-12

## 原 DLL 证据

隔离的 32 位 TCalc 编译探针确认下列源码均可编译：

```text
X:"MACD.DIF"();
X:"MACD.DIF"(12);
X:"MACD.DIF"(12,26,9);
X:"MACD.DIF"(6+6,26,9);
X:"MACD.DIF"(N,26,9);
```

最后一例在调用方公式元数据中声明 `N=12`，证明父公式参数可以作为子指标位置参数。
证据保存在 `output/ida-tcalc-compile-reference-parameter-20260812.json`。原生探针曾在
传入 `CLOSE` 序列时表现不稳定，因此本轮没有把任意逐柱序列参数描述为等价能力。

## 纯 C++ 实现

- AST 分析记录每个双引号引用及其参数表达式；
- 安全标量只允许有限常量、父公式声明参数和 `+ - * / %` 算术；
- 参数按被引用指标的元数据顺序覆盖，参数不足时保留其余默认值，参数过多明确报错；
- 相同指标、相同参数变体共享一次子公式求值，不同参数变体使用独立缓存键；
- 运行时再次验证参数在全部 K 线点上恒定，避免外部构造的分析文档绕过边界；
- CLI、扫描、回测、策略和 HTTP 公式执行路径都会把调用方参数传给自动上下文构建器；
- 引用图继续按指标/输出建立循环边，同时报告参数化引用能力和出现数量。

## 增量验证

- `tdx-formula-engine-tests` 通过，覆盖常量算术、父参数覆盖、子指标默认值、参数变体
  缓存以及动态序列拒绝；
- 平安银行 80 根真实日线执行 `"MACD.DIF"(P,26,9)` 成功，`P=12`，绑定 1 个、
  子指标求值 1 次，最新值为有限数；
- `"MACD.DIF"(CLOSE,26,9)` 在执行前明确拒绝；
- 真实系统与用户合并库仍为 380 条，277 个支持函数、87 个自动符号，静态引用图
  0 个缺失、0 个循环；当前真实库没有参数化引用，因此计数为 0；
- 正式服务继续由原进程 24096 监听 8765，本轮未重启或替换。

机器可读证据：

- `output/ida-tcalc-compile-reference-parameter-20260812.json`
- `output/verify-parameterized-reference-kline-20260812.json`
- `output/verify-parameterized-reference-evaluate-20260812.json`
- `output/verify-formulas-parameterized-reference-20260812.json`
