# tc.dll 交易与 L2 边界

日期：2026-08-12

## 结论

用户新生成的 `ida/tc.dll.i64` 补齐了此前 DLL 盘点中的最后一个数据库。静态证据
表明 `tc.dll` 是通达信交易客户端核心：它维护资金账户、券商交易连接、交易窗口、
委托快捷动作及 TLS/国密隧道。它不是行情主站协议或 L2 逐笔数据解码器。

尤其需要澄清的是，导出名 `TC_GetL2Info/TC_SetL2UserInfo` 不构成绕过订阅的 L2
入口。前者从已经登录的交易客户端对象取得 XML，并读取 `Summary/L2USER` 与
`Summary/L2PASS`；后者只把调用方给出的三个字符串保存到客户端对象。二者都依赖
已有宿主、账户和服务端授权，不生成权限，也不返回 L2 行情记录。

因此本 DLL 当前只用于边界证据，不迁入纯 C++ 公共行情工具。继续投入的收益低于
复用已处理的 `TCalc.dll/TdxW.exe/TDXDeep.dll`，完成用户公式文件的只读解析。

## 导出面

当前 DLL SHA-256：

```text
5926F4154DAEE68C6D766FD38317D3F3CB19FF0EA7224B9CDBDCA2221974F7AF
```

39 个导出中，业务入口可分为：

- 环境和回调：`TC_Init_Environ`、`TC_RegisterCallBack*`、`TC_Uninit`；
- 交易登录：`TC_Login`、`TC_Login2`、`TC_GetLoginRet`、`TC_GetJyStatus`；
- 账户/权限：`TC_GetRightInfo`、`TC_GetClientInfo`、`TC_OperateUser`；
- 交易界面：`TC_CreateAll`、`TC_GetDlg`、`TC_DoLevinJy`、`TC_DoGridJy`；
- L2 凭据桥接：`TC_GetL2Info`、`TC_SetL2UserInfo`；
- 宿主消息：`TC_CORE_MESSAGE`、`TCUnit_ProcessMsg`、`TCUnit_SendAsyCall`。

`TC_Login` 与 `TC_Login2` 分别接收每账户 800 字节和 1824 字节的宿主记录，先用
常量 `tdx_zjzh_tztz_@#$` 解开复制体，再转换为内部 3274 字节账户对象。记录包含
资金账户、股东账户、分支和安全类型等交易态字段；这不是行情账号登录。

## L2 凭据链

`TC_GetL2Info -> sub_10033E60` 的精确行为是：

1. 从宿主 vtable 槽 608 获取当前交易客户端；
2. 从客户端槽 228 获取 XML 文档；
3. 在 `Summary` 节点读取 `L2USER`、`L2PASS`；
4. 复制到调用方缓冲区。

生成同一 XML 的 `sub_10155730` 会建立 `LoginInfo`、`ConnectInfo`、`ProxyInfo`、
`Qos`、`ServerStatus`、`SecurityMode` 和 `Summary` 节点。该版本初始写入的
`L2USER/L2PASS` 是空字符串，真实值必须由既有宿主/已授权会话提供。

`TC_SetL2UserInfo -> sub_10032C20 -> sub_10026330` 只将三个参数格式化保存到对象的
三个字符串成员，不执行认证、权限升级或行情订阅。

## 网络边界

DLL 自带交易网络栈，直接导入 `WSASocketA/WSAConnect/WSASend/WSARecv`，并包含
OpenSSL、TLS、国密、代理和 `TCPTunnelMode` 路径。这证明它能独立连接券商交易
服务，但不能把它等同于 `TdxAsioComm/tpbus` 的行情会话。交易动作字符串包括
`stock.buy`、`PC.Stock.Buy`、`PC.Stock.Sell`，与账户和委托职责一致。

## 证据产物

- `output/ida-tc-module-inventory-20260812.log`
- `output/ida-tc-export-probe-20260812.json`
- `output/ida-tc-core-probe-20260812.json`
- `output/ida-tc-business-strings-20260812.json`
- `output/ida-tc-l2-user-bridge-20260812.json`

本轮仅新增静态证据和文档，没有调用真实交易接口、读取账号值、启动候选服务或
修改正式服务。
