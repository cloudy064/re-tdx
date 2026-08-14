# Level2 逐笔、委托队列与十档静态协议

> 日期：2026-08-02  
> 范围：`TdxW.exe`、`tpbus.dll`、`TaApi.dll` 和当前安装配置。  
> 结论等级：请求与解码结构已确认；真实 L2 权限样本和字段终值仍待动态对照。

## 本轮结论

Level2 不再只是字符串入口。当前已经闭合三条相互印证的路径：

1. `TdxW` 内置直连路径的逐笔成交命令 `1364` 和逐笔委托命令 `1374`；
2. `tpbus` 的 `4655` 逐笔成交、`4671` 买卖一队列、`4680` 五/十档
   SDK 适配路径；
3. 主程序 SDK 回调类型 `1803/18031` 的固定二进制体，已经分别闭合到
   双边多档盘口和选中价位的委托量队列；
4. `FastHQ.Subscribe` 的完整逻辑字段，以及上层传入的 `LX` 参数。

已经新增离线工具 `tdx_level2.py`。它能生成已确认的请求体，解析抓取到的
XOR 响应，并把 SDK JSON 转成稳定 JSON/CSV；它不实现登录、票据或权限绕过。

## 内置直连请求

`TdxW.exe:0xAF9FC0` 和 `0xAF9BC0` 构造相同的 26 字节请求，仅命令号不同：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | `u32` | `0` |
| 4 | `u32` | 固定 `0x00100000` |
| 8 | `u16` | 后续业务体长度 `16` |
| 10 | `u16` | `1364` 逐笔成交；`1374` 逐笔委托 |
| 12 | `u16` | 市场/域编号 |
| 14 | `char[6]` | 六位证券代码 |
| 20 | `u32` | 分页游标；首次为 `0`，之后使用响应游标 |
| 24 | `u16` | 请求条数；客户端上限 `1500` |

例如上海 `600000` 的逐笔成交第一页请求为：

```text
000000000000100010005405010036303030303000000000dc05
```

响应整体使用会话中的单字节 XOR key。解密后头部为：

| 偏移 | 类型 | 含义 |
|---:|---|---|
| 0 | `u16` | 记录数；最高位表示错误，客户端将其归零 |
| 2 | `u32` | 下一页游标 |
| 6 | 变长 | 压缩记录区 |

### `1364` 逐笔成交记录

每条记录解码为 20 字节内部结构；线上的变长布局为：

1. `u16 time_offset`，实际时间为 `time_offset + 06:00:00`，精确到秒；
2. 有符号变长整数 `price_delta`，跨记录累加，价格除以 `10000`；
3. 有符号变长整数 `volume_raw`；
4. 有符号变长整数 `order_count_raw`；
5. 有符号变长整数 `status_raw`；
6. 有符号变长整数 `sequence_raw`。

`TdxW.exe:0x69EA00` 负责响应 XOR 和过滤，`0x85CC80` 是记录解码器。
调试日志的参数顺序还证明其输出包含市场、代码、状态、序号、秒级时间、
价格和成交量。

### `1374` 逐笔委托记录

`TdxW.exe:0x69EFF0` 和 `0x85CDE0` 给出的线上布局为：

1. `u16 time_offset`；
2. 有符号变长整数 `price_delta`，同样按 `10000` 缩放；
3. 有符号变长整数 `volume_raw`；
4. 固定三个字节：`order_type`、`side`、`action`；
5. 有符号变长整数 `order_id`。

过滤代码已确认 `action` 使用 `B/S/C` 表示买、卖、撤单；撤单时 `side`
进一步区分 `B/S`。首个 `order_type` 字节仍缺真实样本标定，工具同时保留
数值和可打印 ASCII，不把它提前命名为某一种交易所业务枚举。

## `tpbus` SDK/应用任务路径

### 请求体

`HQDataMaintain.cpp` 构造三个 `CTAJob_Redirect` 请求：

