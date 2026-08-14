# FINANCE(48/52) 标的资格与 spblock 数据链

> 日期：2026-08-05。范围：非 L2 的 TCalc 公式上下文；运行时为纯 C++。

## 结论

`FINANCE(48)` 与 `FINANCE(52)` 已从公式求值器一路追到客户端本地数据源，未按
指标名称猜测，也没有用云表存在记录来近似资格：

| 字段 | TCalc type-105 偏移 | TdxW 证券主记录 | `spblock.dat` 来源 |
| --- | ---: | ---: | --- |
| `FINANCE(48)` | `+67` | `+522 || +523` | `#沪港通SH ∪ #深港通SZ` |
| `FINANCE(52)` | `+39` | `+521` | `#融资融券` |

它们都是当前标的资格布尔值，成员返回 1，非成员返回 0。

## 逆向证据

1. `TCalc!sub_10026B20` 为两字段请求宿主类型 105；48 返回缓冲 `+67`，52
   返回缓冲 `+39`。
2. `TdxW!sub_60E580` 的 case `0x69` 把主记录 `+521` 写入 `Destination[39]`，
   并把 `+522/+523` 的逻辑或写入 `Destination[67]`。
3. `TdxW!sub_5A6680` 通过 `sub_5EB820`、`sub_5EBA80`、`sub_5EBB30` 填充这
   三个主记录字节；三个辅助函数都是成员 ID 数组查找。
4. `TdxW!sub_607490` 调用 `sub_5EADA0` 加载数组；后者明确读取
   `T0002\hq_cache\spblock.dat`，在对应 `#` 节内逐行调用证券 ID 解析器。

离线证据保存在：

- `output/ida-probe-tdxw-type105.log`
- `output/ida-probe-tdxw-security-flags.log`
- `output/ida-probe-tdxw-finance4852-helpers.log`
- `output/ida-probe-tdxw-finance4852-arrays.log`

## 原生实现

新增 `native/src/finance_eligibility.cpp`：

- GBK/GB18030 解码 `spblock.dat`；
- 只接受“一位市场号 + 六位代码”的成员行；
- 分别维护融资融券、沪港通 SH、深港通 SZ 集合；
- 按规范化的 `sz/sh/bj` 或 `0/1/2` 市场查询；
- 以规范路径、修改时间和文件长度作为进程内共享缓存失效条件。

公式分析器把 48、52 加入已证实上下文 ID，公式计算结果会附带数据源、资格值和
分组计数元数据。

## 验证

本机 `C:\new_tdx\T0002\hq_cache\spblock.dat` 当前包含：

- 融资融券 4,432 个成员；
- 沪港通 SH 1,900 个成员；
- 深港通 SZ 2,062 个成员。

平安银行 `sz:000001` 的 `FINANCE(48)=1`、`FINANCE(52)=1`。真实“北上资金”
和“两融资金”公式均完成 800 根日线计算并具有有效数值末值。全库覆盖从 307
提高到 309，技术指标从 161 提高到 163；含上下文运行体检 `309/309` 通过，
错误、全空输出和空末值均为 0。MinGW CTest `23/23` 通过。

对应输出：

- `output/tcalc-formula-coverage-finance4852.json`
- `output/tcalc-formula-context-runtime-audit-finance4852.json`
- `output/formula-northbound-finance48-000001.json`
- `output/formula-margin-finance52-000001.json`
