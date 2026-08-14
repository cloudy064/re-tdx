# 2026-08-10 TCalc 大盘涨跌家数与 DYNA 快捷函数

## 结论

纯 C++ 公式解释器新增六个零参数函数及同名自动符号：

| 名称 | opcode | 原生处理函数 | 语义 | 公开来源 |
|---|---:|---|---|---|
| `INDEXADV` | 1201 | `sub_10023860` | 对应主要指数的上涨家数 | 指数 K 线记录偏移 31 的 `uint16` |
| `INDEXDEC` | 1202 | `sub_10023BD0` | 对应主要指数的下跌家数 | 指数 K 线记录偏移 33 的 `uint16` |
| `DYNA_NOW` | 1380 | `sub_1002A430` | 现价，无现价时返回昨收 | `DYNAINFO(7)` / `0x054C` |
| `DYNA_ZAF` | 1381 | `sub_1002A530` | 涨幅比例 | `DYNAINFO(14)` / `0x054C` |
| `DYNA_LB` | 1382 | `sub_1002A6D0` | 量比 | `DYNAINFO(17)` / `0x054C` + 日线 |
| `DYNA_ZAS` | 1383 | `sub_1002A830` | 涨速比例 | `DYNAINFO(24)` / `0x053E` |

六项都不需要 Level2，也没有调用 Python。裸符号和空参数调用使用同一自动上下文，
例如 `INDEXADV` 与 `INDEXADV()`、`DYNA_NOW` 与 `DYNA_NOW()` 等价。

## 主要指数选择

`INDEXADV/INDEXDEC` 不等价于固定的深证成指或上证指数。TCalc 根据当前市场和
证券代码选择基准，再按目标 K 线的 `日期|时间` 对齐：

- 深市普通证券使用 `SZ399001`；创业板 `3xx` 使用 `SZ399006`；
- 沪市普通证券使用 `SH999999`；科创板 `688/689` 使用 `SH000688`；
- 北交所使用 `BJ899050`；
- 港股使用恒生指数；期货使用当前品种前缀的 `L9` 主连载体。

真实 2026-08-10 样本分别验证了 `SZ000001 → SZ399001`、
`SZ300750 → SZ399006`、`SH600000 → SH999999` 和
`SH688981 → SH000688`。这也修正了旧上下文只按市场粗略选择基准的问题。

## 动态值边界

`DYNA_NOW` 在公开快照的现价为空或近零时按原生帮助回退昨收；`DYNA_ZAF` 返回
比例而不是百分数。`DYNA_LB` 复用公开累计成交量和日线历史均量，并考虑当前
交易日进度。`0x053E` 解码器给出的涨速单位是百分点，`DYNA_ZAS` 按原生帮助
转换成比例，因此网页显示百分数时应再乘 100。

平安银行 20 根真实日线得到：2026-07-14 为 2232 涨、664 跌，2026-08-10
为 1814 涨、1035 跌；现价 11.35、涨幅比例
`0.014298480786416457`、量比 `1.0686987088539397`、涨速比例
`-0.0007999999797903001`。前两项随日期变化，四个即时值按 TCalc 语义广播。

## 覆盖与验证

- IDA 处理函数证据：`output/ida-tcalc-remaining-nonl2-handlers-v1.json`，
  SHA-256 `4583EF117F482A08B00D2C76326EE01565F7912AE1F65B23ED98466A552D537E`；
- 真实公式结果：`output/native-formula-market-breadth-dyna-live-v17.json`，
  SHA-256 `5D90F4A8939DAEAD4F8EC9534E65EB8933A9892F01C8110A3B87EA5A8F9B28CD`；
- 覆盖报告：`output/native-formula-coverage-market-breadth-dyna-v17.json`，
  SHA-256 `752A91ECE3C084CFD32955C674DA82139050EE86AAF253EBE9781F35795E44C8`；
- 注册表差分：`output/native-formula-registry-next-audit-v17.json`，
  SHA-256 `4886E3107F77959AEA87E14AFA0481DE95D5CA3D8E3213D9EE299D882DF48275`；
- 候选专项契约 1/1、完整契约 221/221，完整报告 SHA-256
  `2C785837F7E8B3453AAD4B8A0E4A19C7F55D1ED82BEA59786F1222C77AC47465`；
- 全量 CTest 102/102。

能力清单现为 252 个支持函数、83 个自动符号；390 条静态注册名已识别 307，
剩余 83。379/379 内置公式继续语法支持且数值安全，退化数值输出为 0。

## 正式发布

正式端口健康、首页、新公式、公式覆盖和连板天梯 5/5 通过，报告
`output/api-contract-market-breadth-dyna-v17-formal.json` 的 SHA-256 为
`D02ADBB7665B77DCCEB43E2E70054A2ACF1D612B9E96054001A3E44836A2463D`。
最终 EXE 为 18,376,820 字节，SHA-256
`CF95C92A0EBD706C49FC981791D3B3149EFCE03C9DB28078F2D8E6E30E09A042`；正式服务
PID 12252，仅监听 `127.0.0.1:8765`，保持 `native_cpp=true`、
`python_runtime=false` 并使用 600 份 JSN 资源。

替换前 v16 已保存为
`output/tdx-tool-market-breadth-dyna-v17-predeploy-rollback-20260810.exe`，大小
18,333,696 字节，SHA-256
`FA5C5356438325A1A0D7D12F707B8D6814F1AF2EFA2F37E8E6CFE97249816B91`。

下一批优先验证 `UNDERCODE/UNDERLYC`、`DPZSCODE/DPZSNAME` 和
`DIVFACTOR`；它们已有非 L2 处理函数证据且可复用现有标的、基准与复权数据链。