| ReqNo | 长度 | 关键字段 |
|---:|---:|---|
| `4653` | 40 | 市场、22 字节代码、附加行情标志、回购时间标志 |
| `4655` | 46 | 市场、22 字节代码、游标、`wantnum`、附加行情标志 |
| `4680` | 37 | 市场、22 字节代码、盘口深度 `5/10` |

`4655` 的客户端缺省 `wantnum=80`，大于 `500` 时回退到 `80`。
`4680` 在代码中明确把深度限制为五档或十档。

### SDK JSON → 内部记录

`tpbus.dll:0x10082805` 的源文件名为 `ProtocolSZSDK2TDX.cpp`。其
`SetAnsData` 分支给出以下字段：

- `4655`：`transactionPrice`、`transactionTime`、`singleVolume`、
  `transactionStatus`；时间从 `HHMMSSxx` 去掉末两位后转换为秒，
  状态 `B/S/其他` 转为 `0/1/-1`，记录大小 18 字节；
- `4671`：读取 `buyList[0].QUANTITY_` 和
  `sellList[-1].QUANTITY_`，数量除以 `100` 后写入 242 字节买卖一队列；
- `4680`：读取 `datetime`、昨收、开高低现、成交量额、持仓量，及
  `buyPrices/buyVolumes/sellPrices/sellVolumes`；买盘数组反向、卖盘正向，
  数量乘 `1000` 写入，每档 20 字节；
- `4653`：分时记录使用 `datetime`、`closePrice/1000`、
  `averagePrice/1000`、`tradeVolume` 和 `reference_price`。

这证明 `4655` 是真正的秒级逐笔成交，不是 `0x0FC5/0x0FC6` 的分钟级
公开 L1 成交明细；`4671` 和 `4680` 分别覆盖买卖一委托队列与五/十档深度。

## `FastHQ.Subscribe` 与权限边界

`tpbus.dll:0x10076A7A` 的逻辑字段为：

```text
CODE, SC, LX, PkgType=0, OperType=1/0,
PushType=3, BatchPush=1
```

这里请求中的 `PushType=3` 与接收分派中的消息类型 `111/112` 不是同一
枚举。`tpbus.dll:0x1007BFBE` 从 `PushType/PushBody` 拆包：`111/112` 是
单条原始体，`115` 是批量容器，容器内每项再标记为 `111` 或 `112`。

`LX` 来自上层 `OneStockStart`/API 参数，不是订阅函数中的固定常量。当前
仍需一份有权限的动态样本，才能把每个 `LX` 值最终标定为成交、委托、
队列或行情深度。

### `111` 即时行情与多档价量推送

`tpbus.dll:0x1007B4E8 → 0x1007598D` 已给出原始体：

```text
+0   u16 market/set_code
+2   char code[22]
+24  s8 depth_count
+35  u32 hq_time
+39  u32 item_number
+43  f32 close
+47  f32 open
+51  f32 high
+55  f32 low
+59  f32 last
+63  f32 lead
+67  u32 volume
+71  u32 rest_volume
+75  f32 amount
+79  u32 volume_in_stock
+83  f32 jjjz
+87  u8 in_out_flag
+88  u8 hktt_flag
+89  u8 volume_unit
+90  f32 ph_volume
+99  level[depth_count]             每档 20 字节
      f32 buy_price, u32 buy_volume, u16 buy_seat_count,
      f32 sell_price, u32 sell_volume, u16 sell_seat_count
```

因此 `111` 的准确边界是“即时行情 + 成对多档价量/席位数”，不是某个
十档窗口的专用消息。它与 `1803` 都能表达多档盘口，但长度、上限、字段
组织和入口完全不同，不能把两者视为同一个协议类型。

### `112` 买一/卖一委托队列推送

`tpbus.dll:0x1007667D` 把该体保存后发布 `OperType=QueueUpdate`；
`0x1007680D` 将其转成 JSON：

