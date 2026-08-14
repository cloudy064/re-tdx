# TCalc 剩余通用绘图原语闭环

日期：2026-08-10

## 结论

本批按 `TCalc.dll` 参数处理、内置帮助和 `TdxW.exe` 最终绘制器三层证据，闭合
`DRAWSL`、`DRAWBMP`、`DRAWGBK`、`DRAWRECTREL`。它们已经进入纯 C++ 公式
解释器的稀疏绘图 IR，并由 Svelte/TradingView 工作台渲染；浏览器没有复制公式
计算逻辑，也没有引入 Python 运行时。

四个注册函数分别对应 opcode `1334/1100/1101/1102`，TdxW 绘制类型分别为
`20/9/10/11`。

## 恢复语义

### DRAWSL

`DRAWSL(COND,PRICE,SLOPE,LEN,DIRECT)` 在每个 `abs(COND)>=1e-5` 的有限值柱上
产生线段。`DIRECT=0/1/2` 分别向右、向左、双向，方向从求值区最后一柱读取；
普通斜率的单位为每根 bar 的价格变化。`SLOPE=10000` 是原生垂直线哨兵，
此时 `LEN` 改为像素高度：右分支向上、左分支向下。IR 保留 bar-price 端点，
垂直分支另发出像素 delta，避免把像素误当价格。

### DRAWBMP

`DRAWBMP(COND,PRICE,'name')` 只接受 `abs(COND-1)<0.0001`，在 bar x、price y
处以自然尺寸绘制 `T0002/signals/name.bmp`，锚点是图片左上角。资源名不接受路径。

### DRAWGBK

`DRAWGBK(COND,COLOR1,COLOR2,HORIZONTAL,'name',STRETCH)` 使用第一个原生真值柱
作为触发点，但颜色、方向和拉伸参数从求值区第一柱读取。任一颜色非零时绘制整窗格
渐变，`HORIZONTAL=0` 为纵向，否则为横向；两个颜色都为零时读取本地图片，
按 BMP 后 PNG 的顺序查找。`STRETCH!=0` 铺满窗格，否则在窗格原点按自然尺寸绘制。

### DRAWRECTREL

`DRAWRECTREL(LEFT,TOP,RIGHT,BOTTOM,COLOR)` 的四个坐标都是窗格宽高的千分比，
范围约定为 `0..999`。颜色为 Windows COLORREF；0 表示不填充，`NOFRAME`
移除边框。C++ 对损坏公式产生的超 int32 参数做有界拒绝，避免未定义转换。

## 本地图片 API

新增只读接口：

```text
GET /api/v1/formulas/signal-image?name=<basename>&format=auto|bmp|png
```

接口只在当前 TDX 根目录的 `T0002/signals` 中查找单层 basename，拒绝 `/`、
反斜杠、盘符、`.`、`..` 和非法格式；自动模式严格保持 BMP 优先，再尝试 PNG，
单文件上限为 64 MiB。当前 `C:/new_tdx/T0002/signals` 存在但为空，所以引用
实际图片前返回 404，这是资源缺失而不是公式解释失败。

## 覆盖与验证

能力清单从 233 增至 237 个支持函数，自动符号仍为 71。390 条 TCalc 静态注册名
的已识别并集由 288 增至 292，剩余由 102 降至 98；379/379 条内置公式继续全部
语法支持、数值安全，退化数值输出为 0。

本地测试覆盖双向/垂直斜线、BMP 锚点、渐变和图片背景、千分比矩形、NOFRAME
及超 int32 参数拒绝。全量 CTest 为 102/102，前端 `svelte-check` 为 0 错误、
0 警告，生产构建成功。8875 候选服务用平安银行 120 根真实日线执行混合源码，
返回 6 层 IR：DRAWSL 2 个事件、DRAWBMP 1 个、两个 DRAWGBK 各 1 个、
DRAWRECTREL 1 个及普通收盘线。候选专项 5/5、完整 API 契约 219/219 均通过；
图片端点另确认缺文件 404、目录穿越 400、非法格式 400。

## 证据与产物

- `output/ida-tcalc-remaining-draw-decompile-v1.json`：TCalc 四个参数处理函数；
- `output/ida-tcalc-remaining-draw-help-v1.json`：嵌入帮助原文恢复；
- `output/ida-tdxw-formula-render-types-remaining-v1.json`：类型 9/10/11/20 分派；
- `output/ida-tdxw-drawsl-renderer-v1.json`：DRAWSL 最终几何；
- `output/native-formula-coverage-remaining-draw-v13.json`；
- `output/native-formula-registry-next-audit-v13.json`；
- `output/api-contract-full-remaining-draw-v13-candidate.json`。

本批没有实现或绕过 L2、券商私有信号、交易账户状态和插件回调。下一组普通
非 L2 候选转为 `MAINZSHQ/TOTALHQINFO/BETAVALUE/SHAPE_SHORT/SHAPE_MID/SHAPE_LONG`，
仍需先闭合宿主回调字段和可用性门控。

## 正式部署

正式发行 EXE 为 22,188,452 字节，SHA-256
`E4D6B952FA9444EAF14860880D5F20D4216C29139601B3733EF9A36D66D983CA`。
服务 PID 32020，只监听 `127.0.0.1:8765`；健康响应为 `native_cpp=true`、
`python_runtime=false`。新 Svelte 入口、1,202,733 字节 JS 和 95,532 字节 CSS
已同步到发行目录并由正式首页引用。

正式端专项契约为 5/5；同一真实日线混合公式再次返回全部 6 层 IR，图片缺失和
目录穿越分别保持 404/400。候选完整契约报告 SHA-256 为
`8EA16CE5DD6604AB0877AA9097D674FF53F3E4DE9AEA61DB0FEE4044917E84ED`，正式专项
报告 SHA-256 为
`30C450477312F00AE6EDB6994643B6D4895F8CD73DAE69D73F1F9DE778535F0A`。

替换前版本保存在
`output/tdx-tool-remaining-draw-v13-predeploy-rollback-20260810.exe`，大小
22,157,009 字节，SHA-256
`3C77449704271FC327FC9C4A12817B9514CC6621597C5050BB412EAAF61BD88B`。
