# 原生 C++ 期货与发行数据模块化

日期：2026-08-11

## 目标

将 `native/src/futures_issuance.cpp` 从单个 1,039 行实现拆成按业务职责组织的实现单元，
同时保持公开 C++ 接口、CLI 参数和 `tdx-futures-issuance-native-v1` JSON schema 不变。

## 结构调整

| 文件 | 职责 | 行数 |
|---|---|---:|
| `futures_issuance_internal.hpp` | 资源/选项类型化目录与内部端口 | 116 |
| `futures_issuance_support.cpp` | JSN 标量、证券身份、缓存文档和通用限制工具 | 157 |
| `futures_issuance_futures.cpp` | 商品期货、月度统计、股指期货及合约详情归一化 | 104 |
| `futures_issuance_ipo.cpp` | IPO 年度、行业、月度、证券与债券发行人归一化 | 106 |
| `futures_issuance_capital.cpp` | 定增、配股、优先股归一化与汇总 | 336 |
| `futures_issuance_service.cpp` | 参数校验、缓存获取和查询结果编排 | 319 |
| `futures_issuance_command.cpp` | CLI 参数适配与文件输出 | 66 |

原根文件已移除。16 个固定资源被集中为五组 `ResourceDefinition` 编译期目录：
3 个期货、3 个 IPO/债券、6 个定增、3 个配股和 1 个优先股资源。6 个 section 与
7 个定增状态也改成编译期目录；服务通过目录批量生成请求列表，不再维护散落常量和临时
`PlacementSource`/`RightsSource` 结构。

这里采用数据驱动目录与函数端口组合，没有引入需要运行时分配的继承层级。

## 兼容性

- 公开头文件 `tdx/futures_issuance.hpp` 未改变。
- CLI 仍为 `tdx-tool market futures-issuance`。
- 输出 schema、字段名称、过滤规则、排序规则和缓存语义未改变。
- 动态合约、IPO 年份和行业详情资源仍按原规则生成。

## 增量验证

仅执行受影响范围：

1. 构建 `tdx-futures-issuance-tests` 与 `tdx-tool`：通过。
2. 运行 `tdx-futures-issuance-tests`：通过。
3. 运行 `--section futures --limit 3` 真实样本：通过。
   - schema：`tdx-futures-issuance-native-v1`
   - 资源数：3
   - 商品期货：74
   - 月度记录：90
   - 股指期货：4

样本保存在 `output/futures-issuance-refactor-sample.json`。本轮未改共享解析器、传输层
或 schema，因此没有运行完整 CTest。