```text
+0   u16 market/set_code
+2   char code[22]
+24  u32 refresh_number
+28  f32 buy_price
+32  f32 sell_price
+36  u32 buy1_count
+40  u32 sell1_count
+54  u32 quantities[buy1_count + sell1_count]
```

数量数组先是买一队列，再是卖一队列。`112` 与 `4671` 的“买卖一队列”
模型最接近；它同时携带两侧，而 `18031` 是用户选中任意一个价格后请求的
单侧队列，所以同样不能直接等同。

### `113/114/116` 未知 schema protobuf 发布链

`tpbus.dll:0x1007BFBE` 对三种类型走同一条确定路径：

```text
PushBody + length
  -> 0x101A5AD0 / 0x101A6060 取出单个长度帧
  -> pointer=[ebp-0x9c], size=[ebp-0xa0]
  -> EventBus("MaintainData.HQPUSHPB")
       .PushType(113/114/116)
       .Body(pointer, size)
```

函数没有按 protobuf 字段做业务解析。对 `C:\new_tdx` 下全部 EXE、DLL、
OCX、PYD 做精确字符串扫描，`MaintainData.HQPUSHPB`、`HQPUSHPB` 只命中
`tpbus.dll`。`ZDPlugins/TdxZdView100.dll` 是除 Chromium 运行库外唯一带
protobuf 运行时痕迹的业务模块，但可见描述符为 `QuantDB.proto`，且没有
上述事件主题；这只能证明它自己使用 protobuf，不能把它认作行情 PB
消费方。

因此本轮保持如下证据等级：`113/114/116` 的帧边界、wire 编码和发布主题
已确认；三种编号分别代表什么业务尚未确认。新增观察点
`tpbus+0x7C0FA` 位于解帧完成、发布开始之前，可记录精确长度、有界原始
样本和字段号/wire type。无描述符检查器不会给 length-delimited 字段擅自
判定 string、packed 或嵌套消息语义。

### EventBus 通配路由与运行时注册表

继续恢复 `CTDXEventBus` 后，`0x100E795A` 明确给出了主题匹配算法。它以
`.` 分词，对每个未到末尾的前缀追加 `.*`，最后追加完整主题：

```text
MaintainData.HQPUSHPB
  -> MaintainData.*
  -> MaintainData.HQPUSHPB
```

不会额外匹配裸 `MaintainData` 或全局 `*`。对应虚表的注册入口是
`0x100E72AE ObserveEvent` 与 `0x100E7666 ObserveEvent2`，发布入口为
`0x100E806D PostEventMsg`，消息构造入口为 `0x100E78D9`。

探针新增显式 `--eventbus-registry` 开关。开启时只调用 EventBus 单例 getter
并读取 `map<string, shared_ptr<vector<ObserveInfo>>>`；不发布事件。第一次
读取正确取得主题键，但把 `shared_ptr<vector>` 误当内嵌 `vector`，长度
校验随即拒绝继续；根据 `sub_100DD9FE` 与 60 字节 `ObserveInfo` 步长修正
一层间接后，第二次快照成功：

```text
MaintainData.MoreSubscribeStock   1 observer  tpbus.dll
MaintainData.SubscribeStock       1 observer  tpbus.dll
MaintainData.UnSubscribeStock     1 observer  tpbus.dll
SManager.SendData                 1 observer  tpbus.dll
```

四个观察者均为 `thread_type=4`、`observe_level=3`，回调虚表归属
`tpbus.dll`。当前没有 `MaintainData.*` 或 `MaintainData.HQPUSHPB`，所以
快照时刻没有通过该 EventBus 消费 `113/114/116` 的回调。更稳妥的业务解释
是“可选插件、特定权限窗口或其他版本可能使用的预留发布链”，而不是把
任一现存 DLL 冒认为消费者。

