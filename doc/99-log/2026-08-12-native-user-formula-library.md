# PriGS 用户公式只读库与解释器接入

日期：2026-08-12

## 结论

纯 C++ 统一工具新增 CLI-only 子命令：

```powershell
tdx-tool formulas user-library --root C:\new_tdx
```

它不加载 32 位 `TCalc.dll`，直接只读解析 `T0002/PriGS.dat`，恢复用户公式的类型、
运行时索引、代码、名称、分类、参数、输出和源码。输出沿用公式解释器现有 library
结构，可直接传给公式工作流；追加 `--scope combined` 可把用户公式与 DLL 内置库
合并。也可以省略中间 JSON，让分析、上下文模板、审计、求值、扫描/监控、组合策略
和回测通过 `--root C:\new_tdx --include-user` 直接只读装载 `PriGS.dat`。该开关保持
显式且仅限 CLI，避免网页/API 无意暴露本地策略源码。

当前真实安装中：

- `PriGS.dat` 版本为 5；
- 用户公式 1 条，属于条件选股，运行时索引为 107；
- 源码动态段 318 字节，解码后 288 个 Unicode 字符；
- 解释器分析为语法支持、可执行、无外部依赖、无未来函数；
- 在平安银行 120 根本地日线上执行成功，输出槽为“选股”；
- 与 379 条内置公式合并后共 380 条，380 条均有源码。

文档和验证摘要不展开用户私有源码正文。

## 文件格式

### PriGS.dat

`TCalc.dll!sub_1009C070` 是加载器，`sub_1009A870` 是保存器。版本 5 布局为：

```text
53-byte header
N * 10-byte dynamic-segment index
N * 5072-byte formula record
8-byte-aligned encrypted dynamic payload
```

头部关键字段：

| 偏移 | 类型 | 语义 |
|---:|---|---|
| 0 | u8 | 文件版本 |
| 1 | u32le | 公式总数 N |
| 5 | u32le | 固定记录起点，必须为 `53 + 10*N` |
| 9 | u32le | 动态区起点，必须再加 `5072*N` |
| 13 | u32le | 加密动态区字节数 |
| 17 | 5*u16le | 五类公式计数 |

每个 10 字节索引是 `u16/u16/u16/u32` 四个动态段长度，按固定记录的
`+5052/+5056/+5064/+5060` 指针顺序排列。第一个动态段为公式源码。动态段实际长度
之和与加密区之间只允许不足 8 字节的块尾填充。

动态区密码变换由 `sub_10046A10 -> sub_100475B0 -> sub_10046E90` 证明：使用标准
Blowfish 初始 P/S 表，不调用密钥扩展；逐 8 字节 ECB 解密，左右 32 位字按本机
小端读取。它不是空字符串密钥的标准 Blowfish。新增私有公共模块复用已有 P/S
常量，避免再复制 1042 个常量。

### PriCS.dat

`sub_10099810` 只保存属性 `flags & 1` 的系统公式参数。当前文件头的总数和四类计数
是 `379` 与 `222/107/15/35`；42 字节头后恰好是 `379 * 2112` 字节。每公式 2112
字节又是 16 个 132 字节参数槽。因此 `PriCS.dat` 不应被当成用户公式源码库。

## 原生实现

- `native/src/formula/formulas_user.cpp`：严格文件解析、原生 Blowfish 变换和 library
  JSON；
- `native/src/formula/formulas_user_command.cpp`：`user/combined` 范围、分类名补齐和
  输出；
- `native/src/common/blowfish.cpp`：新增仅供协议解析器使用的“初始状态直接解密”；
- `native/tests/formulas_tests.cpp`：无网络的合成版本 5 文件契约。

用户库来源标为 `source_text_origin=user-file-decrypted`、
`private_user_data=true`、`network_requests=0`。命令注册为 CLI-only，避免把本地
策略源码自动暴露到网页/API。

## 聚焦验证

- 增量构建：`tdx-tool`、`tdx-formulas-tests`；
- 专项 CTest：1/1，通过，约 0.48 秒；
- 真实用户库：1 条，全部源码恢复；
- 合并库：380 条，全部源码可用；
- 用户公式分析：语法 1/1、可执行 1/1、外部依赖 0；
- 本地真实日线求值：120 根，成功输出；
- 直接装载全库审计：380 条、可执行 228 条、通过 228 条、错误 0；
- 直接装载扫描：评估 1 条证券、错误 0；单轮监控生成 `snapshot`；
- 用户条件公式组合回测：清单引用成功，原生回测完成；
- 相关专项 CTest：3/3 通过，约 2.2 秒；
- 正式 `127.0.0.1:8765` 未重启、未替换。

机器可读产物：

- `output/verify-user-formulas-20260812.json`
- `output/verify-installed-formulas-20260812.json`
- `output/verify-user-formulas-analysis-20260812.json`
- `output/verify-user-formula-evaluate-20260812.json`
- `output/verify-installed-formulas-analysis-direct-20260812.json`
- `output/verify-installed-formulas-audit-direct-20260812.json`
- `output/verify-user-formula-evaluate-direct-20260812.json`
- `output/verify-user-formula-scan-direct-20260812.json`
- `output/verify-user-formula-watch-direct-20260812.jsonl`
- `output/verify-user-formula-strategy-backtest-direct-20260812.json`
- `output/ida-tcalc-pri-file-xrefs-20260812.json`
- `output/ida-tcalc-pri-codecs-20260812.json`
- `output/ida-tcalc-pri-crypto-20260812.json`
