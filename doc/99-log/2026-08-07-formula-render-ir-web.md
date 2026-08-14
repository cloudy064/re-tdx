# 公式绘图 IR 的网页消费层

本轮先在当前二进制与当前数据上重跑全库审计：平安银行 800 根日线、完整公开
上下文和显式只读未来函数模式下，379/379 条全部报告，354 条通过、0 条解释器
错误；14 条仍依赖 L2，6 条仍依赖券商私有 `SIGNALS_QS`，另有 4 条市场不适用、
1 条周期不适用。因此没有用零值或近似数据伪造新的计算兼容项。

可继续推进的真实缺口是网页此前只显示 `tdx-formula-render-ir-v1` 的图元/事件数，
并未消费绘图事件。`FormulaChart.svelte` 现直接使用 C++ 解释器返回的有序 IR：

- 普通输出遵循颜色、线宽、点线和隐藏指令；`COLORSTICK/VOLSTICK` 保留柱图；
- `PARTLINE` 使用每根事件携带的 Windows `COLORREF` 动态变色；
- `STICKLINE/DRAWKLINE` 呈现为价格图元；
- `DRAWICON/DRAWTEXT/DRAWNUMBER/DRAWNUMBER_DIF` 呈现为价格标记；
- `DRAWTEXT_FIX` 使用已物化字符串和相对坐标显示固定文字；
- `DRAWBAND` 显示逐根上下边界。

页面明确显示“TDX IR 预览”。这不是通达信像素渲染器：`STICKLINE` 精确像素宽度、
`DRAWBAND` 动态区域填充及全部图标字形仍不声明等价，后端
`pixel_renderer_equivalent=false` 保持不变，扫描与回测也继续只读取数值输出。

验证包括 Svelte 检查 0 错误/0 警告和生产构建成功。正式服务加载
`assets/index-BuYFVqq1.js` 后，用 Edge DevTools 协议驱动真实页面计算：

| 公式 | 数据与 IR | 浏览器结果 |
| --- | --- | --- |
| `MACD` | 800 点、3 图元 | 柱图与两条线加载，0 错误 |
| `NXX` | 2 图元、800 个 `PARTLINE` 事件 | 动态颜色线加载，0 错误 |
| `CPBS` | 2 图元、43 个文字事件 | 价格文字标记加载，0 错误 |
| `HYDB` | 3 图元、801 个事件 | 800 根行业 K 线与 1 条固定行业文字加载，0 错误 |

浏览器截图保存在 `output/formula-render-hydb.png`；当前全库运行报告为
`output/formula-runtime-audit-20260807-current.json`，覆盖分析为
`output/formula-coverage-20260807-current.json`。
