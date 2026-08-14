# 壳与保护机制

## 主进程 TdxW.exe（2026-07-30 DIE 分析结论）

### DIE 检测结果
```
MSDOS
    Unknown: Unknown
PE32
    Linker: Microsoft Linker(6.00.8447)
    Compiler: Microsoft Visual C/C++(16.00.30319)[C++]
    Library: MFC(10.0)
    Tool: Microsoft Visual Studio(6.0)
    Sign tool: Windows Authenticode(2.0)[PKCS #7]
    Debug data: Records[codeview]
```

### 结论
- **加壳工具**：无
- **保护器**：无
- **混淆器**：无
- **加密段**：仅 4 个标准 PE sections，无额外段
- **数字签名**：有 Windows Authenticode (PKCS #7) 签名

### file 命令输出
```
PE32 executable (GUI) Intel 80386, for MS Windows, 4 sections
```

### 与同花顺对比

| 维度 | 同花顺 (THS) | 通达信 (TDX) |
|---|---|---|
| 主进程 | `hexin.exe`（UPX 壳） | `TdxW.exe`（无壳） |
| 壳类型 | UPX 0.89.6 - 1.02（可 `upx -d`） | 无 |
| 交易终端 | `xiadan.exe`（VMProtect 强壳） | 待确认（可能独立进程或集成在 TdxW.exe 内） |
| 代码混淆 | 无额外混淆 | 无（可能 RTTI 完整） |
| 反调试 | `virusscan.dll` 自保护 | 待测试（可能无或较弱） |
| 数字签名 | 可能有 | 有 Authenticode 签名 |
| 静态分析难度 | 中等（脱壳后可分析，但混淆较多） | **低**（无壳、VS2010 原生、可能有 RTTI） |

## 各 DLL 保护状态（待逐个确认）

| DLL | 预计保护程度 | 理由 |
|---|---|---|
| `TDataParse.dll` | 低（无壳） | 数据处理模块，通常无保护 |
| `TAsioComm.dll` | 低（无壳） | Boost.Asio 通信库，通常无保护 |
| `TEncrypt.dll` | 低（已完成 IDA） | 无明显混淆；Blowfish/Base64/RSA/libcurl 可读 |
| `TJyaid.dll` | 低（已完成 IDA） | 设备指纹和 eTrade 配置逻辑可读，无明显混淆 |
| `TPool.dll` | 低（已完成 IDA） | MFC/TCalc 股票池引擎，导出和 XML/回调逻辑可读 |
| `TJyKey.dll` | 待确认 | 若其它发行版存在，再单独评估 |
| `TPyth.dll` | 不适用 | 当前安装未发现 |
| `TdxDataSDK.dll` | 不适用 | 当前安装未发现 |
| `TCefWnd.dll` | 无（CEF 官方库） | 开源库 |
| `CTPMore.dll` | 待确认 | 期货接口，可能来自第三方 |
| 券商专用插件 | **可能较高** | 如 `TdxLoaderHtzqLct.dll`，特定券商集成 |

## 潜在保护机制（待验证）

### 1. 反调试（Anti-Debug）
- MFC 程序通常不带商业反调试；
- 通达信可能有简单的 `IsDebuggerPresent` 检查（需 IDA 确认）；
- 未发现 VMProtect 或 WinLicense 级别的保护。

### 2. 完整性校验（Integrity Check）
- 有 Authenticode 数字签名，但不代表运行时校验；
- DLL 加载时可能校验签名（尤其是交易相关 DLL）。

### 3. 通信加密（Traffic Encryption）
- `TEncrypt.dll` 用于字段级 Blowfish/Base64、RSA 签名/公钥加密和
  HTTPS POST；
- 当前没有证据表明 `TdxAsioComm` 的 7709 TCP 流经过该 DLL；
- `TaApi` 自身包含 TLS、压缩、分片和心跳配置，登录/行情会话仍需动态
  区分。

### 4. 自保护进程（Self-Protection）
- 字符串中有 `TWarnClient.dll`（预警客户端），可能是异常检测；
- 当前版本 `TEncrypt.dll` 实际依赖 `LIBEAY32.dll`；旧
  `libcrypto-1_132.dll` 字符串不适用于本文件；
- 待确认是否有类似同花顺 `virusscan.dll` 的反注入保护。

## 调试建议

由于主进程无壳：
1. **x64dbg / WinDbg** 可直接附加到 `TdxW.exe`（可能不需要 bypass）；
2. **IDA** 伪代码应有较好的可读性（VC++ 2010 没有现代编译器的激进优化）；
3. **RTTI** (Run-Time Type Information) 可能完整，有助于识别 MFC 类层次结构；
4. **Frida** 挂载先做基本测试（`frida -n TdxW.exe -l probe.js`），观察是否有反注入；
5. **Procmon** 监控 `TdxW.exe` 启动过程，检查是否有反调试行为（加载 `ntdll!DbgUiRemoteBreakin` 之前的行为）。

## 注意事项

- 虽然主进程、`TJyaid.dll` 和 `TPool.dll` 均可直接静态分析，但
  **不要外推所有 DLL 也无壳**；券商插件和其它发行版的 `TJyKey.dll`
  仍可能有独立保护。
- 软件可能检测调试器后**静默降级服务**（不给 Level2 数据、拒绝登录等），而不是直接退出。
- 数字签名是 2025 年 11 月的（文件日期 2025-11-14），考虑验证证书链是否有效。
