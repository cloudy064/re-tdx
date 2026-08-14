# 分时资金与共享 JSN 上游容错

## 触发证据

用户此前在华海药业分时资金中遇到过
`PBRPC server rejected request with RpcID -1`。原 `market funds` 没有重试、
没有陈旧缓存回退，固定 API 又会把该上游故障落为 HTTP 400。

在新增契约巡检的第一次 13 项实测中，还捕获到一次主动基金 JSN 主表瞬时
失败：该轮 12/13，随后同一接口立即恢复，第二轮 13/13。这说明问题不只存在于
PBRPC，公开 7709 JSN 数据链也需要统一容错。旧报告未保留失败正文；巡检现已
补上最多 4 KiB 的 `response_excerpt`，以后同类现场不会丢失。

## 实现

### 分时资金 PBRPC

- `200340` 主表和 `200341` 行业明细分别最多尝试三次，间隔 250/500 ms；
- 识别 `RpcID -1`、HTTP 429/502/503/504、业务错误 4 和 WinHTTP 收发失败；
- 已有成功缓存时返回原数据，`availability=stale-cache`，同时报告主表/明细
  缓存年龄、陈旧状态和上游原因；
- 冷缓存重试耗尽时固定 API 返回
  `503 + upstream_unavailable + retryable=true`，不再伪装成参数错误；
- 测试使用内部 C++ fetcher 注入点稳定复现故障；该入口不暴露给 HTTP、不能
  修改生产上游地址。

### 共享 JSN 与主动基金

- `fetch_jsn_resource(s)_rows` 和单资源 transfer 均改为每轮建立新 7709 连接，
  最多尝试三次；零长度的真实缺失资源不会被误重试；
- 成功来源增加 `attempts`，可观察透明恢复是否发生；
- 所有复用该读取器的公开 JSN 业务自动受益，包括主动基金、机构调研、回购、
  股权、评级、解禁和大宗交易等；
- `ActiveFundService` 额外保留主表、逐股基金明细两级陈旧缓存。刷新失败但有
  缓存时继续返回数据，并报告 `master/detail_stale`、年龄和错误；冷缓存网络
  失败按 HTTP 503 返回。

## 验收

- 分时资金确定性测试：先成功缓存平安银行，再让主表和明细各连续返回三次
  `RpcID -1`，最终仍得到 `found=true`、`stale-cache` 和两侧错误原因；
- 冷缓存同一故障精确尝试三次后失败；
- JSN 重试测试覆盖第三次恢复、三次耗尽和最终错误保持；
- 主动基金测试覆盖主表与详情同时失败后的双层陈旧缓存；
- 临时服务连续五次强制刷新主动基金均为 HTTP 200、`live`、`attempts=1`；
- 临时和正式服务 `recon api-contracts --profile full` 均为 13/13；
- 全量原生 CTest 为 42/42。

正式 `8765` 服务当前 PID 为 `16112`，命令行为
`dist/tdx-tool/bin/tdx-tool.exe serve --root C:\new_tdx --port 8765`，EXE
SHA-256 为
`2625D88C6014C7928B3FC4EE2BE14A465B810F7F27B536C936769D29D52532D4`。
健康状态为 `ok=true`、`native_cpp=true`、`python_runtime=false`，功能目录仍为
89 项。正式巡检证据为
`output/probes/api-contracts-official-current.json`。

Svelte HTML/JS/CSS SHA-256 分别仍为
`0F1FBB7E19DCFAACE0A9C10A6741AFA1244A53E7977FF8B958D0D454573D5777`、
`B03C287DF8EF4C36EBC0E03456ACD934439D820E7BC606540FD80E8F66384411`、
`733085A8400D3DBF634DA5CD2ADAADD04F312BF75397F9102E303FE42665830A`，
本轮未修改或覆盖用户 UI。

## 后续建议

下一批收益最高的工作不再是继续堆积页面，而是把相同的
`availability/cache/upstream_error` 契约扩展到高扇出的 JSN 固定服务，并用
故障矩阵检查主表、动态详情和批量资源在冷/热缓存下的 HTTP 语义。功能探索则
优先检查尚未业务化的动态 JSN 关系和同 ReqId 的真实模板参数变体；当前唯一
ReqId 已有专用 C++ 源，不能再把“请求号已覆盖”误写成“全部参数分支已验收”。

代码级复核共找到 24 个业务源、42 个共享 JSN 调用点。共享三次重连已经覆盖
全部调用点，但除主动基金外，其余服务尚未统一暴露陈旧缓存状态。下一轮建议按
以下优先级推进：

1. 个股工作台高频链：机构调研、股权/回购、一致预期、行业画像；
2. 榜单主从链：机构龙虎、评级、外资预警、解禁、大宗交易；
3. 把上述真实样本加入 `recon api-contracts`，分别验证冷缓存 503、热缓存
   `stale-cache` 和动态详情局部失败；
4. 对 TQLEX 73 个模板/34 个唯一 ReqId、PBRPC 58 个模板/29 个唯一 ReqId
   建立模板变体矩阵，优先找同请求号但占位参数、Entry 或模块不同的分支；
5. 非 L2 的下一项协议收益点是把现有 `market watch` 轮询升级为服务端 L1
   推送/SSE，并继续闭合 `FastHQ.Subscribe` 的公开行情接收链；它与 L2 授权
   前置条件无关。

## 状态更新

上述第 1、3 项已在同日后续完成：共享 JSN 已加入有界陈旧缓存，高频个股服务
已统一暴露上游健康，契约巡检扩展为 18 项。最新实现、部署和下一批优先级见
[共享 JSN 陈旧缓存与个股接口健康契约](2026-08-06-native-jsn-shared-stale-cache.md)。
