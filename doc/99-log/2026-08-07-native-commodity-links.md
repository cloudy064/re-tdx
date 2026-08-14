# 商股联动、涨价题材与事件股票关系链

日期：2026-08-07

## 结论

通达信“商股联动”不是一个孤立表，而是两条完整的公开 JSN 主从链：

```text
SPHQ.sp 商品行情
  list/func_zjtc103_1.jsn
    ├─ commodity_id -> zjtc4/<id>.jsn  关联股票
    └─ commodity_id -> zjtc5/<id>.jsn  行业、指数或 ETF

ZJTC.sp 涨价题材
  list/func_zjtc101_1.jsn
    ├─ theme_id -> zjtc1/<id>.jsn       长期关联股票
    └─ theme_id -> zjtc2/<id>.jsn       历史驱动事件
                    └─ driver_id -> zjtc3/<id>.jsn  当次事件股票
```

七类模板现已全部固定为纯 C++ `market commodity-links` 和固定 API
`/api/v1/market/commodity-links`，运行时不调用或转发 Python。Svelte 数据中心
与个股工作台也已接入。

## 新鲜数据核验

2026-08-07 从深圳双线主站 `110.41.147.114:7709` 重新下载：

- `func_zjtc101_1.jsn`：13,508 字节，MD5
  `ef6ba6832909e3d8f38718d7ea999f4b`，21 个涨价题材；
- `func_zjtc103_1.jsn`：43,644 字节，MD5
  `3251f383832ec93a12b8c412a7d6768a`，221 条商品报价；
- `zjtc1/70.jsn`：13 只锆金属长期关联股；
- `zjtc2/70.jsn`：9 条锆金属历史驱动；
- 最新驱动 `zjtc3/20028.jsn`：东方锆业、爱迪特两只股票；
- `zjtc4/X100102003.jsn`：1/3 焦煤 6 只关联股；
- `zjtc5/X100102003.jsn`：煤炭 ETF 与一个指数证券，共 2 条。

题材选择键也用另一条当前样本交叉验证：环氧树脂主题 ID `1089` 对应
`zjtc1/1089` 14 只长期关联股、`zjtc2/1089` 8 条驱动；最新事件 ID `19948`
继续选择 `zjtc3/19948`，得到 2 只事件股票。

## 一个必须保留的数据特征

商品主表的 `$ZQDM` 是关联关系选择键，不是报价行唯一主键。221 行中只有
219 个去重关系 ID：

- `X110301003` 同时对应“FU 燃料油”和“燃料油”；
- `X140201005` 同时对应“铂”和“铂金”。

因此模型增加独立 `quote_id=<commodity_id>:<source_rank>` 保留每条报价；
`commodity_id` 仍负责选择共享的 `zjtc4/5` 股票关系。商品详情返回 `quotes[]`，
不会误去重，也不会因选择第二条报价而展示第一条名称的假详情。

## 字段与公式

商品主表规范化 `zxjg/price0..price4` 为最新、前日、5/10/30/60 日参考价，
并严格复现客户端：

```text
day_pct = (zxjg - price0) * 100 / abs(price0)
N_day_pct = (zxjg / priceN - 1) * 100
```

分母为空或为零时返回 `null`。`zjtc1/3` 的 `CFJ` 和 `zjtc4` 的 `price1`
只是触发价/三月复权参考价；客户端依赖宿主 `$NOW` 计算的当前收益，以及
`zjtc5` 的实时行情、换手率等 syscol 不存在于 JSN，接口明确保留为 `null` 或
`current_quote_available=false`，不伪造行情。

## 命令与 API

支持七个视图：

```powershell
tdx-tool market commodity-links --view commodities
tdx-tool market commodity-links --view commodity --commodity-id X100102003
tdx-tool market commodity-links --view themes
tdx-tool market commodity-links --view theme --theme-id 70
tdx-tool market commodity-links --view driver --theme-id 70 --driver-id 20028
tdx-tool market commodity-links --view security --market sz --code 002167
tdx-tool market commodity-links --view catalog
```

`security` 视图先用完整题材主表的 `$S_ZQDM` 反查，再只下载命中题材的
`zjtc1/2`；关联商品只读取一次 221 行报价主表。它不会为了查一只股票而批量
拉取 221 张 `zjtc4`，从而把请求成本限制在实际命中关系内。东方锆业当前命中
锆金属、钛金属、稀土永磁三个题材，返回 8 个来源资源。

## 网页

- 数据中心新增“商股联动”，可在 221 条商品报价和 21 个涨价题材之间切换；
- 商品详情展示共享报价、关联股票、行业/指数/ETF；
- 题材详情展示长期关联股、历史驱动，点击驱动再精确展开事件股票；
- 个股工作台“商股联动”页签展示该票命中的题材、驱动事件和商品报价；
- 股票、ETF 和可解析指数均可继续进入个股工作台。

## 覆盖与验证

- `recon jsn-variants`：`259/616` 模板已有类型化命令；通用独占为 357；
  已下载 339 个文件全部匹配，已下载但仍通用独占从 76 降至 69，解析错误 0；
- CTest：57/57；
- Svelte：`svelte-check` 0 错误/0 警告，生产构建通过；
- API full 契约：56/56，其中新增商品主表、商品关系和涨价题材 3 项；
- 当前服务：`http://127.0.0.1:8765`，PID `38484`，功能目录 102 项，
  `native_cpp=true`、`python_runtime=false`；
- 部署 EXE SHA-256：
  `5ABA85C4C84035DD7CD4683F93163DB247AEF8E2A43A9BB037E2CAF889544E56`；
- 当前网页资产：`index-BA1uBiqB.js`、`index-BBvfIhLg.css`；SHA-256 分别为
  `74350914EFDAFFF76E42A7C24BE02B37230DD25B359663055F6A26868C03CB6E`、
  `B126D3CCB7BA8856FD8C3D7370C7061D5579E316B8EC04DA59A7D082FD249BCC`。

结构化在线证据保存在：

- `output/probes/commodity-links-commodities-current.json`；
- `output/probes/commodity-links-commodity-jiaomei-current.json`；
- `output/probes/commodity-links-theme-70-current.json`；
- `output/probes/commodity-links-driver-20028-current.json`；
- `output/probes/commodity-links-security-002167-current.json`；
- `output/probes/api-contracts-commodity-links-current.json`；
- `output/probes/jsn-variants-commodity-links-current.json`。

## 下一步

在当前公开非 L2 缺口里，`zxjx101`“公告精选”已经成为下一项优先候选；应先
核实其列表、正文/证券关联键与时间覆盖，再决定是并入现有公告/市场情报能力，
还是建立独立类型化服务。
