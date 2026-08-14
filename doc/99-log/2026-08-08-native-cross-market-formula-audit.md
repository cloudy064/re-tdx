# 2026-08-08 跨市场公式审计与网页工作台

本轮修复了公式解释器“单次执行支持扩展市场、全库审计和网页仍偏 A 股”的接入
不一致。底层 7727 行情和解释器本身无需替换；缺口位于审计聚合上下文和网页
证券代码校验。

## 实现

- `formulas audit` 接受 TDX 扩展市场 ID `3..255` 及五个期货别名；
- 审计可传 `--option-name/--expiry/--risk-free`，与 `formulas evaluate` 一致；
- 扩展市场只自动绑定 `IVOLAT`，A 股专属外部依赖逐公式标记为
  `market_inapplicable`，不再提前请求无关数据或形成假错误；
- 新增只读 `/api/v1/formulas/audit`，返回 379 条逐公式状态和汇总；
- Svelte 公式工作台增加港股、五个期货市场、五个期权市场，支持扩展市场 1—9
  字节线码以及期权显示名、到期日、无风险利率；
- 网页新增“全库审计”按钮和错误摘要，扩展市场回测仍明确限制在深沪京，避免
  把尚未验证的跨市场回测当成已支持能力。

## 真实样本

- 港股 `31:00700`：`SHORTVOL` 通过；
- 中金所期货 `47:IFL9`：`CCL` 输出持仓量；
- 中金所期权 `7:HO8W03UX`、显示名 `HO2608-C-2500`：
  `VOLATILITY` 输出 `HV/隐含波动率/波动差`；
- 港股与期权全库审计均报告 379/379、有明确状态、0 条解释器错误。

证据文件：

- `output/probes/formula-audit-hk-context-20260808.json`
- `output/probes/formula-audit-option-context-20260808.json`
- `output/probes/formula-hk-short-cross-20260808.json`
- `output/probes/formula-futures-ccl-cross-20260808.json`
- `output/probes/formula-option-ivolat-cross-20260808.json`

## 验证

- CTest：98/98；
- Svelte check：0 error / 0 warning；
- Svelte 生产构建成功；
- 新增 3 条固定契约，临时与正式服务完整契约均为 163/163：
  - `output/probes/api-contracts-full-temp-20260808-cross-market-formula.json`
  - `output/probes/api-contracts-full-formal-20260808-cross-market-formula.json`

正式发行包已部署到 `127.0.0.1:8765`，PID `38464`。健康检查为
`native_cpp=true`、`python_runtime=false`、580 个 JSN 文件、51,935 个证券、
379 条公式和 1,159 个板块；主页命中新资源 `index-B3qfI9Au.js` 与
`index-CdVDQIMZ.css`。

发行版 `tdx-tool.exe` SHA-256：
`F08414FEEEDB4A6769FB0644436A2673697C329CB9D65217E7AD4E6AEAEDC4DF`。
