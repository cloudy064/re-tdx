# TdxW 运行拓扑只读快照

日期：2026-08-12

## 结论

统一工具新增纯 C++ `recon runtime-topology`。它把此前依赖 Process Explorer、
PowerShell 或一次性脚本的进程/模块/网络检查固化为可重复 JSON 快照，并可用
`--baseline` 比较前后变化。该命令不注入进程、不 attach、不读取进程内存、不发送
订阅，也不读取或输出会话凭据。

随后补充的 `--watch-seconds N --interval-ms N` 会定时采样同一目标。观察文档仅保存
首帧、末帧、每帧轻量汇总和真正变化时的完整 diff，不重复堆叠稳定全量快照；最短
间隔 50 ms，最长观察 3,600 秒，单次最多 20,001 帧。

命令现在还会默认从自动发现或显式 `--root` 的公开 `connect.cfg` 读取服务器目录，按
“远端地址+端口”精确注释连接。只嵌入命中的 section、索引、公开名称、primary 标志
和配置端点；`user.ini` 等私密状态完全不进入输出。可用 `--no-config-match` 关闭。

目标进程默认作为根递归纳入仍存活的全部子进程，使 CEF 或辅助进程持有的模块与 TCP
行不会被漏掉；`--no-children` 可退回只看直接命中的进程。每项明确给出
`direct_match/root_pid/tree_depth/descendant`，并区分父进程仍存在和父 PID 已退出。

默认行为只返回与目标可执行文件处于同一安装目录的模块，避免把上百个系统 DLL
混入主要证据；`--all-modules` 可显式展开全部模块，`--no-modules` 和
`--no-connections` 可缩小采样范围。`--pid` 用于同名多实例精确选择。

## 输出与比较语义

快照 schema 为 `tdx-runtime-topology-v1`，每个进程提供：

- PID、父 PID、线程数、镜像路径、启动 UTC 时间、WOW64 状态；
- 根 PID、树深、直接命中/递归子进程、父镜像与孤立父 PID 状态；
- 本地或全部模块的名称、路径、大小和基址；
- 所属 IPv4/IPv6 TCP 连接的本地/远端地址、端口和状态；
- 进程、模块、连接、监听端点与已建立远端端点汇总。

传入 `--baseline FILE` 时附加 `tdx-runtime-topology-diff-v1`，分别报告新增/消失的
进程、模块和连接。连接身份包含 PID、地址族、两端地址端口与 TCP 状态，因此连接
建立、关闭或状态变化都不会被静默吞掉。进程身份还包含启动时间，PID 被 Windows
复用时会正确报告旧实例消失、新实例出现。输出文件与基线相同会在写入前拒绝。

观察 schema 为 `tdx-runtime-topology-watch-v1`，提供样本数、稳定/变化次数、峰值
进程/模块/连接数、观察到的所有 PID、模块名和已建立远端端点。单帧或观察文档都能
作为下一次 `--baseline`；观察文档使用其末帧参与比较，便于连续链式采样。
观察汇总还记录子进程和父 PID 已退出进程的峰值。

配置关联不会做 DNS 查询或网络探测，也不会只按端口猜测。一个端点同时出现在多个
section 时保留所有匹配；没有命中的已建立远端端点进入
`unmatched_established_remote_endpoints`，不会被误标。显式 `--root` 必须确实包含
`T0002` 与 TdxW 可执行文件，不能静默回退到另一套安装目录。

## 当前真实样本

`output/runtime-topology-tdxw-20260812.json` 捕获到两个同名 32 位 TdxW 进程：

| PID | 父 PID | 线程 | 本地模块 | 全部模块 | 关键本地模块 | TCP |
|---:|---:|---:|---:|---:|---|---:|
| 16216 | 9756 | 1 | 21 | 149 | TaApi、TCalc、TdxAsioComm、tpbus | 0 |
| 29424 | 21428 | 11 | 26 | 110 | TaApi、TBigData、TCalc、TdxAsioComm、tpbus | 0 |