主程序还维护一套独立的界面单元枚举：`TICK_UNIT=3`、
`TICKSTAT_UNIT=18`、`TICKZST_UNIT=19`、`ORDER_UNIT=29`、
`HQ10_UNIT=30`、`TICK2_UNIT=31`、`QUEUE_UNIT=32`。这些值由
`TdxW.exe:0x4A8BF0` 注册，当前证据只证明它们是窗口/组件类型，不能直接
当成 `FastHQ.Subscribe.LX`；工具没有把两套枚举强行合并。

主程序的 `TdxWL2` 登录回调还显示：`RightInfo` 包含特定权限标记后才设置
L2 状态，并取得 `QSHQToken`。这是一条明确授权边界。本项目只处理本机
合法会话产生的数据，不构造或绕过权限票据。

当前安装没有 `SDKPlugins/bin/TdxDataSDK*.dll`，运行态加载了 `tpbus.dll`、
`TaApi.dll` 和 `TdxAsioComm.dll`。检查时运行中的 `TdxW.exe` 没有 TCP
连接，也没有落盘 Level2 数据日志，因此本轮没有伪造“真实行情已通过”的
结论。

## 工具用法

生成逐笔成交请求：

```powershell
python doc/90-scripts/tdx_level2.py build-direct `
  --kind transaction --market 1 --code 600000
```

解析抓取并已知 XOR key 的响应体：

```powershell
python doc/90-scripts/tdx_level2.py parse-direct `
  --kind transaction --input capture.bin --xor-key 0x5a `
  --format json --output capture.json
```

转换 `ProtocolSZSDK2TDX` 收到的 JSON：

```powershell
python doc/90-scripts/tdx_level2.py parse-sdk-json `
  --func-id 4680 --input sdk-depth.json --output depth.json
```

解析探针或调试器保存的 `1803/18031` 固定回调体：

```powershell
python doc/90-scripts/tdx_level2.py parse-sdk-binary `
  --data-type 1803 --input sdk-1803.bin --limit 20 `
  --output sdk-1803.json
```

解析 `tpbus` 保存的 `111/112` 原始推送体：

```powershell
python doc/90-scripts/tdx_level2.py parse-tpbus-push `
  --push-type 112 --input tpbus-112.bin --limit 20 `
  --output tpbus-112.json
```

检查探针保存的 `113/114/116` PB；这不是业务 schema 解码：

```powershell
python doc/90-scripts/tdx_level2.py inspect-protobuf `
  --input tpbus-113.bin --max-fields 100 --sample-bytes 64 `
  --output tpbus-113-wire.json
```

单元测试覆盖精确请求字节、游标头、两类压缩记录、XOR、三个 SDK JSON
适配器、两类固定回调体、两类 `tpbus` 固定推送、未知 schema protobuf
wire 检查、EventBus 显式快照开关、探针汇总和 CLI；启动器也会把
`hook-refused/probe-error` 计入失败，而不再只统计 Frida 传输异常。当前
全量回归为 161 项通过。

## 后续最高收益动作

1. 在客户端确有 L2 权限且连接活跃时，只抓一只股票的一次订阅，记录
   `LX → FuncID/推送类型` 对照；
2. 保存一页 `1364` 和 `1374` 的解密后原始体，与客户端逐笔窗口逐行对齐，
   标定 `order_type`、数量单位和状态尾字段；
3. 保存一次 `4671/4680` JSON 或内部 Body，验证队列方向和深度数量缩放；
4. 完成上述对照后，再把离线工具接入有权限的会话 transport；在此之前
   不把静态结构包装成“匿名可直连 Level2”。

## 主程序 SDK 回调链补充

继续追踪 `TdxW.exe` 后，SDK 动态加载和回调注册已经闭合：

```text
usercomm.ini
  Other/AutoUseNoSDKL2Agent, Other/SDKL2Agent
        ↓
SDKPlugins/bin/TdxDataSDKPR.dll 或 TdxDataSDK.dll
        ↓ GetProcAddress
fnSetTokenID / fnInitTdxDataSDK / fnSubscribeData / fnReqData /
fnRegisterCallBackFunc / fnExitTdxDataSDK
        ↓
fnRegisterCallBackFunc(TdxW.exe:0x68C750)
```

