# 公式渲染事件派发模块化

日期：2026-08-11  
原文件：`native/src/formula_render_events.cpp` (589 行)  
最大单函数：572 行 (`render_primitive_events`，占比 97%)  
根文件现：55 行  
实现单元：7 个（含根）  
最大实现单元：149 行  

## 动机

原文件 589 行中的 572 行集中在单一函数 `render_primitive_events` 内，该函数为 9 路条件分派链（PLOYLINE、DRAWLINE、DRAWSL、DRAWBMP、DRAWGBK、DRAWRECTREL、DRAWNUMBER_DIF、DRAWGBK_DIV、fallback），外加三个标注辅助函数 (`apply_stick_event_fields`、`apply_icon_event_fields`、`apply_annotation_event_fields`)。每条分支内部有 20-150 行的逐根遍历逻辑。单函数集中度 97% 高于文件总行数排序候选，因此优先拆解。

## 手段

### 1. 编译期派发表

将 9 路 `if`/`else if` 条件链改为一张编译期 `PrimitiveEventRenderer` 函数指针表：

```cpp
struct PrimitiveEventEntry {
    std::string_view function;
    PrimitiveEventRenderer render;
};

inline constexpr std::array<PrimitiveEventEntry, 8> primitive_event_renderers{{
    {"PLOYLINE", render_polyline_events},
    {"DRAWLINE", render_draw_line_events},
    {"DRAWSL", render_slope_line_events},
    {"DRAWBMP", render_bitmap_events},
    {"DRAWGBK", render_pane_background_events},
    {"DRAWRECTREL", render_pane_rectangle_events},
    {"DRAWNUMBER_DIF", render_sequence_events},
    {"DRAWGBK_DIV", render_region_background_events},
}};

constexpr bool unique_primitive_functions() {
    for (std::size_t i = 0; i < primitive_event_renderers.size(); ++i)
        for (std::size_t j = i + 1; j < primitive_event_renderers.size(); ++j)
            if (primitive_event_renderers[i].function ==
                primitive_event_renderers[j].function)
                return false;
    return true;
}

static_assert(unique_primitive_functions(),
              "primitive event renderers must dispatch on distinct functions");
```

### 2. 责任分层

**根分派器** (`formula_render_events.cpp`, 55 行)：遍历表、委托匹配项、回退到通用。

**六个实现单元**（函数签名 `void (*)(const PrimitiveRenderContext&, Json&))`）：
- `formula_render_events_segments.cpp` (148 行)：PLOYLINE / DRAWLINE / DRAWSL；保留原内联 `append(bool right)` lambda
- `formula_render_events_surfaces.cpp` (114 行)：DRAWBMP / DRAWGBK / DRAWRECTREL；嵌套守卫扁平为 early `return`
- `formula_render_events_sequence.cpp` (89 行)：DRAWNUMBER_DIF 状态机（五变量：`active`、`active_source`、`active_end`、`active_start`、`active_count`）
- `formula_render_events_regions.cpp` (146 行)：DRAWGBK_DIV；两局部 lambda 提升为匿名命名空间 `native_true` / `native_integer`，保留专用 epsilon `0.0001` 和偏置 `0.503000020980835` 以及说明注释
- `formula_render_events_general.cpp` (95 行)：`render_general_events` 通用回退；门控、PARTLINE 和 DRAWBAND 内联；调用三标注辅助函数，保持原字段插入顺序
- `formula_render_events_annotations.cpp` (149 行)：`apply_stick_event_fields`、`apply_icon_event_fields`、`apply_annotation_event_fields`；最后一个函数重测 `price_text` / `fixed_text` 并 early `return`，因为循环无条件调用之

所有实现单元均位于 `namespace tdx::formula_render_detail`，保留原文件开头的四条 `using namespace` 声明。

### 3. 保持语义

- **JSON 字段插入顺序**即输出顺序，所有标注辅助调用顺序保持不变
- 嵌套 lambda 仅在原地可读性优于外提时保留，否则提升至匿名命名空间
- DRAWGBK_DIV 的专用数值容差（不同于全局 `1e-6`）和偏置保留，附带原注释
- `context.string_available` 布尔守卫分支完全保留，无合并优化

## 验证

### 编译验证

- g++ 15.1.0 `-fsyntax-only` 通过所有 7 个单元
- MSYS2 CMake + mingw32-make 成功链接 `libtdx_native.a`、构建 `tdx-tool.exe`
- 六个测试二进制通过：`tdx-formula-engine-tests`、`tdx-announcement-signals-tests`、`tdx-curated-data-tests`、`tdx-native-tests`、`tdx-block-trades-tests`、`tdx-recon-contract-tests`
- 修复五项累积缺陷（前三轮遗留）：
  1. `announcement_signals_internal.hpp` 位置（`src/` → `include/tdx/`）
  2. `extern constexpr` ill-formed → `inline constexpr`
  3. 幽灵 `#include "tdx/args.hpp"` ×3（`Args` 实际在 `common.hpp`）
  4. 缺少 `#include "tdx/jsn_data.hpp"` ×2
  5. `excerpt` 丢失默认参数 `limit = 240`

