# 应用消息与数据插件层

> 更新时间：2026-07-31。证据来自 `TdxW.exe`、`tpbus.dll`、
> `TaApi.dll`、`TDataParse.dll`、`TBigData.dll` 和 `TPool.dll` 的已完成
> IDA 数据库。
> 本文区分应用任务、文件解析和 UI 数据插件，不再按 DLL 名称推测一条
> 虚构的串行数据链。

## 已确认的调用关系

```text
TdxW.exe
├── TDataParse.dll
│     └── 按需解析本地/缓存中的记录块
├── TBigData.dll
│     └── 窗口单元、配置、消息转发、债券公式/表格 UI
├── TPool.dll
│     └── TdxW 回调供数 → TCalc 条件公式 → 本地股票池状态/文件
└── tpbus.dll
      ├── TP_Init
      ├── TP_CreateTPData("TdxW")
      ├── TP_CreateTPData("TdxWL2")
      ├── SessionManager / TDXEventBus
      ├── DataService_FW / DataService_HQ / DataService_Util
      └── TaApi_CreateInstanceEx
             └── CTAEngine / CTAJob_InetTQL
                    └── FastHQ.Subscribe
```

这条应用消息链与
[TdxAsioComm 的 7709 UserComm 传输链](01-network-layer.md)同时存在。
`tpbus.dll` 不导入 `TdxAsioComm.dll`，而是只从 `TaApi.dll` 静态导入
`TaApi_CreateInstanceEx`。在找到共同的上层调用者或会话映射前，不能把
两个网络栈画成一条路径。

## `TdxW.exe` 动态加载点

| 模块 | 加载函数 | 实际解析的接口 |
|---|---:|---|
| `TBigData.dll` | `TdxW.exe:0x41CF50` | 10 个 `BigData_*` / `BigDataUnit_*` 接口 |
| `TDataParse.dll` | `TdxW.exe:0x7218F0` | 仅 `fn_TGetImageData` |
| `TPool.dll` | `TdxW.exe:0x731270` | 11 个 `TPool_*` 业务接口 |
| `SEPlugins\TEncrypt.dll` | `TdxW.exe:0x725320` | 9 个加密、RSA、POST、下载接口 |
| `tpbus.dll` | `TdxW.exe:0x910480` | `TP_Init`、`TP_Exit`、`TP_CreateTPData`、`TP_DestroyTPData`、`TP_GetFile` |

`TdxW.exe:0x9105B0` 调用 `TP_Init`，随后创建 `"TdxW"` 和
`"TdxWL2"` 两个 TPData 对象；`0x910970` 负责退出和释放。

## `tpbus.dll` 的真实角色

`tpbus` 不是只有几个任务队列转发函数的薄封装。编译路径、调用关系和
字符串共同确认它包含：

- `SessionManager` 与 `TDXEventBus`；
- `DataService_FW`、`DataService_HQ`、`DataService_Util`；
- `HQDataMaintain`、`ProtocolSZSDK2TDX`、`ProtocolTrans`；
- 登录流程、自选股同步、云数据历史和本地配置；
- `t_CloudDataHistory`、`t_key_cache` 两组 SQLite 缓存；
- 按线程类型分派的 `PostTask`、`PostSyncTask`、
  `PostDelayedTask`。

`TP_Init` 读取 `connect.cfg`，建立应用核心并创建 TPData。它还读取
`UseIncrementalSync`、`UseDelayConnect`、`CloudDataHistory`、
`QueryExtendInfo` 等应用层开关。

### `tpbus -> TaApi`

`tpbus.dll:0x100D98D2` 是当前最关键的桥：

1. 若不在指定任务线程，先通过 `PostSyncTask(4, ...)` 切换线程；
2. 调用唯一的 TaApi 导入 `TaApi_CreateInstanceEx`；
3. 注册 `"JobNotify"` 回调；
4. 创建 `"Sync"` 会话/任务对象；
5. 从 `datacache.json/BestHost` 读取候选主机并注入会话管理器。

`tpbus` 导出的 `TaApi_CreateAppCore` / `TaApi_DestroyAppCore` 实际只是
`TP_CreateAppCore` / `TP_DestroyAppCore` 的别名，不代表它调用了
`TaApi.dll` 的同名 AppCore 工厂。