`TdxW.exe:0x68D450` 是实际注册点，`0x68A540` 负责选择 DLL 并解析导出。
当前安装目录没有这两个可选 SDK DLL，但主程序内的两套包装和解码逻辑仍
完整存在。它们与 `tpbus FastHQ.Subscribe` 是两条应分开记录的路径。
本机全目录复查也没有 `SDKPlugins` 目录或相关配置键；通达信官方公开页面
未提供这两个精确文件名的下载或开发包。因此这里描述的是主程序兼容代码，
不是对当前安装组件的盘点结论，也不建议从非官方 DLL 下载站寻找。当前
客户端真正可动态验证的是已经加载的 `tpbus.dll` 路径。

`fnReqData`/回调类型与客户端内置直连路径的静态对应为：

| SDK 类型 | 静态业务含义 | 内置路径/结构 |
|---:|---|---|
| `1801` | 逐笔成交 | 内置命令 `1363/1364`；SDK 源记录 52 字节；内部记录 20 字节 |
| `1802` | 逐笔委托 | 内置命令 `1373/1374`；SDK 源记录 40 字节；内部记录 20 字节 |
| `1803` | 双边多档盘口 | 内置请求 `1369`，界面按场景请求 `11/1000` 档；固定体 `0x7D10` 字节；通知 `0x551` |
| `18031` | 选中价位的逐笔委托量队列 | 内置请求 `1371`，携带方向/模式、选中价格、游标和上限 `5000`；固定体 `0x4E2C` 字节；通知 `0x551/lParam=2` |
| `1804` | 最多 50+50 个价量数组 | 转成两个最多 50 项的内部数组 |
| `1807/18071` | 行情更新结构 | 写入主行情状态并通知窗口 |

`1803` 的 `11/1000` 请求上限和双边价量结构证明它不是“某一价位下的
委托队列”；`18031` 只有在用户选择某一盘口价位后才发起，并携带所选
价格，证明它才是逐价委托队列。当前仍不把两组的买/卖方向字节强行命名，
该枚举需要实盘样本对照。同理，界面类型 `HQ10_UNIT=30`、`QUEUE_UNIT=32`
与 SDK 数据类型、`FastHQ.Subscribe.LX` 是三套不同枚举。

### SDK 源记录

`1801` 每条 52 字节：

```text
+0  u64 time_raw
+8  f64 price
+16 u64 volume_raw
+36 u32 aux_1
+40 u32 aux_2
+48 u32 direction       1=买、2=卖、其他=中性/未知
```

`1802` 每条 40 字节：

```text
+0  u64 time_raw
+8  f64 price
+16 u64 volume_raw
+24 u32 order_id
+36 u32 record_type      1=B、2=S、3=BC、4=SC
```

主程序再把两者压成 20 字节内部记录。成交内部记录是
`u16 time + s32 price + s32 volume_quotient + u8 remainder + u8 status +`
`s32 aux_1 + s32 aux_2`；委托内部记录把后半段换成
`u8 remainder + u8 side + u8 action + 3 字节保留 + s32 order_id`。
这也独立验证了委托 `B/S/C` 和买撤/卖撤组合。

### `1803` 双边多档盘口固定体

回调源体固定为 `32016`（`0x7D10`）字节：

```text
+0      u32 header_0              语义待实盘标定
+4      u32 header_1              语义待实盘标定
+8      u32 first_side_count      0..1000
+12     u32 second_side_count     0..1000
+16     f64 first_price[1000]
+8016   u32 first_volume[1000]
+12016  u16 first_aux[1000]       每项占 4 字节槽位
+16016  f64 second_price[1000]
+24016  u32 second_volume[1000]
+28016  u16 second_aux[1000]      每项占 4 字节槽位
```

