# TCalc 原生 DRAWICON 图集与 LINESTICK 复合语义

## 结论

公式工作台不再用箭头或“圆圈 + 数字”近似 `DRAWICON`。纯 C++ 工具现可离线解析
`TCalc.dll` 的 PE 资源，提取 `RT_BITMAP/2060`，生成浏览器可直接使用的透明 PNG、
原始 BMP 和可审计清单；公式 IR 则把图标类型、精灵位置、价格锚点和
`DRAWABOVE` 对齐方向交给 Svelte。整个过程不加载 DLL，也不依赖 Python。

`LINESTICK` 同时完成语义补齐：它不是线型别名，而是“零轴柱状线 + 指标线”的
复合输出。当前系统公式仅 `SLZT/神龙在天` 使用该指令，网页按“先柱后线”绘制。

## 证据

- 通达信官方函数说明限定 `DRAWICON(COND,PRICE,TYPE)` 的图标编号为 `1..51`，
  `DRAWABOVE` 表示图形向下对齐；同一说明把 `LINESTICK` 定义为同时绘制柱状线
  和指标线：<https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html>。
- 当前 `TCalc.dll` SHA-256 为
  `13facaa52dac552c5be1f63331781219de9bf798c443af8f5e4afc193a02e7f5`，匹配
  `tdx-2025-11-14`。其数值资源 `RT_BITMAP/2060` 是 `1800×18`、24 bpp、
  未压缩 DIB；可严格切成 100 个从左到右、1 起始的 `18×18` 单元。
- 系统公式实际引用类型 `1/2/47/48/49/50/51`。资源中的 47—51 分别可见绿色
  G 盾牌、红色 R/水滴、“榜”、D、K；类型 49 和 51 的横向偏移分别为
  864 和 900 像素。
- 官方可调用范围与资源容量是两个边界：API 声明 `official_type_min=1`、
  `official_type_max=51`，但保留 `sprite_cells=100` 供审计，不擅自把 52—100
  宣布为可用公式参数。
- 379 条系统公式中只有 `SLZT` 使用 `LINESTICK`：
  `青龙:SAR(125,1,7),LINESTICK;`。真实 240 根日线产生 116 个有限点，IR 明确
  给出 `zero-baseline-stick`、`indicator-line`、基线 0 和
  `sticks-then-line`。

## 纯 C++ 接口

```powershell
tdx-tool formulas icons --root C:\new_tdx `
  --output output\tcalc-drawicon.png `
  --bmp-output output\tcalc-drawicon.bmp `
  --manifest output\tcalc-drawicon.json
```

固定服务接口为：

- `GET /api/v1/formulas/icons`：资源、尺寸、哈希、编号边界和端点清单；
- `GET /api/v1/formulas/drawicon-strip.png`：以白色为透明色键的 RGBA PNG；
- `GET /api/v1/formulas/drawicon-strip.bmp`：带文件头的原始 24 bpp BMP。

服务启动时只解析和缓存一次图集。`DRAWICON` 事件携带 `icon_price`、
`icon_type`、可用性、`icon_sprite_x` 和上下对齐；Svelte 使用 TradingView 的
时间/价格坐标转换放置原生 18×18 单元，缩放、平移、尺寸或主题变化时重算位置。
`LINESTICK` 则先建立零基线 HistogramSeries，再叠加原指标 LineSeries。

## 验收

- 原生 CTest：101/101；
- Svelte：`npm run check` 为 0 错误、0 警告，生产构建成功；
- DRAWICON/LINESTICK 专项固定契约：5/5；
- 固定 API 全量：193/193，实际发出 194 次网络请求；
- 首次全量中 `formula-cross-market-futures-live` 曾遇到一次 WinHTTP 12002，
  独立重试 1/1 通过，随后 60 秒请求超时配置下的完整复跑为 193/193；这被保留
  为上游瞬时超时证据，没有隐藏成解释器成功；
- 原生导出样本：`output/probes/tcalc-drawicon-native.png` 34,990 字节，MD5
  `aab4ffb60647d36abde45b6fb33b54aa`；BMP 97,254 字节，MD5
  `64c70e7550e8bedde8846378c814a4a0`；
- 全量报告：`output/api-contracts-drawicon-linestick-final-full3.json`。
- 全量验收后重启释放一次性缓存；正式服务 PID `30080`，仅监听
  `127.0.0.1:8765`，健康检查为 379 条公式、100 个图集单元且无 Python 运行时。

## 明确保留的边界

PNG 使用当前位图的白色背景作为透明色键；若后续 TCalc 指纹或 DIB 结构变化，
提取器会拒绝而不是套用旧坐标。TradingView 负责坐标系统，字体度量、遮挡顺序、
抗锯齿和 GDI 像素取整仍不声明等价，因此
`pixel_renderer_equivalent=false` 保持不变。本轮不涉及 L2、授权会话或私有
`SIGNALS_QS` 数据。

## 下一项选择依据

对 379 条当前源码再扫描后，系统公式对 `CROSSDOT/CIRCLEDOT/POINTDOT/STICK/`
`DRAWBMP/DRAWRECTREL/DRAWRECTABS/DRAWTEXTABS` 的实际调用数均为 0，不应把它们
当作下一批高收益实现。`DOTLINE` 有 3 条、`LINETHICK` 有 21 条公式使用，但两者
已进入现有线型协议。真正仍可能造成肉眼差异的是 20 条混合展示公式的跨图元覆盖
顺序，例如“上榜标注”同时使用 `DRAWICON/DRAWTEXT_FIX/STICKLINE/STRCAT`，
`WAVEKX` 同时使用 `DRAWKLINE/DRAWLINE/PARTLINE/RGB`。下一轮优先审计这些真实
混合样本的逐语句覆盖关系，而不是扩展没有系统样本的函数名。