### TaApi 引擎

`TaApi_CreateInstanceEx` 创建 `CTAEngine`。该 DLL 内可确认的任务族包括：

- `CTAJob_InetTQL`；
- `CTAJob_TC50_Login`、`CTAJob_TC50_SimpCall`、
  `CTAJob_TC50_FuncCall`；
- `CTAJob_RPCInvoke`；
- `CTAJob_RAW`。

默认配置包含请求/响应分段、压缩阈值、创建/事务超时、心跳和代码页。
`TaApi.dll:0x10008610` 是 `CTAJob_InetTQL` 接收分片后的交付点，日志格式
直接给出 `Recv Fragment` 与 `Data Size`。

## `FastHQ.Subscribe` 请求锚点

`tpbus` 在 `HQDataMaintain.cpp` 中构造名为 `FastHQ.Subscribe` 的
`CTAJob_InetTQL`。已恢复的请求字段如下：

| 字段 | 当前语义 |
|---|---|
| `CODE` | 证券代码 |
| `SC` | 市场/域编号 |
| `LX` | 行情类型，枚举值待动态标定 |
| `PkgType` | 当前固定为 `0` |
| `OperType` | `1` 订阅，`0` 退订 |
| `PushType` | 常用 `3`；维护视图等路径使用 `1` |
| `BatchPush` | 当前固定为 `1` |

`LX` 已进一步确认来自上层 `OneStockStart`/公开调用参数，而不是
`FastHQ.Subscribe` 内部写死的枚举。当前仍缺一份有 Level2 权限的动态
样本来完成 `LX → 数据类型` 的终值对照。

接收侧消息类型已进一步恢复，注意它与请求字段 `PushType=3` 不是同一
枚举：

| 接收类型 | 已确认内容 | 与 SDK 路径的关系 |
|---:|---|---|
| `111` | 即时行情字段，随后为成对的买卖多档价格、数量和席位数 | 与多档盘口业务相近，但不是 `1803` 的同一结构 |
| `112` | 买一/卖一价格、两侧委托数及合并数量数组；发布 `QueueUpdate` | 与 `4671` 买卖一队列相近，不等于选中任意价位的单侧 `18031` |
| `115` | 批量容器，每项再标记为 `111` 或 `112` | 只负责打包 |
| `113/114/116` | 长度帧解包后的 PB 体，经 `MaintainData.HQPUSHPB` 原样发布 | 字段号/wire type 可检查；业务 schema 仍待真实样本与消费方 |

`111` 固定头为 99 字节，每档 20 字节；`112` 固定头为 54 字节，后接
`u32 quantities[buy1_count + sell1_count]`。离线解析与被动探针有界展开均
已实现。

`tpbus.dll:0x1007A75E` 已进一步闭合接收后的状态落表：先由
`sub_1007950B` 融合到每证券维护对象，再发布
`MaintainData.HQINFOUpdate`；对象 `+792/+796` 分别保存代码和市场号。
`sub_10067DEF` 会把 111 体的即时价量字段及 `20*depth_count` 档位数据写入
持久状态，而不是将原始包直接交给 UI。112 则由 `0x1007667D` 保存队列体并
发布 `HQDataNotify/QueueUpdate`。因此主程序接收侧已闭合到“解帧 → 融合 →
状态对象 → EventBus”，剩余动态边界是合法会话里的 `LX` 终值，而不是状态表
是否存在。

期货/扩展市场插件另有独立 `HQDataService`：它用 9221 每批请求最多 100 个
证券，将五档、持仓、结算、涨跌停和行情时间归一为 712 字节缓存。最新静态
证据纠正了早期判断：`RedirectData` 是 4611/4612/4618/4630/4631/4632 的
结构化请求翻译入口，不是服务器地址清单；底层重定向目标保存在 `Target`。
该链仍不属于 7709 主站。完整布局见
[TTPlugin 批量行情与状态记录](../03-memory-layout/02-ttplugin-batch-quote.md)。

