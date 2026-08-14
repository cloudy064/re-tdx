# 原生 C++ 机构持仓与股东链路模块化

日期：2026-08-11

原 870 行 `institution.cpp` 同时承担 TQLEX 重试、持久缓存、身份与参数校验、机构/股东
归一化、两种查询编排和 CLI。现按数据来源与查询层拆为：

| 文件 | 职责 | 行数 |
|---|---|---:|
| `institution_internal.hpp` | 内部端口，以及入口、基址、重试次数、缓存上限常量 | 62 |
| `institution_cache.cpp` | TQLEX 请求、重试、持久缓存与旧缓存导入 | 178 |
| `institution_support.cpp` | 路径、时间、JSON、URL、证券身份和请求参数支持 | 175 |
| `institution_security_normalize.cpp` | 单证券机构历史、流通股东和来源摘要 | 57 |
| `institution_holder_normalize.cpp` | 股东引用、TQLEX 行及跨股票持仓归一化 | 134 |
| `institution_security_service.cpp` | 单证券查询编排 | 108 |
| `institution_holder_service.cpp` | 跨股票股东查询编排 | 203 |
| `institution_command.cpp` | CLI 参数与输出 | 91 |

旧根文件已移除。公开的 `InstitutionService`、两种查询对象和 JSON schema 保持不变；
三次 TQLEX 尝试、64 MiB 持久缓存边界及两类缓存键集中在内部接口，不再散落于查询控制流。

增量验证：`tdx-institution-lhb-tests`、`tdx-institution-resilience-tests` 与 `tdx-tool`
构建、测试通过。真实 `sz000001` 样本返回机构历史、十大流通股东和可查询股东引用各一组，
共两个来源、零错误，schema 仍为 `tdx-security-profile-native-v2`。样本位于
`output/institution-refactor-sz000001.json`。本轮未运行完整 CTest。
