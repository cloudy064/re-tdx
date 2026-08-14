# JSN 动态候选的 CFG 族与客户端页面作用域

日期：2026-08-12

## 问题

`jsn candidates` 原本将 CFG 的 `refunit` 数字当作 `cloud_cfg` 全目录唯一 ID，再用
“双方属于同一个类型化 C++ 命令”过滤。通达信实际会在大量互不相关页面中重复使用
`unit id=1/2/...`；一个 C++ 命令也可能整合多个业务页面。因此旧规则会把同命令内无关
主表交叉展开为动态路径。

真实安装上的直接反例是：

- `func_xnxs102.cfg` 的 `refunit=1` 只应连接 `XNXS.sp` 中的
  `func_xnxs101`；
- `func_ydyl103.cfg` 的 `refunit=1,2` 只应连接 `YDYL.sp` 中的
  `func_ydyl101/102`；
- 旧队列却产生了 `xnxs/hy57.jsn` 和 `ydyl1/299.jsn`。已知真实路径分别是
  `ydyl1/hy57.jsn` 和 `xnxs/299.jsn`。

旧全量队列共有 85,107 条候选、84,960 个本地缺失项，这些数字混入了跨页面假关系。

## 客户端原生关系模型

当前实现为每个 CFG 单元保存精确配置名和去除三位页号后的配置族，并递归读取
`T0002` 下真实 `.sp` 页面里的 `CfgName=` 列表。一个 `refunit` 主表只有满足以下至少一项
才可参与候选展开：

1. 主表和详情表属于同一配置族；
2. 两个精确配置名共同出现在至少一个客户端 `.sp` 页面。

随后仍保留原有的引用 ID、类型化命令交集、主表非空、键列完整、安全 ASCII、709 路径
长度和本地缺失检查。这不是按文件名强制隔离：例如 `SJQD5.sp` 明确同时装载
`func_bwyq101` 与 `func_sjqd101_2`，对应跨配置族的 `refunit=21201` 仍然有效；
`HSGT/SJQD/BYGTJ/CGFX` 的真实跨页关系也全部保留。

## 净化结果

- 全量候选：85,107 → 82,616，剔除 2,491 条无页面证据的组合；
- 本地缺失唯一资源：84,960 → 82,469；
- `XNXS` 的六条 YDYL 假候选归零，`YDYL` 不再包含 `ydyl1/299.jsn`；
- `GQZY` 从错误的五张来源表收敛到客户端页面可证明的四张：
  `func_gqzy101/102/103/109_1.jsn`；
- `func_gdrs101_1.jsn` 与 GQZY 的 `unit id=18501` 相同，但没有和详情配置共同出现在
  GQZY 页面中，因此不再被错误计为质押详情主表；
- GQZY 候选从 2,474 条修正为 1,141 条，其中本地缺失 1,128 条。

API 合约 `jsn-candidates-live` 同步增加 `page_scoped_refunit_masters` 精确断言，不再以旧的
2,000 条膨胀数量作为成功条件。

## 剩余公开资源

当前 622 个静态/动态模板中 588 个已有类型化命令。剩余 34 个静态模板全部没有本地下载
文件；对财经日历、财务指标、定增预案、可分离转债和交换债五个不同族做主站元数据抽样，
结果均为 `missing`，因此没有据此伪造业务入口。

净化后的 `qhtj2` 队列则找到三个有明确主表证据的非空资源：

| 资源 | 大小 | 状态 |
|---|---:|---|
| `qhtj2/47ICL8.jsn` | 2,955 | available |
| `qhtj2/47IHL8.jsn` | 2,733 | available |
| `qhtj2/47IML8.jsn` | 3,009 | available |

它们已经由现有纯 C++ `market futures-issuance --contract-key` 类型化处理；真实
`47ICL8` 查询返回 135 个净持仓历史点，没有必要再创建重复入口，也没有批量下载文件。

## 验证

- `tdx-jsn-variants-tests`：通过；新增“同页面跨配置允许、无关同 ID 拒绝”双向 fixture；
- `tdx-recon-contract-tests`：通过；
- 真实安装全量候选和 `qhtj2` 三项只读探测通过；
- 临时 8879 上 `health`、`features`、`jsn-candidates-live` 为 3/3；
- 临时服务已停止，正式 `127.0.0.1:8765` 未操作。

本次只改变候选关系推导和对应合约，没有改变 JSN 解码、下载协议、缓存或公开 schema，
因此按增量验证规则未运行完整 CTest。机器证据位于
`output/tdx-jsn-candidates-qhtj2-page-scoped.json` 和
`output/scratch-jsn-page-scope-contracts.json`。
