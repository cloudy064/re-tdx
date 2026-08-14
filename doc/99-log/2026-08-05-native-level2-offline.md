# Level2 离线解码与授权会话只读预检

## 本轮结果

- 新增纯 C++ `level2 build`，精确构造内置逐笔成交 `1364` 和逐笔委托
  `1374` 的 26 字节请求体；只输出字节，不建立连接或发送；
- 新增纯 C++ `level2 decode`，支持 direct transaction/order、SDK
  `1803/18031`、`tpbus 111/112` 及未知 schema protobuf wire 七种格式；
- 新增 `level2 session` 和 `/api/v1/level2/status`，只读检查 TdxW 进程、
  已加载关键模块与已知 SHA-256；多实例时返回候选 PID，由用户显式选择；
- Svelte 新增“Level2 协议实验室”，展示版本前置条件、解码能力和严格边界；
- 原生测试扩展为 15 组，覆盖精确请求字节、截断保护、固定体和 PB wire。

## 精确请求验证

以下命令：

```powershell
tdx-tool level2 build --kind transaction --market 1 --code 600000 `
  --cursor 287454020 --count 1500 --compact
```

输出 26 字节请求：

```text
000000000000100010005405010036303030303044332211dc05
```

其中 `0x0554=1364`、市场号 `1`、代码 `600000`、小端游标
`0x11223344`、条数 `1500` 均与静态恢复布局一致。

## 当前会话验证

本机存在两个 `TdxW.exe` 实例。不指定 PID 时接口返回 `ambiguous=true` 和
候选列表，不会任意选中一个实例；分别指定 PID 后，当前两个实例均识别到
已知 `TdxW.exe` 与 `tpbus.dll`，并列出 `TdxAsioComm.dll`、`TCalc.dll`、
`TaApi.dll` 等关键模块。`ready_for_passive_capture=true` 只表示版本前置条件
满足，不代表拥有 L2 权限，也不代表已经捕获或订阅。

## 安全与证据边界

- `level2 build` 的结果不会自动连接或发送；
- `level2 decode` 只读取用户提供的合法捕获文件，未知 PB 仅报告字段号、
  wire type 与有界样本，不猜字段名；
- `level2 session` 使用 Windows 只读进程/模块查询，不附加进程、不安装 Hook、
  不读取内存、账号、Token 或会话票据；
- 所有输出都显式包含 `entitlement_bypass=false`，会话结果包含
  `attached=false`、`subscription_sent=false` 和 `token_accessed=false`；
- 主动 L2 获取仍需要用户自己的合法授权账号、真实业务事件和独立登录/会话
  证据。当前实现不是权限绕过，也不能据此声称实时 L2 已可用。
