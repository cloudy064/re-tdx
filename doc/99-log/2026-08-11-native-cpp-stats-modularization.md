# 原生 C++ 市场统计资源模块化

日期：2026-08-11

## 目标

拆分原 988 行的 `stats.cpp`。该文件同时承担 0x06B9 文件协议、ZIP 解压、三类统计
资源解析、JSON/估值投影、网络下载和 CLI，职责边界已超过单个实现单元的合理范围。

## 结构

| 文件 | 职责 | 行数 |
|---|---|---:|
| `stats_internal.hpp` | 协议安全上限、证券标识和值对象端口 | 39 |
| `stats_support.cpp` | 时间、参数、证券代码规范化 | 82 |
| `stats_archive.cpp` | 0x06B9 请求、分块响应和 ZIP 安全解压 | 224 |
| `stats_parse.cpp` | tdxstat、tdxstat2、tipinfo 解析与本地载入 | 193 |
| `stats_document.cpp` | 资源 JSON 和 PE/PB 估值投影 | 340 |
| `stats_service.cpp` | 多端点下载、行情/财务并行聚合 | 151 |
| `stats_command.cpp` | CLI 参数适配与输出 | 94 |

原根文件已移除。ZIP 大小、条目数量、单条解压上限和协议命令号集中在内部常量目录；
公开 `tdx/stats.hpp`、结构体和函数签名均未改变。

## 增量验证

1. 构建 `tdx-native-tests` 与 `tdx-tool`：通过。
2. `tdx-native-tests` 中既有 0x06B9 请求、分块长度、stored/deflate ZIP、CRC、
   缺失成员和三类行解析测试：通过。
3. 真实下载样本：
   - schema：`tdx-stats-native-v1`
   - 命令号：`0x06B9`
   - tdxstat：7,975 条
   - tdxstat2：7,975 条
   - tipinfo：5,618 条
   - 请求 `SZ000001` 返回 1 条

样本位于 `output/tdx-stats-refactor-sample.json`。未修改共享传输实现或 schema，因此没有
运行完整 CTest。