`TdxW.exe:0x69B4D0` 将两侧转换为每档 13 字节的界面记录，还会分别标记
每侧委托量最大的前三档。请求端 `0x69B3F0` 使用命令 `1369`，常规界面
请求 11 档，深度场景请求 1000 档。因此其准确业务边界是“多档/千档
双边盘口”，十档只是其中一个显示子集。

### `18031` 选中价位委托队列固定体

回调源体固定为 `20012`（`0x4E2C`）字节：

```text
+0   u32 header_0          语义待实盘标定
+4   u32 header_1          语义待实盘标定
+8   u32 count             0..5000
+12  u32 quantity[count]
```

`TdxW.exe:0x69B8E0` 把每个数量转换为 6 字节界面记录。用户在盘口中选中
某一价格后，`0x69B800` 才构造命令 `1371` 的 48 字节请求；其尾部包含
方向/模式字节、所选 `float` 价格、游标和 `u16 count=5000`。这条 UI
因果链与单列数量布局共同确认其为“该价格下的委托队列”。

### `1804` 双价格数量队列固定体

2026-08-12 使用已处理的 `TdxW.exe.i64` 复核 `0x68C750`，补齐此前只有用途
描述的固定布局。源体为 432 字节：

```text
+8    f64 first_price
+16   f32 first_quantity[50]
+216  f64 second_price
+224  f32 second_quantity[50]
+424  u32 first_count       0..50
+428  u32 second_count      0..50
```

主程序将它投影成两组最多 50 项的数量数组，但该消费层没有给出可靠的买卖方向
枚举，因此纯 C++ `sdk-1804` 解码器保留 `first/second`，不强行命名买卖侧。

### `1807/18071` 行情更新固定体

`TdxW.exe:0x68C750` 的两个回调类型共用同一转换分支，源体固定为 380 字节：

```text
+0    u64 time_epoch_ms
+8    f64 last_price
+16   f64 open_price
+24   f64 high_price
+32   f64 low_price
+40   f64 amount
+48   u64 volume
+56   u64 special_volume_raw
+64   u32 quote_kind_raw
+68   u32 auxiliary_68_raw
+72   u32 auxiliary_72_raw
+76   f64 pre_close_price
+108  f64 ask_price[10]
+188  f32 ask_quantity[10]
+228  f64 bid_price[10]
+308  f32 bid_quantity[10]
+348  f64 average_bid_price
+356  f32 total_bid_quantity
+364  f64 average_ask_price
+372  f32 total_ask_quantity
```

基础字段由 150 字节主行情记录的独立字段映射交叉确认，十档和尾部四项则与
96 字节扩展行情记录及本地盘口快照的“买均/总买/卖均/总卖”消费顺序一致。
证券身份来自请求/回调注册表，不嵌入源体。`+68/+72` 在该消费点没有稳定业务名，
纯 C++ 解码器保留原始值；类型值 22/23 会走特殊量处理，但这不足以给字段强行命名。

### SDK 回调关联表（本地 25 字节记录）

`TdxW.exe:0x68C510` 在调用 `fnReqData` 后维护一张进程内关联表，每项严格为
25 字节；它不是 SDK 网络体、会话结构或 Token：

```text
+0   u32 callback_key_1_raw
+4   u32 callback_key_2_raw
+8   u16 market_raw             0..2
+10  char code[6]
+16  u8 reserved_zero
+17  u32 data_type
+21  u32 registry_mode_raw
```

SDK 回调 `0x68C750` 用 `callback_key_1_raw + callback_key_2_raw + data_type`
查找记录，再从 `market_raw + code` 恢复证券身份。`registry_mode_raw == 9`
的项在匹配回调后保留，其他值的项在匹配后从表中删除。两个 callback key 的
业务来源在该消费层没有名字，因此离线输出只保留 raw 命名，不将其解释成
session、request id 或 token。`18031` 走独立全局关联，不进入这张表。

