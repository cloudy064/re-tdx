# 2026-08-08 可转债客户端投影收敛

本轮没有把 `func_kzz102`、`gxjty_zq_dfkzz102`、`gxjty_zq_xkzz102` 和
`func_kzz103` 建成四套重复业务，而是恢复为 `market convertible-bonds` 现有
生命周期的可审计投影。

## 真实数据结论

- `view=pending` 仍以 `dfkzz201_1` 为主表，当前 156 条；`func_kzz102_1`
  共有 138 条，与主表公共 137 条，主表独有 19 条、投影独有 1 条；
  `gxjty_zq_dfkzz102_1` 共有 156 条，与主表公共 155 条，双方各独有 1 条。
- `view=subscriptions` 仍以 `func_kkzss101_1` 的 323 条发行申购为主表。
  `gxjty_zq_xkzz102_1` 的 20 条“新可转债”全部按申购代码精确命中，但有 3 条
  发行规模不同。API 保留投影值、主表值、差额和冲突标志，不按正股覆盖或拼接。
- `view=listed` 的 `func_kzz103_1` 与 `kjhz_kjhzsy201_1` 均为 2 只可交换债且
  证券集完全一致。前者只作为第二投影补充 `DQSHJ`、`LLZH`、`HSCFBL` 和
  `QSCFBL`；更完整的收益表继续保持主来源。

Svelte 数据中心增加待发投影覆盖标签、“新可转债”投影表及冲突提示；可交换债
详情增加第二投影验证和未付息合计。所有响应继续保留原始行与来源健康。

## 验证

- CTest：98/98；
- 可转债专项 API 契约：3/3；
- 临时服务完整 API 契约：158/158；
- Svelte check：0 error / 0 warning，生产构建成功；
- JSN 审计：565/621 类型化、578 个正式下载文件、514 个已下载模板，
  `generic_downloaded_template_count=0`、解析错误 0。

关键证据：

- `output/tdx-convertible-pending-projections-native.json`
- `output/tdx-convertible-new-bond-projection-native.json`
- `output/tdx-exchangeable-projection-native.json`
- `output/tdx-jsn-variants.json`
- `output/probes/api-contracts-kzz-projections-focused-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-kzz-projections.json`

正式发行包已部署到 `127.0.0.1:8765`，PID `35988`。健康检查为
`native_cpp=true`、`python_runtime=false`、578 个 JSN 文件、47,669 个证券键、
51,935 个证券、379 条公式和 1,159 个板块；主页已命中新 Svelte 资产。
正式服务首轮仅因国企改革公开 L1 瞬时不可用得到 157/158，该失败项立即复测
1/1 通过，完整重跑为 158/158：

- `output/probes/api-contract-state-owned-retry-20260808-kzz-projections.json`
- `output/probes/api-contracts-full-formal-20260808-kzz-projections-rerun.json`

发行版 `tdx-tool.exe` SHA-256：
`8E3F7D034CF8AD8BAB2D6A18C227D3C1EF40988E0CA42BD03AEEED43D35CF6B0`。

## 下一候选审计

隔离区最后一个小样本 `func_xnxs101_1` 已继续展开。`XNXS.sp` 和 CFG 声明页面
为“虚拟现实”，但当前主表唯一分组实际是 `299 快中子反应堆 / 内容应用`，成员为
`SH600328`、`SH601106`、`SH601727`。动态资源 `xnxs/299.jsn` 已验证非空，3 条
记录均含近 3/5/20 日和近三月参考价、看点及完整投资逻辑。

这不是空数据，但页面声明与当前业务内容冲突；若直接挂到“虚拟现实”会制造错误
语义。因此主表和动态详情暂留在
`output/tdx-jsn-probes/20260808-next-small/`，未混入正式镜像。后续适合以明确的
`legacy-client-theme` 来源并入主题机会对账，或确认客户端是否复用了该页面后再
正式类型化。下载证据为 `output/probes/jsn-xnxs299-download-20260808.json`。
