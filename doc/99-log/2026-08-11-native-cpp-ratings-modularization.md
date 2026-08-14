# 原生 C++ 评级链路模块化

日期：2026-08-11

## 问题

原 887 行的 `ratings.cpp` 同时包含港股/美股/研究行业三项主资源、三类证券身份、港股名称
补全、评级倾向分类、三类摘要、四类归一化、资源缓存、明细查询和 CLI。固定资源与查询
控制流耦合，新增一个评级市场需要同时修改多处分支。

## 调整

| 文件 | 职责 | 行数 |
|---|---|---:|
| `ratings_internal.hpp` | 三项类型化资源目录和共享内部端口 | 74 |
| `ratings_support.cpp` | 路径、JSON、三类证券身份、过滤和参数支持 | 225 |
| `ratings_names.cpp` | 港股关系、A/H 配置与目标代码名称补全 | 87 |
| `ratings_summary.cpp` | 港股、美股和行业汇总 | 103 |
| `ratings_normalize.cpp` | 倾向分类与四类主表/报告归一化 | 180 |
| `ratings_fetch.cpp` | 三路主资源、明细缓存和失败负缓存 | 109 |
| `ratings_query.cpp` | 三视图选择、明细装配和结果 schema | 202 |
| `ratings_command.cpp` | CLI 参数、研究行业目录和输出 | 66 |

原根文件已移除。三项固定资源由单一编译期目录表达，名称补全、归一化、摘要和缓存不再与
CLI 或结果投影处于同一翻译单元。

## 兼容性

- `tdx/ratings.hpp` 的公开结构、归一化函数和服务接口未改变。
- `hong-kong`、`us`、`industries` 三视图和倾向筛选语义未改变。
- 输出 schema 保持 `tdx-market-ratings-native-v1`。

## 增量验证

1. 构建 `tdx-ratings-tests` 与 `tdx-tool`：通过。
2. 评级专项测试：通过。
3. 港股目录代表样本为 `live`，主目录含 619 只证券，返回 3 条、三路来源、0 个明细错误，
   schema 为 `tdx-market-ratings-native-v1`。

样本位于 `output/ratings-refactor-hong-kong.json`。本轮未修改共享 JSN 解析器、传输层或
schema，因此依照增量验证规则未运行完整 CTest。
