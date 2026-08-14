# 专利、月度解禁、大盘因素与关系资源收口

日期：2026-08-07

## 结论

本轮把上一轮新增下载的 6 份非空 JSN 全部提升为专用纯 C++ 业务语义；501 份
本地 JSN 中已不再存在“只能通用浏览、没有业务接口”的资源。未触碰 Level2。

新增或扩展的命令：

```text
tdx-tool market patent-statistics --input-dir output/tdx-jsn
tdx-tool market unlocks --view monthly-pressure --input-dir output/tdx-jsn
tdx-tool market overview-factors --input-dir output/tdx-jsn
tdx-tool market strategic-themes --view catalog
tdx-tool market institution-analysis --view development-bank-holdings
```

固定 API 分别为：

```text
GET /api/v1/market/patent-statistics
GET /api/v1/market/unlocks?view=monthly-pressure
GET /api/v1/market/overview-factors
GET /api/v1/market/strategic-themes
GET /api/v1/market/institution-analysis?view=development-bank-holdings
```

## 真实数据

| 能力 | 资源 | 结果 |
| --- | --- | ---: |
| 公司专利 | `func_gszl101_1.jsn` | 3,970 家公司，深 2,104 / 沪 1,538 / 北 328 |
| 月度解禁压力 | `func_dxfjj101_1.jsn` | 25 个月，2026-08 至 2028-08 |
| 大盘影响因素 | `func_dpfx101_1.jsn` | 17 项：利好 2 / 中性 3 / 利空 6 / 未评级 6 |
| 军工公司系 | `func_gfjg102_1.jsn` | 8 个公司系，95 个去重成员 |
| 互联网+主题 | `func_hlw101_1.jsn` | 30 个主题，1,195 个去重成员 |
| 国开系参股 | `func_tzcg105_1.jsn` | 5 只股票，报告期 2026-03-31 |

主题关系合并后为 26 个大类、598 个规范主题、655 条大类—主题关系、43,994 条
去重主题—股票关系，覆盖 5,438 只证券。互联网+旧目录中部分 ID 与新目录相同、
名称相同但成员截面不同，因此使用 `HLW:<源ID>` 命名空间；没有强行合并两个不同
时间或分类口径的成员集。军工公司系中 7 个与国防军工目录完全相同的主题正常
合并，兵装系作为新增主题保留。

## 语义边界

- 专利源总数可能包含客户端没有单列的专利类别；接口同时输出源总数、发明/
  实用新型/外观三类合计及差值，不用三类合计覆盖源值。
- 月度解禁 `jjsl/jjsz` 是亿股/亿元，规范字段按 `×100000000` 转为股/元；25 条
  公式核验、0 条不一致。该表是全市场月度压力，没有单票或股东详情键。
- 大盘 17 因素的描述日期并不一致，是客户端分析快照，不包装成实时信号。
- 国开系参股的持股数、占比嵌在中文披露中；本轮保留股东名次和详情原文，不用
  不稳定的文本正则伪造结构化数值。

## 验证与证据

- C++ 全量测试 91/91；
- Svelte 静态检查 0 错误、0 警告，生产构建通过；
- 正式服务重点在线契约 6/6，全量固定 API 契约 129/129；
- 正式实例为 `127.0.0.1:8765`、PID 804，健康检查识别 501 份 JSN；
- JSN 覆盖为 482/616，本地文件 501，`generic_downloaded_template_count=0`，
  解析错误 0；
- 真实输出：
  - `output/tdx-market-patent-statistics-20260807.json`
  - `output/tdx-market-unlock-monthly-pressure-20260807.json`
  - `output/tdx-market-overview-factors-20260807.json`
  - `output/tdx-market-strategic-themes-expanded-20260807.json`
  - `output/tdx-institution-development-bank-holdings-20260807.json`
  - `output/tdx-jsn-variants-20260807-all-downloaded-typed.json`
  - `output/tdx-api-contracts-selected-patents-themes-20260807.json`
  - `output/tdx-api-contracts-full-20260807-all-downloaded-typed.json`

剩余 134 个模板没有本地文件；其中前一轮挑出的 19 个高价值静态候选已经证实
远端缺失。后续应继续从客户端运行时新增文件和动态主从键中发现真实数据，不应
把残留 CFG 文件名直接宣传成可用功能。