样本只说明采样瞬间没有属于这两个 PID 的 TCP 行，不推断客户端永远离线，也不根据
线程数猜测哪个实例承担业务。紧接着生成的
`output/runtime-topology-tdxw-diff-20260812.json` 变化为 0，证明稳定快照不会误报。
一秒、200 ms 间隔的
`output/runtime-topology-watch-tdxw-20260812.json` 共取得 6 帧、5 次比较，仍为
0 个变化事件；其作为下一次基线也得到零差异。该结果只证明观察器稳定，不代表
真实登录或切票过程没有网络事件。

当前 `C:\new_tdx\connect.cfg` 共解析 6 个分组、69 个公开端点：`dshost=16`、
`hfhost=2`、`hqhost=43`、`infohost2=8`，`infohost/wthost` 当前为空；按 TdxW 的
零基 `PrimaryHost=1` 语义，HQ primary 为第 2 项 `110.41.2.72:7709`。端点组现在
附带证据分级角色：HQ/DS 协议已验证，HF 确认为 L1HF/L2HF 共享高频池，INFO/INFO2
只确认加载器语义，WTHOST 保持 unresolved。严格根校验后的
`output/runtime-topology-watch-config-correlated-20260812.json` 取得 6 帧，空闲实例
仍无连接，因此实际匹配为 0；69 个目录项已附着，私密键命中为 0。

最终进程树样本 `output/runtime-topology-watch-tree-tdxw-20260812.json` 仍找到
16216/29424 两个直接命中的根、0 个存活子进程和 0 条 TCP；两者的父 PID 当前均已
退出，因此 `peak_orphaned_parent_count=2`。这不会被写成“原客户端从不创建 CEF”；
它只描述当前长期存活的两个实例。专项测试通过真实创建的短生命周期子进程证明递归
捕获、根 PID、深度、父镜像及 `--no-children` 均有效。

## 聚焦验证

- 增量构建：`tdx-runtime-topology-tests`、`tdx-tool`；
- 专项测试：合成模块/连接差异与当前测试进程的真实模块枚举均通过；
- 观察专项：合成变化压缩、稳定帧、峰值/端点并集以及同 PID 不同启动时间均通过；
- 配置关联专项：精确命中、未命中保留、多帧 section 并集、禁用开关及无私密内容
  均通过；当前安装 69 个公开端点与观察器目录数一致；
- 进程树专项：真实父进程启动短生命周期子进程，默认递归捕获；子项根 PID/深度/
  父镜像正确，`--no-children` 只保留直接目标；
- 空进程名返回 `running=false`，指定 PID 加 `--no-modules --no-connections` 可只取
  进程身份；互斥参数与基线覆盖均以退出码 2 拒绝；
- `--interval-ms` 脱离观察模式、超长观察和低于 50 ms 的间隔均以退出码 2 拒绝；
- `--root` 与 `--no-config-match` 互斥；不存在的显式根以退出码 2 拒绝，不再回退；
- 临时 18881 上 `health/features/openapi` 为 3/3，新命令在功能目录中只出现一次；
- `serve --self-test` 返回 `ok=true`、`feature_count=156`；正式 8765 的 PID 24096
  未重启、未替换；
- 未运行完整 CTest 或完整 API 契约集。

## 后续用法

在登录前、登录后、切换一只股票、退订和断线重连后分别保存快照，再链式使用上一帧
作为 `--baseline`，即可把服务器端点和模块加载变化缩小到具体动作窗口。该命令提供
的是安全的观察面；要解释应用层 Body 或收到的行情帧，仍需要合法业务事件与独立的
被动抓取证据。

对于容易瞬时出现的连接，可直接执行：

```powershell
tdx-tool recon runtime-topology --process TdxW.exe `
  --root C:\new_tdx --watch-seconds 30 --interval-ms 100 --no-modules `
  --output output/runtime-login-watch.json
```

`--no-modules` 能把高频观察聚焦在 TCP 表；需要观察按需加载 DLL 时再去掉该参数。
