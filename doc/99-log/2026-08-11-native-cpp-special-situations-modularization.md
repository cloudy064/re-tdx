# 原生 C++ 特殊情形链路模块化

日期：2026-08-11

## 目标

拆解 794 行的 `special_situations.cpp`，把十项资源路径、资源语义、行归一化、行情附加、
缓存抓取、查询编排和 CLI 从同一个实现文件中分离。

## 落地结果

原根文件已移除，形成七个实现单元：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `special_situations_catalog.cpp` | 55 | 10 项类型化资源目录与重复路径检查 |
| `special_situations_support.cpp` | 217 | 字段、证券身份、本地 JSN 和参数公共支持 |
| `special_situations_normalize.cpp` | 176 | 按 `ResourceKind` 归一化十类来源 |
| `special_situations_quotes.cpp` | 42 | L1 行情匹配与溢价投影 |
| `special_situations_service_fetch.cpp` | 80 | 主数据、行情缓存和抓取 |
| `special_situations_service_query.cpp` | 207 | 过滤、排序、汇总和响应组合 |
| `special_situations_command.cpp` | 47 | CLI 参数与输出 |

原先依赖十个路径字符串比较的控制流改为 `ResourceDefinition` 目录和 `ResourceKind`；事件
kind/label 与资源路径集中配置，并在编译期拒绝重复资源。公开类和命令签名保持不变。

## 增量验证

- `tdx-special-situations-tests` 与 `tdx-tool` 增量构建通过；
- 专项测试覆盖吸收合并、B 转 H、市值预警、重大重组和三类新三板事件并通过；
- 本地 JSN 代表样本返回 4,036 条匹配、10 个来源、3 条投影记录，行情关闭时错误为 0；
- `realtime-corporate` 定向契约通过；
- schema 保持 `tdx-market-special-situations-native-v1`，未运行完整 CTest。

## 后续候选

当前最大生产实现为 785 行的 `native/src/bond_reference.cpp`。
