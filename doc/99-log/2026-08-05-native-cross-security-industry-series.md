# 跨证券行情与行业指数公式上下文

> 日期：2026-08-05。实现位于 `native/src/formula_context.cpp` 与
> `native/src/formula_engine.cpp`；运行时为纯 C++，不加载原版 DLL，也不调用 Python。

## 结论

本轮补齐两类不依赖 Level2 的 TCalc 行情序列：

- `"SH999999$AMO"`、`"SZ399001$AMO"` 等跨证券 `O/H/L/C/V/AMO`
  引用，按目标 K 线的日期和时间对齐；
- `HY_INDEXO/H/L/C/V/A/ADV/DEC` 行业指数序列，以及用于绘图文字的
  `HYBLOCK` 行业名称。

由此，`SCAMO` 和 `HYDB` 两个系统技术指标进入原生解释器。全库含只读上下文
覆盖由 314 增至 316，其中技术指标由 168 增至 170；800 根平安银行日线的
316 条候选全部运行通过，315 条具有数值末值。唯一日线全空项仍是只适用于
分钟图的期货参考结算价。

## SCAMO 跨证券序列

公式中的带引号证券字段会规范化为 `EXTERNAL#<证券>$<字段>`。支持显式
`SH/SZ/BJ` 前缀，也支持通达信常见的六位代码推断；行情由 `0x052D` K 线链
取得并按 `date|time` 绑定，不把当前股票的 OHLCVA 冒充外部证券。

真实 `SCAMO` 使用 6 条依赖，涵盖上证综指、深证成指、科创 50 和创业板指。
2026-08-05 最新输出为：

- 市场总额 `26596.3451` 亿元；
- 沪主板 `10544.1375` 亿元；
- 深主板 `7256.4762` 亿元；
- 创业科创 `8795.7314` 亿元。

结果保存在 `output/formula-scamo-cross-security-000001.json`。

## HYDB 与 TCalc/type 120

IDA 恢复的注册表和求值器链为：

| 符号 | TCalc 求值器 | 35 字节行业 K 线字段 |
| --- | --- | ---: |
| `HY_INDEXC` | `sub_10025890` | `+19` |
| `HY_INDEXH` | `sub_10025B60` | `+11` |
| `HY_INDEXL` | `sub_10025E30` | `+15` |
| `HY_INDEXO` | `sub_10026100` | `+7` |
| `HY_INDEXV` | `sub_100263D0` | `+27` |
| `HY_INDEXDEC` | `sub_100252F0` | `+31` |
| `HY_INDEXADV` | `sub_100255C0` | `+31` |

这些函数先请求宿主 type 120。TdxW case 120 在返回结构 `+151` 写入行业代码
尾号，并在 `+173` 标记研究行业模式；TCalc 据此构造 `880%03d` 或
`881%03d`，再按当前周期请求行业指数 K 线。求值器按日期/周期对齐，OHLCV
遇到小于 `1e-5` 的值时沿用前值。

纯 C++ 上下文复用本地 `tdxzs3.cfg/tdxhy.cfg` 关系。默认优先普通通达信行业
的直接叶子归属，无普通行业时回退研究行业；对 `sh:880xxx/881xxx` 本身则
直接使用当前指数。平安银行真实映射为 `880471 银行`，2026-08-05 行业指数
收盘 `3404.49`，`HYDB` 最新行业涨幅 `-1.486464%`。

`HYBLOCK` 是字符串，而当前解释器的数值计算和绘图副作用分离。求值时为
`DRAWTEXT_FIX` 绑定无害的数值占位，同时将真实文本保存在
`context_metadata.formula_text_symbols.HYBLOCK`；行业代码、名称和指数证券也
保存在 `context_metadata.industry_index`。因此 API/前端可以显示“银行”，但
不能把 `DRAWKLINE` 的零返回值误解为行业收盘价。

## 产物与验证

- 离线探针：`output/ida_probe_tcalc_industry_symbols.py`；
- 反编译日志：`output/ida-probe-tcalc-industry-symbols.log`；
- SCAMO 实测：`output/formula-scamo-cross-security-000001.json`；
- HYDB 实测：`output/formula-hydb-industry-index-000001.json`；
- 覆盖报告：`output/tcalc-formula-coverage-professional.json`；
- 运行审计：`output/tcalc-formula-context-runtime-audit-professional.json`；
- MinGW CTest：24/24 通过。

## 明确边界

TdxW 的行业模式可由桌面设置切换。本工具当前采用普通 `880xxx` 行业优先、
`881xxx` 研究行业回退的确定性规则，尚未读取桌面进程内的模式全局量。
跨证券序列只开放已验证的 OHLCVA 字段；任意字符串字段和未经证明的外部
公式引用仍保持不可执行。
