# 2026-08-09 无框价格文字与数字的原生坐标和格式化

## 目标

继续收敛 `DRAWTEXT`、`DRAWNUMBER` 的非 `DRAWCFRAME` 分支。此前 IR 已能表达
价格标注，但网页仍使用通用 above/below 标签、边缘翻转和碰撞避让，数字也使用
通用 12 位有效数字格式；这些行为并不是 TdxW 原生 renderer 的实现。

本批只使用 Python 进行离线 IDA 自动化。发行程序、公式解释器、API、契约和网页
运行时仍全部由 C++/Svelte 实现，不转发 Python。

## IDA 证据

新增或复核以下探针：

- `output/probes/ida-tdxw-price-annotation-disasm.json`：无框文字
  `sub_961290`、数字 `sub_9593C0` 及其调用链；
- `output/probes/ida-tdxw-text-mode-xrefs.json`：`TextOutA` 辅助模式；
- `output/probes/ida-tdxw-number-format-xrefs.json`：普通/特殊数字格式入口；
- `output/probes/ida-tdxw-number-precision-xrefs.json` 与
  `ida-tdxw-number-precision-fields.json`：精度字段读取和默认值；
- `output/probes/ida-tdxw-render-precision-setters.json`：renderer 精度写入来源。

对应 `.log` 保留了 IDA 9.0 批处理过程。当前 `user.ini` 的 Arial `+15` 字体配置
也与 renderer 构造函数的行高计算进行了交叉核对。

## 恢复出的精确语义

### 共同真值与缺失值

两类函数都只在 `abs(COND - 1) < 0.0001` 时绘制；条件为一般非零数并不成立。
价格为 `DRAWNULL`/缺失值时直接跳过，不生成绘图事件。

### `DRAWTEXT`

- `&` 拆行，最多消费 10 段；空段不画字，但仍按字符串 `"A"` 的测量高度推进；
- 横坐标就是 bar 的原生 x；基础纵坐标为 `priceY - 8`；
- `DRAWABOVE` 先上移 `line_count * (native_chart_row_height - 2)`，再减 8；
- 非空行逐行调用 `TextOutA`，下一行按实测文字高度推进；
- 不做边缘夹取、上下自动翻转或碰撞避让，后出现的源码事件直接覆盖先前事件。

当前 Arial `+15` 下原生图表行高为 16px。构造函数恢复出的计算式为：

```text
max(16,
    abs(font_height) + 1
    + (font_height > 15 ? 2 * abs(font_height) - 30 : 0)
    + (font_height < 0 ? 5 : 0))
```

例如 Elder 配置 `+17` 得到 22px。通用 `TextOutA` 包装层的辅助模式还会应用
`0 -> 0px`、`1 -> -1px`、`>1 -> -3px` 的 y 调整；当前价格标注路径为模式 0。

### `DRAWNUMBER`

- 横坐标为 `barX - 3`；普通纵坐标为 `priceY`；
- `DRAWABOVE` 时纵坐标为 `priceY + 2 - native_chart_row_height`；
- 同样不做夹取、翻转或碰撞避让。

普通 `IndexInfo` 格式模式会先把接近整数的值输出为 0 位小数；否则使用
`2` 或 `min(chart_precision + 1, 4)` 位小数。原生图表精度默认值为 2，普通股票
的小数数字因此通常显示 3 位。特殊格式模式则走独立固定 0—5 位小数分支，并在
格式化前加入 `1e-6`。API 可通过 K 线上下文元数据显式提供
`price_precision`、`index_info_format_mode` 和 `index_info_format_precision`；缺省值
与当前原生股票图表保持一致。

## 实现

- `native/src/formula_engine.cpp`：加入原生数字格式上下文、精确真值/缺失值筛选、
  十段拆行、无框坐标规则和事件字段；
- `native/src/formula_render_profile.cpp`：公开字体辅助模式、y 调整、当前原生图表
  行高、计算规则及证据来源；
- `web/src/charts/FormulaChart.svelte`：按原生坐标逐行渲染文字和数字，空行保留
  高度，删除该路径的通用翻转/夹取/避让；
- `web/src/types.ts`：补齐环境、图元和事件协议类型；
- `native/src/recon.cpp`：新增
  `formula-price-annotations-inline-post` 固定 POST 契约，full 从 198 增至 199；
- 两组 C++ 测试覆盖普通/特殊数字格式、空行、`DRAWABOVE`、精确条件、缺失价格和
  对错误翻转策略的拒绝。

这纠正了旧记录中“普通价格标签会按上下空间自动翻转”的近似。`DRAWCFRAME` 和
`DRAWNUMBER_DIF` 仍使用各自已经恢复的独立几何，不受本批无框规则替代。

## 验收与部署

- 原生完整构建成功；CTest 101/101；公式引擎和 recon 契约直接测试均通过；
- `npm run check` 为 0 errors、0 warnings，`npm run build` 成功；
- 正式服务新契约 1/1：
  `output/api-contracts-price-annotations-selected.json`；
- 相邻语义聚焦回归 5/5，覆盖无框、带框、`DRAWNUMBER_DIF`、CPBS 与 FSCAGE：
  `output/api-contracts-price-annotations-focused.json`；
- quick 8/8：`output/api-contracts-price-annotations-quick.json`。

正式 full 报告 `output/api-contracts-price-annotations-full.json` 为 186/199。失败的
13 项与公式绘图无交集：`jsn-discovery-live`、`jsn-candidates-live` 因本次服务未传
`--jsn-root` 返回明确 400；两个债券远端契约发生 WinHTTP 12002；其余是当天远端
样本数量、历史固定基线或“本地镜像优先”前置条件不再满足。保留该报告而不把
环境失败伪记成 199/199，也不在本批放宽既有数据契约。

服务已部署于 `127.0.0.1:8765`，PID `37028`，使用 `C:\new_tdx` 和纯 C++
运行时缓存。健康接口报告 `native_cpp=true`、`python_runtime=false`、379 条公式；
主页入口为 `assets/index-BPF4mnjk.js` 与 `assets/index-C0l2BFMl.css`。发行 EXE
SHA-256 为
`55A45DAE5535323C14F22F8B09027F06CD99FAAF315B92E888A8992589CBB353`。
