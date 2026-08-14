# 认证与加密辅助层

> 更新时间：2026-07-31。结论来自 `SEPlugins\TEncrypt.dll.i64`、
> `TJyaid.dll.i64` 与 `TdxW.exe.i64` 的导出、伪代码和动态函数指针
> 交叉引用。

## 定位

`TdxW.exe:0x725320` 按需加载 `SEPlugins\TEncrypt.dll`。初始化成功的
最低条件是同时取得 `T_Encrypt` 和 `T_Encode`；RSA、HTTPS POST 和下载
接口按功能使用。

当前证据表明它服务于登录参数、Web API、签名、RSA 密码保护和文件下载。
没有发现它位于 `TdxAsioComm` 的 7709 send/recv 路径中。

## 导出语义

| 导出 | 已恢复语义 |
|---|---|
| `T_Encrypt` | Blowfish 分组加/解密 |
| `T_Encode` | Base64 编码/解码 |
| `T_RSAEncode` | SHA-1 摘要后使用 RSA 私钥签名，再 Base64 |
| `T_RSAEncode2` | RSA 公钥 PKCS#1 v1.5 加密，再 Base64 |
| `T_PostUrlVerify` | 同步 HTTPS POST |
| `T_DownloadFile*` | libcurl 异步下载、清理、代理设置 |

### Blowfish 封装

`T_Encrypt` 的密钥调度含 18 项 P-array 和 4×256 S-box，分组宽度为
8 字节，可确认是 Blowfish：

- 加密输出前 4 字节保存原文长度；
- 数据补齐到 8 字节倍数；
- 解密要求密文数据部分为 8 字节倍数；
- 默认 key 字符串为 `tdx_verify_crypt`，调用者可传自定义 key；
- 主程序通常把 Blowfish 输出继续交给 Base64。

主程序中的实际用途包括 `tdx_secureuser`、券商登录码等 URL/模板参数
替换，因此这是应用字段保护，不等同于 TCP 流加密。

### RSA

- `T_RSAEncode`：读取 PEM 私钥，对输入做 SHA-1，再调用
  `RSA_sign(NID_sha1, ...)`；
- `T_RSAEncode2`：读取 PEM 公钥，调用
  `RSA_public_encrypt(..., RSA_PKCS1_PADDING)`；
- `TdxW.exe:0x910C10` 使用 RSA 私钥签署带 `msgId`、`deviceid`、
  `channel`、`timestamp`、`device` 的 JSON；
- `TdxW.exe:0x73F090` 使用内嵌公钥加密行情账号密码，再 POST JSON。

### HTTPS POST

`T_PostUrlVerify` 内嵌 libcurl，并明确把 `CURLOPT_SSL_VERIFYPEER` 与
`CURLOPT_SSL_VERIFYHOST` 设为 `0`。因此它虽然走 HTTPS，却不校验证书链
或主机名。复现时不应照搬这一安全设置。

## TJyaid 设备身份

`TJyaid.dll` 导出 `GetXUserHardInfo` 和两个 `eTrade.xml` 配置函数。
`TdxW.exe:0x59B3C0` 按需加载该 DLL、调用前者并把结果缓存到全局
`CString`。静态恢复出的返回格式是：

```text
<CPU name>;<CPUID signature>;<disk serial>
```

三个字段的来源如下：

| 字段 | 生成方式 |
|---|---|
| CPU name | 读取 `HKLM\HARDWARE\DESCRIPTION\System\CentralProcessor\<n>` 的 `ProcessorNameString` |
| CPUID signature | 执行 CPUID leaf 1，将 EAX 以小写 `%08x` 格式化 |
| disk serial | 枚举磁盘 0–15，使用 `PhysicalDrive`、SCSI 和旧 IDE IOCTL 回退链，保留首个非空序列号 |

它是明文、分号分隔的稳定设备特征，不是哈希。TdxW 的主要消费点是：

| TdxW 地址 | 用途 |
|---:|---|
| `0x5D0F50` | 解析第 3 段，替换模板变量 `##disksns##` |
| `0x668C60` | 构造 `{"HARDINFO":"<完整值>"}` |
| `0x73F090` | 行情账号校验 POST 的 `deviceId`；密码另经 `T_RSAEncode2` |
| `0xAE7940` | `/TQLEX?Entry=UserServ.c_user_accord_update` 的第 3 个参数 |
| `0xB012B0` | TQL 请求的 `deviceid`，取值失败时回退为 `TDX` |

另外两个导出 `GetWtDefInfoFromETradeXML_More` 和
`ProcessHostFromETradeXML` 解析或筛选 `eTrade.xml` 中的登录模式、营业部、
安全模式和主机配置。它们进一步说明 TJyaid 是设备身份/交易配置辅助层，
没有证据表明它参与 7709 帧加密或传输。

本轮只做二进制和 IDA 数据库静态分析，没有执行导出函数，也没有读取或
记录当前机器的真实 CPU、CPUID 或磁盘序列号。

## 结论与边界

- P8 已完成算法和主要调用场景的静态识别；
- `GetXUserHardInfo` 的静态生成格式和主要消费点已经闭合；
- 尚未闭合完整登录时序、服务端 challenge 和票据生命周期；
- 登录动态分析应优先 hook RSA/POST 的输入输出，而不是假设所有 7709
  行情帧都经过 `T_Encrypt`。
