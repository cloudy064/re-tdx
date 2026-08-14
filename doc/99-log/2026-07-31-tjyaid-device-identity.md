# TJyaid 设备身份与交易配置静态分析

## 目标

利用用户新生成的 `TJyaid.dll.i64`，恢复三个导出的真实语义，并核实
`GetXUserHardInfo` 在 TdxW 登录、行情账号和 TQL/SSO 请求中的使用方式。

## 输入与边界

```text
ida/TJyaid.dll
ida/TJyaid.dll.i64
ida/TdxW.exe.i64
```

工作区 DLL 与 `C:\new_tdx` 安装副本的 SHA-256 均为：

```text
E7B0C2E3EF17F8B05F2B6AD12FA2BF31062F76F9045130F59FE526F614CF0981
```

分析只读取二进制和 IDA 数据库。没有执行 TJyaid 导出函数，没有读取
`eTrade.xml`、账户配置或当前机器的真实硬件标识。

## 导出

| 导出 | 地址 | 语义 |
|---|---:|---|
| `GetWtDefInfoFromETradeXML_More` | `0x10005CE0` | 提取 eTrade 登录模式、营业部和安全模式定义 |
| `GetXUserHardInfo` | `0x10006030` | 生成三段式设备指纹 |
| `ProcessHostFromETradeXML` | `0x10005CB0` | 筛选、处理 eTrade 主机定义 |

DLL 共识别 311 个函数，主要依赖 MFC42、MSVCRT、KERNEL32 和 ADVAPI32；
它没有 socket/Asio 导入，不符合 7709 网络传输实现的特征。

## 设备指纹算法

`GetXUserHardInfo` 依次取得三个字段，最后用 `%s;%s;%s` 写入调用者缓冲区：

```text
<ProcessorNameString>;<CPUID leaf 1 EAX as %08x>;<first disk serial>
```

1. CPU 名称来自注册表
   `HKLM\HARDWARE\DESCRIPTION\System\CentralProcessor\<n>` 的
   `ProcessorNameString`。
2. CPUID 辅助对象执行 leaf 0、1 和 `0x80000001`；该导出实际取 leaf 1
   的 EAX，并以小写、8 位十六进制 `%08x` 输出。
3. 磁盘路径枚举索引 0–15，在首个非空序列号处停止。底层含
   `\\.\PhysicalDrive%d`、`\\.\Scsi%d:`、`\\.\IDE21201.VXD` 多条
   Windows 版本/驱动兼容回退，并使用 `0x74080`、`0x7C088`、
   `0x2D1400`、`0x700A0`、`0x4D008` 等 IOCTL。

因此该值是明文、分号分隔的稳定设备特征，不是 MD5、RSA 或其它哈希。

## TdxW 消费链

`TdxW.exe:0x59B3C0` 拼接 `<安装目录>\TJyaid.dll`，动态解析
`GetXUserHardInfo`，向其提供 1024 字节缓冲区，随后卸载 DLL并缓存返回值。

关键消费点：

| 地址 | 已恢复用途 |
|---:|---|
| `0x5D0F50` | 用分号解析第 3 段，替换 `##disksns##` |
| `0x668C60` | 构造 `{"HARDINFO":"<完整设备指纹>"}` |
| `0x73F090` | 构造行情账号校验 JSON：`quotesAcc/deviceType/pw/deviceId/terminalType`，POST 到 `DFHKCheckURL` 的 `/quotes/tdxgetdata` |
| `0xAE7940` | 把完整值放入 `/TQLEX?Entry=UserServ.c_user_accord_update` 的第三个参数 |
| `0xB012B0` | 写入 TQL JSON 的 `deviceid`；为空时使用 `TDX` |

行情账号校验中的密码由 `T_RSAEncode2` 单独做 RSA 公钥 PKCS#1 v1.5
加密；设备指纹本身直接成为 JSON 字符串字段。由此可以确认 TJyaid 与
认证/设备绑定有关，但仍不能把它视为 7709 行情流的加密层。

## 另外两个导出

TdxW 静态导入 `GetWtDefInfoFromETradeXML_More` 和
`ProcessHostFromETradeXML`：

- 前者读取 `eTrade.xml`，上层把结果整理为 `LoginMode`、营业部
  `ID/NAME/HOSTTYPE` 和 `SecurityMode/SecurityType/Prompt` JSON；
- 后者按 `FilterHostType` / `HostType` 处理主机定义，并参与交易登录
  主机配置生成。

这两条路径是交易入口配置，不是行情收发或实时数据解析。

## 结论与下一步

- TJyaid 的高收益静态问题已经闭合，不需要继续投入 IDA 标注才能推进
  行情主线；
- 设备指纹只需在后续登录动态实验中确认出现时序，不应采集或提交真实值；
- 下一步仍应优先记录 `FastHQ.Subscribe` 序列化 Body，并在 TaApi 接收
  分片与 `TdxAsioComm` send/recv 之间做同一请求的时间对齐；
- `TPool.dll` 仍偏股票池/UI，目前没有高于上述动态主线的收益。

> 2026-07-31 后续更新：TPool 也已完成静态分析，确认是回调供数的本地
> 股票池/公式筛选引擎。详见
> [TPool 股票池与公式筛选引擎静态分析](2026-07-31-tpool-stock-pool-engine.md)。
