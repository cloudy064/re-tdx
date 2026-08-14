# TCalc TQFLAG 复权模式闭环

## 结论

本轮闭合 TCalc 静态注册表中的 `TQFLAG`。它不是行情字段，也不通过价格序列推导：
求值器初始化 K 线时接收一个有符号 32 位复权模式参数，保存到对象偏移 `0xEC82`，
`TQFLAG` 再把该值转换成 float 并广播到每根柱。通达信长 K 线请求的复权模式语义为：

- `0`：不复权/原始；
- `1`：前复权；
- `2`：后复权。

纯 C++ 解释器现同时支持裸符号 `TQFLAG` 和零参数调用 `TQFLAG()`，能力清单由
217 个支持函数、62 个自动符号增至 218/63。该项无需登录或 Level2，但依赖实际
装入的 K 线复权上下文，因此分析器单独返回
`has_adjustment_mode_dependency=true`，不把它误报成纯 OHLCV。

## 注册、处理与写入链

静态注册记录为：

```text
name=TQFLAG
opcode=1253
handler=sub_1000E670 (RVA 0xE670)
```

`sub_1000E670` 对每根柱执行 `fild dword ptr [ecx+0xEC82]`，再以 float 写入输出
数组。求值器初始化函数 `sub_10014790(..., a7)` 在 `0x100147D9` 执行：

```text
*(DWORD *)(this + 0xEC82) = a7
```

调用方把自身的复权标志原样传入 `a7`，同一值也进入宿主 K 线获取链。项目已恢复
的长 `0x052D` K 线请求在偏移 28 使用 uint16 复权模式，0/1/2 分别对应
raw/qfq/hfq；两条证据链在正常客户端值域上对齐。

静态证据文件包括：

- `output/ida-tcalc-function-registry-jsonl-v4.log`；
- `output/ida-tcalc-daily-fields.log`；
- `output/ida-tcalc-evaluator-fields.log`；
- `output/ida-tcalc-tqflag-callers.json`；
- `output/ida-tcalc-tqflag-callers.log`。

## 32 位原 DLL 固定向量

新增直接调用原 DLL 处理函数的探针：

- `output/native_probe_tcalc_tqflag.cpp`；
- `output/native_probe_tcalc_tqflag.exe`；
- `output/native-tcalc-tqflag-probe.json`。

固定结果为：

```text
none       -> 0,0,0,0
qfq        -> 1,1,1,1
hfq        -> 2,2,2,2
negative   -> -1,-1,-1
full_dword -> 257,257,257
empty      -> 空序列
```

`-1` 和 `257` 证明处理函数读取并广播完整有符号 32 位整数，不能仅按低字节、
布尔量或固定 0/1/2 查表实现；0/1/2 只是正常 K 线请求使用的公开值域。

## 纯 C++ 映射与接口边界

解释器从输入 K 线文档顶层 `adjustment_mode` 建立自动上下文：

| 文档值 | `TQFLAG` |
| --- | ---: |
| 缺失、空串、`none`、`raw` | 0 |
| `qfq`、`front`、`forward`、`fixed_qfq` | 1 |
| `hfq`、`back`、`backward`、`fixed_hfq` | 2 |
| 数值 `0..2` | 原值 |

未知字符串、非法类型和范围外数值明确拒绝，避免静默伪造复权状态。随后完成的
HTTP 复权批次已让公式 GET/POST 与全库审计主动接受 `adjust`，并复用现有
`apply_kline_adjustment`；前复权/后复权在线请求会真实返回 1/2。完整接口证据见
[公式 HTTP 复权记录](2026-08-09-native-formula-http-adjustment.md)。

## 验证与发布

- 原 DLL：raw/qfq/hfq、负数、完整 dword、空序列固定向量通过；
- `tdx-formula-engine-tests`、`tdx-recon-contract-tests`：通过；
- CTest：101/101；
- 临时服务专项：4/4；
- 临时服务 full API：213/213（214 次网络请求）；
- 正式服务专项：4/4。

报告为：

- `output/native-tqflag-targeted-api.json`；
- `output/native-tqflag-full-api.json`：首次 212/213，唯一失败为
  `jsn-discovery-live` 上游 WinHTTP 12002；
- `output/native-tqflag-jsn-retry-api.json`：失败项单独重试 1/1；
- `output/native-tqflag-full-api-rerun.json`：完整重跑 213/213；
- `output/native-tqflag-formal-api.json`。

正式 EXE 为 42,419,468 字节，SHA-256
`A6E981CE3C5467F41DB0245CC7F6EB9B3C0682C3A4B5711FA2307F3332AD7CCF`；正式服务
PID 42916，仅监听 `127.0.0.1:8765`，`native_cpp=true`、
`python_runtime=false`。临时 8875 已关闭，替换前版本保存在
`dist/tdx-tool/bin/tdx-tool.exe.previous`。

## 后续

公式 GET/POST、审计和网页图表的复权上下文已经闭合。扫描与回测仍维持既有原始
行情口径，尚未接受 `adjust`；这是有意的安全边界，因为变更历史交易信号口径需要
独立验证手续费、调仓时点和缓存身份，不能由展示接口参数静默扩散。
