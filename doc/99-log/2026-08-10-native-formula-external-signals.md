# TCalc EXTERNVALUE/EXTERNSTR 外部信号文件闭环

## 结论

纯 C++ 公式解释器新增 `EXTERNVALUE(namespace,id)` 与
`EXTERNSTR(namespace,id)`，并新增诊断命令：

```powershell
tdx-tool formulas extern-signals --root C:\new_tdx --market sz --code 000001
```

它们不依赖 L2、登录、券商账户或插件 DLL。数据来自：

- `T0002/signals/extern_user.txt`
- `T0002/signals/extern_sys.txt`

每条记录严格按 `market|code|external_id|text|float_value` 解析。当前
`C:\new_tdx\T0002\signals` 目录存在但两份文件均不存在，因此平安银行正式公式
响应返回数值 0 和文本单空格。这是原宿主缺失哨兵，不是数据接入报错。

## 回调纠错与原生链路

本轮纠正了一个重要的早期判断。`CMainCalcInterface::RegisterCallBackFunc` 依次注册
`sub_61B630`、`sub_62B8D0`、`sub_630950`；EXTERN 两个处理函数调用的是第一个
回调，而不是第三个。此前追到的 `sub_630950 -> Viewthem.dll INFO_ShowGN(9)` 是
相邻 UI 功能，不属于 EXTERN 数据链。

正确链路为：

```text
TCalc EXTERNVALUE/EXTERNSTR
  -> first registered formula callback
  -> TdxW sub_61B630
  -> sub_60FFF0 selector 37
  -> sub_4FDF30 lookup
```

selector 37 会把输出缓冲区清零、把文字区初始化为单空格，并把当前证券与两个
公式参数交给查找器。两张表首次使用时分别从上述文件加载。

## 精确查询语义

- namespace 参数先按 TCalc 的 float32 路径转整数，再取低字节；0 选择 user，
  非 0 选择 system，因此 256 仍选择 user；
- id 参数同样先经 float32 转整数；
- 市场 0..2 按市场号和证券代码数值匹配；
- 扩展市场 31 与 71 互相兼容并按代码字符串匹配，其他扩展市场要求市场号和代码
  都相同；
- 重复键按文件顺序第一条记录命中；
- 命中时数值使用记录中的 float32，文本执行宿主 AllTrimEx；
- 缺记录时 `EXTERNVALUE` 为 0，`EXTERNSTR` 为一个空格。

TdxW 的 system 表加载还带有客户端运行版次门控。无头工具没有对应的专有运行时
全局量，因此只要文件存在就解析，并在诊断结果中明确报告没有应用该门控；不会
伪造宿主版次。

## 实现与验证

`native/src/external_signals.cpp` 实现 GBK 文件解析、首条命中目录和诊断 JSON；
`formula_context.cpp` 在检测到依赖时加载当前证券的数值/文本绑定；
`formula_engine.cpp` 实现 namespace 低字节、字符串求值和能力报告；服务端公式
GET/POST、扫描、策略与回测由同一上下文路径受益。测试覆盖 user/system、重复键、
31/71 兼容、256 命名空间、缺失哨兵、中文文本及元数据。

能力变化：

- 支持函数由 230 增至 232；
- 自动符号保持 71；
- 390 条静态注册名的识别并集由 285 增至 287，剩余 103；
- 379 条内置公式保持源码、语法、数值安全和展示可信 `379/379`，降级数值输出 0。

验证结果：

- 全量构建成功；
- CTest `102/102`；
- 候选专项 API `3/3`；
- 候选 full API `218/219`，唯一失败是独立的 `jsn-discovery-live` 在 31.7 秒出现
  WinHTTP 12002；60 秒单项重试 `1/1` 通过；
- 正式专项 API `3/3`；
- 正式平安银行公式末点为 `V=0, NS=0, S=1`，其中 S 是
  `STRCMP(EXTERNSTR(0,999),' ')`。

主要证据：

- 覆盖报告 `output/native-formula-coverage-external-signals-v11.json`，SHA-256
  `1C2BC38D16B8EC9227CEEC9376C0971F8C18DFF9D5C794244020434117E33FAB`；
- 注册表差分 `output/native-formula-registry-next-audit-v11.json`，SHA-256
  `6E1EC5E0EE60BDC7923548F551B6B01E59A594B318728133B9DF2B8B50D9936A`；
- selector 37 探针 `output/ida-probe-tdxw-extern-selector37-v2.log`，SHA-256
  `27502DE9B528348667590BDECA60CB2351743945E40F3A3DCB0497F00B72FA5F`；
- 正式公式响应 `output/api-formula-external-signals-v11-formal.json`，SHA-256
  `F33509330D2212629DE1775F47909496436CC6F12EDB1EFB52CACD49082A6FAF`；
- 正式专项契约 `output/api-contract-external-signals-v11-formal.json`，SHA-256
  `C7A7154E9450002CC3FD0AC1DBD066462880A5F55456F7EDDA180576A66150EB`。

## 部署与下一步

正式 EXE 为 18,935,936 字节，SHA-256
`32E3937072C5D2BBD8EA4E1A427D4BB89AC44764636BBF426C1F154AA465B11C`。
服务 PID 27796，仅监听 `127.0.0.1:8765`，健康状态为 `native_cpp=true`、
`python_runtime=false`，stderr 为 0。上一正式版保存在
`output/tdx-tool-external-signals-v11-predeploy-rollback-20260810.exe`，SHA-256
`457BC6C12DA6473A4B7CF31CF2064CB51ABD089DD76AA5109E751250A5DBCD3A`；候选
8875 已关闭。

下一项优先审计静态注册表中相邻的 `EXTDATA_USER`（opcode 1298，
`sub_100116B0`），因为它可能提供按时间对齐的用户外部序列。只有回调载荷、时间
索引、刷新和缺失语义全部闭合后才进入纯 C++ 实现。其后再考虑
`DRAWSL/DRAWBMP/DRAWGBK/DRAWRECTREL` 展示函数。L2、账户、券商私有信号和
通用插件回调继续排除。
