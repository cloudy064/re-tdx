# 2026-08-08 港股历史沽空量与剩余 JSN 缺口审计

本轮先对 XNXS 收敛后剩余的 55 个 `generic-only` JSN 模板做 709 元数据探测，
再从非 L2 扩展行情中固定港股历史沽空量。

## 剩余 JSN 的真实状态

- 55 个模板中 35 个远端已不存在；
- 20 个仍非空，总计 156,693,757 字节，全部属于债券全市场/沪深拆分表；
- 隔离下载的国债沪深投影各 225 条，合计精确覆盖现有 `bond-reference` 国债
  主表 450 条；企业债深市样本 120 条也是现有 1,590 条主表的子集；
- 因此未把约 149.4 MiB 高度重复数据并入正式镜像，正式 JSN 仍为 580 个文件。

证据：

- `output/probes/jsn-generic55-probe-20260808.json`
- `output/probes/jsn-bond-split-sample-download-20260808.json`
- `output/probes/jsn-bond-split-sample-catalog-20260808.json`

## 港股历史沽空量

7727 扩展市场的 31/48 港股日 K 将辅助字段标记为 `hk_short_volume`。腾讯控股
`31:00700` 的 60 日样本中，最近 3 个与 `GGRL104` 重叠的交易日沽空股数 3/3
逐日精确一致；日 K 还能提供更长历史及事件表尚未发布的最新交易日。

新增纯 C++ `market hk-short-history` 与固定 API：

- 输出逐日沽空股数、5/20 日移动均值和日变化；
- 将日 K 成交量按港股 100 股/手换算为成交股数，计算沽空股数占比；
- 保留 `GGRL104` 的沽空金额、成交金额和按金额计算的比例作独立对账；
- 明确区分“股数占比”和“成交额占比”，不混用两个口径；
- 港股事件页增加 TradingView Lightweight Charts 柱线图和对账徽标。

真实样本：`output/tdx-hk-short-history-00700-native.json`。

## 验证

- CTest：98/98；
- Svelte check：0 error / 0 warning，生产构建成功；
- 港股专项契约：4/4；
- 临时服务完整契约首轮 159/160，唯一失败为既有公式组合扫描的公开 K 线上游
  在响应头前断开；单项立即复测 1/1，完整重跑 160/160。

关键报告：

- `output/probes/api-contracts-hk-short-focused-20260808.json`
- `output/probes/api-contracts-formula-strategy-retry-20260808-hk-short.json`
- `output/probes/api-contracts-full-temp-20260808-hk-short-rerun.json`

正式发行包已部署到 `127.0.0.1:8765`，PID `40108`。健康检查为
`native_cpp=true`、`python_runtime=false`、580 个 JSN 文件、47,669 个证券键、
51,935 个证券、379 条公式和 1,159 个板块；主页命中新 Svelte 资产，腾讯控股
正式接口仍为 60 日、3/3 重叠日精确一致。正式服务完整契约一次通过 160/160：

- `output/probes/api-contracts-full-formal-20260808-hk-short.json`

发行版 `tdx-tool.exe` SHA-256：
`259327EC1281215031258C3AF0F57535EC87B2359BF88FACD613660EA39F88B4`。
