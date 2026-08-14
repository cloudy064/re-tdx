# TCalc MULTIPLIER 与 7727 合约乘数闭环

## 结果

纯 C++ 公式执行器现可自动绑定裸符号 `MULTIPLIER`。它不是成交量或股本字段，
而是期货、期权合约的每手合约乘数；标准 A 股和非衍生品目录记录按原宿主语义返回
0。运行时不加载通达信 DLL，也不依赖 Python。

这项能力使用公开 7727 `0x23F0/0x23F5` 合约目录。目录记录偏移 56 的 little-endian
u32 保存原始乘数，TCalc type 105 回调结构偏移 42 则以 signed int16 暴露其低字；
解释器按后一口径广播到全部 K 线。

## 原生证据

TCalc 静态注册表中 `MULTIPLIER` 对应 opcode 1252，处理函数 RVA `0x26A40`。
离线 IDA 探针确认它只请求一次宿主 type 105，并读取返回结构偏移 42 的有符号
16 位整数，然后把该值广播到输出序列。

32 位原 DLL 固定向量向该偏移注入
`0, 1, 10, 100, 200, -3, 32767, -32768`，所有非空序列均保持原值逐柱广播，
且每次求值只发生一次回调。证据文件为：

- `output/ida_probe_tcalc_capital_multiplier.py`；
- `output/ida-tcalc-capital-multiplier.log`；
- `output/native_probe_tcalc_multiplier.cpp`；
- `output/native-probe-tcalc-multiplier.json`。

对公开 7727 原始目录的独立扫描进一步锁定偏移 56：

| 市场:代码 | 合约乘数 |
| --- | ---: |
| `47:IF2608` / `47:IFL9` | 300 |
| `47:IC2608` | 200 |
| `29:A2609` / `28:AP2610` / `30:AD2608` | 10 |
| `66:LC2608` | 1 |
| `7:HO8W03UX` | 100 |

原始探针与结果位于 `output/native_probe_7727_instrument_raw.cpp` 和
`output/native-probe-7727-instrument-raw.json`。`TdxW.dll` 的 `sub_6A17B0`
也可见 `0x23F5` 请求构造立即数，记录在
`output/ida-probe-tdxw-23f5-instruments.log`。

## 纯 C++ 接线

`market instruments` 现解析并返回 `contract_multiplier`，同时以
`contract_multiplier_source=tdx-7727-0x23f5-offset56-u32` 标记来源。
公式上下文只为目录类别 3/12 的衍生品读取精确合约记录，并将偏移 56 的低 16 位
按有符号数绑定到 `MULTIPLIER`；标准股票和其它目录类别绑定 0，不通过名称猜测。

公式结果会返回：

- `contract_multiplier` 与 `contract_multiplier_raw`；
- `contract_multiplier_category`；
- `contract_multiplier_mode=tcalc-opcode1252-type105-offset42-signed-int16-broadcast`；
- 扩展合约来源 `tdx-7727-0x23f5-offset56-u32-low-word`，或标准股票零值来源。

能力清单新增单项 `custom_formula_contract_metadata_symbols`。`MULTIPLIER` 是宿主
外部符号，只支持裸符号写法，不把未经原生证据支持的 `MULTIPLIER()` 伪装成函数。
服务器的扩展市场依赖过滤也同步接受该符号，并顺带与解释器已有的
`ISJYDATE/LOCALDAYNUM` 对齐。

## 真实验证与发布

- C++ 全构建通过，CTest 101/101；
- `IFL9` CLI 返回 300，`HO8W03UX` 返回 100，`sz000001` 返回 0；
- 临时服务新增专项 4/4；完整 API 215/215，共 216 次网络请求；
- 正式服务健康、首页、`MULTIPLIER` 和期货 CCL 契约 4/4；
- 证据为 `output/native-cli-contract-multiplier*.json`、
  `output/native-multiplier-targeted-api.json`、
  `output/native-multiplier-full-api.json` 和
  `output/native-multiplier-formal-api.json`。

正式 EXE 为 42,800,813 字节，build/dist SHA-256 均为
`2F5DB9432DF4B9E63E8973A2780711DAED3B06FD80CCA1B5F4F3B0DA6236D3DD`；正式服务
PID 37180，仅监听 `127.0.0.1:8765`，保持 `native_cpp=true`、
`python_runtime=false`。上一版保存在 `tdx-tool.exe.previous`，SHA-256 为
`6E87A1656F30D04A4865E0E9F94598446FBF36725F8FEBFACE985DB80520FD70`。
旧进程退出后 Windows 句柄仍短暂占用 EXE，首次覆盖被拒绝；等待后重试并校验
build/dist 哈希一致才启动新服务，没有运行半更新版本。

## 下一步

390 条 TCalc 注册名与现有能力的识别并集由 262 增至 263，剩余 127。本记录当时
列为下一组的共享 K 线辅助字段 `ZSTJJ/QHJSJ` 已随后完成，识别并集现为 265，
剩余 125，详见
[K 线辅助字段记录](2026-08-09-native-formula-kline-auxiliary.md)。之后再按真实调用价值研究板块/文字入口和
`FINONE/GPJYONE/BKJYONE/SCJYONE/GPONEDAT`。继续排除 L2、账户状态、券商私有
信号和插件回调。
