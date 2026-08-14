# 公式展示真实性 schema 7 与返回占位澄清

## 目标

复核 379 条内置公式的展示函数和样式指令，确认旧报告中的 56 条
`semantic_surrogate` 究竟是 renderer 缺口，还是只存在于解释器返回值层；同时
修正同一语句出现多个 series renderer 指令时的选择顺序。

## 实现

- `formulas analyze` 升级为 `analysis_schema_version=7`，新增
  `render_semantics_materialized`、`semantic_surrogate_scope`、
  `presentation_return_surrogates`、`unsupported_presentation_directives` 和
  `unsupported_presentation_functions`。
- 展示调用与普通辅助函数分开审计；已支持的颜色、线宽、NODRAW、DRAWABOVE、
  DRAWCFRAME 和八类 series renderer 都纳入明确白名单，未知指令不再静默通过。
- `VOLSTICK/COLORSTICK/STICK/LINESTICK/CIRCLEDOT/CROSSDOT/POINTDOT/DOTLINE`
  按源码顺序选择 renderer，最后一个指令生效。
- 固定 `/api/v1/formulas/coverage` 契约验证 schema、总量、数值安全、展示真实性、
  IR 物化和未知指令；内联样式契约验证 renderer 指令正反顺序。

## 全库结果

| 指标 | 结果 |
|---|---:|
| 公式总数 / 源码 / 语法 | 379 / 379 / 379 |
| 数值输出安全 / 降级 | 379 / 0 |
| 含绘图公式 / IR 可用 / 展示语义已物化 | 90 / 90 / 90 |
| 展示语义可信 | 379 |
| 未知展示指令 | 0 |
| 绘图返回兼容占位 | 56 |

56 条均为 `presentation-return-only`：绘图函数在表达式求值层需要一个兼容返回值，
但对应展示行为已经进入 IR。它们没有传播到数值输出，因此不是 56 个网页渲染缺口。
`pixel_renderer_equivalent=false` 仍然保留：这里证明语义和几何协议已物化，并不声称
浏览器字体栅格、抗锯齿和整数取整与原生 GDI 像素完全一致。

## 验证与部署

- CTest：101/101。
- Svelte 检查：0 errors / 0 warnings；生产构建成功。
- 正式 full API 契约：202/202，203 次网络请求。
- 报告：`output/probes/api-contracts-full-current.json`。
- 服务：`http://127.0.0.1:8765`，PID 33048，仅监听环回地址。
- EXE SHA-256：`08ECF95B9A867F7FCA43AFF7DCBA4D853F4F414AC43297C4BC27A4CAFE82F3B2`。
- 运行时：`native_cpp=true`、`python_runtime=false`、公式 379、图标单元 100。
