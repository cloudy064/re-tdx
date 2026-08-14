# 云配置语义变体与上市公司路演

## 目标与结论

此前已经确认 TQLEX 34/34、PBRPC 29/29 个唯一 ReqId 都被专用 C++ 功能使用，
但请求号计数无法证明同一请求号下不同参数、Entry 或模块分支也已闭合。本轮新增
纯 C++ `recon cloud-variants`，按传输、路由和解析后的规范请求 JSON 建立可重复
审计矩阵，并从矩阵中补齐了两个真正没有 ReqId 的上市公司路演路由。

对 `C:\new_tdx` 的真实结果为：

- 73 份 TQLEX 模板、58 份 PBRPC 模板，共 131 份；
- 归并为 109 个语义变体，22 份重复模板被合并，解析错误为 0；
- TQLEX 34/34 个 ReqId、PBRPC 29/29 个 ReqId 均由类型化 C++ 命令拥有；
- 两个无 ReqId 的 `CWSearch.tzx_rcache / ly:` 变体也由 `market roadshows` 拥有；
- `fixed=109`、`generic-only=0`、`fully_fixed=true`。

报告逐项保留传输、ReqId、Entry、模块、变体摘要、来源 XML、占位符、请求字段、
选择器、出现次数和固定命令。`fixed-command` 的含义是存在类型化 C++ 业务命令，
仅能被通用 `cloud tqlex/pbrpc` 发送不算闭合。`--gaps-only` 可专门输出未知分支，
一旦出现未知或解析失败，命令返回非零。真实证据为
`output/probes/cloud-variants-current.json`。

## 无 ReqId 路演路由

`T0002/cloud_cfg/gp_sc_szly.xml` 包含两个活动模板：

- `{"action":"get","key":"ly:1_zxly"}`：全市场路演主表；
- `{"action":"get","key":"ly:$$stock_market$$_$$stock_code$$"}`：单票历史。

它们没有 ReqId，只能按 `Entry= CWSearch.tzx_rcache` 与 KV key 选择。真实主表返回
2,503 条，市场分布为深市 661、沪市 1,641、北交所 185；类型以业绩说明会
2,404 条为主，另含上市仪式、再融资、可转债路演、重大事项说明会等。平安银行
`ly:0_000001` 当前返回 38 条，最近一条开始于 2026-03-23。

该 KV 响应的结果集字段是 `ColName+Content`，普通 TQLEX 通用解析器通常接收
`ColDes+Content`。新服务使用专用兼容解析器同时接受两者，输出统一证券身份、
日期时间、类型、摘要和 URL；支持市场/代码、文本、类型、状态、日期、offset/
limit 过滤，成功结果进进程内缓存。只对 429、502、503、504、业务错误 4 和
WinHTTP 传输失败做三次有限重试；有缓存时重试耗尽可返回 `stale-cache`，业务或
解析错误不会错误地使用陈旧缓存。

入口如下：

```powershell
tdx-tool market roadshows --root C:\new_tdx --market sz --code 000001 --limit 20
```

固定 API 为 `/api/v1/market/roadshows`。响应明确保留 `request_id=null`、真实
`entry+key` 选择依据、尝试次数和缓存状态。原始与原生证据分别为
`output/probes/roadshows-master-raw.json`、
`output/probes/roadshows-000001-raw.json` 和
`output/probes/roadshows-000001-native.json`。

## 验证与部署

- 新增 `tdx-roadshows-tests`、`tdx-cloud-variants-tests`，全量 CTest 44/44；
- 临时服务新端点契约 2/2；
- 正式服务完整契约 26/26、网络请求 26 次，证据为
  `output/probes/api-contracts-official-current.json`；
- 正式服务 PID `21272`，命令行
  `dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`；
- 部署 EXE SHA-256：
  `BC55F41531B3AAB27D8E0B8CCAC9B9BFB5889DABE85EFF741A9AA57C9F8F13DE`；
- 功能目录 91 项，运行时仍为纯 C++，`python_runtime=false`。

本轮未修改用户的 Svelte UI。发布 HTML、JS、CSS SHA-256 仍分别为
`0F1FBB7E19DCFAACE0A9C10A6741AFA1244A53E7977FF8B958D0D454573D5777`、
`B03C287DF8EF4C36EBC0E03456ACD934439D820E7BC606540FD80E8F66384411`、
`733085A8400D3DBF634DA5CD2ADAADD04F312BF75397F9102E303FE42665830A`。

## 后续高收益方向

1. ~~还原公开 L1 行情订阅的生命周期，在现有 `market watch` 轮询基础上评估
   单连接增量读取，并由本地服务通过 SSE 分发快照、五档和分时更新。~~ 已完成，
   详见 [公开 L1 持久会话与 SSE](2026-08-06-native-l1-sse-stream.md)；
2. ~~对 616 个 JSN 静态/动态模板做与云变体相同的“参数分支—类型化命令”覆盖
   审计，并提升首批关系链。~~ 已完成，详见
   [JSN 语义覆盖与个性数据全景](2026-08-06-native-jsn-variant-panorama.md)；
3. 用真实多节点 TPool 文件继续标定提示、声音、板块保存等宿主副作用的输入输出，
   保持只读模拟，不启动原 worker、不写回通达信池文件。
