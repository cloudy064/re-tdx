# 整体架构

> 更新时间：2026-07-31。本文优先记录当前安装和运行态证据；未被调用关系
> 闭合的模块角色明确标为推断。

通达信 PC 端是 MFC 原生 C++ 主程序、CEF 81、多组业务插件和独立消息/
网络组件组成的混合架构。

## 运行进程

| 进程 | 当前证据 |
|---|---|
| `TdxW.exe` | 1 个主进程，PE32、MFC 10.0、无壳 |
| `chrome\tdxcef.exe` | 7 个实例；CEF 81 / Chromium 81.0.4044.138 |

当前权限未取得 parent PID，因此只确认进程集合，不把 7 个 CEF 实例的
直接父子关系写死。

## 已确认分层

| 层 | 模块 | 证据 |
|---|---|---|
| 主框架/UI | `TdxW.exe`, `RibbonBar.dll`, `PTFrame.dll` | 静态依赖和运行模块 |
| Web UI | `chrome\TCefWnd.dll`, `libcef.dll`, `tdxcef.exe` | 文件版本和运行进程 |
| TCP 传输 | `TdxAsioComm.dll` | 主程序静态导入工厂、运行时加载、IOCP/WSA 交叉引用 |
| 可选 TCP 插件 | `SEPlugins\TAsioComm.dll` | 独立工厂和同类 Asio 实现；当前快照未加载 |
| 应用消息 | `tpbus.dll` → `TaApi.dll` | TPData、SessionManager、`CTAJob_InetTQL` 与 `FastHQ.Subscribe` 已闭合 |
| UI 数据插件 | `TBigData.dll` | 窗口单元、配置、消息转发、债券公式/表格 |
| 本地记录解析 | `TDataParse.dll` | 主程序按需加载，以两阶段接口输出 1032 字节记录 |
| 股票池/公式筛选 | `TPool.dll` + `TCalc.dll` | TdxW 回调供数、本地条件图执行、XML/历史/板块输出 |
| 认证/Web 辅助 | `SEPlugins\TEncrypt.dll` | Blowfish、Base64、RSA、HTTPS POST 与下载 |

`TPool.dll` 已确认不是网络连接池。它没有 socket/HTTP 导入，运行线程
通过 TdxW 注册回调取得证券、行情和历史数据，再借助 `TCalc` 执行条件
公式并更新本地股票池状态。

## 当前网络路径

```text
connect.cfg / newhost.lst
          │ node + port
          ▼
TdxW.exe::sub_413B20              同步建立连接
TdxW.exe::sub_414240              worker + 发送队列 + 读取循环
          │ VUserComm 15 槽接口
          ▼
TdxAsioComm.dll::CUserComm        Boost.Asio + IOCP + WSARecv/WSASend
          │
          ▼
81.71.32.47:7709                  当前一次运行态观测
```

从读取循环收到的 `(buffer, length, context)` 会交给 `TdxW.exe` 上层回调。
首个行情帧边界判断尚未定位，所以此处不能继续画成
`TdxAsioComm -> TDataParse` 的已确认直连。

详见 [UserComm 网络传输层](../02-engine/01-network-layer.md)。

## 当前应用消息链

```text
TdxW
  └── tpbus::TP_Init
        ├── TPData("TdxW")
        ├── TPData("TdxWL2")
        └── SessionManager
              └── TaApi_CreateInstanceEx
                    └── CTAEngine / CTAJob_InetTQL
                          └── FastHQ.Subscribe
```

`FastHQ.Subscribe` 已恢复 `CODE / SC / LX / PkgType / OperType /
PushType / BatchPush` 字段。`TDataParse` 和 `TBigData` 是从主程序分别
按需加载的旁支，没有发现 `TDataParse -> TBigData` 直连。

`TdxAsioComm` 的 7709 UserComm 路径和 `tpbus -> TaApi` 应用会话路径
暂时并列；尚缺证据证明二者是同一连接的上下层。

