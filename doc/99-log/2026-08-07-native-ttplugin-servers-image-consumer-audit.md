# TTPlugin 服务器配置与 TDataParse 消费方全量审计

## 目标

继续收口两个容易被静态字符串误导的边界：TTPlugin 的 `Target`、服务器 URL
和当前连接信息究竟是什么；TDataParse 线标签 `1C..1F` 及返回后的 `0D`
文本区是否只是尚未找到消费方。全部新增运行能力必须是纯 C++、只读、离线
且脱敏。

## TTPlugin 调用链结论

`TTPlugin.dll:0x100BCA10` 是服务器 URL 解析器，已恢复以下格式：

- 直连：`tcp://host:port`、`tcp6://[ipv6]:port`、`ssl://host:port`；
- 代理：`http://proxy:port/target:port`、`socks4://...`、`socks5://...`；
- authority 可带 `user:pass@`，端口范围为 `1..65534`；
- `TDXProxy` 是单独的三段逗号配置，不能作为普通 URL 原文输出。

`0x101E0650` 通过 `GetPrivateProfileStringA` 或宿主配置回调读取
`MDUserServerNormal_Num/%d`、`MDUserServerExtend_Num/%d`；共同构造链还覆盖
`MDUserServer_Num/%d` 和 `TraderServer_Num/%d`。当前安装的 9 个小型
INI/CFG/CONF/TXT 候选中没有这些落盘键，这只说明“文件中未发现”，不排除
宿主回调在运行时注入。

`HQDataService.Target` 由构造器清零，经 `0x10286350` 接收宿主数值选项，
`0x1028A6D0` 把它复制到 `CTAJob_Redirect`，任务对象最终只保存一个字节。
因此它是调用方提供的 `uint8` 路由号，不是 URL 或服务器地址。

`0x10324DE0` 的 `CurrentConnectInfo` 返回已连接地址和端口，
`CurrentConnectInfoEx` 另含 HostID 与辅助路由文本。二进制还提供登录标识、
密码、机器信息等敏感 getter；新工具不调用任何运行时 getter。

主要静态证据：

- `output/ida-ttplugin-target-connect-xrefs.json`
- `output/ida-ttplugin-server-url-helpers.json`
- `output/ida-ttplugin-target-config-chain.json`
- `output/ida-ttplugin-export-dispatch.json`
- `output/ida-ttplugin-user-constructor-xrefs.json`

## 纯 C++ 离线命令

新增：

```powershell
tdx-tool recon ttplugin-servers --root C:\new_tdx
tdx-tool recon ttplugin-servers --input <config.ini> --url <caller-owned-url>
```

它发现或读取小型配置文件，解析四个服务器族和六种 URL，输出主机、端口、
代理/目标结构及是否含凭据。原始 URL、账号密码和 `TDXProxy` 内容永不进入
JSON；单元测试同时用配置 URL 和调用方临时 URL 注入哨兵秘密，证明输出中均
不存在原值。命令不加载 DLL、不查询运行时连接、不登录且不发送网络请求。

当前只读扫描结果位于 `output/tdx-ttplugin-server-config.json`：9 个候选文件、
0 个匹配 section、0 个声明服务器；边界字段确认 `credentials_emitted=false`
和 `network_sent=false`。

## TDataParse 全部消费者

对 TdxW 中已解析 `fn_TGetImageData` 函数指针的全部直接交叉引用逐一反编译：

| 入口 | 读取用途 | 是否读取 `1C..1F/0D` |
|---|---|---|
| `0x59DA50` | 昨收 | 否 |
| `0x5B4DD0` | 时间、最新价、持仓、量额、价格/档位 | 否 |
| `0xA19E10` | 深度聚合 | 否 |
| `0xA26C90` | 分钟 UI、OHLC、深度、`1G..1J` | 否 |
| `0xA637E0` | 深度和买卖队列 | 否 |
| `0xA85600` | 时间间隔与成交量差 | 否 |

`0x7218F0` 只负责 `GetProcAddress`，不计作消费入口。由此可以比“业务含义待
证明”更精确地说：`1C/1D/1E/1F` 是线协议中保留的 `double` 字段，`0D` 是
168 字节文本区，但当前 TdxW 构建返回后没有消费它们。`0D` 在解析器内部仍
参与状态继承和相邻记录去重；不能据此猜测名称，也不能断言未来版本永远不用。

证据为 `output/ida-tdxw-tgetimagedata-callers.json`。纯 C++ 输出升级为
`tdx-image-data-snapshots-v3`，新增 `consumer_audit`，记录六个入口、加载器、
未消费标签和 `preserved_unconsumed_in_current_build` 状态；1032 字节布局及
原 DLL 差分基线不变。

## 验收与发行

- 完整 C++ 测试：95/95 通过；
- 正式 HTTP 契约：130/130 通过，131 次网络请求、0 失败；
- 正式功能目录：141 项；
- 发行文件 SHA-256：
  `7D1280FF980A764D89D8CDF920BBE277EC0D36595925E11DFB2B259145205F49`；
- 正式服务 PID `2716`，监听 `127.0.0.1:8765`；
- 契约报告：`output/tdx-api-contracts-full.json`。

## 剩余边界

- 9221 扩展市场剩余状态字段和 `LX/data_type/push_type` 仍需合法运行样本
  交叉验证；
- 服务器配置未在当前文件中落盘不代表运行时不存在，后续只能在调用方自己
  的合法上下文中观察非敏感 CurrentConnectInfo 元数据；
- 本轮不实现主动登录、L2 订阅、票据读取或权限绕过。
