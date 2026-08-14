# UserComm 网络传输层

> 证据日期：2026-07-30。分析对象是当前安装中的根目录
> `TdxAsioComm.dll` 与 `SEPlugins\TAsioComm.dll`，并用 `TdxW.exe.i64`
> 反查调用点。本文只描述传输层，不把它误写成行情帧解析器。

## 模块身份

| 项目 | `TdxAsioComm.dll` | `SEPlugins\TAsioComm.dll` |
|---|---:|---:|
| 大小 | 893,792 bytes | 790,416 bytes |
| SHA-256 | `1df4d21ba80194f2bb9dd4824ae5f4e9e603afe254cc4b69ba40e82bf669317a` | `d3d4fa6b2281fe5e63dfbe78fa7784b80431a0fe96fc2500b4757fd70ef57cff` |
| 文件版本 | 1.4.30.1 | 1.1.10.1 |
| IDA 函数数 | 4,075 | 3,658 |
| 当前主进程加载 | 是 | 否 |
| 工厂创建对象大小 | 224 (`0xE0`) bytes | 117 (`0x75`) bytes |
| `CUserComm` 虚表 | 15 个业务槽位 | 15 个业务槽位 |

两者不是简单改名副本：对象布局、收发函数数量、代码地址和工厂实现均
不同。当前主程序静态导入根目录版本的 C++ 装饰名工厂，不能用插件版本
替代分析。

## 已确认的底层实现

两个 DLL 都：

- 静态链接/包含 Boost.Asio 风格实现，存在 `asio.system error`、
  `asio.misc error`、`socket_select_interrupter` 等字符串；
- 使用 Windows IOCP：
  `CreateIoCompletionPort`、`GetQueuedCompletionStatus`、
  `PostQueuedCompletionStatus`；
- 使用 overlapped Winsock：
  `WSASocketW`、`WSARecv`、`WSASend`、`WSAIoctl`；
- 用 `getaddrinfo` / `freeaddrinfo` 解析节点；
- 暴露 `MakeUserCommModule` / `DelUserCommModule` 工厂；
- 通过 RTTI 确认实现类 `CUserComm` 继承接口 `VUserComm`。

这证明它们是通用 TCP 传输封装。库内没有发现行情帧类型、7709、登录或
心跳等业务字符串，因此协议组帧/解码大概率在调用层，而不是这两个 DLL
内部。

## 主版本关键地址

### 工厂与类型

| 地址 | 含义 |
|---:|---|
| `0x10002FB0` | `MakeUserCommModule`，分配并清零 `0xE0` 字节后构造 `CUserComm` |
| `0x10002E40` | `DelUserCommModule`，RTTI cast 到 `CUserComm` 后析构 |
| `0x1009BB68` | `VUserComm` 纯虚接口表 |
| `0x1009C248` | `CUserComm` 实现虚表 |
| `0x1000FCD0` | `CUserComm` 构造路径（由工厂调用） |

### IOCP / socket 锚点

| 操作 | 直接调用函数 |
|---|---|
| `CreateIoCompletionPort` | `0x10010990`, `0x1001E310` |
| `GetQueuedCompletionStatus` | `0x1001A5B0`, `0x10020B70` |
| `WSARecv` | `0x1001E250`, `0x10021490` |
| `WSASend` | `0x100177D0`, `0x100204F0`, `0x10021590` |
| `WSASocketW` | `0x1001CD60`, `0x10020E70` |
| `getaddrinfo` | `0x1001F650` |

## `CUserComm` 虚表（主版本）

以下语义由伪代码和 `TdxW.exe` 的实际间接调用共同推断。标为“已确认”的
项目已有调用上下文；其余名称仍应在 IDA 中保守标注。

