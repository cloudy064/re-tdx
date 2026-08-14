# 模块地图

> 2026-07-31 当前安装版。模块分为“已确认加载/调用”“存在但未触发”和
> “当前安装缺失”，避免把历史字符串和命名猜测混入运行架构。

## 主线优先级

| 模块 | 当前证据 | 角色 | 优先级 |
|---|---|---|---:|
| `TdxW.exe` | 已反编译、运行中 | 主框架、网络对象调用者和上层回调宿主 | ★★★ |
| `TdxAsioComm.dll` | 静态依赖、已加载、已反编译 | 主 TCP 传输；Boost.Asio + IOCP | ★★★ |
| `TaApi.dll` | 已加载；`tpbus` 只导入 `CreateInstanceEx` | CTAEngine、TQL/TC50/RPC/RAW 任务 | ★★★ |
| `TDataParse.dll` | 按需加载，主程序只取 1 个导出 | 本地/缓存图表记录解析 | ★★☆ |
| `TBigData.dll` | 已加载，10 个业务导出 | 大数据窗口/公式/消息插件 | ★★☆ |
| `tpbus.dll` | 已加载，16 个业务导出 | 应用会话、事件总线、行情服务中枢 | ★★★ |
| `SEPlugins\TAsioComm.dll` | 存在且已反编译，未加载 | 可选/插件 TCP 传输 | ★★☆ |
| `SEPlugins\TEncrypt.dll` | 主程序按需加载 9 个导出 | Blowfish/Base64/RSA/HTTP/下载 | ★★☆ |
| `TCPlugins\AddinMiniQuote.dll` | 存在，2 个导出 | 迷你行情插件 | ★★☆ |
| `TJyaid.dll` | 主程序静态依赖、已反编译 | 设备指纹与 `eTrade.xml` 配置辅助 | ★★☆ |
| `TPool.dll` | 主程序按需加载、已反编译 | 股票池、条件公式、本地历史和界面 | ★☆☆ |
| `TCalc.dll` | 主程序静态依赖、已反编译 | 技术指标、选股、专家系统和五彩 K 线公式引擎 | ★★☆ |
| `tc.dll` | 已反编译，39 个导出 | 券商交易客户端、账户/委托 UI 与 L2 凭据宿主桥接 | ★☆☆ |
| `ZDPlugins\TdxTopicView100.dll` | 主程序动态加载 `IData_GetObject` | 云配置页面对象与宿主回调边界 | ★★★ |
| `ZDPlugins\TdxZdView100.dll` | `TdxTopicView100` 静态依赖 | datasource、`reqformat`、protobuf/PBRPC 编解码 | ★★★ |
| `ZDPlugins\TPData100.dll` | `TdxTopicView100` 静态依赖 | TP 会话、匿名登录与数据收发 | ★★☆ |

## 已确认的网络接口

```text
TdxW.exe
  └── TdxAsioComm.dll
        ├── MakeUserCommModule -> CUserComm
        ├── 15 槽 VUserComm 接口
        ├── IOCP
        └── WSARecv / WSASend
```

`SEPlugins\TAsioComm.dll` 实现同名工厂和同样 15 槽接口，但对象大小、代码
路径和版本均不同，当前主线不能将两者合并。

## 数据与消息候选

### `TaApi.dll`

导出：

```text
TaApi_CreateAppCore
TaApi_CreateInstance
TaApi_CreateInstanceEx
TaApi_DestroyAppCore
```

还提供 COM 和 JNI 入口。`tpbus.dll` 只静态导入
`TaApi_CreateInstanceEx`，用它创建 `CTAEngine`，注册 `JobNotify` 并
建立 `"Sync"` 会话。`CTAJob_InetTQL` 是当前应用消息主线。

### `TDataParse.dll`

```text
fn_TGetImageData
fn_sync_getdata
```

体积仅 207,816 bytes。主程序只解析 `fn_TGetImageData`，先查询记录数，
再分配每条 1032 字节并解码；调用点位于本地/缓存文件读取路径，不是
当前 TCP 收包入口。`fn_sync_getdata` 在主程序中未被引用。

### `TBigData.dll`

关键导出：

```text
BigDataUnit_Create
BigDataUnit_ProcessMsg
BigData_RegisterCallBack
BigData_GetCurrentGPInfo
BigData_Init_Environ
```

主程序完整加载 10 个接口，用它创建窗口单元、加载
`cloud_cfg\<name>.cfg`、转发窗口/业务消息。债券公式、表格和图表符号
占主导，当前不再把它当作全市场行情主表。

### `tpbus.dll`

关键导出：

```text
GetEventBus
PostTask
PostSyncTask
PostDelayedTask
TP_Init
TP_CreateTPData
TP_GetFile
```

同时转出 `TaApi_CreateAppCore` / `TaApi_DestroyAppCore`，但二者只是
TP AppCore 的别名。`tpbus` 内含 SessionManager、TDXEventBus、
DataService_HQ 和 HQDataMaintain，并构造 `FastHQ.Subscribe`。

### `TdxTopicView100.dll` / `TdxZdView100.dll`

`TdxW.exe:sub_730FC0` 从 `ZDPlugins` 加载 `TdxTopicView100.dll`，解析
`IData_GetObject` 并注册三个宿主回调。工厂随后创建云页面对象；
`TdxW.exe:sub_A986E0` 最终把 `cloud_cfg\<name>.xml` 路径传给该对象。

