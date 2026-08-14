# TCalc RAND 与本地安全/亮点分闭环

## RAND 原生语义

`RAND` 对应 opcode 1181、处理函数 `TCalc.dll!sub_1000CA40`。32 位探针直接调用
该地址，并以同一 `time64` 秒值运行 MSVC CRT LCG，八项边界输出完全匹配：

```text
input : 10,null,1,32768,1000000,0.99,10.49,10.5
output: 2,null,1,245,31869,null,5,8
```

实现保留以下细节：

- 每次 `RAND` 调用重新播种，单次调用内按柱推进；
- 参数先收窄为 float32，有效范围为 `1..1000000`；
- 模数为 `trunc(N+0.503000020980835)`；
- 缺失或越界柱返回缺失，不消耗随机状态；
- 默认种子是求值时的当前秒；context 的 `formula_random_seed` 可覆盖为
  `uint32`，用于测试、审计与重放。

证据文件为 `output/native-tcalc-rand-probe-v1.json`。

## SAFESCORE/SHINESCORE 本地来源

TCalc opcode 1356/1388 请求 TdxW type 167。宿主偏移 51/67 分别调用
`sub_4FA4A0/sub_4FA510`；继续反编译加载器 `sub_4F5560` 后确认实际来源为：

```text
T0002/hq_cache/specgpext.txt
market|code|main_business|safety_score|shine_score|...
```

加载器用 `atol/atof` 解析前五个字段并写入 18 字节记录。两项分数分别位于记录
偏移 6/10；`SHINESCORE` 仍先检查安全分字段是否大于原生有效阈值，因此实现不会
把两个字段误当作完全独立的可用值。该目录已经用于 `MAINBUSINESS`，本次扩展同一
缓存解析器，不增加网络或登录依赖。

## 运行结果与覆盖

平安银行 120 根日线使用显式种子 1：随机前四项 `2,8,5,1`，所有安全分为 92，
亮点分为 7。候选健康、公式覆盖、新公式契约 3/3 通过。

能力清单由 258/87 增至 261 个支持函数、87 个自动符号；390 条静态注册名识别
由 313 增至 316，剩余 74。379/379 内置公式继续语法支持且数值安全，退化数值
输出为 0。按增量验证规则，仅运行公式引擎、契约测试、一个真实样本和三项候选
API，没有运行全量 CTest 或完整 API 套件。

解释器同时完成职责拆分：函数注册集合位于 `formula_registry.cpp`，原生函数运行
时位于 `formula_functions.cpp`，`formula_engine.cpp` 降至约 4,800 行。

下一项非 L2 高价值注册函数为 `CALCSTOCKINDEX`，需要有界跨证券嵌套求值和循环
检测。本批构建尚未替换 8765 正式服务，将与下一阶段功能合并做一次发布回归。
