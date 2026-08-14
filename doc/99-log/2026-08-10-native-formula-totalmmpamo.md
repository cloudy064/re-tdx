# 2026-08-10 TCalc TOTALMMPAMO 全市场买卖盘金额

## 结论

纯 C++ 公式解释器新增 `TOTALMMPAMO(1..4)`。四项数据来自公开 7709
`0x0547` 的 `SH999997` 特殊聚合记录，单位为亿元，不需要 Level2：

| 选择项 | 精确含义 | 当前来源 |
|---|---|---|
| 1 | 买一总金额 | 公开 L1，沪深两个原始分量之和 |
| 2 | 卖一总金额 | 公开 L1，沪深两个原始分量之和 |
| 3 | 前五档买盘总金额 | 公开 L1；Level2 终端会切到前十档 |
| 4 | 前五档卖盘总金额 | 公开 L1；Level2 终端会切到前十档 |
| 5 | 总委买金额 | Level2，当前明确不可用 |
| 6 | 总委卖金额 | Level2，当前明确不可用 |

选择项 5/6 没有用五档合计近似。静态分析会返回
`TOTALMMPAMO#5#LEVEL2` / `#6#LEVEL2` 不可用绑定，HTTP 自动求值因此返回
400；这与项目暂缓 L2 的边界一致。

## 原生处理链

静态注册表记录为 opcode 1376、处理函数 `TCalc.dll!sub_1000EC10`。处理函数：

1. 把参数末值截断为整数；
2. 请求宿主 callback type 168 的 88 字节结构；
3. 选择偏移 64、68、72、76、80、84；
4. 除以 10000 后广播到整条公式序列；
5. 非 1—6 参数广播 0。

TdxW type 168 固定调用 `sub_401AB0("999997", 1)`，再用 `sub_52CC90`
读取 150 字节 L1 快照和可选 96 字节十档结构。非 Level2 分支使用 L1 快照偏移：

```text
1 = offset60  + offset100
2 = offset64  + offset104
3 = offset72  + offset112
4 = offset76  + offset116
```

`TdxW!sub_85DD80` 证明这些偏移对普通证券是五档价差，但对指数类记录会直接写入
`wire_integer × 10000`。`TOTALMMPAMO` 最后的 `/10000` 恰好抵消该缩放，所得原始
整数之和就是帮助中标注的亿元数。`SH999997` 的历史名称仍是 B 股指数，但这里被
客户端作为全市场金额载体复用，因此不能把其五组字段展示成普通委托簿。

Level2 打开时，选择项 3/4 改读 96 字节十档结构的两组市场分量；5/6 始终读取
该结构中的总委买/总委卖槽位。公开 L1 响应没有这四个 Level2 槽位，所以本实现
只承诺 1—4 的公开五档口径。

## 纯 C++ 实现

`market.cpp` 现保留 `0x0547` 五档的原始变长整数。仅在 `SH999997` 上增加：

- `level_semantics=market-aggregate-sentinel...`；
- `market_amount_summary.values_by_selector[1..4]`；
- 四个具名金额字段、`raw_component_pairs` 和 `unit=100m-yuan`；
- 选择项 5/6 的 `null` 与 `level2_required_selectors=[5,6]`。

公式上下文在需要 `TOTALMMPAMO` 时只追加一次 `SH999997` 请求，绑定
`TOTALMMPAMO#1..#4`。函数沿用原生“参数末值决定整条序列”的广播行为，并支持
动态末值在 1—4 间选择。协议解码器另公开离线入口，单元测试用人工构造的加密
`0x0547` 包验证 `76+34=110`、`12+18=30`、`122+92=214`、`51+79=130`。

## 验证与产物

- `output/ida-tcalc-totalmmpamo-handler-v1.json`；
- `output/ida-tcalc-totalmmpamo-help-v1.json`；
- `output/ida-tdxw-totalmmpamo-aggregate-v1.json`；
- `output/ida-tdxw-totalmmpamo-decoders-v1.json`；
- `output/ida-tdxw-999997-xrefs-v1.json`；
- `output/native-market-totalmmpamo-depth-v1.json`；
- `output/native-formula-totalmmpamo-smoke-v1.json`；
- `output/native-formula-totalmmpamo-l2-boundary-v1.json`；
- `output/native-formula-coverage-totalmmpamo-v15.json`；
- `output/native-formula-registry-next-audit-v15.json`。

覆盖为 379/379，支持函数 244、自动符号 75；390 条静态注册名已识别 299，
剩余 91。协议、解释器和 API 契约离线测试均已通过。

全量 CTest 为 102/102；候选服务完整 API 契约为 220/220，报告
`output/api-contract-full-totalmmpamo-v15-candidate-final.json` 的 SHA-256 为
`24AAC9570B9847DAC476101598AACC4E9A77FB2E61F49653CEF5B5C2AFF7019A`。正式服务
在首页、健康检查、宿主汇总公式、K 线辅助字段和连板天梯五项复核中 5/5 通过，
报告 `output/api-contract-totalmmpamo-v15-formal.json` 的 SHA-256 为
`911C8C58AF5A63B143C6AE62CDB74AD18249C9B481E15ECCF5CBB82FF285F20A`。

最终发布 EXE 为 19,117,568 字节，SHA-256
`5BE3BE732B37467E3753BA4FF878FF8610F601C616B3CA9540619303D60EE1B7`；正式服务
PID 6176，仅监听 `127.0.0.1:8765`，健康接口保持 `native_cpp=true`、
`python_runtime=false`。替换前版本保存为
`output/tdx-tool-totalmmpamo-v15-predeploy-rollback-20260810.exe`，SHA-256
`6D66BEF15BC8FFC18FE762A508CCFCCC7AE69FCF77E06829618BDB206C44CE0A`。