详见[应用消息与数据插件层](../02-engine/02-application-data-layer.md)。

## 统一工具内部模块边界

纯 C++ `tdx-tool` 的 API 契约巡检已从单一总控翻译单元拆开，命令编排不再
持有全部业务断言和 POST 正文：

```text
recon.cpp                         CLI / HTTP 巡检编排
  ├─ recon_contract_catalog.cpp   契约 ID、标题、路径和 profile
  ├─ recon_contract_requests.cpp  POST action/body 映射表
  ├─ recon_contract_formula.cpp   公式与绘图契约验证
  └─ recon_contract_market.cpp    验证策略链
       ├─ market_data             行情、机构、债券
       ├─ market_research         主题、研究、情报
       └─ market_corporate        公司事件、发行、日历、缓存
```

公共 JSON 取值与断言工具位于内部头文件 `recon_contract_internal.hpp`，不扩大
公开 API。新增契约先进入 catalog；需要 POST 时再登记 request map；响应断言只
进入所属领域验证器，避免命令层重新长成条件分支集合。

公式解释器也按注册、运行时、目录和上下文职责拆分：

```text
formula_engine.cpp                 词法/语法、静态分析、AST 编排与结果文档
  ├─ formula_registry.cpp          函数/符号能力集合和依赖分类
  ├─ formula_functions.cpp         数值函数与原生状态机运行时
  ├─ formula_catalog_internal.cpp  技术指标查找、输出序号和执行策略校验
  ├─ formula_context.cpp           财务、行情、板块等宿主上下文编排
  └─ formula_nested.cpp            CALCSTOCKINDEX 有界跨证券嵌套执行
```

`formula_engine.cpp` 现约 4,900 行。`INSORT/INSUM` 与
`CALCSTOCKINDEX` 共用目录选择器，避免各自复制指标类型、参数和输出顺序判断；
嵌套模块独立维护证券解析、3,000 根预热、同周期下载、日期时间对齐、单缺口续值、
四层深度上限和循环检测。

HTTP 服务也已按目录、公式控制器和运行编排拆分：

```text
server.cpp                         套接字生命周期、路由编排与市场接口控制器
  ├─ server_catalog.cpp            功能目录与 OpenAPI 路径注册
  └─ server_formula.cpp            公式、云计算、TPool、扫描与回测控制器
```

公式控制器通过只持有必要引用的 `FormulaHttpState` 访问根目录、板块、公式库和
JSN 目录，不依赖完整 `ApiState`。`server.cpp` 已降至约 4,700 行；新增公式路由
进入独立控制器，固定路径说明进入目录模块，避免总控文件再次超过 5,000 行软阈值。

## 已排除的当前版本假设

- 当前安装不是 CEF 49，而是 CEF 81；
- 主程序使用根目录 `TdxAsioComm.dll`，不能把
  `SEPlugins\TAsioComm.dll` 写成唯一主网络层；
- 当前安装未发现 `TPyth.dll`、`TdxDataSDK.dll`、`TQ.dll`、
  `TQSRun.dll`、`TAsioComm1.dll`；
- 未发现的旧模块字符串可能是兼容代码或历史残留，不能作为当前组件图。

## 与同花顺的当前可比项

| 层 | 同花顺 | 通达信 |
|---|---|---|
| 主进程保护 | UPX（已知版本） | `TdxW.exe` 无壳 |
| UI | Qt/CEF/HTMLayout 混合 | MFC + CEF 81 |
| 网络调度 | 内置 IOCP | `TdxAsioComm::CUserComm` 封装 IOCP |
| 消息总线 | `hx_ipc_core` 等 | `tpbus` 事件/任务总线 + `TaApi` CTAEngine |
| 数据模型 | 已识别 CDataType 链 | 实时主表仍待定位；`TBigData` 已排除为全市场主表首选 |
