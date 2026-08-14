# 公式标注字体选择与 GDI 文本路径

## 目标

继续收紧 `DRAWTEXT/DRAWTEXT_FIX/DRAWNUMBER/DRAWNUMBER_FIX/`
`DRAWNUMBER_DIF` 的显示边界：确认原客户端选用哪一项字体、配置从哪里来、
背景模式、文字测量和最终 Win32 输出 API，并让纯 C++ API 与 Svelte 工作台消费
当前安装的真实显示配置。

本轮不宣称浏览器字形栅格化与 Windows GDI 逐像素相同。

## TdxW 原生证据

`TdxW.exe!sub_977560` 的绘图类型分派为：

| 函数 | 原生类型 | renderer | 文本 API |
| --- | ---: | --- | --- |
| `DRAWTEXT` | 4 | `sub_961290` | `TextOutA` |
| `DRAWNUMBER` | 6 | `sub_9593C0` | `TextOutA` |
| `DRAWTEXT_FIX` | 7 | `sub_958F90` | `TextOutA` |
| `DRAWNUMBER_FIX` | 8 | `sub_959620` | `TextOutA` |
| `DRAWNUMBER_DIF` | 23 | `sub_9626E0` | `DrawTextA` |

五个 renderer 都通过 `sub_68F020` 从 TdxW 字体表选择字体。前四个固定选择
index 1；`DRAWNUMBER_DIF` 先选择 index 1，样式参数为 2 时再选择 index 13。
所有路径都显式调用 `CDC::SetBkMode(dc, 1)`，即 `TRANSPARENT`。

普通文字与数字经 `sub_692690/sub_692770` 落到 `TextOutA`；
`DRAWNUMBER_DIF` 经 `sub_692850` 落到 `DrawTextA`，公共 flags 为 `0x824`，
数字和字母分支分别为 `0x826`、`0x825`。宽高测量统一使用
`sub_6921E0 -> GetTextExtentPoint32A`，因此编码边界是 Win32 ANSI，而不是
浏览器原生 UTF-8 字体测量。

字体表 index 1 由 `sub_690350` 构造。配置加载器 `sub_8BB5D0` 从
`T0002/user.ini` 的 `[Other]` 读取：

- `NewFontStyle`：切换新字体预设表；
- `FONTNAME2/FONTSIZE2/FontWeigth2`：旧字体表 index 1 对应的用户序号 2；
- `ElderStyle`：非零时正高度加 2、负高度减 2。

恢复的旧表缺省值是 `Arial/+15/400`；`NewFontStyle=1` 时 index 1 是
`微软雅黑/-12/400`。`sub_68FB20` 最终调用 `CreateFontA`。当前安装的
`C:\new_tdx\T0002\user.ini` 为 `NewFontStyle=0`、`ElderStyle=0`、
`FONTNAME2=Arial`、`FONTSIZE2=15`、`FontWeigth2=400`，所以实际逻辑字体为：

- face `Arial`；
- height `+15`，语义是 GDI cell height；
- weight `400`；
- `DEFAULT_CHARSET=1`；
- `ANTIALIASED_QUALITY=4`；
- italic/underline/strikeout 均为 false。

相关机器证据：

- `output/probes/ida-tdxw-formula-font-index1-data.json`
- `output/probes/ida-tdxw-font-default-tables.json`
- `output/probes/ida-tdxw-font-mode-functions.json`
- `output/probes/ida-tdxw-font-config-load.json`
- `output/probes/ida-tdxw-font-factory-data.json`
- `output/probes/ida-tdxw-font-factory-callers.json`
- `output/probes/ida-tdxw-dc-state-helpers.json`
- `output/probes/ida-tdxw-formula-text-helpers.json`

通用离线辅助脚本 `doc/90-scripts/ida_data_inspect.py` 可按地址或 IDA 名称导出
原始字节、DWORD、C 字符串和引用函数；它只读 IDB，不推断配置加载后的运行值。

## 纯 C++ 与网页实现

新增 `formula_render_environment_document()`。CLI 与 GET/POST
`/api/v1/formulas/evaluate` 都返回 `tdx-formula-render-environment-v1`，只从
`user.ini` 读取上述显示键，不返回其他配置内容。缺文件或并发写入异常时回退到
恢复的默认表，不影响公式数值求值。

每个文字/数字图元同时发布原生类型、renderer、字体 selector/index、用户配置
序号、透明背景、测量 API、输出 API 与编码。`DRAWNUMBER_DIF` 另发布 index 13
条件切换和三个 `DrawTextA` flags。

Svelte 公式图表按响应中的 face、逻辑高度和 weight 生成标注样式；当前安装因此
使用近似 `Arial 15px/400`。旧的统一 11px monospace、连续数字粗体和通用
text-shadow 已移除。数据与执行链仍完全是 C++，Python 只用于离线 IDA 取证。

真实验收时发现 GET 内置公式路径最初漏挂顶层 `render_environment`，而 POST/CLI
已经返回。正式接口检查因此暴露了单元测试未覆盖的适配缺口；GET 已补齐，
`formula-cpbs-text-live/formula-fscage-number-live/`
`formula-tjcjl-stickline-live` 也加强为必须存在顶层字体环境，避免以后回退而不报警。

## 验收

- CTest：101/101；
- Svelte `npm run check`：0 errors、0 warnings；
- Svelte 生产构建成功：`assets/index-Di_sC_Gj.js`、
  `assets/index-Dz936ObQ.css`；
- quick：8/8，报告 `output/api-contracts-formula-font-quick.json`；
- 文字/数字专项：3/3，报告
  `output/api-contracts-formula-font-selected.json`；
- full：196/196、197 次请求，报告
  `output/api-contracts-formula-font-full.json`。

发行 EXE SHA-256 为
`0F1471150951EEA9E389342025C3CC6384A529A9AE87B586563019BA4CE5DF71`。
最终正式服务 PID `21076`，只监听 `127.0.0.1:8765`；健康检查为 379 条公式、
100 个图标单元、JSN 可用且 `python_runtime=false`。

## 明确边界与下一步

`pixel_font_equivalent=false` 保持不变。浏览器使用 CSS 字体，Windows 原客户端
使用 `CreateFontA/GetTextExtentPoint32A/TextOutA/DrawTextA`；字体 fallback、hinting、
抗锯齿、字符集映射和整数像素取整都可能不同。因此本轮闭合的是“选择了什么字体、
怎样测量与输出”的语义，不是 GDI bitmap 逐像素复刻。

后续已把 `DRAWCFRAME` 的原生框线、leader、填充 Alpha、适用函数和 HIGH/LOW
坐标规则单独恢复，不再使用通用网页 panel，见
[DRAWCFRAME 原生记录](2026-08-09-native-formula-drawcframe.md)。
