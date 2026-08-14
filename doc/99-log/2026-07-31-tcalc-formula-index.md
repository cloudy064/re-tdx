# TCalc 公式索引与只读导出

## 目标

利用新生成的 `TCalc.dll.i64`，恢复技术指标、条件选股、专家系统和五彩
K 线的枚举接口，生成当前版本的系统技术指标列表，并评估合并用户公式的
安全方式。

## 输入与边界

```text
ida/TCalc.dll
ida/TCalc.dll.i64
ida/TdxW.exe
ida/TdxW.exe.i64
```

```text
TCalc.dll SHA-256
13FACAA52DAC552C5BE1F63331781219DE9BF798C443AF8F5E4AFC193A02E7F5

TdxW.exe SHA-256
F5F2E6025A4D80BB1AFBCB2A51C753D3C1909E09AA9D1BC8F7C3B701B081F74C
```

静态实验只读取 DLL 和 IDA 数据库。运行时实验附加现有 PID 16216，先检查
模块、全局槽和虚表实例；因接口对象未初始化而停止，没有调用 TCalc 方法，
也没有打开 `PriGS.dat/PriCS.dat`。

## 结论

- `kind 0/1/2/3` 分别为技术指标、条件选股、专家系统、五彩 K 线；
- DLL 内置数量为 `222/107/15/35`；
- `kind 4` 为容量 500 的内部保留集合，当前没有内置记录；
- `tag_INDEXINFO` 大小为 5072 字节，代码、名称、分类和来源标志已定位；
- 技术指标分类 16 个，条件选股分类 6 个；
- `flags +5068` 可区分系统、临时、缺省和用户公式；
- `InitMain` 和析构均可能影响公式目录，不应对真实用户目录创建独立宿主。

## 实现与验证

新增离线工具 `extract_tcalc_formulas.py`，按 SHA-256 选择版本配置并通过
PE 节表映射 RVA。新增 Frida 工具 `dump_tcalc_runtime.py`，仅对已初始化
对象调用 getter。

测试共 10 项通过，包括当前 DLL 的真实集成校验。离线工具生成
[222 条系统技术指标 CSV](../02-engine/tcalc-system-indicators.csv)，另在
仓库外验证四类 JSON 总计 379 条。

完整接口、结构和安全边界见
[TCalc 公式与指标引擎](../02-engine/04-formula-engine.md)。