`113/114/116` 的边界也已精确到 `tpbus.dll:0x1007BFBE`：先由
`0x101A5AD0/0x101A6060` 取出一段长度帧，再把指针、长度和 `PushType`
写入事件消息；本函数自身不调用 protobuf 业务解析器。当前安装全部
EXE/DLL 中，完整主题 `MaintainData.HQPUSHPB` 只出现在 `tpbus.dll`。
因此现有证据能确认“发布方与 wire 编码”，不能确认成交、委托或盘口等
业务名称。探针在发布前的 `tpbus+0x7C0FA` 被动读取该段，避免依赖未知
消费方。

### EventBus 路由与现有观察者

`CTDXEventBus` 实现在 `tpbus.dll` 内，主虚表位于 `0x10434038`。关键槽为：

| 虚表偏移 | 实现 | 语义 |
|---:|---:|---|
| `+0` | `0x100E72AE` | `ObserveEvent` |
| `+4` | `0x100E7666` | `ObserveEvent2` |
| `+24` | `0x100E806D` | `PostEventMsg` |
| `+28` | `0x100E1DC4` | `SendEventMsg` |
| `+44` | `0x100E78D9` | 构造 EventBus 消息 |

分发前的 `0x100E795A` 用点号拆分主题。例如 `A.B.C` 生成 `A.*`、
`A.B.*`、`A.B.C` 三个查找键；不会生成裸 `A` 或全局 `*`。因此
`MaintainData.HQPUSHPB` 的潜在观察者只能按精确主题或
`MaintainData.*` 注册。

2026-08-02 对当前进程的 `map<string, shared_ptr<vector<ObserveInfo>>>`
做了显式只读快照，得到 4 个主题：

| 主题 | 观察者数 | 回调模块 |
|---|---:|---|
| `MaintainData.SubscribeStock` | 1 | `tpbus.dll` |
| `MaintainData.UnSubscribeStock` | 1 | `tpbus.dll` |
| `MaintainData.MoreSubscribeStock` | 1 | `tpbus.dll` |
| `SManager.SendData` | 1 | `tpbus.dll` |

快照中没有 `MaintainData.HQPUSHPB` 或 `MaintainData.*`。所以在该时刻，
`113/114/116` 虽被发布，却没有通过此 EventBus 接收它们的已注册回调。
该结论不外推到尚未加载的插件、其他客户端版本或稍后打开的权限窗口；
可选动态观察点会继续记录新注册主题。

关键构造点：

- `tpbus.dll:0x10076A7A`：单证券订阅/退订；
- `tpbus.dll:0x1007D4C4`：由 JSON 列表构造批量请求；
- `tpbus.dll:0x1007E892`：`MoreSubscribeStock` 的增删批量请求；
- `tpbus.dll:0x100676B5`：处理 `FastHQ.Subscribe` 应答/状态；
- `TaApi.dll:0x1009BBA0`：构造 `SPEC=3874` 的
  `FastHQ.Subscribe` TQL 内容。

## Level2 请求与 SDK 适配

`TdxW.exe` 还保留一条内置 Level2 直连路径。两类请求均为 26 字节：

- 命令 `1364`：逐笔成交；
- 命令 `1374`：逐笔委托；
- 请求字段为固定头、市场、六位代码、`u32` 分页游标和最多 `1500` 条；
- 响应先做会话单字节 XOR，随后读取 `u16 count + u32 next_cursor`；
- 记录使用 TDX 有符号变长整数，时间为距 06:00 的秒数，累计价格按
  `10000` 缩放。

关键函数为：

- `TdxW.exe:0xAF9FC0`：构造 `1364` 并分页追加逐笔成交；
- `TdxW.exe:0xAF9BC0`：构造 `1374` 并分页追加逐笔委托；
- `TdxW.exe:0x85CC80`：逐笔成交变长记录解码；
- `TdxW.exe:0x85CDE0`：逐笔委托变长记录解码；
- `TdxW.exe:0x69EA00/0x69EFF0`：XOR、记录过滤和界面数据交付。

`tpbus.dll:0x10082805` 则是 `ProtocolSZSDK2TDX.cpp` 的 JSON 适配入口：