### 二进制等价验证

构建两个二进制：原 589 行单体（从会话记录恢复）与 7 单元拆分版。两个样例输入：

**样例 1** (rf.txt, 12 条语句)：
```
MA5:MA(CLOSE,5); MA10:MA(CLOSE,10);
PLOYLINE(CLOSE>MA5,CLOSE,'RGB(255,0,0)','RGB(0,255,0)',1,'dash');
DRAWLINE(CLOSE<MA10,HIGH,LOW,'RGB(0,0,255)',1);
DRAWSL(CLOSE>MA5,LOW,5,0,'RGB(128,0,128)',1);
STICKLINE(CLOSE>OPEN,OPEN,CLOSE,2,0),COLOR0000FF;
DRAWICON(CLOSE>MA5,LOW,1); DRAWTEXT(CLOSE>MA5,HIGH,'信号');
DRAWNUMBER_DIF(CLOSE>MA5,HIGH,CLOSE); DRAWGBK_DIV(CLOSE>MA5,RGB(255,255,0),0);
PARTLINE(CLOSE>MA5,CLOSE); DRAWBAND(MA5,RGB(255,0,0),MA10,RGB(0,0,255));
```
12 根日线 K 线输入 (rk.json, 2531 字节)。

**样例 2** (rf2.txt, 8 条语句)：覆盖 DRAWBMP、DRAWGBK、DRAWRECTREL 和 `_FIX` 标注路径。

SHA256 对比：
```
SAMPLE1 BASE:  6B6B989BFE11AE01DCE0975A140E8719AD453B8CE620652170EA20FA5BCC2546 (87,593 bytes)
SAMPLE1 SPLIT: 6B6B989BFE11AE01DCE0975A140E8719AD453B8CE620652170EA20FA5BCC2546 (87,593 bytes)
SAMPLE2 BASE:  6B24A7CC14301F265AF37EAE280B0164CCCB35DD62B93872E48CD887D6C04759 (50,232 bytes)
SAMPLE2 SPLIT: 6B24A7CC14301F265AF37EAE280B0164CCCB35DD62B93872E48CD887D6C04759 (50,232 bytes)
```

字节级别完全一致，浮点精度、字段顺序、逻辑分支无偏差。

### 覆盖验证

样例 1 + 样例 2 联合执行图元事件数：

| 图元函数 | 种类 | 事件数 |
|---|---|---:|
| MA | line | 0 |
| PLOYLINE | polyline | 10 |
| DRAWLINE | draw-line | 1 |
| DRAWSL | slope-line | 8 |
| STICKLINE | stick | 12 |
| DRAWICON | icon | 8 |
| DRAWTEXT | text | 3 |
| DRAWNUMBER_DIF | sequence-number | 12 |
| DRAWGBK_DIV | background | 8 |
| PARTLINE | part-line | 12 |
| DRAWBAND | band | 12 |
| DRAWBMP | bitmap | 8 |
| DRAWGBK | pane-background | 1 |
| DRAWRECTREL | pane-rectangle | 1 |
| DRAWNUMBER | number | 8 |
| DRAWTEXT_FIX | text | 8 |
| DRAWNUMBER_FIX | number | 8 |
| DRAWKLINE | candlestick | 12 |

表派发的 8 个渲染器 + 通用回退路径（PARTLINE、DRAWBAND、STICKLINE、DRAWICON、DRAWTEXT、数字标注）全部执行。

## 尺寸对比

| 维度 | 原 | 现 |
|---|---:|---:|
| 文件数 | 1 | 7 |
| 总行数 | 589 | 796 |
| 根文件 | 589 | 55 |
| 最大实现单元 | 589 | 149 |
| 最大单函数 | 572 | <150 |

净增 207 行（+35%），消除 97% 单函数集中，最大单函数从 572 降至 <150。

## 公开 API 完整性

- 唯一公开符号 `render_primitive_events(const PrimitiveRenderContext&)` 保持签名和语义不变
- 输出 JSON schema `tdx-formula-render-ir-v1` 字段顺序、浮点精度、事件结构不变
- `PrimitiveRenderContext` 及其九个成员字段保持原型
- CLI `formulas evaluate --source-file ... --input ... --market ... --period ...` 无变化

## 归档

原始 589 行文件归档至 `native/src/formula_render_events.cpp.removed`，从会话记录恢复保存。
