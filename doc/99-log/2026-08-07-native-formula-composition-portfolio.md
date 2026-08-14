# 多公式同 K 线组合与组合回测

## 目标

在单公式扫描、专家回测和持续策略池之外，补齐真正的多条件策略组合：多个条件
必须在同一证券、同一日期和时间的 K 线上同时成立，并能用固定股票池验证历史
组合表现和逐股贡献。保持纯 C++，不依赖 Python，不涉及 L2。

## 策略清单与执行语义

新增 `formulas strategy`，清单支持 1—16 条规则。每条规则可以携带不超过
16 KiB 的 `source`，也可以用 `formula` 引用内置条件选股；参数按规则隔离。
组合符为：

- `all`：所有规则命中；
- `any`：至少一条命中；
- `at-least`：至少命中 `minimum_matches` 条。

解释器对每条规则使用同一份证券 K 线，逐点校验结果数量和 `date|time` 后再组合，
因此不会把不同日期、不同证券的最近扫描结果误相与。规则证据保留命中 ID、输出
和值；纯展示输出不参与真假判定。未来函数、数值信号不安全公式和不可执行依赖
在清单规范化阶段直接拒绝。

示例位于 `output/probes/custom-strategy.json`：五期均线趋势与三期价格动量要求
同周期确认。

## 组合回测

`--mode backtest` 使用固定输入股票池和所有证券 `date|time` 的交集：

1. 本根共同 K 线收盘确认每只证券是否命中；
2. 下一根共同 K 线开盘才按当前命中证券等权调仓；
3. 持仓权重随开盘到开盘收益漂移，调仓计入佣金和滑点；
4. 数据末端强制平仓；
5. 输出逐股毛贡献、分摊成本、净贡献、持有区间、进入和退出次数。

逐股净贡献之和必须与 `final_equity - initial_capital` 对账。任一证券取数失败会
拒绝回测，避免悄悄改变固定股票池。历史外部依赖只放行带归档实际披露日的
`FINANCE/FINVALUE`；其他依赖的历史安全性未证明时明确拒绝。

## CLI、API 与网页

```powershell
tdx-tool formulas strategy --strategy output\probes\custom-strategy.json `
  --mode scan --securities sz000001,sh600000 --period day --lookback 20

tdx-tool formulas strategy --strategy output\probes\custom-strategy.json `
  --mode backtest --securities sz000001,sh600000 --period day --pages 2 `
  --initial-capital 100000 --commission-bps 2.5 --slippage-bps 1
```

新增确认 POST：

- `/api/v1/formulas/strategy/scan`，确认头
  `X-TDX-Action: formula-strategy-scan`；
- `/api/v1/formulas/strategy/backtest`，确认头
  `X-TDX-Action: formula-strategy-backtest`。

请求正文不会被保存；响应中的策略摘要不包含 `source/source_text` 或内部
`formula_definition`，只报告规则 ID、标签、公式代码、参数、分析和源码 MD5。
Svelte 公式页新增“多公式组合策略”工作台，可编辑清单、选择固定股票池、执行
扫描/组合回测并查看逐股归因。

## 验证

- 新增确定性 C++ 测试，锁定 `all/any/at-least`、未来函数拒绝、严格同 K 线
  组合、下一开盘执行、共同时间轴和逐股归因对账；
- 平安银行与浦发银行真实日线扫描：请求 2、完成 2、命中 2、错误 0；
- 两页真实日线组合回测：2 只证券、1,600 根共同 K 线、923 次调仓、2 行逐股
  归因、取数错误 0；这只是实现验证结果，不构成策略收益建议；
- 策略 API 专项契约 2/2；部署态完整契约 97/97；
- C++ 完整测试 66/66；Svelte 0 错误、0 警告，生产构建成功；
- 功能目录由 113 项增至 114 项，并包含 `formulas strategy`；
- `serve --self-test`：`feature_count=114`、`jsn_available=true`、`ok=true`。

验证产物：

- `output/probes/custom-strategy.json`；
- `output/probes/formula-strategy-scan.json`；
- `output/probes/formula-strategy-backtest.json`；
- `output/probes/formula-strategy-api-scan.json`；
- `output/probes/api-contract-strategy-20260807.json`；
- `output/probes/api-contract-full-20260807-formula-strategy.json`。

部署服务为 `http://127.0.0.1:8765`，PID `25380`。发行包 SHA-256 为
`62E4662DFF352C9664B32C837DA0E8B3CC1A5A254DF45FC7783ADE3296D84729`。
