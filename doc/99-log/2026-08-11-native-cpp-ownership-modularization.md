# 原生 C++ 股东与质押链路模块化

日期：2026-08-11

## 结果

`ownership.cpp` 从 1,483 行缩减为 331 行，并按资源目录、归一化、抓取缓存、查询和
命令边界拆分：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `ownership_internal.hpp` | 81 | 27 个 JSN 资源常量和最小内部端口 |
| `ownership_support.cpp` | 262 | 字段解析、证券映射、筛选和来源摘要 |
| `ownership_changes.cpp` | 274 | 增减持、计划、股东人数、内部人和承诺归一化 |
| `ownership_pledges.cpp` | 279 | 质押、风险、解除、统计与机构明细归一化 |
| `ownership_fetch.cpp` | 286 | 主表/榜单/股东人数/动态详情的缓存抓取 |
| `ownership.cpp` | 331 | 视图选择、详情关联、统计核对和响应聚合 |
| `ownership_commands.cpp` | 67 | CLI 参数和输出适配 |

资源地址不再散落于抓取逻辑。公开 `tdx/ownership.hpp`、`OwnershipService`、CLI 参数和
`tdx-market-ownership-native-v1` schema 均未改变。

## 等价性与验证

- 六个实现区段逐字符核对通过；
- 27/27 个原资源地址进入内部目录且值完全一致；
- `tdx-ownership-tests`、`tdx-tool` 编译链接通过，专项测试通过；
- 真实执行增持比例榜成功，返回一条记录且 `availability=live`；响应保存在
  `output/ownership-modularization-ranking.json`，结构证据保存在
  `output/ownership-modularization.json`；
- 未运行完整 CTest 或完整 API 契约套件。

当前目录不是 Git 工作树，因此未执行 `git diff --check`。
