# 公式扫描、回测与组合策略复权闭环

## 结论

本轮把上一阶段仅用于公式求值和审计的复权上下文，安全扩展到公式扫描、单票专家
回测、多公式组合扫描和固定股票池组合回测。所有入口默认仍为 `none`；调用方显式
传 `adjust=qfq|hfq` 后，才会在每只深沪京证券上执行与 `/api/v1/kline` 相同的
0x000F 本地公司行为复权，再建立公式上下文和计算信号。

因此以下内容现在使用同一口径：

- 送入公式的 OHLC；
- `TQFLAG/TQFLAG()` 的 0/1/2 模式；
- 条件选股命中；
- 专家系统下一根开盘成交价；
- 组合策略的共同时间轴、调仓价和逐股归因。

## 请求与错误边界

GET 扫描/回测直接读取查询字符串；POST 扫描、回测和两个组合策略入口从 JSON body
读取 `adjust/anchor_date`。复权模式在进入逐证券循环前统一规范化和校验：

- 空值、`raw`、`none` 规范化为 `none`，不增加除权请求；
- `front/back` 规范化为 `qfq/hfq`；
- `fixed_qfq/fixed_hfq` 保留固定锚点语义；
- 未知模式直接 HTTP 400，不会变成一组 `fetch_errors`；
- 只允许深沪京证券，公司行为复权不扩张到指数或扩展市场。

扫描和组合策略对每只证券独立计算除权因子。单票回测返回完整
`adjustment_mode/adjustment`；多证券结果返回公共 `adjustment_mode` 和
`adjustment_summary`，其中明确 `security_scope=per-security`，不会把第一只股票
的事件明细冒充为整个股票池的公共事件。命中行另保留自身的
`adjustment_mode`。

服务端不存在这三类公式结果缓存；请求 URL/body 本身包含复权模式。Svelte 的持续
扫描签名也纳入复权选择，切换模式会形成新的监控配置，不复用旧快照。后续阶段已在
不牺牲逐证券因子正确性的前提下加入 512 项严格有界共享输入缓存、singleflight 和
服务端首轮有界并发，并补齐全部公式 CLI 的复权参数，详见
[CLI 与缓存闭环](2026-08-09-native-formula-cli-adjustment-cache.md)。

## 解释器与结果元数据

`backtest_formula_document` 现从公式执行结果透传完整复权元数据；
`scan_formula_documents` 的命中行透传模式。组合策略求值也保留单证券元数据，组合
回测额外检查所有证券使用同一模式，混合口径直接拒绝。

单元测试使用显式 qfq 文档和 `TQFLAG/TQFLAG()` 覆盖扫描、专家回测、组合求值、
组合扫描和组合回测，防止以后只保留响应标签而丢失解释器上下文。

## Svelte 工作台

已有“不复权 / 前复权 / 后复权”选择现在同时进入：

- 自定义和内置专家回测；
- 自定义和内置条件扫描；
- 扫描会话的配置身份；
- 组合策略扫描和组合回测。

生产资源为 `index-CE9yr0eD.js`，1,198,299 字节，SHA-256
`7E079C2ACBBA53B89E05BDCB21C3E01F4587BD74493895915F5B75B4415D7545`；CSS 为
`index-BBbdHCcI.css`，SHA-256
`2C0AB14A91D23491D8B6566D6445AFD43601AB3232C03A296635E137C81CD418`。

## 契约与验证

原有四条真实 POST 契约升级为 qfq，并让源码直接引用 `TQFLAG`：

- `formula-inline-scan-post`；
- `formula-inline-backtest-post`；
- `formula-strategy-scan-post`；
- `formula-strategy-backtest-post`。

契约同时检查 qfq 模式、0x000F 来源、本地因子方法、完整股票池和下一共同开盘归因。
另直接验证 `adjust=mystery` 返回 HTTP 400。

验证结果：

- 公式引擎、组合策略和契约评估器专项测试通过；
- CTest 101/101；
- Svelte production build 通过；
- 临时服务专项 4/4；
- 临时服务 full API 214/214，共 215 次网络请求；
- 正式服务专项 4/4。

报告：

- `output/native-formula-workflows-adjust-targeted-api.json`；
- `output/native-formula-workflows-adjust-full-api.json`；
- `output/native-formula-workflows-adjust-formal-api.json`。

## 发布

正式 EXE 为 42,436,977 字节，build/dist SHA-256 均为
`ED2ECAA12C8AC3E48F2B1639EDEBBE83CF603870465FA76BE981FEC5344BA614`。正式服务
PID 38756，仅监听 `127.0.0.1:8765`；健康检查为 `native_cpp=true`、
`python_runtime=false`，临时 8875 已关闭，首页引用新 JS 资源。

发布时旧正式进程退出后文件句柄仍短暂占用，第一次覆盖被 Windows 明确拒绝；未把
该状态当成成功。确认正式进程消失并停止遗留临时服务后重新复制，强制比较
build/dist 哈希一致，再启动和执行正式专项。
