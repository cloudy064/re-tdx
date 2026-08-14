# 原生 C++ 公式语义分析与渲染指令支持拆分

日期：2026-08-10

## 拆分结果

`formula_engine.cpp` 从 3,886 行降至 2,813 行。新增：

- `formula_analysis.cpp`（1,034 行）：源码语义审计、依赖与上下文绑定判定、
  公式库覆盖统计、能力清单、显式上下文模板和 K 线时间戳提取；
- `formula_render_support.cpp`（76 行）：TCalc 命名色、`COLOR/RGBX` COLORREF
  解码以及合法展示指令识别；
- `formula_render_support_internal.hpp`：分析器与绘图 IR 共享的最小内部接口；
- `TDX_FORMULA_SOURCES` 显式登记两个新翻译单元。

分析模块只读取 AST、静态注册集合和 JSON 文档，不持有行情执行环境。运行时求值、
字符串环境和绘图事件生成仍留在解释器执行侧，因此没有为了拆分而公开可变执行状态。

## 行为保持

- `analysis_schema_version`、能力清单和 390 项静态注册边界未变；
- 自动上下文、显式 L2/券商上下文、未来函数和展示退化判定保持原逻辑；
- `COLOR*`、`RGBX*`、`LINETHICK*` 以及展示指令白名单由分析器和渲染器使用
  同一实现，避免两套表发生漂移；
- 公开 C++ 头文件、CLI、HTTP schema 和公式执行结果未变化。

## 增量验证

- `tdx-formula-engine-tests` 通过；
- `tdx-tool` 增量链接通过；
- 公开 `sz:000001` 40 根日线执行带 `COLORRED/LINETHICK2` 和 `DRAWICON`
  的公式成功：语法、执行与展示语义均为真，生成 2 个绘图图元、4 个图标事件；
- 输入：`output/probes/formula-analysis-render.tdx`；
- 证据：`output/native-formula-analysis-render-v30.json`；
- 未运行完整 CTest、API 合约或前端检查，因为没有修改传输、缓存、公共 schema
  或网页代码。

## 后续边界

解释器主文件现在约 2,800 行。后续可以继续把 K 线规范化与执行环境初始化迁到
运行时模块，再通过一个渲染服务接口抽取约千行绘图 IR；语义分析已经不再阻碍这两
个运行期模块独立演进。
