# 基金报告期持仓纯 C++ 固定化

## 目标与边界

`jj_ccfx_dqcc.xml` 当前使用普通 JSON `500050—500052`：基金筛选主表、
所选基金行业持仓和所选基金股票持仓。旧 PBRPC `500007` 已被客户端注释。
原生工具此前只能通过通用 `cloud tqlex/workflow` 查看原始表，本次将三条请求
接入 `market fund-analytics`：

- `reported-holdings`：基金、净值、报告期、换手、股票/行业数量与集中度、
  股票市值、基金公司、规模、成立日和经理；
- `reported-holding-industries`：行业、持仓市值及占股票投资比例；
- `reported-holding-securities`：股票、当前价/涨跌、报告期持股数量/市值及
  占基金净值比例。

这些字段来自公开报告期。股票数量、市值和比例不是实时完整仓位；明细中的
现价/涨跌可以随上游行情变化，不能反推基金当前实际持仓。

## 最近完整披露期回退

2026-08-06 实测 `500050` 能返回基金 `000326` 的 2025 年报主表，但同基金
`500051/500052` 在 `20251231` 均为零行；回退至 `20250630` 后分别返回
11 个行业和 106 只股票。因此默认明细查询按半年报/年报倒序寻找最近有行的
报告期，并输出：

- `requested_report_date=2025-12-31`；
- `resolved_report_date=2025-06-30`；
- `latest_report_fallback_used=true`。

显式传入 `report_date=20251231` 时不回退，严格返回 `availability=empty`。
默认回退与显式日期写入不同缓存键，修复了“默认请求先命中后，显式空请求被
回退缓存替代”的语义污染。

`turnoverRate` 在客户端用百分比格式显示但上游返回比例；接口同时输出原始
`turnover_ratio` 和乘 100 后的 `turnover_pct`。金额字段统一带 `_yuan`，
百分数字段统一带 `_pct`。

## 证据与验收

- 原始证据：`output/probes/tqlex-500050-current.json`、
  `tqlex-500051-000326-current.json`、`tqlex-500052-000326-current.json`，
  以及对应 `20250630` 明细；
- 业务化证据：`output/probes/fund-reported-holdings-000326-current.json`、
  `fund-reported-industries-000326-current.json`、
  `fund-reported-securities-000326-current.json`；
- 专项测试覆盖基金主表、换手比例/百分比、行业和股票持仓字段；
- 临时 API 验证默认回退 11/106 行、显式年报空结果、显式半年报 106 行，
  并确认原有风险视图继续返回。

全量 CTest 39/39 通过。

## 正式部署

正式 `8765` 服务已部署，PID 为 `6972`，EXE SHA-256 为
`7DFDAB5808EFA56A25A8C3B3E6959E20E0DAFD1714AAE16C1EE5A3F5AB156046`。
健康检查为 `ok=true`、`native_cpp=true`、`python_runtime=false`；功能目录仍为
88 个命令，`market fund-analytics` 的说明已更新为十三类视图。

正式 API 按“默认明细→显式年报”顺序复验，默认行业/股票为 11/106 行并回退
到 2025 半年报，随后显式年报仍稳定返回 `empty`，证明缓存隔离生效。原有基金
风险视图和异常证券空关系同时通过回归。主页 HTTP 200；Svelte HTML/JS/CSS
哈希仍为 `0F1FBB...5777`、`B03C28...4011`、`733085...30A`，没有覆盖用户 UI。

## 请求级覆盖复核

部署后重新读取正式 `/api/v1/tqlex/configs` 与 `/api/v1/pbrpc/configs`，并排除
通用 `tqlex/pbrpc/cloud_workflow` 实现后，对全部唯一请求号检查专用业务源：

- TQLEX：73 个启用模板、34 个唯一 ReqId，34/34 均已由专用 C++ 模块引用；
- PBRPC：58 个启用模板、29 个唯一 ReqId，29/29 均已由专用 C++ 模块引用。

这是 ReqId 级覆盖，不表示同一 ReqId 的每个重复模板、参数组合和 UI 分支都已
逐个固定验收。下一阶段应转向模板变体矩阵、正常空关系/瞬时上游失败契约回归，
以及 616 个 JSN 候选中尚未产品化的高价值动态关系。

该部署随后已被同日的固定 API 契约巡检版本替代；当前正式进程与哈希见
`2026-08-06-native-api-contract-audit.md`，本节只保留当时验收记录。
