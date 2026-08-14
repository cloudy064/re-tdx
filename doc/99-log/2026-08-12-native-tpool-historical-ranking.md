# TPool 历史窗口跨证券排名闭环

日期：2026-08-12

## 结果

此前显式拒绝的公式规则 `noperate=5/6/7 + nbeginday/nendday` 已进入纯 C++
解释器。实现没有把窗口内全部“证券 × K 线”记录放进一个总体，也没有退化为每只
证券只取最新值；它复现 `TPool.dll` 的真实模型：

1. 对窗口内每个历史偏移建立一个候选组；
2. 每个组在全部可求值证券之间独立执行排名；
3. 将各组命中的证券去重后取并集。

检查结果以 `history_window_mode=per-offset-cross-security-ranking-union` 明确暴露该语义。

## DLL 证据

本轮直接使用用户处理后的 `ida/TPool.dll.i64`，新增：

- `output/ida-tpool-history-ranking-helpers.json`：7 个容器、插入、排序和去重辅助函数；
- `output/ida-tpool-history-ranking-disasm.json`：`sub_1001B9E0` 完整汇编，保留调用寄存器
  和栈参数，弥补 Hex-Rays 对 `this` 参数的省略。

关键结论如下：

- `sub_10019A40/sub_10019B80` 均在一个以整数为键的红黑树中查找或创建节点，节点
  `+16` 是 29 字节候选记录数组；
- `sub_100178A0` 将一条 29 字节记录追加到当前节点；证券身份在记录头部，排名值位于
  `+25`；
- 无历史字段时键为 `0`；单一历史偏移时键为 `nbeginday`；范围窗口时键为
  `实际偏移 - nendday`。范围键虽做了归一化，仍与每个实际历史偏移一一对应；
- 主函数逐个树节点调用 `sub_10018A10`，因此排名总体是“某一偏移上的全部证券”，
  不是所有偏移的全局混排；
- 排序为值降序，相同值由后进入的证券优先。操作 5/6/7 分别执行精确名次、前/后 N、
  名次尾部选择；
- `sub_100174B0` 在最终 85 字节证券数组中按市场和代码去重，证明多个偏移命中同一证券
  时只返回一次。

## 原生实现

- `native/src/cloud/tpool_rule.cpp` 负责单证券公式输出解析和窗口观测提取，排名规则会
  保留每个可求值偏移的 `offset_from_latest/value`；
- 新增 `native/src/cloud/tpool_ranking.cpp`，集中承载单组排序、偏移分组、DLL map key、
  跨组证券并集和总求值结果回填；
- `native/src/cloud/tpool_evaluate.cpp` 继续只做行情/公式编排，在三类排除过滤完成后调用
  排名模块，过滤证券不会偷偷进入总体；
- `ranking_population_complete` 只有在股票池未截断、历史组齐全且每组都有全部已选证券
  时才为真；缺失历史点会留下不完整总体，而不是用别的偏移补位；
- 多组结果写入 `ranking_results` 与 `matched_offsets_from_latest`。最新单组规则继续保留
  原来的 `rank_descending/rank_from_bottom/ranking_population` 标量字段，公开响应 schema
  仍是 `tdx-tpool-native-evaluation-v1`。

## 验证

- 增量构建 `tdx-tpool-tests`、`tdx-tool` 并通过；专项测试覆盖 3 个历史观测提取、
  2 个偏移分别选中不同证券后取并集、DLL 键顺序和原有同值次序；
- `output/tpool-history-ranking-live-fixture.xml` 使用 4 只证券、CCI、最近 3 个交易日的
  前 1 名规则；`output/tpool-history-ranking-live.json` 返回 3 组、12 条观测、完整总体：
  偏移 0/1 命中 `sh600519`，偏移 2 命中 `sz000002`，最终 2 只证券命中；
- `output/tpool-latest-ranking-compat-live.json` 验证最新单组仍返回 1 组、每证券 1 条结果、
  4 只总体及原标量名次字段；
- 临时端口完成 health、features、OpenAPI、内联历史排名 POST、非法 `source_name` 拒绝
  5 项聚焦契约：功能数 149，POST 返回 4 只证券/3 个排名组/2 只命中证券且不保留正文，
  非法文件名返回 400；正式 `8765` 服务未触碰；
- 未运行完整 CTest。改动局限于 TPool 领域规则/排名实现和诊断扩展，没有修改共享解析器、
  传输、缓存或部署边界。

## 保留边界

- 公式结果为 null/不可计算的证券不会加入对应偏移组，并使完整性标志为 false；当前不会
  仿造 DLL 未初始化浮点或用相邻日期补值；
- 原 worker、声音/UI 动作、板块保存、池历史写回和宿主副作用继续不执行；
- `nset=3/4` 仍是当前财务/行情快照分支，不应用历史窗口属性。