`TdxTopicView100.dll` 静态依赖 `TdxZdView100.dll` 和 `TPData100.dll`。
其中 `TdxZdView100.dll` 同时出现 `datasource`、`reqformat`、
`pb_rpc_req/pb_rpc_ans`、`ReqSelect/AckSelect` 与完整 protobuf 运行时，
并已确认它是 `reqformat=22` 的编解码层：

- `sub_100025B0` 解析 `pb_rpc_req:...;ReqByte={...}` 描述符并序列化；
- `sub_102A3770` 对格式 22 创建类型 `1144` 的取数句柄；
- `sub_102ABDE0` 调用序列化函数并把二进制消息送到传输层；
- `sub_10376A50` 初始化 `proto\`，`sub_1000C430` 解开
  `proto\zdproto.dat`。

恢复的 `protocol_mp.proto` 明确给出 `pb_rpc_req/pb_rpc_ans` 字段和
RpcID/StartPos 分页规则，且已由真实 TQLEX 请求验证。因此不再把这项
工作归给 `tpbus.dll`。`TPData100.dll` 暴露 `CreateFetchDataHandle` 和
`ITPConn_*`，负责下游会话/传输。

### `TPool.dll`

TPool 的 11 个业务导出覆盖初始化、回调注册、启动/停止、窗口和状态查询。
`TdxW.exe:0x731570` 传入安装路径、用户路径、`connect.cfg` 和
`CMainCalcInterface`，随后注册三个宿主回调。TPool 自身不导入 socket、
WinINet、WinHTTP 或加密库，所有证券/行情数据由宿主回调提供。

`TPool_ProcessPool(name, operation)` 的操作值已恢复：

| operation | 语义 |
|---:|---|
| `-1` | 停止股票池及工作线程 |
| `0` | 暂停 |
| `1` | 运行/恢复 |
| `2` | 打开或切换到该股票池 |

规则保存为 `tpool\<name>.xml`，包含 `cell/flow/func/stk/psatt` 节点；
运行时调用 `TCalc` 的 `GetIndexInfo/NewOneCalc/DelOneCalc`，并可写入历史
日志和 `blocknew\*.blk`。它是本地策略/筛选消费者，不是网络连接池，也
不是实时行情主表。

### `TCalc.dll`

TCalc 导出 `CMainCalcInterface`，维护五个 5072 字节记录数组。公开的
`kind 0/1/2/3` 分别是技术指标、条件选股、专家系统和五彩 K 线，当前
DLL 内置 `222/107/15/35` 条。`GetIndexNum/GetIndexInfo/GetTreeInfo`
可以直接枚举名称和分类。系统技术指标已由只读 PE 提取器导出，结构和
运行安全边界见[公式与指标引擎](../02-engine/04-formula-engine.md)。

## 加密与认证

`SEPlugins\TEncrypt.dll` 导出：

```text
T_Encrypt
T_Encode
T_RSAEncode
T_RSAEncode2
T_PostUrlVerify
T_DownloadFile*
```

它依赖 `LIBEAY32.dll`、`CRYPT32.dll`、`WLDAP32.dll`。静态分析确认
`T_Encrypt` 是 Blowfish，`T_Encode` 是 Base64；RSA、HTTPS POST 和
下载路径均已定位。仍无证据表明 7709 行情流经过该 DLL。

`TJyaid.dll` 只有 104,824 字节，导出：

```text
GetWtDefInfoFromETradeXML_More
GetXUserHardInfo
ProcessHostFromETradeXML
```

`TdxW.exe` 静态依赖它，并在 `0x59B3C0` 动态调用
`GetXUserHardInfo`。该函数返回：

```text
ProcessorNameString;%08x(CPUID leaf 1 EAX);disk serial
```

第三段通过多个 `PhysicalDrive` / SCSI / 旧 IDE IOCTL 读取路径择一取得。
完整值进入行情账号校验、用户绑定及 TQL/SSO 的 `deviceid/HARDINFO`
字段；第三段还会单独用于 `##disksns##` 模板替换。另外两个导出负责
解析、筛选 `eTrade.xml` 的登录模式、营业部、安全模式和主机定义。
因此它属于设备身份和交易配置辅助层，不是 7709 行情传输实现。

## UI 与辅助模块

当前已加载但不是协议主线：

- `chrome\TCefWnd.dll`、`libcef.dll`、`tdxcef.exe`；
- `RibbonBar.dll`、`RibbonIcon.dll`、`PTFrame.dll`；
- `DuiWnd.dll`、`DuiWndPlugin.dll`；
- `GNPlugins\TE_*.dll`；
- `TCalc.dll`、`TQQCalc.dll`、`TGear.dll` 等公式/界面业务库。

## 当前安装缺失

以下模块只在旧字符串/初始化推断中出现，实际安装扫描未发现：

```text
TPyth.dll
TdxDataSDK.dll
TQ.dll
TQSRun.dll
TAsioComm1.dll
AddinMiniQuoteEx.dll
```

不再把它们作为当前版本攻坚入口。若后续升级或其它发行版出现，再按哈希
和版本重新纳入。

## 下一轮 IDA / 动态顺序

1. 动态记录 `tpbus` 的 `FastHQ.Subscribe` 序列化 Body；
2. hook `TaApi` 的 `CTAJob_InetTQL` 接收分片；
3. 对齐 `TdxAsioComm` send/recv，判断两套会话路径的关系；
4. 在登录动态实验中记录设备 ID 字段出现的时序，但不要采集或入库真实值；
5. 若需要补充 TdxW 内部行情结构，可运行一个最小股票池并观察
   `sub_61B630` 的数据类型调用；该旁支优先级低于前 3 项。
