# 原生 C++ 公式当前财务上下文拆分

日期：2026-08-10

## 拆分结果

`formula_context.cpp` 在动态行情拆分后的 1,149 行基础上继续降至 1,077 行。新增：

- `formula_context_current_finance.cpp`（198 行）：负责当前财务文档抓取、字段映射、
  派生财务值、流通/总股本和省份编号；
- `formula_context_current_finance_internal.hpp`（40 行）：声明
  `CurrentFinanceRequirements`、`CurrentFinanceContext` 和 Builder 入口；
- `kCurrentFinanceFields`：用 26 项 `constexpr` 路径表替代主函数中成排的
  `bind(selector, path)`；
- `CurrentFinanceContextBuilder`：统一管理财务文档所有权，避免把指向临时 JSON 的
  指针暴露给动态市盈率等后续消费者。

需求对象集中表达当前 `FINANCE`、`DYNAINFO(39)`、`CAPITAL`、历史股本、
`DYBLOCK`、`HSL` 和 `TOTALCAPITAL` 是否需要触发财务抓取。主组合器只消费
财务值、稳定的记录指针、当前流通股本和省份编号。

## 行为保持

- 26 个直接 selector 的 JSON 路径未改变；
- `FINANCE(3)` 证券类型、`FINANCE(42)` 上市天数和 `FINANCE(21)` 派生差值
  算法未改变；
- `CAPITAL/TOTALCAPITAL` 仍按股数除以 100 转为“手”；
- `DYBLOCK` 仍复用证券关系模块的地区文字绑定；
- `DYNAINFO(39)` 获得的 `finance_record` 由返回对象持有，生命周期覆盖后续
  动态行情装配；
- 无财务依赖时 Builder 不发起网络请求，返回空值对象和空记录。

## 增量验证

- `tdx-formula-engine-tests` 通过；
- `tdx-tool` 增量编译、链接通过；
- 使用动态行情拆分后的证据作为基线，重新执行公开 `sz:000001` 20 根日线、
  17 个输出；
- 20×17 共 340 个输出值逐项精确相等，变化数为 0；
- 动态市盈率仍为 `3.771480083465576`，`CAPITAL` 绑定和 29 个财务绑定均存在；
- 基线：`output/native-formula-dynamic-quote-context-v36.json`；
- 新证据：`output/native-formula-current-finance-context-v37.json`；
- 未运行完整 CTest 或 API 合约，因为公共 schema、共享传输、缓存协议和服务路由
  均未修改。

## 后续边界

`formula_context.cpp` 已接近纯组合根。剩余较大的内联职责主要是证券状态、主业/评分/
自定义板块等本地宿主字段，以及外部信号/用户序列编排。它们应按宿主数据源分组，
不再继续向财务或动态行情 Builder 填充分支。