| FuncID | 数据 | 已恢复字段/结构 |
|---:|---|---|
| `4653` | 分时 | `datetime/closePrice/averagePrice/tradeVolume/reference_price` |
| `4655` | 秒级逐笔成交 | `transactionPrice/transactionTime/singleVolume/transactionStatus` |
| `4671` | 买卖一队列 | `buyList/sellList[].QUANTITY_`，内部体固定 242 字节 |
| `4680` | 五/十档盘口 | OHLC、量额、持仓和四个价格/数量数组，每档 20 字节 |

对应 `CTAJob_Redirect` 请求体长度为 `40/46/37` 字节；`4680` 的请求字节
明确选择 5 档或 10 档。纯 C++ `level2 build --format sdk-4653|sdk-4655|sdk-4680`
现可离线复现这些请求体；`level2 decode --format sdk-1801|sdk-1802` 可解析授权
SDK 回调中的 52/40 字节逐笔记录。完整字段表和工具见
[Level2 静态协议归档](../99-log/2026-08-02-level2-static-protocol.md)及
本轮 [原生 SDK 逐笔记录](../99-log/2026-08-12-native-level2-sdk-ticks.md)。

后续对已处理的 `TdxW.exe.i64` 复探还精确恢复 `1804` 的 432 字节布局：
`+8/+216` 为两侧 `f64` 价格，`+16/+224` 为两组各 50 个 `f32` 数量槽，
`+424/+428` 为各侧 `u32` 计数。`level2 decode --format sdk-1804` 已实现；
主程序此层没有给出可靠买卖枚举，因此输出保持 `first/second`，等授权样本标定方向。

同一回调入口还确认 `1807/18071` 共用 380 字节行情更新体。纯 C++
`level2 decode --format sdk-1807|sdk-18071` 会输出时间、昨收、开高低现、
累计量额、双边十档以及买卖均价/总量；证券代码由 SDK 请求/回调注册表关联，
并不嵌入体内。偏移 `+68/+72` 的消费方没有给出稳定业务名，因此保留原始值，
不会猜成交易状态或持仓量。

批量路径在最后调用 `SetEOR`。这已经是可用于动态 hook 的稳定应用层
边界，但仍不是最终的 TCP 原始帧格式。

## `TDataParse.dll`

### `fn_TGetImageData`

当前主程序只动态解析这个导出，其调用约定为：

```c
int fn_TGetImageData(
    const void *source,
    int source_length,
    void *records,
    int *record_count,
    int mode);
```

调用约定已由多个 `TdxW.exe` 调用点重复验证：

1. `records = NULL, mode = 1`，先取得记录数；
2. 分配 `1032 * record_count` 字节；
3. `mode = 0`，填充定长记录数组。

这些调用点在读取本地/缓存文件块后出现，而不是在
`TdxAsioComm` 收包回调中。导出名虽然含 `ImageData`，输出并不是像素
缓冲，而是本地盘口快照。

输入已恢复为控制字符分隔的增量字段流：`0x03` 开始字段，`0x02` 结束
字段，`0x04` 结束字段并提交记录；字段前两字节为 ASCII 标签。状态跨记录
继承，同时间记录会覆盖，相邻状态仅时间不同则去除，最后从累计量计算单条
成交量。1032 字节输出已经确认包含：

- `+0..+80`：时间、昨收、OHLC、最新价、持仓量、成交量和成交额；
- `+88..+407`：买一至买十、卖一至卖十的价量；
- `+456..+863`：买一/卖一报告数量和各最多 50 项的委托队列；
- `+864..+1031`：168 字节文本区；
- `1G/1H/1I/1J`：消费方标签已确认分别为买均、总买、卖均、总卖；
- `1C..1F`：已确认类型和偏移；当前 TdxW 六个直接消费入口均不读取，按
  线协议保留字段导出，不强行业务命名。
- `0D`：解析器内部参与状态继承和相邻记录去重，但返回后的 168 字节文本区
  同样未被当前六个 TdxW 消费入口读取。

本地包装文件含 24 字节头，压缩长度在 `+8`、解压长度在 `+16`，zlib
流从 `+24` 开始。纯 C++ 命令 `image-data decode` 已实现包装解压、字段流
恢复、JSON 和标准化定长记录输出，不加载原 DLL。合成控制流同时送入原 DLL
和新实现后，两条输出共 2064 字节逐字节完全相同。完整布局和原实现容量
边界见[本地盘口快照结构](../03-memory-layout/01-image-data-snapshot.md)。

