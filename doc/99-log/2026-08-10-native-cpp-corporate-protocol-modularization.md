# 原生 C++ 财务协议与复权链路模块化

日期：2026-08-10

## 结果

生产文件 `corporate.cpp` 从 1,490 行降至 380 行。财务、股本变化、特殊涨跌停和
K 线复权链路按职责拆为：

| 模块 | 行数 | 职责 |
|---|---:|---|
| `corporate_internal.hpp` | 69 | 7 个协议常量、证券标识和跨模块最小内部端口 |
| `corporate.cpp` | 380 | `0x0010`、`0x000F`、`0x0452` 二进制记录解码 |
| `corporate_support.cpp` | 188 | 证券代码归一化、请求编码、端点和 CLI 公共辅助 |
| `corporate_adjustment.cpp` | 294 | 公司行为事件归并、前后复权因子与复权摘要 |
| `corporate_adjustment_cache.cpp` | 312 | 复权输入的并发合并、TTL/LRU 缓存和单票复权编排 |
| `corporate_fetch.cpp` | 184 | 三类 7709 请求的重试、分页和结果聚合 |
| `corporate_commands.cpp` | 175 | finance/capital/limits/kline 四个命令适配器 |

公开 `tdx/corporate.hpp`、CLI 参数和 JSON schema 均未改变。协议号和固定记录尺寸不再
散落于编排代码；缓存并发状态只存在于缓存模块；网络抓取不再依赖命令行解析细节。
这一结构对应 Protocol Codec、Request/Endpoint Support、Adjustment Strategy、Input
Cache、Fetch Service 和 Command Adapter 六个边界。

## 等价性与验证

- 原文件 14 个实现区段逐字符核对通过，协议常量为 7/7；
- `tdx-corporate-tests` 与 `tdx-tool` 增量编译、链接通过；
- `tdx-corporate-tests` 通过，覆盖财务/股本/涨跌停解析和复权语义；
- 代表性真实执行 `market finance --security sh:600521` 成功，保持
  `tdx-market-finance-native-v1`，请求/返回为 1/1；结构证据保存在
  `output/corporate-modularization.json`，真实响应保存在
  `output/corporate-modularization-finance.json`；
- 当前目录不是 Git 工作树，因此未执行 `git diff --check`。

未运行完整 CTest。本轮没有修改共享 transport 或公开 schema，专项测试、机械等价检查
和一个真实 7709 样例已覆盖此次结构调整。验证完成后删除拆分用临时源快照。
