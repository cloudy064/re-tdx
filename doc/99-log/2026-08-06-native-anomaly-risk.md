# 异常波动统计与停牌风险纯 C++ 固定化

## 业务边界

现有 `market abnormal-moves` 固定的是 PBRPC `500107`，返回十九类当天或指定
交易日异常证券。此次新增的普通 JSON 请求不是同一数据：

- `sc_ydjj.xml / HQServ.hq_nlp_abnormal_gold / 2044` 是个股相对分类指数的
  3/10/30 日偏离与滚动涨幅统计；
- `sc_gjyd.xml / HQServ.hq_nlp_tcihq / 200720` 是最近异动、停复牌日、计算
  区间、触发标准/价格、近十日次数和预警状态清单。

因此新增独立纯 C++ `market anomaly-risk` 和固定只读
`/api/v1/market/anomaly-risk`，视图为 `statistics`、`suspension-risk`，没有
把它们塞进 `500107` 的 19 类事件码。

## 完整分页与字段审计

`2044` 客户端默认 `sortTpye=18`，排序字段会随盘中行情变化。20 行分页期间
排序变化会导致同一证券跨页重复；真实复验曾出现 `SH603505` 重复。服务端同时
支持 `sortTpye=1, sort=1`，固定接口改用证券代码稳定排序，当前完整返回
863/863 行且无重复。分页上限仍固定为 100 页、每页 20 行，因为页长 200 会
被服务端以 `Invalid Page Size` 拒绝。

`2044` 的 N001—N028、N038/N039 可由 XML 可见列和真实值闭合：证券/分类指数
身份，3/10/30 日偏离、起始日、周期数、个股/指数涨幅，近十日正向异动次数，
双方最新价/涨幅、当日偏离，以及异常或严重异常参考价提示。提示文字中的参考
价格、百分比和“已触及/已公告”已结构化。N029—N037 在配置中没有业务标签；
接口将九个原值保存在 `upstream_auxiliary_unresolved`，明确禁止猜测字段名。

`200720` 的 N010 是比例，而网格 `plzf=N010*100`。接口同时输出
`reported_deviation_ratio` 和 `deviation_pct`；例如欣天科技 0.43 转为 43 个
百分点。N014 的 0/1/2/3 分别映射为无预警、可能因异动再次停牌、可能因异动
停牌、触发异动。值为 0 的停牌日和复牌日转换为空值。

## 当前结果与验收

- 异常统计 863 只：深圳 457、上海 385、北京 21；10 条有参考价提示；
- 停牌风险 178 只：深圳 98、上海 75、北京 5；175 条无预警、3 条触发异动；
- 当前触发证券为欣天科技、泛微网络和恒银科技；
- `statistics&code=300615` 精确返回欣天科技及严重异常参考价；不存在的股票
  返回 `availability=empty` 和 0 行；非法跨视图 warning 过滤返回 HTTP 400；
- 新增专项测试覆盖市场/名称、三窗口、实时偏离对账、参考价解析、未知字段
  保留、日期空值、预警枚举及比例到百分点转换；全量 CTest 38/38 通过。

真实通用请求证据为 `output/probes/tqlex-2044-current.json`、
`tqlex-2044-code-sort-current.json` 和 `tqlex-200720-current.json`；业务化结果
为 `output/probes/anomaly-risk-*-current.json`。

## 正式部署

正式 `8765` 服务已部署，PID 为 `40160`，EXE SHA-256 为
`0A4429D6982C10C1070100DCD501E92A4530C0590C2D021595BB1A6DBD237A4C`。
健康检查为 `native_cpp=true`、`python_runtime=false`，功能目录由 86 增至 87。
正式 API 复验欣天科技统计、3 条触发风险及融资率 5.04% 回归均通过。主页
HTTP 200；Svelte HTML/JS/CSS 哈希仍为 `0F1FBB...5777`、`B03C28...4011`、
`733085...30A`，没有覆盖用户 UI。

该部署随后已被同日的 `market abnormal-details` 版本替代；当前正式进程与
哈希见 `2026-08-06-native-abnormal-details.md`，本节只保留当时验收记录。
