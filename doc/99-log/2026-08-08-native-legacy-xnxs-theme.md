# 2026-08-08 XNXS 遗留客户端主题收敛

本轮将客户端 `XNXS.sp`、`func_xnxs101_1` 和动态 `xnxs/<分组ID>` 从隔离探测资源
提升为纯 C++ `market thematic-opportunities` 的 `legacy-client-theme` 类型，网页
“主题机会”同步支持筛选、查看成员和逐股详情。

## 真实语义与冲突处理

- `XNXS.sp` 和 CFG 声明页面名为“虚拟现实”；
- 当前主表唯一分组却是 `299 快中子反应堆 / 内容应用`，成员为 `SH600328`、
  `SH601106`、`SH601727`；
- `xnxs/299.jsn` 的 3 条详情均含近 3/5/20 日与近三月参考价、看点和投资逻辑；
- 逐股名称通过本地证券目录解析为中盐化工、中国一重和上海电气。

页面声明与实际数据存在明确冲突。API 不猜测它们属于“虚拟现实”，而是以数据行
名称为分组身份，保留 `page_declared_name=虚拟现实`，并返回
`semantic_mismatch=true` 和说明。这样既能查询真实资源，也不会制造错误板块关系。

加入该来源后，主题机会目录当前为 6 个细分行业组、8 个核心区域组和 1 个遗留
客户端主题，共 15 组、732 条成员关系、559 只去重证券。主表和动态详情均保留
来源、变体和原始行。

## 验证

- CTest：98/98；
- XNXS 与主题机会专项 API 契约：5/5；
- 临时服务完整 API 契约：159/159；
- Svelte check：0 error / 0 warning，生产构建成功；
- JSN 审计：567/622 类型化、580 个正式下载文件、516 个已下载模板，
  `generic_downloaded_template_count=0`、解析错误 0。

关键证据：

- `output/tdx-thematic-legacy-xnxs299-native.json`
- `output/probes/jsn-xnxs299-download-20260808.json`
- `output/tdx-jsn-variants.json`
- `output/probes/api-contracts-xnxs-focused-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-xnxs.json`

正式发行包已部署到 `127.0.0.1:8765`，PID `44656`。健康检查为
`native_cpp=true`、`python_runtime=false`、580 个 JSN 文件、47,669 个证券键、
51,935 个证券、379 条公式和 1,159 个板块；主页已命中新 Svelte 资产。正式服务
完整契约一次通过 159/159：

- `output/probes/api-contracts-full-formal-20260808-xnxs.json`

发行版 `tdx-tool.exe` SHA-256：
`70869D279EB3EA3E4F0434F7CA35A765087A872A85FE9E2C2681E4484EC93C58`。
