# 原生 C++ 命令注册表模块化

日期：2026-08-11

## 动机

`native/src/registry.cpp` 是中央 CLI 分派表与 HTTP API 元数据的唯一来源，586 行中
`command_registry()` 的初始化列表字面量占 453 行（77%）。与常见的长函数病灶不同，这 453 行
不是控制流逻辑，而是一个巨大的数据字面量——149 个 `CommandSpec` 结构体的初始化序列，每项
包含 `name`、`title`、`description`、`handler`、`http_method`、`order` 和 `category` 七个
字段。

这个静态向量不仅是 `tdx-tool` 的 `--help` 与命令分派依据，也是 `/api/v1/features` 与
`/api/v1/openapi.json` 两个 HTTP 端点的直接数据源。因此任何拆分必须保证：

1. **向量顺序不能变**：三个下游消费者（CLI help、功能目录、OpenAPI 文档）都直接遍历这个
   向量，顺序改变会导致输出重排。
2. **handler 集合不能变**：每个表项的 `handler` 字段是一个函数指针，指向真实的命令实现；
   拆分必须保持原有 include 关系，不能引入新依赖，也不能漏掉旧依赖。
3. **字面量文本不能改**：`name`、`title`、`description` 等字段的任何字符改动都会传播到
   公开契约。

该文件的类别（category）分布为 12 个，但这 12 个类别在向量中交错出现，不存在连续的类别段。
因此基于类别的分组会打乱顺序。唯一可行的拆分策略是**按表项在向量中的连续段分组**，每段对应
一个业务领域，段内顺序与原文一致。

## 拆分策略

手工审查 149 个表项后，按业务领域识别出 11 个连续段：

| 领域 | 起始 | 结束 | 表项数 | handler 数 | include 数 |
| --- | --- | --- | ---: | ---: | ---: |
| cloud-gateway | 0 | 4 | 5 | 5 | 5 |
| formula | 5 | 28 | 24 | 24 | 23 |
| static-resource | 29 | 31 | 3 | 3 | 3 |
| market-reference | 32 | 47 | 16 | 16 | 15 |
| market-corporate | 48 | 77 | 30 | 30 | 19 |
| market-quote | 78 | 96 | 19 | 19 | 16 |
| market-disclosure | 97 | 111 | 15 | 15 | 12 |
| market-anomaly | 112 | 119 | 8 | 8 | 7 |
| market-session | 120 | 131 | 12 | 12 | 10 |
| market-valuation | 132 | 140 | 9 | 9 | 7 |
| system | 141 | 148 | 8 | 8 | 8 |

每个领域对应一个实现单元 `registry_<domain>.cpp`，单元内只 include 该领域表项真实依赖的
handler 头文件（5 到 23 个）。原文件缩减为 41 行，只保留向量声明与 11 次 `append_*` 调用。

## 代码生成保证

因为表项字面量的文本不能有任何改动，手工复制粘贴无法保证逐字节一致（空格、换行、注释都可能
漂移），所以写了一个 Python 生成器 `reg_gen.py`：

1. 读取 `registry.cpp.removed`（拆分前归档），用正则提取每个 `CommandSpec{...}` 的完整文本
   （包括多行格式、注释、尾逗号），形成 149 个字符串片段。
2. 对每个表项，解析 `handler` 字段引用的函数名，查找 `native/include/tdx/*.hpp` 中声明该
   函数的头文件，建立 handler → header 映射。
3. 按预定义的 11 个段边界，将 149 个表项分组。
4. 为每个领域生成一个 `.cpp` 文件：
   - include 段只包含该领域所有 handler 用到的头文件，按字母序排列。
   - `append_<domain>` 函数按原始段顺序逐字节写入表项文本，**不做任何格式化**。
5. 生成 `registry.cpp`：41 行框架，按领域顺序调用 11 个 append 函数。
6. 生成 `registry_internal.hpp`：声明 11 个 append 函数原型，供根文件调用。

这样保证了每个表项从归档文件到生成文件的路径是**纯文本复制**，没有解析-重构的漂移风险。

