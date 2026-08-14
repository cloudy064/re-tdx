# 工具链与环境

## 机器上已确认可用

与同花顺项目共享同一工作站和工具链：

| 工具 | 位置 / 版本 | 备注 |
|---|---|---|
| Frida | `%LOCALAPPDATA%\Programs\Python\Python313\Scripts\frida.exe`，**17.5.1** | Python 绑定装在 **Python 3.13**。待测试对 TdxW.exe 的挂载能力。 |
| Python | `%LOCALAPPDATA%\Programs\Python\Python313\python.exe`（3.13.5）、`...\Python39\python.exe`（3.9.13） | **驱动 Frida 用 3.13**（3.9 没装 frida 模块）。 |
| IDA | `C:\Program Files\IDA Professional 9.0\idat.exe`，9.0.24.0925 | IDAPython 位于 `plugins\idapython3.dll`；headless 确认 x86 Hex-Rays 已加载。**TdxW.exe.i64 已完成自动分析。** |
| Detect It Easy | `%LOCALAPPDATA%\Microsoft\WinGet\Packages\horsicq.DIE-engine_*\die\`，3.21 | GUI `die.exe` 与 CLI `diec.exe` 均可用。 |
| FLOSS | `%LOCALAPPDATA%\Microsoft\WinGet\Packages\Mandiant.FLOSS_*\floss.exe`，3.1.1 | 可提取混淆字符串。 |
| Rizin | `%LOCALAPPDATA%\Programs\Rizin\bin\rizin.exe`，0.8.2 | 备选反汇编器。 |
| capa | `%USERPROFILE%\.local\bin\capa.exe`，9.4.0 | 能力检测（识别加密、网络等功能）。 |
| PE-bear | `%LOCALAPPDATA%\Microsoft\WinGet\Packages\hasherezade.PE-bear_*\PE-bear.exe`，0.7.2 | PE 结构分析。 |
| dumpbin | `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\dumpbin.exe` | 导出表分析。 |
| Wireshark | 系统安装 | 网络抓包分析。 |
| Sysinternals | `%LOCALAPPDATA%\Microsoft\WinGet\Packages\Microsoft.Sysinternals.Suite_*\` | `Procmon.exe`（进程/注册表/网络监控）、`strings.exe`。 |

> 2026-07-30：工具链验证复用了同花顺项目 2026-07-18 的机器复核结果。通达信特有的工具需求（如 MFC 静态分析辅助工具）待确认。

## TDX vs THS 工具链差异

| 需求 | 同花顺 | 通达信 |
|---|---|---|
| 脱壳 | UPX `-d`；xiadan.exe 需 VMProtect 脱壳机 | **不需要**（主进程无壳） |
| IDA 分析 | `hexin_unpacked.exe.i64` | `TdxW.exe.i64`（可直接分析） |
| .NET 反编译 | dnSpy（HevoSpace .NET Core 5） | 待确认是否有 .NET 组件 |
| MFC 辅助 | 不适用 | 可能需要 MFC 结构识别脚本 |

## 通达信安装路径

通达信标准安装路径（待运行时确认）：
- Windows: `C:\通达信软件\通达信\` 或 `C:\new_tdx\`（常见变体）
- 主程序: `TdxW.exe`
- 插件目录: `TCPlugins\`, `SEPlugins\`, `PYPlugins\`, `QHPlugins\`
- 数据目录: `data\`, `T0002\` 等

## IDA 反编译状态

- `TdxW.exe.i64`：已完成自动分析（2026-07-30），162MB 数据库，可直接进行伪代码浏览。
- IDA Pro 9.0 x86 Hex-Rays 已确认可用。
- **注意**：TdxW.exe 是 PE32（x86），不是 x64，确保 IDA 以 32-bit 模式分析。

## 无界面跑 IDA 的命令

```powershell
& "C:\Program Files\IDA Professional 9.0\idat.exe" -A "-L<日志>" "-S<脚本.py>" "<目标.dll 或 .i64>"
```
- `-A` 自主模式（不弹框）；`-S` 跑 IDAPython 脚本；可直接对 `.i64` 数据库运行。
- 脚本末尾用 `ida_pro.qexit(0)` 确保退出。

对 TdxW.exe.i64 运行 headless 分析：
```powershell
& "C:\Program Files\IDA Professional 9.0\idat.exe" -A "-LC:\temp\ida_tdx.log" "-S<script.py>" "C:\Users\cloudy064\workspace\ida\tdx\ida\TdxW.exe.i64"
```

## 驱动 Frida 的固定前缀（PowerShell）

```powershell
$py='C:\Users\cloudy064\AppData\Local\Programs\Python\Python313\python.exe'
& $py <脚本.py> <参数...>
```

## dumpbin 快速分析

```powershell
# 分析 DLL 导出表
& "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\dumpbin.exe" /exports <目标.dll>

# 分析 DLL 依赖
dumpbin /dependents TdxW.exe
```

## 目标进程 / 路径速查

- 通达信根目录：待确认（常见 `C:\通达信软件\通达信\` 或 `C:\new_tdx\`）
- 主进程：`TdxW.exe`
- 连续竞价时段：A 股 **09:30–11:30 / 13:00–15:00 CST**。15:00 后逐笔增量结束，但收盘回补、盘后定价和频道保活仍可能继续。
