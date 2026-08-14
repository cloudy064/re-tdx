# TPool 多节点流程状态机与原子状态恢复

## 本轮结论

`TPool.dll` 的流程计时分支已经从“字段名推断”推进到运行代码证据。纯 C++
工具现可在不加载 `TPool.dll`、不启动原 worker、也不改写通达信股票池 XML
的前提下，持续推进多节点或带环 flow，并把状态原子保存到独立 JSON 文件。

```powershell
tdx-tool pool watch `
  --input C:\new_tdx\T0002\tpool\example.xml `
  --root C:\new_tdx `
  --interval-seconds 60 `
  --state-file output\example-tpool-state.json `
  --output output\example-tpool-alerts.jsonl
```

`--state-file` 可在进程重启后恢复 cell 成员、每条 flow 的首次/末次运行 tick、
运行次数、完成状态以及最近 512 个状态事件。写入使用临时文件加原子替换；工具
拒绝让状态文件覆盖输入 XML，也拒绝让状态文件与 JSONL 输出共用同一路径。

## DLL 证据

XML parser `TPool.dll:sub_1001E610` 把每条 `<flow>` 解码为 55 字节记录：

| 偏移 | XML/运行字段 | 已确认用途 |
|---:|---|---|
| `+4/+8` | `startid/endid` | 起止 cell |
| `+12` | `clr` | 流程线颜色，不是清空开关 |
| `+20` | `tran` | flow 启用标志；为 0 时跳过并结束该 flow |
| `+21` | `emptyps` | 运行前清空目标条件节点的旧中间匹配集合 |
| `+22` | `starttype` | 8 类首次启动条件 |
| `+23/+27` | `starttime/starttimetype` | 首次启动偏移及秒/分/小时单位 |
| `+28` | `starttimehms` | 定时启动的 `HHMMSS` |
| `+32` | `cxtype` | 3 类循环方式 |
| `+33/+37` | `cxtime/cxtimetype` | 循环窗口及单位 |
| `+38` | `jgtime` | 两次执行间隔，单位固定为秒且最小按 1 秒处理 |
| `+42` | 运行字段 | 已完成标志 |
| `+43/+47` | 运行字段 | 首次运行 tick、末次运行 tick |
| `+51` | 运行字段 | 运行次数 |

`TimerFunc` 只在池状态为运行时每秒递增池 tick。`sub_1001E610` 读取这个计数
判断首次阈值、重复间隔和窗口过期，因此 `jgtime` 不是毫秒，也不是行情 bar
数量。

首次启动条件已经逐项对应到分支：

| `starttype` | 语义 |
|---:|---|
| `0` | 立即 |
| `1` | 池启动 tick 达到指定偏移 |
| `2` | 开盘 `09:30` 前指定偏移 |
| `3` | 开盘 `09:30` 后指定偏移 |
| `4` | 收盘 `15:00` 前指定偏移 |
| `5` | 收盘 `15:00` 后指定偏移 |
| `6` | 周一至周五的指定时刻 |
| `7` | 每日指定时刻 |

循环条件也已闭合：`cxtype=0` 按 `jgtime` 持续重复；`1` 只在 `cxtime`
窗口内按 `jgtime` 重复，错过首次窗口或窗口结束后永久完成；`2` 首次运行后
完成。时间单位 `0/1/2` 分别为秒、分、小时。

## 原生状态模型

`advance_tpool_flow_state_document` 是确定性的纯状态转换：输入当前规则求值、
上一份状态和显式本地时钟，输出 `tdx-tpool-flow-state-v1`。它有以下约束：

- 要求 cell ID 唯一、flow 引用有效；环允许存在；
- 每次按 XML flow 顺序只推进一轮，因此前一条边的新成员可被后一条边在同一轮使用；
- 股票加入目标 cell，起始 cell 成员保留；
- 每个 cell 的规则仍采用“全部命中”组合；无规则 cell 直接放行；
- 当前求值缺失的历史成员不会被误判命中，会进入 `unavailable_securities`；
- `emptyps` 的原版中间集合不会跨原生求值保存，因为每轮规则集合都从当前行情
  完整重建；事件会明确记录请求和这种处理方式；
- XML 摘要或来源变化会丢弃不兼容旧状态并产生 `state-reset` 事件；系统时钟回拨
  不会倒退池 tick，会产生 `clock-rewind` 事件。

输出明确保留 `original_worker_equivalent=false`。声音、弹窗、保存板块、原版
历史文件、宿主回调和 XML 写回均未模拟，也不会通过这项能力触发。

## 验证

新增确定性测试覆盖：

- `A → B → C` 在同一轮按 XML 顺序传播；
- `C → A` 带环边按池 tick 延迟启动；
- one-shot 完成、5 秒重复间隔、错过首次循环窗口；
- cell 成员与运行次数跨状态恢复；
- pool 来源变化安全重置；
- flow-state 成员进入 watch 告警 diff。

全量 CTest 为 `26/26`。命令级真实 fixture 连续运行两次后，状态从 iteration
`1` 恢复到 `2`，`state_reset=false`，两次均只使用 C++ 可执行文件。

安装目录当前没有可复现的用户 `tpool\*.xml`，只有云配置 XML；因此本轮没有
声称复刻原版的提示、声音、板块保存或历史文件副作用。后续如取得真实多节点池，
最高收益验证是把原版 UI 中的 cell 数量与本状态文件逐轮对照，而不是直接让工具
写回用户池。