纯 C++ `level2 decode --format sdk-correlation` 支持一条或多条精确 25 字节
记录，校验市场、ASCII/NUL 填充和 `+16` 保留字节，并按原生表的最大快照边界
拒绝超过 10001 条的输入。证据为
`output/ida-tdxw-level2-remaining-sdk-20260812.log:502-537,697-736`；既有只读
探针也在 `doc/90-scripts/frida_tdx_level2_probe.js:466-495` 使用同一布局。

## 被动动态探针

新增：

- `doc/90-scripts/capture_tdx_level2.py`：进程选择、SHA-256 预检、场景
  `--label` 和限量 JSONL 输出；不加 `--attach` 时只预检；
- `doc/90-scripts/frida_tdx_level2_probe.js`：只执行 `Interceptor.attach`，
  不改参数、返回值或业务缓冲区，也不创建会话/发送请求；对 `1803/18031`
  和 `111/112` 按上述固定布局输出有限样本，对 `113/114/116` 只输出未知
  schema wire 草图。

观察点覆盖：

1. SDK 批量订阅 `0x68C320`、单票请求 `0x68C510` 和回调 `0x68C750`；
2. 已解码逐笔成交 `0x69EA00`、逐笔委托 `0x69EFF0`；
3. `tpbus FastHQ.Subscribe` `0x10076A7A`，记录 `CODE/SC/LX/OperType`；
4. `tpbus` 的 `111/112` 推送分派 `0x1007B4E8`，结构化输出行情深度或
   买一/卖一队列，并按市场和代码附回最近订阅的候选 `LX`；
5. `tpbus+0x7C0FA` 的 `113/114/116` 解帧完成点，发布前读取 PB 指针和
   长度，不假设证券代码字段或业务消息名。

安全门有三层：启动器先核对运行中 `TdxW.exe` SHA-256，探针核对 PE
内存映像大小，再对每个 Hook 核对无重定位机器码前缀。未知版本默认拒绝；
事件总数、单事件记录数和未知体采样字节数都有硬上限。输出不读取
`RightInfo`、`QSHQToken` 或设备身份。

运行示例：

```powershell
# 只预检，不附加
python doc/90-scripts/capture_tdx_level2.py

# 合法 L2 会话中只观察 600000 的多档盘口，持续 120 秒
python doc/90-scripts/capture_tdx_level2.py `
  --attach --label depth --code 600000 --duration 120 `
  --output output/tdx-level2-depth-600000.jsonl

# 可重复传入各窗口的独立捕获，生成候选映射与置信度
python doc/90-scripts/tdx_level2.py summarize-probe `
  --input output/tdx-level2-depth-600000.jsonl `
  --input output/tdx-level2-queue-600000.jsonl `
  --output output/tdx-level2-summary.json
```

2026-08-02 对当前运行的 `C:\new_tdx\TdxW.exe` 做了短时冒烟测试：主程序
和同目录 `tpbus.dll` 的 SHA-256 均与 IDA 输入一致。新增 PB 观察点首次因
绝对地址重定位使完整机器码校验过严而安全拒绝；改为只通配重定位操作数、
仍核验操作码与后续分支后再次附加，8 个 Hook 全部安装，错误为 0。当时
没有 L2 业务事件，因而
没有把“Hook 安装成功”误报成“真实 Level2 样本已取得”。

动态标定时应固定一只代码，每次只打开一个窗口，并分别用 `transaction`、
`order`、`depth`、`order_queue` 标签观察逐笔成交、逐笔委托、多档盘口和
价位委托队列。`sdk-request/sdk-callback` 给出已知 `data_type`，
`fasthq-subscribe` 给出真实 `LX`，`fasthq-push` 给出 `111/112` 及同代码
时间窗内的候选 `LX`。汇总器只把重复的单候选推送证据评为高置信度；按
标签同现得到的 `LX → data_type` 会明确标为 contextual/ambiguous，不把
相关性伪装成协议定论。
