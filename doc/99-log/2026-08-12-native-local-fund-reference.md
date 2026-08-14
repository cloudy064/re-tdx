# 本地基金份额、参考值、净值与 ETF/LOF 标的闭环

## 目标

在不依赖网络、Python 或既有云端表格的前提下，恢复客户端
`T0002/hq_cache/specjjdata.txt`、`specetfdata.txt` 与 `speclofdata.txt` 的
原生用途，并按手工
重构后的 `funds / registry / server` 边界接入统一 C++ 工具。

## 原生证据

`TdxW.exe.i64` 的下载完成包装分别为 `sub_594250`、`sub_594290` 和
`sub_594270`，实际解析器为：

- `sub_4F4A50 @ 0x4F4A50`：逐行读取固定 8 列 `specetfdata.txt`，形成 48 字节
  ETF 映射记录。它保留证券市场/代码、跟踪标的市场/代码、内部目录 ID 和两个
  日期，并把 `IXIC/NDX/NBI` 改写为 `A_*`；上海市场 `000001` 改写为
  `999999`。两个日期相对于宿主当前日期产生状态 1/2/3。
- `sub_4D6ED0 @ 0x4D6ED0`：逐行读取固定 7 列 `specjjdata.txt`。第 5 列同时
  写入证券记录 `+86/+132`，即万份单位的流通/总份额；第 6 列写入 `+244`
  的单位交易参考值；第 7 列原解析器不消费。
- `sub_4F4ED0 @ 0x4F4ED0`：逐行读取固定 6 列 `speclofdata.txt`，形成 40 字节
  LOF 映射记录。它保存交易市场/代码、标的市场/代码和内部目录 ID，复用 ETF
  的四种指数别名规则，但不包含日期窗口。

字段名没有依靠形状猜测。化工 ETF 博时 `SZ158006` 的第 7 列 `1.0359` 与
现有 ETF 公开资料 `DWJZ=1.0359` 精确一致，确认它是已发布单位净值；第 6 列
`1.0477` 与同日公开收盘 `1.046` 不同，因此只命名为
`unit_reference_value`，不伪装成收盘价或已发布净值。

## 实现

新增独立模块：

- `native/include/tdx/fund_reference.hpp`：查询模型与公共入口；
- `native/src/funds/fund_reference.cpp`：严格 CSV、日期、市场、代码、数值和
  文件大小/行数校验，类型化三个资源；
- `native/src/funds/fund_reference_command.cpp`：薄 CLI 编排；
- `native/tests/fund_reference_tests.cpp`：份额单位、别名、状态、筛选及损坏输入测试。

公开入口为：

```text
tdx-tool market fund-reference --view all|snapshot|etf-mapping|lof-mapping
GET /api/v1/market/fund-reference
```

支持 `market/code/q/as_of_date/limit`。API 固定使用服务启动时的 TDX 根目录，
不能通过请求参数读取任意本地路径。响应显式报告 `source_mode=local` 和
`network_requests=0`。

## 真实安装结果

以 `20260812` 为观察日：

- `specjjdata.txt`：90,190 字节、2,113 条基金快照；
- `specetfdata.txt`：84,317 字节、1,669 条 ETF 映射；
- `speclofdata.txt`：13,320 字节、431 条 LOF 映射；
- 合计 4,213 条，其中 1,987 条带跟踪标的；
- 原生状态 2（日期窗口内）20 条，状态 3（窗口后）1,649 条。

`SH510210` 的跟踪标的按原生规则由 `SH:000001` 归一为 `SH:999999`，并由现有
证券目录补齐名称“上证指数ETF富国”。`SZ160125 南方香港LOF` 则映射到市场
27 的 `HSI`；现有两条云端 LOF 记录均没有标的字段，证明本地表补充的是新增关系。

## 验证

- 增量构建：`tdx-fund-reference-tests`、`tdx-tool`；
- 聚焦 CTest：1/1 通过，约 0.42 秒；
- 真实 CLI：全量、`SZ158006` 快照、`SH510210` ETF 映射及
  `SZ160125/SZ160105` LOF 映射通过；
- 临时服务：既有基金参考合约及 LOF 直接查询、全量汇总、别名、非法视图通过；
- 所有临时端口已关闭，8765 正式服务始终保持原监听；
- 新模块不引用 Python。

本次是新增隔离解析器和端点，没有修改既有 schema、网络传输或缓存，因此按仓库
验证预算未运行全量 CTest/API 套件。机器可读证据见
`output/local-fund-reference-fidelity-20260812.json`。
