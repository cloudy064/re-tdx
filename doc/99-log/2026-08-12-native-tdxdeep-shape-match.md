# TDXDeep 本地形态匹配模板与评分

日期：2026-08-12

## 结论

`TDXDeep.dll` 的“形态匹配选股”已经闭合为纯 C++、CLI-only 功能：

```powershell
tdx-tool market shape-match --root C:\new_tdx

tdx-tool market shape-match --root C:\new_tdx --view score `
  --template-index 0 --market sz --code 000001 --source auto

tdx-tool market shape-match --file C:\path\shapematch.dat --view score `
  --template "方案名称" --candidate output\candidate.json

tdx-tool market shape-match --root C:\new_tdx --view scan `
  --template-index 0 --block "industry:880301" --source local

tdx-tool market shape-match --file C:\path\shapematch.dat --view scan `
  --template-index 0 --input output\candidate-bundle.json
```

第一条命令离线列出模板；后两条分别用本地/公开 K 线或既有
`tdx-minute-v1` JSON 为一只证券评分，最后两条对本地板块或调用方已有的离线
K 线包执行批量选股。运行时不加载原 DLL。当前安装
`C:\new_tdx` 没有 `T0002\shapematch.dat`，实测会返回明确的空库状态，
不会制造示例模板。

原版对话框标题为“形态匹配选股”，包含执行选股、设置形态、删除形态、
自绘形态、保存形态和证券范围。原 worker 通过宿主回调枚举证券后在本地逐只评分，
没有发现云端形态匹配请求。

## 文件格式

`T0002\shapematch.dat` 的整体长度严格为：

```text
4 + N * 94195
```

前 4 字节是所选证券范围：`0` 为全部 A 股，`1` 为沪深北全部证券，`2` 为扩展市场。
每条 94195 字节记录的稳定字段如下：

| 偏移 | 长度 | 语义 |
|---:|---:|---|
| 0 | 4 | 模板 ID/保存时间截断值 |
| 4 | 45 | NUL 结尾 GBK 模板名 |
| 49 | 2 | 类型：0 证券 K 线，1 手绘 |
| 51 | 2 | 周期代码 |
| 53 | 4 | 点数，范围 0..2000 |
| 57 | 2 | 模板证券市场号 |
| 59 | 23 | NUL 结尾证券代码 |
| 82 | 45 | NUL 结尾证券名称 |
| 127 | 35×2000 | 证券模板 K 线区 |
| 70131 | 12×2000 | 手绘点区：`int32 x/y + float value` |
| 94131 | 4 | 匹配阈值百分数 |
| 94135 | 4 | 四个分量启用标志 |
| 94155 | 4×4 | 四个 `float` 权重 |

35 字节 K 线的时间是 `uint16 year + month/day/hour/minute/second`；
`open/high/low/close` 位于 7/11/15/19，成交量位于 27。工具只解析评分实际需要的
开、收和量，同时严格检查记录长度、字符串终止符、点数和 512 MiB 文件上限。

## 原生算法

证券 K 线模板有四个可独立启用并加权的序列：

1. 收盘价；
2. 成交量；
3. `(close - open) / open`；
4. K 线方向：`open >= close ? -1 : +1`。

每个分量把模板与候选末尾同长度窗口计算 Pearson 相关系数，再按权重求和；总分不低于
`threshold_percent / 100` 即命中。手绘模板只取 `x/y` 都非零的活动点，并把对应
位置的手绘值与候选收盘价计算同一相关系数。

IDA 曾把相关函数内部一个运行库调用错误标成 `log(float)`。为避免照着反编译标签实现
错误算法，32 位固定向量探针直接调用原 `TDXDeep.dll` 内部函数，结果为：

```text
identical = 1
scaled = 1
reversed = -1
flat = 保持调用方初值
leading_zero_window2 = 1
```

这些结果与标准差/Pearson 路径一致，而与对数变换不一致。纯 C++ 单元测试固定保存了
这组原 DLL 行为。

## 实现边界

实现按职责拆为 `shape_match_parse.cpp`、`shape_match_score.cpp`、
`shape_match_scan.cpp`、`shape_match_query.cpp` 和 `shape_match_command.cpp`，公开类型位于
`native/include/tdx/shape_match.hpp`。响应 schema 为
`tdx-shape-match-native-v1`。

- 默认只查看本地模板库，不联网；
- `score` 只评价明确选择的一只证券或候选 JSON；
- `scan` 必须在离线 K 线包、重复 `--security`、本地 `--block` 和 `--all` 中明确
  选择一种候选范围；同一证券去重，结果按得分降序排列；
- 扫描默认 `--source local`，最多 16 个工作线程、10,000 个候选；`auto/network`
  只有在显式设置 `--max-network-requests` 后才会发请求，并报告实际请求数、覆盖率、
  错误和截断状态；
- `--all` 按模板库头部范围筛选本地 TNF：范围 0 只保留 A 股，范围 1 保留沪深北
  全部证券；扩展市场范围必须提供显式候选，不能用本地三市场目录冒充；
- 周期 8/12/13 不能由现有公开 K 线加载器直接表达，必须显式传 `--period`；
- 没有 HTTP 路由，尚未把用户私有模板暴露给长期服务；
- 当前没有用户真实 `shapematch.dat`，因此记录布局与算法已有原 DLL 固定证据，
  但仍需未来真实模板做一次端到端评分对照。

## 验证与证据

- 最新 `tdx-shape-match-tests` 专项 CTest 1/1（约 0.5 秒）；
- 固定夹具覆盖 GBK 元数据、证券模板、手绘模板、批量排序/匹配过滤/截断、文件缺失
  和错误长度；
- `C:\new_tdx` 真实空库样本输出
  `output/verify-shape-match-empty-20260812.json`；
- 候选服务自检为 `feature_count=160, ok=true`；正式 8765 服务未重启、未替换。

主要 IDA/原 DLL 证据：

- `output/ida-tdxdeep-shapematch-20260812.log`
- `output/ida-tdxdeep-shape-dialogs-20260812.log`
- `output/ida-tdxdeep-shape-handlers-20260812.log`
- `output/ida-tdxdeep-shape-worker-20260812.log`
- `output/ida-tdxdeep-shape-screening-20260812.log`
- `output/ida-tdxdeep-shape-algorithm-20260812.log`
- `output/ida-tdxdeep-shape-similarity-20260812.log`
- `output/native_probe_tdxdeep_shape_similarity.cpp`