| 槽位 / 偏移 | 实现地址 | 当前语义 | 证据等级 |
|---:|---:|---|---|
| 0 / `+0x00` | `0x10015540` | 析构/释放 | 已确认 |
| 1 / `+0x04` | `0x10014370` | 带超时的异步连接候选 | 推断 |
| 2 / `+0x08` | `0x100147F0` | 配置并发起异步连接 | 已确认 |
| 3 / `+0x0C` | `0x10014D20` | 队列发送/异步写，支持回调与超时 | 结构已确认 |
| 4 / `+0x10` | `0x10014980` | 另一种异步 IO 操作 | 待命名 |
| 5 / `+0x14` | `0x10014B00` | 另一种异步 IO 操作 | 待命名 |
| 6 / `+0x18` | `0x10014BA0` | 运行/停止 IO worker；`-1` shutdown，`0` 当前线程运行，正数启动线程 | 已确认 |
| 7 / `+0x1C` | `0x1001F890` | 读取/复位内部完成状态 | 结构已确认 |
| 8 / `+0x20` | `0x10015030` | 带超时的同步连接 | 已确认 |
| 9 / `+0x24` | `0x10014E90` | 不带超时的连接 | 已确认 |
| 10 / `+0x28` | `0x100227E0` | 写入（伪代码错误标签为 `"write"`） | 结构已确认 |
| 11 / `+0x2C` | `0x1001DC50` | 读变体，可带超时 | 结构已确认 |
| 12 / `+0x30` | `0x1001DB90` | 读变体，无超时参数 | 结构已确认 |
| 13 / `+0x34` | `0x1001DE10` | 同步读，调用方传入 200 ms 超时 | 已确认 |
| 14 / `+0x38` | `0x1001DD80` | 同步读，无超时参数 | 已确认 |

槽位 1/2 和 8/9 都先把布尔参数映射为地址族常量 `23` / `2`
（Windows 的 IPv6/IPv4 值），再处理 host/service。

## `TdxW.exe` 调用路径

### 同步建立连接

`sub_413B20(node_name, port)`：

1. 必要时调用 `MakeUserCommModule`；
2. 把端口格式化为十进制字符串；
3. 解析节点名；
4. 配置超时大于 0 时调用虚表槽 8，否则调用槽 9；
5. 成功条件是返回值为 1 且内部错误标志未置位。

这是当前从上层节点配置到传输对象的最短已确认路径。

### 工作线程与收发循环

`sub_414240`：

1. 创建/复用 `CUserComm`；
2. 调用槽 2 发起异步连接；
3. 调用槽 6 运行 IO worker；
4. 有待发送队列时调用槽 3；
5. 用槽 13（200 ms）或槽 14 读取数据；
6. 把 `(buffer, length, context)` 交给上层回调；
7. 退出时以槽 6 的 `-1` 模式 shutdown，再销毁对象。

相关清理函数：

- `sub_412FC0`：等待工作线程，必要时 shutdown/销毁；
- `sub_413510`：带原子状态保护的销毁。

## 当前边界

- 已确认连接与收发的传输调用，但尚未定位 7709 数据在上层回调中的第一
  个帧边界判断。
- `TdxAsioComm.dll` 没有业务协议证据，不应继续在这里搜索行情字段 ID。
- `TDataParse.dll` 已确认用于本地/缓存记录解析，不是该 recv 回调的直接
  下一跳。下一跳应继续从 `sub_414240` 的上层回调反查，并与
  `tpbus -> TaApi -> CTAJob_InetTQL` 的独立会话链做动态时序对齐。
- 插件版本的 `CUserComm` 同样有 15 个槽位，但地址和对象布局不同；除非
  运行态加载它，否则主线以根目录版本为准。

## tpbus/TaApi 会话定时器

独立应用会话的静态定时行为现已闭合。当前 tpbus 内嵌配置把
`JobTimeout` 设为 150000 ms，创建/事务超时均为 3000 ms，心跳配置为
`TimeSpan=30, OnIdle=YES, JustNoQueue=NO`。`CTAPeer::OnHeartBeatTimer`
在空闲达到 30 秒时建立心跳任务；超过约 `2*TimeSpan+1` 秒仍无有效活动时发出
事件并关闭/失败该 peer。`CTAEngine::OnHeartBeatTimer` 同时清理过期任务和
peer。`DisConnect/LazyTimeOut/MaxReConTimes/JobTimeOut/MaxTimeOutTimes/`
`LoginStateNoChangeHost/ReConnectTimeOut/EnableTimeoutProc` 均是已确认选项。

纯 C++ `recon session-config` 只读输出这些静态默认值和 `connect.cfg` 公开
服务器组；对 `user.ini` 中会话/令牌类字段仅报告存在性、编码长度和是否为加密
封套，不输出值或哈希，也不发送登录或心跳。
