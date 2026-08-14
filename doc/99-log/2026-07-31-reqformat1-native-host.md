# reqformat=1 TPData 原生宿主与当前路由状态

> 日期：2026-07-31  
> 范围：`TPData100.dll`、`TdxTopicView100.dll` 和
> `TdxZdView100.dll`。  
> 目标：恢复客户端真实 ABI，不用 Frida 注入即可独立验证格式 0—3。

## 已恢复 ABI

`TPData100.dll` 导出并由云页面实际使用：

- `ITPConn_SetEnvironPath`
- `ITPConn_Init` / `ITPConn_UnInit`
- `ITPConn_SetUser`
- `ITPConn_LoginAnony` / `ITPConn_IsLogined`
- `ITPConn_GetSession` / `ITPConn_SetActiveSession`
- `CreateFetchDataHandle` / `CreateFetchDataHandleEx`
- `DeleteFetchDataHandle`

`FetchDataHandle` 的关键虚表：

- `vtable[5]`：配置 `(reqformat, entry, body, flag)`；
- `vtable[6]`：异步启动；
- 完成后向创建时指定的窗口消息回传 handle。

真实页面初始化顺序为：

```text
SetEnvironPath(安装目录, T0001\, 0)
ITPConn_Init()
ITPConn_SetUser("", 0, 当前日期)
ITPConn_LoginAnony(100, hwnd, message)
等待 IsLogined(100)
GetSession(100) → SetActiveSession
CreateFetchDataHandleEx(session, hwnd, completion_message)
configure → start → Windows 消息循环
```

这解释了早期 Frida 单次脚本失败的原因：异步匿名登录和请求完成都依赖
客户端 UI 线程消息循环，简单 `sleep` 轮询不能替代宿主窗口。

## 独立宿主

`tdx_reqformat1_host.cpp` 是 32 位 Windows 消息循环宿主，支持格式 0—3，
输出原始响应并显示 TPData 错误码。编译产物位于：

```text
output\native\tdx_reqformat1_host.exe
```

示例：

```powershell
output\native\tdx_reqformat1_host.exe `
  --allow-network `
  --root C:\new_tdx `
  --format 1 `
  --entry pcwebcall_jjzt_etfjj_etfzt `
  --params '004,' `
  --output output\native\reqformat1-etfzt.json `
  --verbose
```

工具默认关闭网络，必须显式 `--allow-network`。

## 当前实测

原生宿主已成功完成：

- TPData 初始化；
- 匿名登录；
- session 获取；
- fetch handle 创建；
- 格式 0/1/2 配置和异步完成消息；
- 原始响应输出。

但当前匿名服务端没有注册 XML 中旧 `reqformat=1` 名称的直接路由。示例
`pcwebcall_jjzt_etfjj_etfzt` 返回：

```text
S999(-7415): ... Domain=, Name=pcwebcall_jjzt_etfjj_etfzt, RF=00
```

把请求改经 `CWServ.SecuInfo`/格式 0 能进入另一层处理，但当前样例返回：

```json
{"ErrorCode":-1005,"ErrorInfo":"数据库执行失败","hitCache":false}
```

格式 2 控制请求可完成并返回 `{}`。这些结果说明 ABI、会话和完成回调已
闭合，剩余阻塞在服务端路由/业务参数，不是 DLL 加载或消息循环问题。

因此当前优先级调整为：

1. 保留原生宿主作为格式 0—3 的诊断基线；
2. 不再把 97 个格式 1 名称逐个盲扫；
3. 优先实现已发现、无需旧路由的格式 11 和公开 7709 功能；
4. 后续若从新版配置或客户端流量取得格式 1 的路由映射，再用该宿主快速
   验证。

