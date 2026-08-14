# 战略主题层级与逐股逻辑纯 C++ 迁移

> 日期：2026-08-07  
> 范围：主题投资 24 张主表、`zttzty/<主题ID>.jsn`、统一命令/API、Svelte 页面。  
> 目标：移除正式流程对 `update_tdx_theme_logic.py` 和预生成 JSON 的依赖。

## 重新确认协议语义

覆盖审计中一组高分缺口都具有 `$S_ZQDM/$ZQDM/S_NUM/gname`，进一步与
`GN_GNZ.xml`、`hy_tree1_gnz.xml` 和 24 份页面 CFG 对照后，确认它们不是
24 个互不相关的板块页，而是“主题投资”的统一层级：

```text
24 个战略主题大类
  -> 617 条大类—内部主题关系
    -> 567 个去重主题
      -> 主表证券成员
      -> zttzty/<主题ID>.jsn 实时成员及逐股入选逻辑
```

主表 `$ZQDM` 是内部主题 ID，不是股票代码。详情行里的 `$SC/$ZQDM` 才是
证券市场和代码；`tzlj/xxsm` 是入选逻辑与完整说明。详情非空时，其成员集合
优先于主表，从而能反映主表发布后新增的证券。

## 原生实现

新增 `market strategic-themes`，五种视图均由 C++ 直接执行：

- `catalog`：完整大类和主题目录；
- `categories`：大类分页；
- `themes`：按大类或关键词筛选主题；
- `theme`：选中主题的成员、详情和逐股逻辑；
- `security`：从一只股票反查所有战略主题。

服务一次批量读取 24 张主表并建立进程内缓存，校验同一主题跨大类出现时名称、
成员集合和声明数量一致；9 条源数据重复成员保留审计计数，业务关系去重。
目录不会密集请求全部 567 个详情，只有 `view=theme` 才额外读取所选主题的
一个 `zttzty` 资源。所有来源均返回端点、尝试次数、陈旧状态和上游错误。

固定接口为：

```text
GET /api/v1/market/strategic-themes?view=catalog&limit=600
GET /api/v1/market/strategic-themes?view=theme&theme_id=657&limit=5000
GET /api/v1/market/strategic-themes?view=security&market=sz&code=000063
```

Svelte 工作台在“板块 → 战略主题”新增双栏页：左侧可按 24 个大类和关键词
筛选 567 个主题，右侧展示成员证券、详情来源和入选逻辑，并可跳转个股工作台。

## 在线结果

2026-08-07 正式服务实测：

| 项目 | 数量 |
| --- | ---: |
| 战略主题大类 | 24 |
| 大类—主题关系 | 617 |
| 去重主题 | 567 |
| 主表原始主题—证券成员 | 55,136 |
| 去重主题—证券关系 | 42,142 |
| 关联证券 | 5,382 |
| 主表重复成员 | 9 |
| 声明计数异常 | 0 |

主题 `657 / 5G概念` 的主表成员为 441 只，当前动态详情为 452 只，452 行均
带可审计原始字段；中兴通讯 `SZ000063` 可反查 36 个主题。

## 覆盖与回归

- 24 张主表和一个 `zttzty/*` 动态模板登记到类型化资源映射；
- JSN 覆盖从 284/616 提升到 309/616；
- 340 个下载文件全部匹配，296 个下载模板中通用独占只剩 23 个，解析错误 0；
- 新增目录与详情两条在线契约，正式服务 full 为 73/73；
- C++ 单元/协议测试 64/64；Svelte 检查 0 错误、0 警告；
- 发布服务注册 109 项功能，生产页面截图保存在
  `output/probes/strategic-themes-current/web.png`。

证据文件：

- `output/probes/strategic-themes-current/catalog.json`；
- `output/probes/strategic-themes-current/theme-657.json`；
- `output/probes/strategic-themes-current/security-000063.json`；
- `output/probes/api-contracts-strategic-themes-current.json`；
- `output/probes/api-contracts-official-current.json`；
- `output/probes/tdx-jsn-variants-current.json`。

## 下一批优先级

主题投资已经闭合，不再建设一个重复的“通用专题引擎”。剩余下载缺口中，
优先处理已有非空样本且能并入现有模型的资源：`func_jgcg108` 机构持股、
`func_jqgz103` 解禁信息；随后评估 `rdhs` 热点回溯与 `ydyl/ydyl1` 行业区域
机会主从链。空表和缺少可证明主从键的 CFG 候选继续保持通用入口，不猜字段。
