# Phase 1：实际安装与关键导出表首轮侦察

## 目标

把初始化文档中基于字符串和常见版本的目录/模块推断，替换成当前安装的
可复核证据。

## 前置条件

- 安装目录：`C:\new_tdx`
- 运行进程：`TdxW.exe`
- 工具：PowerShell、`netstat`、Visual Studio `dumpbin.exe`
- 全程只读；未读取或归档账户、会话和用户配置内容

## 安装盘点

盘点时约有 26,038 个文件、5.93 GB、81 个 DLL、6 个 EXE、17 个顶层
目录。客户端运行期间持续写缓存，所以计数只用于描述规模。

主程序：

```text
path    C:\new_tdx\TdxW.exe
size    14350256
sha256  f5f2e6025a4d80bb1afbcb2a51c753d3c1909e09aa9d1bc8f7c3b701b081f74c
```

当前安装只有 `chrome\`，`tdxcef.exe` / `libcef.dll` 均标记为
CEF 81.3.10、Chromium 81.0.4044.138。旧文档中的 `chrome49\` 不适用于
此版本。

## 运行态

- 1 个 `TdxW.exe`；
- 7 个 `tdxcef.exe`；
- 主进程加载根目录 `TdxAsioComm.dll`、`TBigData.dll`、`tpbus.dll`、
  `TaApi.dll` 等 38 个安装目录内模块；
- 当前权限无法查询 parent PID，也无法用 `Get-NetTCPConnection` 枚举，
  但 `netstat -ano` 可读取连接。

唯一观察到的主进程 TCP 连接：

```text
198.18.0.1:<ephemeral> -> 81.71.32.47:7709 ESTABLISHED
```

本机地址 `198.18.0.1` 可能受本地网络/代理环境影响，不能当作 TDX 协议
属性。远端节点同时出现在 `connect.cfg` 与 `T0002\newhost.lst` 的第 18
项，两份节点列表均使用 7709 端口。

## 主程序静态依赖

项目内 DLL：

```text
TCalc.dll
TdxAsioComm.dll
Viewthem.dll
TJyaid.dll
TMarquee.dll
TQQCalc.dll
invest.dll
TGear.dll
TControl.dll
RibbonBar.dll
```

`TdxW.exe` 从 `TdxAsioComm.dll` 只静态导入
`MakeUserCommModule` / `DelUserCommModule`。

## 关键 DLL 导出

### `TDataParse.dll`

```text
fn_TGetImageData
fn_sync_getdata
```

### `TdxAsioComm.dll`

除 `VUserComm` 构造、赋值和虚表外：

```text
MakeUserCommModule
DelUserCommModule
```

### `SEPlugins\TAsioComm.dll`

```text
MakeUserCommModule
DelUserCommModule
```

### `SEPlugins\TEncrypt.dll`

```text
T_DownloadFile
T_DownloadFileClean
T_DownloadFileInit
T_DownloadFileProxyInit
T_Encode
T_Encrypt
T_PostUrlVerify
T_RSAEncode
T_RSAEncode2
```

该 DLL 依赖 `LIBEAY32.dll`，此前文档中的 `libcrypto-1_132.dll` 推断不
适用于当前文件。

### `TBigData.dll`

```text
BigDataUnit_Create
BigDataUnit_ProcessMsg
BigDataUnit_SetCfgName
BigDataUnit_SetScheme
BigData_GetCurrentGPInfo
BigData_GetVersion
BigData_Init_Environ
BigData_PopDlg
BigData_RegisterCallBack
BigData_Uninit
```

### `tpbus.dll`

关键导出包括：

```text
GetEventBus
PostTask
PostSyncTask
PostDelayedTask
TP_Init
TP_CreateTPData
TP_GetFile
TP_Exit
TaApi_CreateAppCore
TaApi_DestroyAppCore
```

### `TPool.dll`

导出 `TPool_Init`、`TPool_StartRun`、`TPool_ProcessPool`、
`TPool_RegisterCallBack`、`TPool_GetPoolGPInfo` 等 11 项。接口形态更像
业务/UI pool 单元，暂不能把它断言为网络连接池。

> 2026-07-31 更新：IDA 静态分析已确认它是 TdxW 回调供数、
> `TCalc` 条件计算和本地文件输出组成的股票池引擎，不是网络连接池。
> 详见 [TPool 静态分析](2026-07-31-tpool-stock-pool-engine.md)。

### `TCPlugins\AddinMiniQuote.dll`

```text
Addin_GetObject
MiniQuote_GetVersion
```

## 缺失模块

当前安装未发现：

```text
TPyth.dll
TdxDataSDK.dll
TQ.dll
TQSRun.dll
TAsioComm1.dll
AddinMiniQuoteEx.dll
```

最后一项在当前版本对应的是不带 `Ex` 的 `AddinMiniQuote.dll`。

## 工具结果

- 新增只读脚本 `doc/90-scripts/recon_install.py`；
- 3 个单元测试通过；
- `dumpbin` 导出/依赖扫描成功；
- FLOSS 可执行文件存在，但沙箱内启动被拒绝，未完成混淆字符串补扫。

## 结论

1. 当前主网络模块是根目录 `TdxAsioComm.dll`，不是
   `SEPlugins\TAsioComm.dll`；
2. 当前版本的目录结构、CEF 版本和可用插件与初始化字符串推断有明显差异；
3. Phase 1 的导出表验收门已达到，但冷启动 parent PID、Frida 和多时序
   网络拓扑仍未完成。