消费方审计覆盖 `0x59DA50/0x5B4DD0/0xA19E10/0xA26C90/0xA637E0/
0xA85600`；`0x7218F0` 只是动态加载器。纯 C++ JSON schema v3 将该范围、
入口地址与未消费标签写入 `consumer_audit`，从而把“尚未命名”和“当前版本
确实没有消费”区分开。

### `fn_sync_getdata`

主程序没有引用这个导出名。函数的第一个参数选择两种文本解析路径：

- 模式 `0`：以 `|` 和 `,` 分隔；
- 模式 `1`：以 `$` 分隔；
- 其它模式返回 `-1`。

DLL 中存在旧版 zlib `inflate 1.1.3` 版权字符串，但没有字符串交叉引用；
当前已确认的缓存解压发生在 TdxW 调用链，使用 zlib 1.2.13，随后才把原始
字段流交给 `fn_TGetImageData`。不能把 DLL 内未引用的版本字符串当作实际
解压路径。

## `TBigData.dll`

`TdxW.exe:0x41D140` 初始化 DLL，传入程序路径、用户路径和环境标志，并
注册三个主程序回调。之后：

- 每个单元由 `BigDataUnit_Create` 绑定到窗口；
- 配置文件为 `cloud_cfg\<name>.cfg`；
- `BigDataUnit_ProcessMsg` 处理销毁、窗口尺寸变化和业务消息；
- `BigData_GetCurrentGPInfo` 返回当前市场/证券代码；
- `BigData_Uninit` 释放单元和缓存。

二进制中大量出现债券收益率/现值/应计利息公式、`CGridCtrl`、
`CChartGrid`、`colheader`、JSON 等符号。当前证据更符合“可配置的大数据
界面/公式插件”，而不是全市场行情主表。此前把它列为全市场数据模型的
假设已降级。后续已经恢复 `calc/calcref/calctype/calcflag` 描述结构、36 项
固定函数注册表及普通表达式求值链；当前 660 个 CFG 中的 1370 个计算列已全部
进入纯 C++ 审计/逐行复算。`BigData_RegisterCallBack` 的三个宿主回调也已闭合，
44 项系统列总表、列级 `refzqdm` 和 unit 级 `refunit` 继承已恢复；
`syscol` 可从公开 `0x054C/0x0547`、公开涨速/五档 `0x053E`、行情核心 ETF IOPV、公开 `0x0010` 财务、本地行业层级或离线
快照按关联证券补值；`$S_ZQDM` 还会展开为成员批次，按客户端原生口径计算六项
板块聚合。类型 163 封单字段与 `DYNAINFO(39)` 动态 PE 也已按原生口径闭合，
当前系统列自动来源覆盖为 2570/2570（100%）。详见
[TBigData 计算列与债券函数](10-tbigdata-cloud-calculations.md)。

## `TPool.dll`

### 宿主初始化

`TdxW.exe:0x731270` 动态解析 11 个业务导出；`0x731570` 随后：

1. 传入安装路径、用户路径、`connect.cfg` 和
   `CMainCalcInterface` 调用 `TPool_Init`；
2. 用 `TPool_RegisterCallBack` 注册 `sub_61B630`、`sub_62B8D0`、
   `sub_630950`；
3. 以主窗口句柄和消息号 `2849` 调用 `TPool_StartRun`。

三个回调分别把 TdxW 的数据查询、UI/路由动作和通用操作分派能力提供给
TPool。第一回调包装 `sub_60FFF0`，TPool 以数据类型
`4/32/102/104/105/111/122/125/126/134` 读取证券、即时行情或历史数据。
第二回调 `sub_62B8D0` 是七参数 `__stdcall` UI/动作路由，TPool 直接使用
`9/12/15/31/32/57/88`；其中 88 把 7 字节 `market+code` 记录批量保存为板块。
第三回调 `sub_630950` 的逻辑签名为四参数，TPool 以操作 10 在编辑器选择证券并
返回 25 字节记录，也以操作 31 在历史更新路径执行 65 字节输入、24 字节输出的
辅助计算。这里的数字是内部回调类型，
尚不能直接等同于网络协议消息号。

