# 公式 CLI 复权与有界共享输入缓存闭环

## 结果

公式复权不再只存在于 HTTP/Svelte 路径。纯 C++ 统一命令的执行、全库审计、扫描、
持续监控、单票专家回测和多公式策略现在都在建立公式上下文之前应用同一条 0x000F
本地公司行为因子链，并把规范化后的 `adjustment_mode` 交给 `TQFLAG/TQFLAG()`。

批量 qfq/hfq 的性能边界也已固定：进程级缓存只保存生成因子所需的原始日线和
0x000F 股本变更输入，不缓存目标周期价格或公式结果；服务端首次抓取使用有上限的
worker，重复请求复用低频输入。运行时仍是纯 C++，没有 Python 转发。

## CLI 一致性

以下六个入口都接受：

- `--adjust none|qfq|hfq|fixed_qfq|fixed_hfq`；
- `--anchor-date DATE`；
- `--adjust-cache-ttl-seconds 0..86400`，默认 900 秒；
- `--refresh-adjustment`。

入口包括 `formulas evaluate/audit/scan/watch/strategy/backtest`。扫描、监控和策略的
既有 `--workers` 继续控制批量行情装载；输入 JSON 已经带有同模式复权元数据时按
幂等文档处理，不会重复乘因子。若文档已使用另一模式，或固定复权锚点不同，则明确
拒绝。

`formulas watch` 每轮仍刷新目标周期行情，但不会把高频行情刷新误当成低频公司行为
刷新；因此实测第一轮两只证券为 2 次 miss，第二轮为 2 次 hit。只有显式
`--refresh` 或 `--refresh-adjustment` 才刷新复权输入。

扫描的原始磁盘缓存名同时从仅含周期，修正为包含 `period/pages/page_size`，避免先前
较短历史页冒充较长请求；在线行情请求也会传入显式 `--root`，与其它入口的节点选择
一致。

## 有界共享缓存

`adjust_security_kline_document` 是 CLI 和服务端共用的唯一装载入口。共享缓存：

- 键包含市场、代码、TDX 根目录和显式主站集合；
- 每个证券并行获取 20 页日线与 0x000F 股本变更；
- 相同键的同时 miss 通过 `shared_future` 合并为一个生产者；
- TTL 默认 900 秒，可关闭或显式刷新；
- LRU 上限严格为 512 项；若 512 项全部仍在生产，额外键以
  `capacity-bypass` 完成但不进入缓存，进程内条目数不会临时越界；
- 旧刷新完成晚于新刷新时，generation 校验阻止旧结果覆盖新值；
- 响应公开 hit/miss/coalesced/refresh/failure/eviction/capacity-bypass 计数。

多证券 `adjustment_summary` 由共享实现生成，除逐证券状态计数外还包含
`tdx-kline-adjustment-input-cache-v1` 全局快照。它不把某一只股票的事件列表冒充为
整个股票池的公共事件。

## 服务端首轮并发

`/api/v1/formulas/scan`、`/api/v1/formulas/strategy/scan` 和
`/api/v1/formulas/strategy/backtest` 新增 `workers=1..16`，默认 4。线程只并发
执行目标 K 线下载和逐证券复权；本地证券元数据、板块数据、财务上下文及策略上下文
仍按原股票顺序串行构建。因此共享 `BlockData` 不产生竞争，结果顺序、错误行与固定
股票池语义不变。响应的 `fetch_workers` 返回本次实际 worker 数。

POST body 现也完整合并 `workers`、`adjust_cache_ttl_seconds` 和
`adjust_refresh`；此前这些低层参数只有查询字符串可用。Svelte 工作台为条件扫描和
组合策略显式发送 4 个 worker。

## 实测证据

六种 CLI 工作流均以真实 qfq 行情通过：

- `output/native-cli-adjust-evaluate.json`；
- `output/native-cli-adjust-audit.json`（379/379，错误 0）；
- `output/native-cli-adjust-scan.json`；
- `output/native-cli-adjust-backtest.json`；
- `output/native-cli-adjust-strategy.json`；
- `output/native-cli-adjust-strategy-backtest.json`；
- `output/native-cli-adjust-watch.jsonl`（第一轮 miss、第二轮 hit）。

