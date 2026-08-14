# TCalc 板块代码序号与证券名称查询闭环

## 结果

纯 C++ 公式解释器新增三个非 Level2 函数：

- `GNBKZSCODE(N)`：返回当前证券第 N 个概念板块的十进制代码字符串；
- `FGBKZSCODE(N)`：返回当前证券第 N 个风格板块的十进制代码字符串；
- `GETNAMEOFCODE(MARKET,CODE)`：按市场和代码返回通达信证券目录名称。

真实 `SZ000001 平安银行` 当前有 1 个直接概念、10 个直接风格。升序结果为：

- 概念：`880609 跨境支付`；
- 风格首项：`880679 周期股`；其后为 `880721/880801/880805/880821/880826/
  880829/880845/880846/880883`；
- `GETNAMEOFCODE(1,HYZSCODE)=银行`；
- 越界的 `GNBKZSCODE(99)` 返回空字符串。

运行时不加载 Python。新增的 IDA Python 仅是离线证据探针。

## TCalc 与 TdxW 证据

`output/ida-tcalc-block-metadata-v2.log` 中三个处理函数分别为：

- opcode 1363 `sub_10044F40`：取单参数末柱，整数化后减一，读取 type-167
  偏移 115 的概念数量及偏移 117 起的 DWORD 代码；
- opcode 1364 `sub_10045020`：同样取一基序号，从概念段之后读取风格代码；
- opcode 1365 `sub_100451B0`：把第一参数转为 uint16 市场，把第二参数解析为
  字符串池代码，再调用宿主 type 120 并广播返回名称。

`output/ida-probe-tdxw-finance-event-fields.log` 的 TdxW case 167 证明宿主枚举
103 字节板块目录记录：类型 4 为概念、类型 5 为风格，只有当前证券的直接成员
才进入结果。概念先写入、风格随后写入，两个子区间分别调用 `qsort`，总数组最多
60 项。新增离线探针 `output/ida_probe_tdxw_type167_block_codes.py` 又恢复比较器
`sub_607BF0`，证明排序键是无符号 DWORD 数值升序，不是名称或文本顺序；日志为
`output/ida-tdxw-type167-block-codes.log`，SHA-256
`6257FF5AA68A5EE5CD643EC5803E23B32451860D72B6CB3F89B06684C4309508`。

同一 TdxW 日志的 case 120 会定位证券目录记录，并把记录偏移 31 的名称复制到
返回结构偏移 8。这与恢复出的官方公式用法
`GETNAMEOFCODE(1,HYZSCODE)` 一致。

## 纯 C++ 实现

`blocks.cpp/blocks.hpp` 增加三市场 TNF 证券名称目录缓存。缓存以 TDX 根目录为键，
同时比较 `szs.tnf/shs.tnf/bjs.tnf` 的修改时间与大小；任一主表变化就重新解析，
避免用户更新通达信数据后继续看到陈旧名称。

`formula_context.cpp` 从 `infoharbor_block.dat` 的直接成员关系构造 type-167 数组：
概念和风格各自按 32 位数值升序，先限制概念，再用剩余容量限制风格，合计严格
不超过 60。上下文同时带入已加载板块的名称覆盖和 TNF 根目录，但内部路径与
覆盖表不会透传给客户端。

`formula_engine.cpp` 保留 TCalc 的字符串池行为：三个函数在字符串表达式中返回
精确文字，在普通数值求值中只保留不透明句柄；若句柄流入数值输出，分析器会
标记 `degraded_numeric_output`，扫描和回测不能误用。名称未命中、序号小于 1 或
超过数量时均返回空字符串。

能力清单由 218 个函数增至 221，自动符号保持 69；
`custom_formula_block_metadata_functions` 现为
`FGBKZSCODE/GETNAMEOFCODE/GNBKZSCODE/INBLOCK`。390 条静态注册表中的已识别
并集由 270 增至 273，剩余 117。

## 契约与验证

新增固定契约 `formula-block-code-name-inline-post`，同时检查：

- 四个自动依赖 `FGBKZSCODE/GETNAMEOFCODE/GNBKZSCODE/HYZSCODE`；
- 平安银行概念、风格、行业代码的名称和值；
- 越界空字符串；
- type-167 数值排序、概念/风格数量、60 项上限；
- type-120 TNF 名称来源与缓存模式。

验证结果：

- CTest `101/101`；
- 临时服务完整 API `218/218`，共 219 次网络请求；
- 生产候选健康、首页、覆盖、板块代码专项 `4/4`；
- 正式 8765 同四项 `4/4`；
- `formulas analyze` 仍为内置公式 `379/379` 语法支持、`379/379` 数值安全、
  降级数值输出 0。

证据文件及 SHA-256：

- `output/native-formula-coverage-block-code-name-v5.json`：
  `8DF98F4073C7669F25A631CD3ACDF1CE2D9142CAB9C9BFDF8B5CE45827CAAEC6`；
- `output/native-formula-registry-next-audit-v5.json`：
  `82877AA6D6A75FF3D57181CAFCBDC3331970EE53C2487D0FCBE69C187B221FFA`；
- `output/tdx-api-contract-full-v5.json`：
  `41457CB0C0A23FE36EAC566EBD03F0EE2FB9DDF12DF33E269F344594EAA1AC37`；
- `output/native-block-code-name-v5-candidate-api.json`：
  `916FB6BF8597439799930F978D32104F96BEDFD64AE50A3A6B93B824A5B26C6A`；
- `output/native-block-code-name-v5-formal-final-api.json`：
  `B18639E76FA0D19111E43001EE0D3BD6C07F5929B38A0E7C75EB8868CE979EEA`。

## 部署状态与下一步

正式 EXE 为 43,194,300 字节，build/dist SHA-256 均为
`3A9ADD51651E04A7C7A93ADB9C41ECF25E33B517BC19285FD96E87F8E2E9EF88`。
服务 PID 31004，仅监听 `127.0.0.1:8765`；健康状态为 `native_cpp=true`、
`python_runtime=false`。旧版本已保存在
`output/tdx-tool-block-code-name-v5-predeploy-rollback-20260810.exe`，大小
43,001,028 字节，SHA-256
`9DF1E5CD98684057383463DB6D02394FBC3D0519D07D7200E56328A95F4766A7`。
临时 8875 已关闭。

下一批高收益非 L2 目标转为
`FINONE/GPJYONE/BKJYONE/SCJYONE/GPONEDAT`。`ZHBLOCK/SIMIBLOCK` 已保留
精确已知边界，但在运行时提供者与活动目录生命周期闭合前继续延后。
