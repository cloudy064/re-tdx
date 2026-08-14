# 股东增减持与股权质押纯 C++ 迁移

## 目标

把客户端 `ZCJC/ZCJCTJ/GQZY/GQZYTJ` 页面从已验证的 JSN 主从链迁入统一
`tdx-tool`。运行时只使用 C++，同时提供全市场视图、单票详情、质押机构下钻、
固定 API、独立 Svelte 页面和个股侧栏。

## 上游资源与关联键

一次 7709 会话更新以下 13 张主表：

| 资源族 | 主表 | 用途 |
|---|---|---|
| 增减持 | `func_zcjc101/107` | 最新实际增持、减持 |
| 增减持计划 | `func_zcjc105/111` | 拟增持、拟减持 |
| 增减持统计 | `func_gdzjc102/105` | 37 个月、11 个年度的金额与公司数 |
| 股权质押 | `func_gqzy101/102/103/109` | 最新质押、预警、平仓、解押 |
| 质押统计 | `func_gqzy105` | 13 个月的公司、笔数、股份和占比 |
| 质押机构 | `func_gqzy106/108` | 信托、券商汇总及动态 ID |

详情严格使用客户端 CFG 与页面脚本中的动态键：

- `zcjc/<市场号><六位代码>.jsn`：近一年股东实际增减持；
- `gqzy/<市场号><六位代码>.jsn`：单票完整质押历史与风险线；
- `xtzy/<机构ID>.jsn`：信托或券商的质押证券明细。

北交所主表原始市场值 `44` 在模型内规范化为 `BJ/2`；缺失或不支持市场号的
异常行不会伪造证券关联。

## 真实数据验证

2026-08-04 重新直连主站得到：

- 有效实际增减持 1,496 条，其中原始最新增持表 477 行里有 1 行缺失市场号；
- 拟增减持 2,173 条；
- 最新质押、预警、平仓和解押合计 1,705 条；
- 增减持月度 37 行、年度 11 行，质押月度 13 行；
- 信托 71 家、券商 104 家。

主从链样本：

- `SZ000009`：主表实际变动 1 条，动态增减持 6 条；主表质押风险 2 条，
  动态完整质押历史 4 条；
- 券商动态 ID `5001`：机构主表 104 行中命中目标，`xtzy/5001.jsn` 返回
  3 只质押证券；
- 三组真实详情均无动态下载错误。

上游增减持、质押股份使用“万股”，月度/年度增减持金额使用“亿元”。接口
统一输出基础单位“股”和“元”，并在换算入口取整，避免 `0.14 * 1e8` 的
二进制浮点尾差。质押机构市值本身已是元，不重复缩放。

## 原生实现

新增 `market ownership`：

```powershell
tdx-tool market ownership --root C:\new_tdx --view changes --market sz `
  --code 000009 --details --output output\ownership-000009.json

tdx-tool market ownership --root C:\new_tdx --view institutions `
  --category broker --institution-id 5001 --details
```

首轮支持五个视图：`changes`、`plans`、`pledges`、`statistics`、`institutions`；
随后增加 `insiders` 与 `commitments`，当前共七个视图。
核心主表缓存 5 分钟，单票/机构详情及不存在结果缓存 15 分钟。服务端只暴露
固定 `/api/v1/market/ownership`，不提供任意上游 URL 或命令执行入口。

网页新增“股权变动”独立页面；个股“更多”抽屉新增同名页签，一次关联实际
增减持历史、拟增减持计划和完整质押历史。样式复用用户重设计后的主题变量，
深色和亮色均可用。

## 验收

- `svelte-check`：0 errors / 0 warnings；
- Vite production build：成功；
- `tdx-native-tests`、`tdx-repurchases-tests`、`tdx-ownership-tests`：3/3 通过；
- `serve --self-test`：`feature_count=32`，Web 资源、板块、公式与 JSN 索引通过。

## 后续

董监高持股变动与承诺不减持已在随后完成，并入同一命令与 API，见
[董监高变动与承诺不减持扩展](2026-08-04-native-insider-commitments.md)。