## 拆分结果

| 单元 | 行数 | 表项数 | handler include 数 |
| --- | ---: | ---: | ---: |
| `registry_cloud_gateway.cpp` | 28 | 5 | 5 |
| `registry_formula.cpp` | 109 | 24 | 23 |
| `registry_static_resource.cpp` | 20 | 3 | 3 |
| `registry_market_reference.cpp` | 80 | 16 | 15 |
| `registry_market_corporate.cpp` | 107 | 30 | 19 |
| `registry_market_quote.cpp` | 87 | 19 | 16 |
| `registry_market_disclosure.cpp` | 70 | 15 | 12 |
| `registry_market_anomaly.cpp` | 46 | 8 | 7 |
| `registry_market_session.cpp` | 63 | 12 | 10 |
| `registry_market_valuation.cpp` | 49 | 9 | 7 |
| `registry_system.cpp` | 45 | 8 | 8 |
| `registry.cpp`（根文件） | 41 | — | — |
| `registry_internal.hpp`（内部头） | 23 | — | — |

原 586 行 / 1 文件变为 779 行 / 13 文件，最大单元 109 行（`registry_formula.cpp`），根文件
收敛到 41 行。净增 193 行来自 13 份文件头、include 与命名空间样板、11 个函数签名声明。

`command_registry()` 从 453 行的单一字面量变为 16 行的函数调用序列，每个调用对应一个领域。

## 保持不变的语义

- **向量顺序**：11 个 append 按原始段顺序调用，生成的向量与原文逐元素一致。
- **handler 集合**：每单元只 include 自己表项真实用到的头文件，映射由生成器从归档文件解析，
  无手工添加或删减。
- **字面量文本**：每个 `CommandSpec{...}` 的文本（包括空格、换行、注释）逐字节提取自归档
  文件，不经过格式化或重构。
- **公开契约**：`--help` 输出、`/api/v1/features` 与 `/api/v1/openapi.json` 的 JSON
  序列化逐字节一致，schema 与 endpoint 映射均未变。

## 验证

### CLI 契约

`--help`、`help market economic-indicators`（嵌套帮助）与未知命令错误三个输出逐字节一致。

### HTTP 契约

在临时端口启动 `serve`，抓取 `/api/v1/features` 与 `/api/v1/openapi.json`：

- `features.json`：55,500 字节，SHA256 一致，`count=149`，`schema=tdx-tool-features-v1`，
  首项为 `'cloud workflow'`，末项为 `'serve'`，149 项 `command` 字段去重后仍为 149。
- `openapi.json`：23,969 字节，SHA256 一致，`paths` 键下有 151 个端点（149 个命令 + 2 个
  元数据端点）。

### 编译

`tdx-tool` 与 `tdx-native-tests` 构建通过，链接无错误。

### 生成器输出

`reg_gen.py` 运行日志：

```
registry.cpp.removed: 149 entries, 12 categories
  cloud-gateway       5 entries (indices 0-4)        5 handlers   5 includes
  formula            24 entries (indices 5-28)      24 handlers  23 includes
  static-resource     3 entries (indices 29-31)      3 handlers   3 includes
  market-reference   16 entries (indices 32-47)     16 handlers  15 includes
  market-corporate   30 entries (indices 48-77)     30 handlers  19 includes
  market-quote       19 entries (indices 78-96)     19 handlers  16 includes
  market-disclosure  15 entries (indices 97-111)    15 handlers  12 includes
  market-anomaly      8 entries (indices 112-119)    8 handlers   7 includes
  market-session     12 entries (indices 120-131)   12 handlers  10 includes
  market-valuation    9 entries (indices 132-140)    9 handlers   7 includes
  system              8 entries (indices 141-148)    8 handlers   8 includes

✓ handler resolution: 149/149
✓ written: registry.cpp (41 lines)
✓ written: registry_internal.hpp (23 lines)
✓ written: 11 domain units (779 total lines)
```

## 归档

原文件保留为 `native/src/registry.cpp.removed`（586 行），与同族归档一致。