### 规则、执行与输出

每个股票池保存为 `tpool\<name>.xml`，主要节点包括：

- `cell`：条件节点、位置、颜色和文本；
- `flow`：节点间流转、时间条件和转移属性；
- `func`：公式集合、指标编号、周期、比较操作和值；
- `stk`：市场、代码、进入时间/价格、收益、涨幅、成交量等状态；
- `psatt`：删除、提示音、提示框、保存板块和历史记录等行为。

工作线程读取 XML，调用 `TCalc.dll` 的
`GetIndexInfo/NewOneCalc/DelOneCalc` 执行条件公式，再更新池内证券和
状态。可能输出 `_in_pool_his.txt`、`_status_his.txt`、日期 `.dat/.log`
和 `blocknew\*.blk`。该 DLL 没有 socket/HTTP/加密导入；其数据来自宿主，
因此它不是 7709 网络层或独立行情源。

每日历史使用 `tpool/<池>/<节点>/<YYYYMMDD>.<扩展名>`：

- `sub_10018380` 维护 `.log` 入池记录，字段为
  `market/code/indate/intime/inprice`；写入前按 `market+code` 删除旧项再追加；
- `sub_10017CC0` 序列化 `.dat` 全量快照，在上述 5 项后追加
  `income/now/rise/volume/maxrate/maxperiod/maxtime/maxprice/idaynum`，共 14 项。
  其中 XML 名为 `maxtime` 的字段在原导出界面中明确显示为“最高日期”，不是时分秒。

纯 C++ `pool history` 不加载 DLL，也不写回文件。它可扫描三个既有 tpool 根目录，
按池、节点、日期和快照/入池类型过滤，输出类型化价格、时间、市场/证券标识、缺失
字段、非法字段、重复项、SHA-256 和跨文件汇总。文件超过限额时按日期倒序保留最新项。
`sub_1003A720` 还把入池历史导出为 GBK
`市场|代码|名称|进入日期|进入时间|进入价`；`sub_1003AC40` 的状态历史再增加
`最高收益率|最高周期|最高日期|最高价格`。纯 C++ 的 `--native-text-output` 已按相同
中文表头、列次序、两位小数和 CRLF 复现；名称只读自本地证券目录，输出路径与输入
历史文件、JSON 输出发生碰撞时会在写入前拒绝。
两种 `*_his.txt` 也可反向解析。GBK 与 UTF-8 自动识别；进入时间保留原文件的分钟
精度。状态文本的“最高日期”只有 `MM/DD`，解析结果不会补造年份，并能在不提供外部
名称表时使用文件自带名称完成字节级回转。

### 导出数据布局

- `TPool_GetPoolStatus(buffer, capacity)`：每项 131 字节；前 2 字节为
  运行状态，偏移 2 为系统池标志，偏移 3 起为池名称。
- `TPool_GetPoolGPInfo(buffer, capacity)`：每项 34 字节，由 2 字节
  `setcode` 和 32 字节代码区组成；空缓冲调用可先查询数量。
- `TPool_ProcessPool(name, op)`：`-1/0/1/2` 分别表示
  停止、暂停、运行和打开/切换。

TdxW `0x731470` 最多读取 50 个 131 字节状态项，并按查询到的数量分配
`34 * count` 字节接收股票列表。

## 下一步动态验证

按收益排序：

1. 在 `tpbus.dll:0x10076A7A` 或创建出的 `CTAJob_InetTQL` 上记录
   `Name` 与序列化 `Body`，切换 1 只普通股票完成订阅/退订对照；
2. 在 `TaApi.dll:0x10008610` 记录分片号、长度和解码后内容，闭合
   `SPEC=3874` 的应答结构；
3. 再把同一时序与 `TdxAsioComm` 的 send/recv 长度对齐，判断两套网络
   路径是否共享节点或分别承载不同服务；
4. 只有在需要标定 TdxW 内部行情记录时，才借助一个最小 TPool 规则观察
   `sub_61B630` 回调；不要把这些回调类型号误写成线上协议号。
