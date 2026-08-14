# 条件公式策略池与持续告警

## 目标

把一次性的条件公式扫描继续闭环为可长期运行的策略池：周期重算、成员进入/
退出、信号更新、断点恢复和可选通达信自选板块文件。保持纯 C++、不依赖
Python，不涉及 L2；网页提供当前会话内的同语义监控。

## 原生命令

新增统一子命令 `formulas watch`，完整复用 `formulas scan` 的内置/自定义源码、
证券范围、参数、周期、历史页数、缓存和严格财务时点选项：

```powershell
tdx-tool formulas watch `
  --source-file output\probes\custom-selection.tdx `
  --formula WATCH_SELECT --param N=5 `
  --securities sz000001,sh600000 --period day --lookback 10 `
  --interval-seconds 60 `
  --state-file output\formula-watch-state.json `
  --block-output output\formula-watch.blk `
  --output output\formula-watch.jsonl
```

每轮结果按 `security_id` 差分：

- `entered`：新进入策略池；
- `exited`：离开策略池；
- `updated`：证券仍在池内，但触发日期、时间或信号载荷变化；
- `snapshot/resume/heartbeat`：首次快照、同配置断点恢复和无变化心跳；
- `degraded/error`：不完整扫描或整轮错误。

状态文件使用 `tdx-formula-watch-state-v1`，仅保存公式源码 MD5、配置摘要和活跃
成员，不复制公式正文。相同配置重启会继续比较；源码内容或扫描配置变化会明确
重建快照。

## 安全与写入边界

- 每个周期在上一请求完成后才计时，不会重叠扫描；
- 任一证券取数或公式执行失败时，本轮只发 `degraded`，不更新状态和 `.blk`，
  因而不会把上游故障误报为成员退出；
- 状态、JSONL、`.blk` 和输入/源码/公式库路径必须互不覆盖；
- `.blk` 只有显式提供 `--block-output` 才写入，绝不默认修改真实通达信目录；
- `.blk` 使用 `0/1/2 + 六位代码` 的 `blocknew` 格式并原子替换；
- 未来函数与数值语义不安全的公式继续无法进入扫描和策略池。

## 网页

Svelte 公式工作台的条件扫描区域新增“会话监控”。用户可以设置秒级间隔，页面
保留最近 200 条快照、进入、退出、更新、降级和错误事件。扫描条件或源码变化时
自动重建快照；不完整响应保留上一轮成员。浏览器关闭后会话停止，后台断点续跑
和 `.blk` 输出继续由原生命令承担。

## 验证

- 真实行情以平安银行、浦发银行和 `RESULT:CLOSE>MA(CLOSE,N)` 运行：
  `lookback=10` 建立 2 个活跃成员；
- 修改配置后得到 `state_reset=true` 和 2 个 `entered`；相同配置重启得到
  `type=resume`、`previous_count=2`、变化数 0；
- `.blk` 精确输出 `0000001` 与 `1600000`；
- 加入无 K 线的 `sz999999` 后得到 `degraded`；状态文件和 `.blk` 前后
  SHA-256 均不变；
- C++ 完整测试 65/65；
- Svelte 0 错误、0 警告，生产构建成功；
- 功能目录由 112 项增至 113 项并包含 `formulas watch`；
- 部署态接口契约 95/95：
  `output/probes/api-contract-full-20260807-formula-watch.json`。

验证产物包括：

- `output/probes/formula-watch-events.jsonl`；
- `output/probes/formula-watch-reset.jsonl`；
- `output/probes/formula-watch-resume.jsonl`；
- `output/probes/formula-watch-degraded.jsonl`；
- `output/probes/formula-watch-state.json`；
- `output/probes/formula-watch.blk`。

部署服务为 `http://127.0.0.1:8765`，PID `24656`；发行包 SHA-256 为
`D45DC6C45A98CBAD10587FEA38AE33065A3F2E29B2F72FAF46A5101D97EE891F`。

