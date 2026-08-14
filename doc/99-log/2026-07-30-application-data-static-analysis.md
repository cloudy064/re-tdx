# 应用消息、数据插件与加密层静态分析

## 目标

利用用户完成的 IDA 数据库，判断 `TDataParse`、`TBigData`、`tpbus`、
`TaApi` 和 `TEncrypt` 的真实角色，并找到下一阶段可动态验证的协议入口。

## 输入

```text
TdxW.exe.i64
TDataParse.dll.i64
TBigData.dll.i64
tpbus.dll.i64
taapi.dll.i64
TEncrypt.dll.i64
libeay32.dll.i64
```

所有被分析 DLL 与 `C:\new_tdx` 当前安装中的同名文件哈希一致。分析期间
只读取二进制和 IDA 数据库，没有读取账户、票据或用户配置内容。

## 新增工具

- `ida_module_inventory.py`：导出入口、两层调用树、关键字符串；
- `ida_dynamic_module_xrefs.py`：主程序动态 DLL/导出名交叉引用；
- `ida_address_xrefs.py`：指定导入槽、函数指针或函数地址的引用与伪代码。

原始 headless 日志保存在 `C:\tmp\tdx_ida_*.log`，不纳入仓库。

## 主要结论

1. `TdxW.exe` 动态加载四组模块，但它们不是一条串行流水线：
   - `TDataParse` 解码本地/缓存记录；
   - `TBigData` 是窗口/公式/表格插件；
   - `TEncrypt` 服务认证、Web POST 和下载；
   - `tpbus` 才是应用会话、事件和行情服务中枢。
2. `TdxW` 调用 `TP_Init` 后创建 `"TdxW"`、`"TdxWL2"` 两个 TPData。
3. `tpbus` 只从 `TaApi` 导入 `TaApi_CreateInstanceEx`，随后注册
   `JobNotify`、创建 `"Sync"` 会话并加载 `datacache.json/BestHost`。
4. 行情订阅通过名为 `FastHQ.Subscribe` 的 `CTAJob_InetTQL` 构造，
   字段包括 `CODE`、`SC`、`LX`、`PkgType`、`OperType`、`PushType`、
   `BatchPush`；`OperType=1/0` 对应订阅/退订。
5. `fn_TGetImageData` 采用“先查询数量，再分配 `1032 * count`，最后解码”
   的两阶段调用；没有证据表明它直接消费 TCP 推送帧。
6. `T_Encrypt` 是 Blowfish，`T_Encode` 是 Base64；RSA 和 HTTPS POST
   的主要调用场景已定位。`T_PostUrlVerify` 关闭证书链和主机名校验。

## 输出

- [应用消息与数据插件层](../02-engine/02-application-data-layer.md)
- [认证与加密辅助层](../02-engine/03-auth-crypto-layer.md)

## 下一步

优先动态记录 `FastHQ.Subscribe` 的序列化 Body 与
`TaApi.dll:0x10008610` 的接收分片，完成一只普通股票的订阅/退订对照。
这比继续盲扫大型 UI DLL 更接近独立协议复现。

若继续补 IDA 数据库，优先处理仅 104,824 字节的 `TJyaid.dll`：
`GetXUserHardInfo` 的输出已确认进入设备标识、签名和行情账号登录参数。
`TPool.dll` 更偏股票池/UI，`AddinMiniQuote.dll` 与主程序期待的
`AddinMiniQuoteEx.dll` 名称不一致，当前收益较低。

> 2026-07-31 更新：该候选已经完成静态分析，结果见
> [TJyaid 设备身份与交易配置静态分析](2026-07-31-tjyaid-device-identity.md)。
> TPool 随后也已完成，结果见
> [TPool 股票池与公式筛选引擎静态分析](2026-07-31-tpool-stock-pool-engine.md)。
