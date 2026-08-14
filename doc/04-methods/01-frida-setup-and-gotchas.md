# Frida 接入与坑

> 基于同花顺项目的经验适配。通达信可能有不同的反注入机制，待实战验证。

## 基本事实
- 版本 **17.5.1**，装在 **Python 3.13**（`...\Python313`）。用 Python 3.9 会 `ModuleNotFoundError: frida`。
- **同花顺 `hexin.exe` 已验证可用**，通达信 `TdxW.exe` 待测试。
- 驱动方式（不进 REPL，便于脚本化）：写 Python 脚本用 `frida` 绑定，`frida.attach('TdxW.exe')` → `create_script` → `script.load()` → RPC 调用 → `session.detach()`。

## 坑 1：Frida 17 移除了全局 Module 导出 API（最坑）
`Module.enumerateExports(name)` / `Module.findExportByName(name, fn)`（全局静态）**在 Frida 17 被移除**。旧写法若包在 try/catch 里会**静默返回空**，导致误判"某模块 0 导出 / 找不到函数"。
- ❌ 旧：`Module.enumerateExports('TDataParse.dll')`
- ✅ 新：`Process.findModuleByName('TDataParse.dll').enumerateExports()`
- ✅ 新：`Process.findModuleByName(mod).findExportByName(fn)`

> 同花顺项目早期因此坑得出过错误的"0 导出"结论。TDX 项目从一开始就要使用新 API。

## 坑 2：RPC 里 `exports_sync` vs `exports`
不同版本：`script.exports_sync.foo()`（17.x）或 `script.exports.foo()`。兼容写法：
```python
exp = getattr(script, "exports_sync", None) or script.exports
```

## 坑 3：内存/图表缓冲会漂移
UI/图表相关的堆缓冲**跨 attach 地址会变**（重绘重分配）。同一份地址只在**同一次 attach 会话内**有效。要"扫描→跟指针→读值"就**在一个脚本会话里一次做完**。

## 坑 4：CEF 渲染，GDI 抓字无效（打通达信）
主行情页面用 CEF（Chromium）渲染，**不是 GDI**，hook `ExtTextOutW/TextOutW/DrawTextW` 抓不到。要取行情数据应直接读内存或 hook DLL 函数。

## 坑 5：通达信特有 — 自保护未知
同花顺有 `virusscan.dll` 反注入但 Frida 仍可 attach。通达信的自保护机制尚未测试：
- 可能使用 `ObRegisterCallbacks` 阻止进程打开；
- 可能使用 `NtSetInformationThread` 隐藏线程；
- 测试方法：先用 `frida -n TdxW.exe -l probe.js` 最小探针尝试。

## 常用骨架
```python
import frida
s = frida.attach("TdxW.exe")
sc = s.create_script(r'''
  rpc.exports.foo = function(){
    // 用 Process.enumerateRanges / Memory.scanSync / Module 实例方法
  };
''')
sc.load()
exp = getattr(sc, "exports_sync", None) or sc.exports
print(exp.foo())
s.detach()
```

## 稳定可用的 Frida API（实测）
`Process.enumerateModules()` / `Process.findModuleByName()` / `Process.enumerateRanges('rw-')` / `Memory.scanSync()` / `ptr().readDouble()/readU16()/readPointer()/readByteArray()` / `Process.findRangeByAddress()` / `Interceptor.attach()`。

## TDX 特定 hook 目标
- `tpbus.dll:0x10076A7A` — `FastHQ.Subscribe` 单证券请求构造
- `TaApi.dll:0x10008610` — `CTAJob_InetTQL` 接收分片交付
- `TDataParse.dll!fn_TGetImageData` — 本地/缓存记录的两阶段解析
- `TEncrypt.dll!T_RSAEncode*` / `T_PostUrlVerify` — 登录和 Web 请求的
  明文、签名/密文、应答对照
- `TdxAsioComm.dll` 的 `MakeUserCommModule`，以及已恢复的
  `CUserComm` 虚方法 — 验证连接、收发长度和回调时序
