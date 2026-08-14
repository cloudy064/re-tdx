# 安装目录布局

> **状态**：2026-07-30 已对正在运行的 `C:\new_tdx` 做首轮只读盘点。
> 文件计数会因缓存和行情文件持续写入而变化；本文只把路径存在性、PE
> 依赖和已加载模块作为当前版本证据。

## 安装基线

| 项目 | 观测值 |
|---|---|
| 安装根目录 | `C:\new_tdx` |
| 主程序 | `TdxW.exe`，14,350,256 bytes |
| 主程序 SHA-256 | `f5f2e6025a4d80bb1afbcb2a51c753d3c1909e09aa9d1bc8f7c3b701b081f74c` |
| 文件总数 | 约 26,038（运行中动态变化） |
| DLL / EXE | 81 / 6 |
| 顶层目录 / 文件 | 17 / 91 |

可用
[`recon_install.py`](../90-scripts/recon_install.py)
对以后版本重复生成同口径盘点。

## 已确认的顶层目录

```text
C:\new_tdx\
├── BlockMap\
├── chrome\
├── files\
├── funcs\
├── funcs_jy\
├── GNPlugins\
├── jspages\
├── proto\
├── QHPlugins\
├── SEPlugins\
├── T0001\
├── T0002\
├── TCPlugins\
├── Update\
├── vipdoc\
├── webs\
└── ZDPlugins\
```

当前版本没有此前由字符串推断出的 `PYPlugins\`、`SDKPlugins\`、
`BDPlugins\` 或 `chrome49\`。这些名称可能来自兼容旧版的死字符串，不能
继续当作当前安装布局。

## 可执行文件

| 路径 | 大小（bytes） | 初步角色 |
|---|---:|---|
| `TdxW.exe` | 14,350,256 | MFC 主进程 |
| `chrome\tdxcef.exe` | 1,421,712 | CEF 子进程 |
| `AutoUpEx.exe` | 1,963,864 | 更新程序 |
| `NodeTool.exe` | 1,832,856 | 节点工具，待确认 |
| `QsAssist.exe` | 155,112 | 行情辅助程序，待确认 |
| `TdxSP.exe` | 47,592 | 辅助程序，待确认 |

`chrome\tdxcef.exe` 和 `chrome\libcef.dll` 的文件版本均为
`81.3.10+gb223419+chromium-81.0.4044.138`，因此当前版本确定使用
CEF/Chromium 81。

## 关键 DLL 存在性

| 路径 | 大小（bytes） | 状态 |
|---|---:|---|
| `TDataParse.dll` | 207,816 | 存在，导出 2 个同步/图像数据接口 |
| `TdxAsioComm.dll` | 893,792 | 存在，主程序静态依赖且运行时已加载 |
| `SEPlugins\TAsioComm.dll` | 790,416 | 存在，独立通信插件 |
| `SEPlugins\TEncrypt.dll` | 439,136 | 存在，加密/下载接口 |
| `TBigData.dll` | 1,600,344 | 存在，运行时已加载 |
| `tpbus.dll` | 5,729,816 | 存在，运行时已加载 |
| `TPool.dll` | 555,544 | 存在、已反编译；当前运行快照未加载 |
| `TCPlugins\AddinMiniQuote.dll` | 593,168 | 存在；文件名不带 `Ex` |
| `TPyth.dll` | — | 未发现 |
| `TdxDataSDK.dll` | — | 未发现 |
| `TQ.dll` / `TQSRun.dll` | — | 未发现 |
| `TAsioComm1.dll` | — | 未发现 |

“未发现”只约束 2025-11-14 这一安装版本，不代表其它发行版从未包含。

## 主程序 PE 依赖

`dumpbin /dependents TdxW.exe` 确认以下项目内依赖：

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

系统/运行时依赖包括 `mfc100.dll`、`MSVCR100.dll`、`MSVCP100.dll`、
`WSOCK32.dll`、`WS2_32.dll` 等。主程序从 `TdxAsioComm.dll` 只导入：

```text
MakeUserCommModule
DelUserCommModule
```

这比 DLL 文件名字符串更强：主程序与根目录网络模块之间存在确定的静态
工厂接口。

## 运行态快照

观测时有：

- 1 个 `TdxW.exe`；
- 7 个 `tdxcef.exe`；
- 主进程已加载 38 个安装目录内模块，包括 `TdxAsioComm.dll`、
  `TBigData.dll`、`tpbus.dll`、`TaApi.dll`、`TJyaid.dll` 和 CEF 三件套；
- `TDataParse.dll`、`SEPlugins\TAsioComm.dll`、`TEncrypt.dll`、
  `TPool.dll` 在该时点未加载；2026-07-31 对 PID 16216 再次只读枚举时
  仍未加载，符合其按股票池功能触发的加载方式。

当前权限无法通过 CIM 获取 parent PID，所以 CEF 进程的直接父子关系尚待
Procmon 或提升权限后闭合。

## `T0002` 配置与数据分类

只记录路径/扩展名，不归档用户配置内容：

- 缓存目录：`hq_cache\`、`cloud_cache\`、`ds_cache\`、`zst_cache\`；
- 用户目录：`User\`、`pad\`、`note\`、`diary\`；
- 行情/板块文件：`*.dax`、`*.dat`、`*.cfg`、`*.lc1`；
- 本地数据库/压缩包：`*.db`、`*.zip`；
- 节点配置：`newhost.lst`。

`connect.cfg` 与 `T0002\newhost.lst` 都包含行情节点列表。当前连接命中第
18 个节点 `81.71.32.47:7709`；这里仅记录端点匹配，不归档配置全文。

## 后续补证

1. 用 Procmon 记录冷启动，闭合主进程、CEF 和辅助 EXE 的父子关系；
2. 分别在未登录、登录后、打开分时/K 线/L2 页面时记录模块增量；
3. 确认 `TDataParse.dll` 和 `SEPlugins\TAsioComm.dll` 的按需加载条件；
4. 比对客户端升级前后的只读盘点，建立模块版本差分。
