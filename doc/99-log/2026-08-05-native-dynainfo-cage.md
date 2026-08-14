# DYNAINFO(28/29) 即时笼子价与 type 121

> 日期：2026-08-05。范围：非 L2 的 TCalc 公式上下文；运行时为纯 C++。

## 结论

`DYNAINFO(28)` 与 `DYNAINFO(29)` 已沿真实宿主调用链恢复为即时买入/卖出
笼子价，不是涨跌停价，也不是一个静态百分比：

| 字段 | TCalc 宿主请求 | TdxW 返回偏移 | 首选基准 |
| --- | --- | ---: | --- |
| `DYNAINFO(28)` | type 121 / selector 198 | `+4` | 卖一 |
| `DYNAINFO(29)` | type 121 / selector 198 | `+8` | 买一 |

基准缺失时依次使用另一侧盘口、现价、昨收。普通证券应用 `102%/98%`，北交所
应用 `105%/95%`；股票还要求至少偏离 10 个最小价位，最后被当日涨停/跌停
边界截断。

## 逆向证据

1. `TCalc!sub_10047770` 将 `DYNAINFO` 映射为内建 token 100000，运行表指向
   `sub_1002A400 -> sub_10028670`。
2. `sub_10028670` 的 28/29 分支均请求宿主 type 121、selector 198，并把返回
   缓冲 `+4/+8` 复制成整段公式常量序列。
3. `TdxW!sub_60FFF0` case 121 的 selector-198 分支读取 150 字节 L1 记录，调用
   `sub_93AC50` 和 `sub_93AFD0` 后写回这两个偏移。
4. 两个辅助函数明确体现盘口回退、2%/5%、10 个价位、价格精度和涨跌停截断。

离线证据：

- `output/ida-probe-tcalc-dynainfo-evaluator.log`
- `output/ida-probe-tdxw-finance-callback.log`
- `output/ida-probe-tdxw-cage-helpers.log`

## 原生实现与验证

新增 `native/src/market_cage.cpp`，并在公式上下文需要 28/29 时直接读取
`0x0547` 五档。该记录同时含现价、昨收、成交量等普通快照字段，因此盘口公式
不再额外请求 `0x054C`。公式结果暴露所用命令、模式、上下基准、结果和价位
精度元数据。

后续又接入公开行情命令 `0x0452` 的特殊涨跌停全表：命中记录时，
`DYNAINFO(26/27)` 以及 28/29 的最终截断直接使用服务端给出的精确上下限；
未命中或全表临时不可用时才回退板级规则。全表当前 775 条，按本地自然交易日
缓存一次。`SH600182` 实测精确边界为 `13.67/12.37`，避免按主板规则误算成
约 `14.32/11.72`；`SZ001232` 等无涨跌幅限制样本也由表中哨兵边界表达。

平安银行真实盘口样本：卖一 `11.25`、买一 `11.24`，计算结果为上限 `11.48`、
下限 `11.02`。系统 `FSCAGE` 在 800 根 1 分钟线上成功执行；全库含上下文覆盖
由 309 增至 310，技术指标由 163 增至 164。800 根日线全库体检 `310/310`
通过，错误、全空输出、空末值均为 0；MinGW CTest `24/24` 通过。

对应输出：

- `output/tcalc-formula-coverage-dynainfo2829.json`
- `output/tcalc-formula-context-runtime-audit-dynainfo2829.json`
- `output/formula-fscage-dynainfo2829-000001.json`
- `output/probe-special-limits-live.json`
- `output/formula-special-limit-probe-600182.json`

## 边界

笼子价核心、盘口回退及当前 `0x0452` 特殊证券精确边界已经闭合。未出现在
`0x0452` 表中的普通证券仍按主板/创业板/科创板/北交所板级规则计算；当服务端
全表不可用时，结果元数据会明确标记 `board-rate-fallback`，不会伪装成特殊
证券精确值。
