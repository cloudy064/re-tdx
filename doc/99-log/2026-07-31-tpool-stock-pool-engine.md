# TPool 股票池与公式筛选引擎静态分析

## 目标

利用用户新生成的 `TPool.dll.i64`，确认名称中的 Pool 是否表示网络
连接池，并恢复 TdxW 的加载方式、回调边界、运行控制和导出数据布局。

## 输入与边界

```text
ida/TPool.dll
ida/TPool.dll.i64
ida/TdxW.exe.i64
```

工作区 DLL 与 `C:\new_tdx` 安装副本的 SHA-256 均为：

```text
EA828C10399823C4985F955C9FC0B3D1259F9CA507FE79E4530510F7CBFF42D1
```

TPool 数据库大小为 8,612,716 字节，共识别 1,549 个函数。分析只读取
二进制、IDA 数据库和当前进程模块列表，没有打开用户股票池配置，也没有
触发或修改任何股票池。

## 导出与依赖

除 `DllEntryPoint` 外共有 11 个业务导出：

```text
TPool_Init
TPool_RegisterCallBack
TPool_StartRun
TPool_StopRun
TPool_Uninit
TPool_ProcessPool
TPool_GetPoolStatus
TPool_GetPoolGPInfo
TPool_ChildWindow_Create
TPool_ChildWindow_Delete
TPool_DllPreTranslateMessage
```

主要依赖 MFC100、USER32、GDI32、GDI+ 和 `TCalc.dll`。没有 WSOCK、
WS2_32、WinINet、WinHTTP、curl 或加密库导入；动态 DLL 字符串也只显示
UI 辅助模块。因此 TPool 不是网络连接池，也不自行建立行情连接。

## TdxW 加载与回调

`TdxW.exe:0x731270` 按需加载 TPool 并解析全部 11 个业务导出。
`TdxW.exe:0x731570` 完成初始化：

```text
TPool_Init(
    install_path,
    user_path,
    <install_path>\connect.cfg,
    CMainCalcInterface *);

TPool_RegisterCallBack(
    sub_61B630,
    sub_62B8D0,
    sub_630950);

TPool_StartRun(main_hwnd, 2849);
```

- `sub_61B630` 包装 TdxW 的 `sub_60FFF0` 数据查询接口；
- `sub_62B8D0` 是带消息类型的 UI、路由和宿主动作分派；
- `sub_630950` 是按操作码分派的通用数据/辅助查询接口。

TPool 使用第一回调的数据类型包括
`4/102/104/105/111/122/125/126/134`，使用第三回调的操作包括
`10/31`。这些是 TdxW 进程内回调编号，不是已经确认的网络消息号。

2026-07-31 对运行中的 TdxW PID 16216 做只读模块枚举时，TPool 尚未
加载，符合只有进入股票池功能后才初始化的按需加载行为。

## 规则执行模型

TPool 使用工作线程执行每个已启动的股票池。池定义位于：

```text
tpool\<pool-name>.xml
```

XML 结构包含：

| 节点 | 内容 |
|---|---|
| `cell` | 条件节点、位置、颜色、文本 |
| `flow` | 起止节点、转移方式、时间条件 |
| `func` | 指标/公式、周期、比较操作和值 |
| `stk` | 市场、代码、入池时间/价格、收益、涨幅、成交量等 |
| `psatt` | 删除、提示、声音、保存板块、历史记录 |

执行时通过 TdxW 回调取得证券和行情记录，再调用 `TCalc.dll` 的
`GetIndexInfo`、`NewOneCalc`、`DelOneCalc` 计算条件。输出包括
`_in_pool_his.txt`、`_status_his.txt`、日期 `.dat/.log` 和
`blocknew\*.blk`。`http://www.treeid/openpool_<name>` 只是 TdxW 内部
UI 路由。

## 运行控制

TdxW 的 treeid 命令与 `TPool_ProcessPool(name, operation)` 对应：

| 命令 | operation | 效果 |
|---|---:|---|
| `stoppool_<name>` | `-1` | 停止并回收对应工作线程 |
| `pausepool_<name>` | `0` | 暂停 |
| `runpool_<name>` | `1` | 启动或恢复 |
| `openpool_<name>` | `2` | 打开/切换到该池 |

## 导出结构

`TPool_GetPoolStatus`：

- 每项 131 字节；
- 偏移 `0`：2 字节运行状态；
- 偏移 `2`：系统池标志；
- 偏移 `3`：池名称字符串；
- TdxW 一次最多读取 50 项，并检查是否存在状态 `1`。

`TPool_GetPoolGPInfo`：

- 支持先以空缓冲查询数量；
- 每项输出 34 字节；
- 偏移 `0`：2 字节 `setcode`；
- 偏移 `2`：32 字节证券代码区。

TdxW 按 `34 * count` 分配输出缓冲并缓存股票列表。

## 结论与收益

- “TPool 是网络连接池”的旧假设已经排除；
- 它是本地股票池、条件公式、状态窗口和文件输出引擎；
- 它不会直接帮助恢复 7709 帧，但其 `sub_61B630` 回调是观察 TdxW
  内部行情记录类型与字段的可选动态锚点；
- 当前最高收益仍是 `FastHQ.Subscribe` 序列化 Body、TaApi 接收分片和
  TdxAsioComm 收发的同一时序对齐。

## 2026-08-05：规则执行补充

继续沿 `sub_1001B9E0` 拆出比较帮助函数后，确认 709 字节 `func` 运行结构
的关键偏移：

| 偏移 | XML 字段 | 用途 |
|---:|---|---|
| `+656` | `nfirst` | 第一输出序号 |
| `+658` | `cfirst` | 第一输出名 |
| `+674` | `noperate` | 操作符 |
| `+676` | `nsecond` | 第二输出序号；负数表示使用常数 |
| `+678` | `csecond` | 第二输出名 |
| `+694` | `fsecond` | 第二操作数常量或排名数量 |
| `+702..704` | `bnost/bnotp/bnotq` | ST、停牌、退市类排除条件 |
| `+705` | `nperiodnum` | 计算回看长度 |

`noperate` 已从实际帮助函数逐项确认：

| 值 | 语义 |
|---:|---|
| `0` | 等于 |
| `1` | 第一输出大于第二输出/常数 |
| `2` | 第一输出小于第二输出/常数 |
| `3` | 第一输出上穿第二输出/常数 |
| `4` | 第一输出下穿第二输出/常数 |
| `5` | 精确第 N 名（正负值决定首尾方向） |
| `6` | 前/后 N 名 |
| `7` | 排名尾段选择 |
| `8` | 上一根为局部低点 |
| `9` | 上一根为局部高点 |

纯 C++ 统一工具新增：

```powershell
tdx-tool pool inspect --root C:\new_tdx
tdx-tool pool inspect --input C:\new_tdx\T0002\tpool\example.xml
tdx-tool pool evaluate --input example.xml --limit 20 --pages 1
```

`pool inspect` 自动识别 UTF-8/GB18030，只读输出 flow、func、stk、公式引用、
证券覆盖和逐规则兼容性。`pool evaluate` 对 MA/MACD/KDJ/RSI/BOLL 的
比较、上穿/下穿和拐点逐票求值，不加载 DLL、不启动工作线程、不写通达信
目录。跨证券排名和多节点 flow 组合尚未作为最终池结果执行，JSON 中固定用
`flow_graph_evaluated=false` 与 scope 字段说明边界。

仓库样例 `output/tpool-native-fixture.xml` 已用两只证券验证：日线
`MACD.DIF > 0` 共完成 2 条规则求值，两条均命中。全部原生测试为 14/14。
