# 限售解禁同键多批次状态容错

## 现场问题

`DATETOCUR` 第二轮 full API 首次得到 209/211。其中 `jsn-discovery-live` 是冷缓存
扫描超过 30 秒；预热后通过。真正的业务失败来自：

```text
GET /api/v1/market/unlocks?market=sz&code=000001&include_details=1
HTTP 400
unlock rows sharing a detail key disagree on status or reason
```

诊断版错误定位到真实主表键 `20260812000973`：两行的日期、证券和原因
“非公开发行限售”一致，但批次状态分别为“实施”和“未实施”。

## 根因与修复

`$ZQDM=<YYYYMMDD><证券代码>` 是股票加日期级的股东详情键，同一天同一股票可以
包含多个锁定批次；它并不承诺所有批次状态或原因相同。旧实现正确聚合股份数，
却把状态/原因差异当作事件身份损坏并抛错，导致主表中任意一只股票的合法混合
批次让所有证券查询都返回 400。

现在保持日期、证券、市场身份冲突仍为硬错误，但状态和原因按批次保存：

- 每个 `lots[]` 保留自己的 `progress/reason`；
- 事件返回去重后的 `progresses[]/reasons[]`；
- 多状态时 `progress="混合状态"`、`mixed_progress=true`；
- 多原因时 `reason="多种原因"`、`mixed_reason=true`；
- `--progress/--reason` 过滤既匹配单值，也匹配聚合数组；
- 汇总新增 `mixed_progress_events`，不把混合事件强行归为已实施或待实施。

## 真实验证

正式响应中的冲突事件为：

```json
{
  "detail_id": "20260812000973",
  "progress": "混合状态",
  "progresses": ["实施", "未实施"],
  "reason": "非公开发行限售",
  "mixed_progress": true,
  "lot_count": 2,
  "availability": "live"
}
```

该事件仍能展开 5 条股东明细。新增单元向量还覆盖同键多状态、多原因和逐批次字段；
最终 CTest 101/101、解禁/公式/健康专项 5/5、full API 211/211、正式专项 5/5。
真实输出另存 `output/native-unlocks-mixed-progress-real.json`，正式发布信息与
`DATETOCUR` 批次一致。
