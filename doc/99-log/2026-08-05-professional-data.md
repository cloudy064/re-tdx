# 通达信专业财务与交易数据包

> 日期：2026-08-05。实现位于 `native/src/professional_data.cpp`，运行时为纯
> C++，不加载通达信 DLL，也不转发 Python。

## 已确认的官方来源

- 财务清单：`https://data.tdx.com.cn/tdxfin/gpcw.txt`
- 财务包：`https://data.tdx.com.cn/tdxfin/gpcwYYYYMMDD.zip`
- 交易清单：`https://data.tdx.com.cn/tdxgp/gpszsh.txt`
- 交易文件：`https://data.tdx.com.cn/tdxgp/gp{sz|sh|bj}CODE.dat`
- 官方说明：`https://www.tdx.com.cn/article/stockfin.html`

下载器只允许上述固定官方目录，按清单校验长度和 MD5，并把已验证文件缓存在
`output/tdx-professional-cache`。`gpsh999999.dat` 在部分 CDN 返回的清单中被
省略，但固定官方 URL 仍可访问；该特殊文件走 HTTPS 固定源和严格记录结构校验。

报告季进行中时，官方可能以相同文件名滚动更新 `gpcw` 包，不同 CDN 节点会短时
出现清单与文件版本漂移。下载器会给清单和文件加官方域内的破缓存参数，最多重试
三次，并在内存中同时通过长度和 MD5 后才原子写入缓存；失配响应不会污染有效缓存。

## 二进制格式

### `gpcwYYYYMMDD.dat`

- 20 字节头：版本、报告期、证券数、11 字节索引尺寸、每证券数据尺寸。
- 每条索引为 6 字节代码、1 字节保留值、4 字节小端数据偏移。
- 当前样本的保留字节对深沪京证券均为 0，不能误当市场号；市场由代码推断。
- 每证券数据为 584 个小端 `float`，对应 `FINVALUE(1)..FINVALUE(584)`；
  `FINVALUE(0)` 是报告期。
- 官方 `20260331` 样本有 5,534 个索引、每证券 2,336 字节。包中有两组重复
  代码，解析器采用确定性的“后记录覆盖前记录”，得到 5,532 个唯一证券。

### `gp*.dat`

文件由连续 13 字节记录组成：

| 偏移 | 长度 | 含义 |
| ---: | ---: | --- |
| 0 | 1 | 数据 ID |
| 1 | 4 | `YYYYMMDD` 小端日期 |
| 5 | 4 | 第一个 `float` 字段 |
| 9 | 4 | 第二个 `float` 字段 |

同一格式承载三类函数：

- 普通证券文件：`GPJYVALUE`，帮助已定义 ID 1–44。
- `gpsh880xxx.dat` 板块文件：`BKJYVALUE`，帮助已定义 ID 5–19。
- 特殊 `gpsh999999.dat`：`SCJYVALUE`，帮助已定义 ID 1–42。

`N` 选择第一或第二字段；`TYPE=0` 只在原始日期返回，`TYPE=1` 向前延续最近
值，`TYPE=2` 在缺失日期返回 0。个股样本还观察到 ID 45、47–50，市场样本
观察到 43–45、99；当前帮助没有定义，工具保留原始 ID 和数值但不擅自命名。

## 工具与公式能力

```powershell
tdx-tool market professional --kind catalog
tdx-tool market professional --kind stock --security sz:000001 --field 3 --history
tdx-tool market professional --kind board --security 880201 --field 5 --history
tdx-tool market professional --kind market --field 31 --history
tdx-tool market professional --kind finance --security sz:000001 `
  --period 20260331 --field 0 --field 308
tdx-tool market professional --kind finance-series --security sz:000001 `
  --field 271 --field 308 --from 20250101 --limit 8
```

`finance-series` 按报告期升序返回最近若干个实际包含目标证券的季度，默认 8 期、
最多 80 期，同时返回未成功包的独立诊断。默认会排除晚于本机当前日期的官方占位
包。命令和 `/api/v1/market/professional?kind=finance-series&market=sz&code=000001`
使用同一套纯 C++ 实现。平安银行实测已连续返回 `20251231` 与 `20260331`，且
缓存文件与当次官方清单的长度、MD5 完全一致。

解释器已支持 `FINVALUE/GPJYVALUE/BKJYVALUE/SCJYVALUE` 的静态参数审计和上下文
绑定。叠加后续 `WINNER/COST` 历史股本、内外盘和 `FINANCE(43/44)` 官方
`FN184/FN183` 同比上下文后，系统公式含上下文覆盖当前为
305；平安银行 800 根日线的 305 条
候选全部执行通过、0 错误、0 全空输出。新增覆盖包括 11 个资金/涨跌停/股东
指标、PE/PB/PCF/PS 四个估值指标，以及 PEG 和业绩增长两条选股公式。

自动 `FINVALUE` 当前为“最近几个官方季度包中，包含该证券的最新报告期值”，
并在结果 `context_metadata` 中标注 `latest-available-report-constant`。它适合当前
估值和链路验证，但尚未按历次报告期构造无未来信息的历史财务序列，不能直接
作为严谨历史回测依据。`finance-series` 解决的是显式多报告期读取；由于官方包未
提供首次公告时间，它也明确标注 `report-period-series-no-announcement-date`，不能
仅凭报告期日期冒充无前视偏差的回测序列。
