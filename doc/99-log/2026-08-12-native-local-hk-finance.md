# 本地港股财务缓存

## 结论

`T0002/hq_cache/hkcwdata.dat` 已形成独立的纯 C++ 能力：

- CLI：`tdx-tool market hk-finance`；
- HTTP：`GET /api/v1/market/hk-finance`；
- schema：`tdx-market-hk-finance-native-v1`；
- 数据源只读、全程本地，单次查询网络请求数为 0；
- 当前真实文件解出 3,153 条唯一港股代码记录、固定 17 列、32 个非空分类码；
- 报告日期范围为 `20050731..20251231`。

公开结构严格区分“已恢复的原生绑定”和“尚未证实的枚举标签”。后续 HK 消费者证据
已将 16/16 个数据列闭合为核心财务或元数据；源列 15 的币种折算角色已经确认，但其
原生数值到币种名称的枚举仍不猜测。完整语义证据和类型化投影见
[港股财务字段语义闭环](2026-08-12-native-hk-finance-semantics.md)。

## TdxW 证据

`TdxW.exe.i64` 中：

- `sub_640690` 从 `tdxbase/hkcwdata.dat` 下载到本地缓存；
- `sub_51F310` 打开本地文件，以密钥 `SECURE20090531_TDXSS` 初始化 Blowfish，
  按 8 字节块解密，尾部不足 8 字节保持原样；
- 解密文本中的空列按客户端兼容规则补 0，然后严格拆成 17 列；
- 第 0 列查找港股证券，第 2/12 列写入日期槽，第 10 列写入最多 10 字节的
  分类槽；其余数值按固定缩放写入证券或伴随结构。

IDA 证据保存在：

- `output/ida-hkcwdata-xrefs-20260812.json`；
- `output/ida-hkcwdata-field-offset-xrefs-20260812.json`；
- `output/ida-hkcwdata-consumers-20260812.json`。

17 列的原生绑定如下：

| 索引 | 接口键 | TdxW 目标 | 转换 | 当前语义状态 |
| ---: | --- | --- | --- | --- |
| 0 | `code` | 证券查找键 | 5 位字符串 | 港股代码 |
| 1 | `security_152` | `security+152` | `atof/10000` | H 股（万股） |
| 2 | `security_124` | `security+124` | `atol` | 报告日 |
| 3 | `security_196` | `security+196` | `atof` | 营业收入（万报告币种） |
| 4 | `security_240` | `security+240` | `atof/100` | 每股股息（报告币种/股） |
| 5 | `security_156` | `security+156` | `atof/100` | 每股收益（报告币种/股） |
| 6 | `security_160` | `security+160` | `atof` | 总资产（万报告币种） |
| 7 | `security_192` | `security+192` | `atof` | 净资产（万报告币种） |
| 8 | `security_236` | `security+236` | `atof` | 净利润（万报告币种） |
| 9 | `security_244` | `security+244` | `atof` | 每股净资产（报告币种/股） |
| 10 | `companion_37` | `companion+37` | `string[10]` | 分类码 |
| 11 | `companion_56` | `companion+56` | `atof` | 市盈率（TTM） |
| 12 | `security_128` | `security+128` | `atof-to-int` | 上市日 |
| 13 | `security_132` | `security+132` | `atof/10000` | 总股本（万股） |
| 14 | `security_184` | `security+184` | `atof` | 少数股权（万报告币种） |
| 15 | `security_352` | `security+352` | `atof-to-byte` | 币种折算码（枚举名未解析） |
| 16 | `companion_52` | `companion+52` | `atof` | 市盈率（静） |

每条接口记录同时提供类型化的 `native_values` 和逐列 `raw_fields`；缺失值在接口中
保留为 `null`，并在 `native_schema.empty_field_native_substitution=0` 记录客户端
实际写入时的补零行为。

## 实现边界

实现按重构后的职责边界拆分：

- `common/blowfish.cpp` 与常量目录承载通用算法，
  `hk_resource_crypto_internal.hpp/hk_actions_blowfish.cpp` 保留港股资源包装；
- `hk_finance_parse.cpp` 负责尺寸、行长、列数、代码、日期、数值、重复代码校验与
  按文件时间戳缓存；
- `hk_finance_query.cpp` 负责查询、排序、分页、统计和原生 schema；
- `hk_finance_command.cpp` 只负责 CLI 参数编排；
- 命令注册、HTTP handler、静态路由和 OpenAPI 目录仍各自留在既有模块。

这次没有把本地港股数据错误回落到 A 股 `0x0010/0x000F`。核心字段现已投影到每行的
`finance` 对象，但没有把 HK 扩展市场直接宣称为 TCalc `FINANCE` 支持域；公式注册表的
扩展市场边界保持不变。

## 验证

- 增量构建 `tdx-hk-finance-tests`、`tdx-hk-actions-tests`、`tdx-tool` 通过；
- 两个聚焦 CTest 均通过，共用解密抽取未破坏港股公司行动；
- 真实 `00001` 返回报告日 `20250630`、上市日 `19721101`、分类码 `111001`；
- `serve --self-test` 通过，功能数为 154；
- 临时 18768 验证单票、分类查询、400 参数拒绝、功能目录与 OpenAPI 路径；
- 临时服务已自动退出，正式 8765 始终保持 PID 24096；
- 按增量验证预算未运行完整 CTest 或全 API 契约。

真实输出与聚焦证据：

- `output/tdx-market-hk-finance-native.json`；
- `output/tdx-market-hk-finance-catalog.json`；
- `output/hk-finance-api-contracts-20260812.json`。

## 后续

剩余工作集中在源列 15 的币种枚举名称，以及是否存在新的 DLL 证据证明 TCalc
`FINANCE` 支持港股扩展市场。没有直接证据前不复用 A 股公式选择器。
