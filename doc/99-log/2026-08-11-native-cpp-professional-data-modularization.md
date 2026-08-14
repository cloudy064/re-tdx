# 原生 C++ 专业数据链路模块化

日期：2026-08-11

## 问题

原 952 行的 `professional_data.cpp` 同时包含证券与日期工具、两份官方 manifest、MD5 缓存、
ZIP 中央目录和 deflate 解压、101 项字段名称、财务/交易二进制解析、远程获取、JSON 投影和
CLI。安全边界、协议细节与用户入口紧密耦合，任一资源格式调整都需要改动同一个文件。

## 调整

| 文件 | 职责 | 行数 |
|---|---|---:|
| `professional_data_internal.hpp` | 固定 URL、三项大小上限、内部数据结构和端口 | 85 |
| `professional_data_support.cpp` | 证券、日期、数值和边界读取支持 | 124 |
| `professional_data_resource.cpp` | manifest、HTTP、MD5 校验和原子缓存 | 95 |
| `professional_data_zip.cpp` | ZIP 目录、deflate、CRC 与膨胀上限 | 113 |
| `professional_data_catalog.cpp` | 101 项字段目录与公开资源目录 | 96 |
| `professional_data_parse.cpp` | 财务/交易解析、序列和单点语义 | 185 |
| `professional_data_fetch.cpp` | 股票/市场/财务包和多期财务获取 | 184 |
| `professional_data_document.cpp` | 交易与财务 JSON 投影 | 99 |
| `professional_data_command.cpp` | CLI 参数、模式选择和输出 | 109 |

原根文件已移除。固定资源约束集中在内部头，远程资源获取与二进制格式解析分离；ZIP 安全
检查仍独立覆盖中央目录、压缩方法、膨胀大小、流完整性和 CRC。

## 兼容性

- `tdx/professional_data.hpp` 的公开结构和函数未改变。
- 财务、财务序列、交易与目录 schema 未改变。
- 官方 URL、manifest、MD5 校验、缓存和大小上限语义未改变。

## 增量验证

1. 构建 `tdx-professional-data-tests` 与 `tdx-tool`：通过。
2. 专业数据专项测试覆盖财务/交易解析和查询语义：通过。
3. 官方目录样本返回 147 个财务包和 8,262 个交易文件，schema 为
   `tdx-professional-catalog-v1`。

样本位于 `output/professional-data-refactor-catalog.json`。本轮未修改共享 HTTP、JSON 或
API schema，因此依照增量验证规则未运行完整 CTest。
