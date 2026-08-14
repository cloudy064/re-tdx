# TCalc EXTDATA_USER 本地扩展序列闭环

日期：2026-08-10

## 结论

`EXTDATA_USER(dataset_id, mode)` 已从 TCalc 处理器、TdxW selector 38、磁盘
索引和时间对齐四层闭合，并进入纯 C++ 公式解释器。它不是云端、L2 或插件回调；
数据来自通达信安装目录下的两份本地文件：

```text
T0002/extdata/extdata_<dataset_id>.idx
T0002/extdata/extdata_<dataset_id>.dat
```

当前 `C:\new_tdx\T0002\extdata` 不存在，因此正式安装暂时没有可直接演示的用户
扩展数据集。缺文件不是协议失败：公式仍能建立空上下文，并按所选模式得到
`DRAWNULL` 或 0。

## TCalc 端语义

静态注册表记录为 opcode 1298、处理器 `TCalc.dll!sub_100116B0`。公式有两个参数，
都先收窄到 float32 再截断成 int32；处理器只读取两个参数的末柱值，所以数据集和
模式作用于整条输出序列。

处理器先以 selector 38 查询数量，再申请 `(count + 1) * 12` 字节并读取
`date/time/value` 三元组。精确日期和时间命中时总是返回记录值；未命中时：

- mode 1：沿用前一根输出；
- mode 2：返回 0；
- mode 3：反向遍历并沿用后一根输出；
- 其他值：返回 TCalc `DRAWNULL`。

输入记录不会排序。正向模式从文件头推进，反向模式从文件尾推进，因此重复时间戳
也保留原处理器的“正向首条、反向末条”行为。

## TdxW 文件协议

selector 38 进入 `sub_4DAB80`，再由 `sub_4D0780` 读取并缓存数据。缓存最多 100 个
证券/数据集条目，但这是宿主优化，不影响文件格式。

`.idx` 文件长度必须是 29 的正倍数，每条为：

| 偏移 | 长度 | 类型 | 含义 |
|---:|---:|---|---|
| 0 | 2 | uint16 LE | 市场 ID |
| 2 | 23 | char[23] | 大小写敏感、NUL 结尾的证券代码 |
| 25 | 4 | int32 LE | 该证券的数据点数 |

宿主线性寻找市场和代码都精确相等的首条记录；目标 `.dat` 起点是此前所有 idx
记录点数之和乘以 12。这里没有 EXTERN 信号表的 31/71 市场兼容规则。

`.dat` 按 idx 顺序连续存放 12 字节记录：

| 偏移 | 长度 | 类型 | 含义 |
|---:|---:|---|---|
| 0 | 4 | int32 LE | `YYYYMMDD` |
| 4 | 4 | int32 LE | `HHMMSS` |
| 8 | 4 | float32 LE | 数值 |

selector 先按请求的首末日期做闭区间过滤，再复制最多 30,000 点。纯 C++ 读取器
因此采用“完整目标段边界校验、流式日期过滤、过滤后限额”，不会因很长的早期历史
挤掉当前 K 线范围。负点数、无 NUL 代码、损坏 idx 和截断 dat 都会安全报错。

## 纯 C++ 接入

新增命令：

```powershell
tdx-tool formulas extdata-user --root C:\new_tdx --id 73
tdx-tool formulas extdata-user --root C:\new_tdx --id 73 `
  --market sz --code 000001 --start 20260101 --end 20261231 --limit 30000
```

第一种用法列出数据集索引；第二种按 selector 38 的规则选择证券和日期范围。
输出包含精确路径、存在性、idx 记录、起始点、范围命中数、截断状态和数据点。

源码分析器会把静态调用归一成 `EXTDATA_USER#dataset#mode`。自动市场上下文按当前
K 线首末日期只加载一次同一数据集，再分别建立不同模式的时间戳序列；动态参数
继续拒绝，避免运行过程中隐式切换文件。上下文元数据会报告文件、证券命中、源点数、
有限对齐点数和 30,000 点截断状态。

能力清单由 232 增至 233 个支持函数，自动符号仍为 71；390 条 TCalc 静态注册名
的已识别并集由 287 增至 288，剩余由 103 降至 102。379 条内置公式仍全部语法
支持、数值安全且无退化输出。

## 验证

固定夹具包含一条前置证券和一条目标证券，证明 `.dat` 起点来自所有前置 idx 点数。
五根 K 线只在第 2、4 根有外部记录，同时核对四种模式：

```text
mode 1: null, 20, 20, 40, 40
mode 2:    0, 20,  0, 40,  0
mode 3:   20, 20, 40, 40, null
mode 0: null, 20, null, 40, null
```

定向 `formula_engine_tests` 和 `recon_contract_tests` 均通过，全量 CTest 为
102/102。8875 候选服务的健康、功能目录和公式能力契约为 3/3，完整 API 契约为
219/219；无需单独重试。正式 8765 服务替换后同组三项契约再次 3/3，stderr 为
0 字节。

正式二进制为 22,157,009 字节，SHA-256
`3C77449704271FC327FC9C4A12817B9514CC6621597C5050BB412EAAF61BD88B`，当前 PID
38964。替换前版本保存在
`output/tdx-tool-extdata-user-v12-predeploy-rollback-20260810.exe`，其 SHA-256 为
`32E3937072C5D2BBD8EA4E1A427D4BB89AC44764636BBF426C1F154AA465B11C`。

正式服务还用平安银行最近 5 根日线执行了
`Z:EXTDATA_USER(73,2);N:EXTDATA_USER(73,0);`。响应为纯 C++ 引擎、两个精确绑定；
由于数据文件不存在，Z 五点全为 0，N 五点全为 null，且返回两条 selector-38
来源元数据，证明缺文件路径也实际经过自动上下文和解释器，而非只更新能力声明。

## 证据

- `output/ida-probe-tcalc-extern-access.log`，SHA-256
  `46BEE6017149AF8A70953A3AC8514877786E5DFB1E19767879A2BAD3B8296877`；
- `output/ida-tdxw-host-dispatch-core.log`，SHA-256
  `0747219B3E0ED6D58F3CAFCDBAB20634F4E772CBEB5AE5DB9F54C846BACED553`；
- `output/ida-probe-tdxw-extdata-selector38.log`，SHA-256
  `E913D3F7198AF5EB96D67EA8231298D03A0C9C863B9FFD09A48ED3F8C65F31B5`；
- `output/native-formula-coverage-extdata-user-v12.json`，SHA-256
  `E1DB142CDFB90D63F6E667F0640C4F69C49E9CAA646B7284B18925FA8D4A6885`；
- `output/native-formula-registry-next-audit-v12.json`。
- `output/api-contract-full-extdata-user-v12-candidate.json`，SHA-256
  `D6B8B040725916AD82C0CE8CF75B9180429B4C7A79CE17089897C242F5FE352D`；
- `output/api-contract-extdata-user-v12-formal.json`，SHA-256
  `1EF187D24C2996CD2CB8991369036AABE407EABB70D94492FF6345B74822EB87`。

本批没有实现 L2、券商私有 `SIGNALS_*`、账户交易状态或通用插件回调。