另以 `sz000002,sh600519`、两 worker 做同进程冷/热对照：强制 refresh 两项均成功，
端到端 1,204 ms；紧随其后的请求命中两项缓存，端到端 118 ms。该数字只是一轮网络
样本，不作为稳定性能承诺；结构化结果保存在
`output/native-formula-cache-performance.json`。

接口契约 `formula-inline-scan-post`、`formula-strategy-scan-post` 和
`formula-strategy-backtest-post` 现在同时检查：实际两个 worker、完整股票池、qfq
来源/方法、两份输入状态总数，以及 512 项严格缓存上限。最终报告：

- `output/native-formula-cli-cache-targeted-api.json`；
- `output/native-formula-cli-cache-full-api.json`；
- `output/native-formula-cli-cache-formal-api.json`。

## 验证与发布

- C++ 全构建通过；
- CTest 101/101；
- Svelte production build 通过；
- 最终临时服务新增契约 3/3；
- 临时服务首次 full 为 213/214，唯一失败是 `jsn-discovery-live` 的 WinHTTP
  12002 接收超时；该项随后 756 ms 单独通过，使用 60 秒单请求上限的完整重跑为
  214/214、215 次网络请求；
- 正式服务公式专项 4/4。

正式 EXE 为 42,777,890 字节，build/dist SHA-256 均为
`6E87A1656F30D04A4865E0E9F94598446FBF36725F8FEBFACE985DB80520FD70`；旧版本保存在
`tdx-tool.exe.previous`，SHA-256 为
`ED2ECAA12C8AC3E48F2B1639EDEBBE83CF603870465FA76BE981FEC5344BA614`。正式服务 PID
25492，仅监听 `127.0.0.1:8765`，健康检查保持 `native_cpp=true`、
`python_runtime=false`。首页引用 `index-ByEK1oAy.js`（1,198,329 字节，SHA-256
`5511B11282456BBDA8F7040D81676598B67E3549B1450588940365147EC2AFD8`），资源返回
HTTP 200；临时 8875 已关闭。

旧正式进程退出后 EXE 文件句柄仍短暂占用，首次覆盖被 Windows 明确拒绝，哈希校验
阻止了误启动；等待句柄释放后重试，build/dist 哈希一致才启动正式服务。

## 下一批非 L2 解释器方向

重新生成的覆盖报告仍为 379/379 语法支持、379/379 数值信号安全、降级数值输出
为 0。六条未直接执行的系统公式全部只缺券商私有 `SIGNALS_QS`，因此当前内置公式
不存在新的非 L2 阻塞点。

将 390 条 TCalc 静态注册与现有函数、自动符号和外部上下文符号求并集后，有 262 个
注册名已被识别，剩余 128 个；其中大量属于 L2、交易账户状态或插件回调。这里特别
更正分类：`CAPITAL/TOTALCAPITAL` 和 `ADVANCE/DECLINE` 虽不在 automatic-symbol
计数中，但早已作为外部上下文符号由公开财务或指数 K 线自动绑定，不是缺口。

排除这些以后，本记录当时选择的最高收益项是期货 `MULTIPLIER`。这里的 262/128
以及 `output/native-formula-registry-next-audit.json` 是实现前快照；后续已用原 DLL
固定向量和 7727 合约元数据完成闭环，识别并集增至 263，剩余 127，详见
[MULTIPLIER 专项记录](2026-08-09-native-formula-multiplier.md)。下一项转为共享
K 线辅助字段 `ZSTJJ/QHJSJ`，其后是更多行业/板块文字入口和五个单点专业数据入口。仅展示用途的
`DRAWSL/DRAWBMP/DRAWGBK/DRAWRECTREL` 排在真实公式样本之后。结构化队列见
`output/native-formula-registry-next-audit.json`，离线探针与证据为
`output/ida_probe_tcalc_capital_multiplier.py` 和
`output/ida-tcalc-capital-multiplier.log`。
