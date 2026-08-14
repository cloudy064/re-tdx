# JSN 静态资源协议

> `reqformat=11` 已由静态分析和普通 Python 直连共同验证。它不是
> TQLEX/HTTP，而是行情主站上的静态文件服务。

## 结论

客户端把 `cloud_cfg` 中的 `.jsn` 路径交给 TdxW。TdxW 经已经完成
`0x000D` 握手的公开 7709 行情连接：

1. 用命令 `709` 查询文件长度和 MD5；
2. 用命令 `1721` 按最多 30,000 字节分页下载；
3. 原客户端写入 `T0002\cloud_cache`，页面再从缓存解析 JSON。

普通脚本可直接复现，不依赖 TdxW 进程、注入、本地 DLL、账号或登录
票据。项目工具默认写入 `output\tdx-jsn`，不会修改客户端目录。

## 客户端链路

```text
cloud_cfg datasource(reqformat=11, body=list/xxx.jsn)
  → TdxZdView100 CBiFileHandle
  → callback(opcode=40, "任务号|路径")
  → TdxW 资源队列类型 9
  → remote bi/<路径>
  → 7709 command 709 / 1721
```

`reqformat=20` 与这条链不同：它是本地树控件命令执行，不是另一个网络
下载协议。云页面的 JSON/PBRPC 路径仍由 TPData 处理；不能把 TPData 的
结论外推到格式 11。

## 协议布局

命令 `709` 的请求数据是 40 字节 NUL 结尾 ASCII 路径。响应为：

```text
uint32 file_size
uint8  has_md5
char   md5_hex[32]
uint8  nul
```

命令 `1721` 请求固定 308 字节：

```text
uint32 offset
uint32 count
char   remote_path[100]
uint8  reserved[200]
```

响应为 `uint32 returned_size` 后跟对应文件字节。下载器同时校验声明长度
和 MD5，并通过临时文件原子替换结果。

## 行业、主题和证券关联

`list/func_gx_hyzt101_1.jsn` 的一次真实下载为 8,901,469 字节，MD5
`825d3fc338d08e8fe47d45593a3024ae`。解析结果包括：

- 5,534 只证券；
- 110 个通达信叶子行业；
- 每只证券所属行业及行业 PE(TTM)、PB(MRQ)；
- 1,775 个主题和 126,571 条主题—证券关系。

云端 110 个叶子代码与本地 `tdxzs3.cfg` 完全对应。解析器使用在线成分，
并将它们向本地父节点展开，得到 13 个一级、56 个二级、76 个三级行业，
共 145 个节点。技术指标 MACD/KDJ 不在该模型中。

## 使用

```powershell
# 清单；不联网
python doc/90-scripts/download_tdx_jsn.py --root C:\new_tdx --list

# 只查询长度和 MD5
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --probe `
  --resource list/func_gx_hyzt101_1.jsn

# 下载到项目 output，并生成关系模型
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --download `
  --resource list/func_gx_hyzt101_1.jsn --summary

python doc/90-scripts/extract_tdx_hyzt.py `
  --root C:\new_tdx --compact
```

只指定 `--resource` 是 dry-run。动态资源模板可通过 `--market` 和
`--code` 展开；只有显式 `--client-cache` 才写客户端缓存。

批量更新全部 21 个无占位符资源，并生成字段/覆盖目录：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --download --all-static

python doc/90-scripts/catalog_tdx_jsn.py --root C:\new_tdx
```

当前 21 个资源已全部真实下载并通过 MD5，合计 14,873,251 字节、
44,542 行。除行业/主题外，主要覆盖：

| 数据面 | 行数 |
| --- | ---: |
| 财务、估值、成长和机构持仓 | 5,541 |
| 员工、薪酬和研发 | 5,534 |
| 安全评分与风险/亮点类型 | 5,534 |
| 多周期资金净流入 | 5,203 |
| 分红送转和配股 | 4,335 |
| 融资融券 | 3,841 |
| 龙虎榜历史 | 2,215 |
| 股权质押、商誉和解禁 | 2,088 |
| 业绩预告 | 1,810 |
| 分析师评级和盈利预测 | 609 |

字段中文名同时存在于页面 XML 和配套 `.cfg`。编目器会合并两者，因此可
恢复 XML 未显示的 `PS1=PS(TTM)`、`fxlx=风险类型`、
`$ZQDM=龙虎榜关联ID` 等字段，而不是依赖字段缩写猜测。

按证券横向查询所有已下载功能：

```powershell
python doc/90-scripts/query_tdx_jsn.py `
  --root C:\new_tdx `
  --market 0 --code 000001 `
  --output output\tdx-000001-features.json
```

在最初只下载 XML 清单资源时，平安银行实测命中 9 个资源、25 条记录，
包括 17 条高管明细。加入战略主题主表和首批 CFG 隐藏表后，平安银行样本
可命中 15 个资源、72 条记录；继续加入机构等资源族后，中兴通讯当前命中
39 个资源、213 条记录，同时带回逐股主题逻辑、事件成员关系、市场关注度
和多类机构持仓。

动态 `ggxc/$$$SC$$$$$ZQDM$$.jsn` 已验证可直接用市场号和股票代码展开。

战略主题页的主从链也已经闭合：`GN_GNZ.xml` 先用本地树
`hy_tree1_gnz.xml` 选择 24 个战略主题大类，再由相应 `func_*.cfg` 指向
`list/func_*_1.jsn` 主表；主表隐藏列 `$ZQDM` 就是详情资源
`zttzty/<主题ID>.jsn` 所需的内部 ID。因此不再依赖 `reqformat=1/20`
运行态流量。

24 个主表全部真实下载并通过 MD5，当前得到 617 条大类—主题记录、
567 个唯一主题、5,382 只关联证券。逐股详情含入选逻辑 `tzlj`、完整说明
`xxsm` 和 3/5/20/60 日及三个月价格字段。已验证 5G6G 大类全部 9 个主题
以及“成飞概念”，共 10 个详情文件、852 条逐股逻辑。主表中 7 个主题有
9 条重复成员，工具保留服务端声明计数并在关系模型中去重；详情可用时以
详情成员为准，因此也能补入北交所证券。

```powershell
# 更新全部大类主表并直接生成纯 C++ 关系模型
tdx-tool market strategic-themes --root C:\new_tdx `
  --view catalog --limit 600 --refresh

# 读取指定主题的实时成员和逐股入选逻辑
tdx-tool market strategic-themes --root C:\new_tdx `
  --view theme --theme-id 657 --limit 5000 --refresh

# 从指定股票反查全部战略主题
tdx-tool market strategic-themes --root C:\new_tdx `
  --view security --market sz --code 000063 --limit 600
```

目录查询把 24 张主表作为一个批次校验和缓存；只有 `view=theme` 会再请求一个
选中主题的详情资源，不会无意中把 567 个详情同时打到服务器。该能力由
`market strategic-themes`、`/api/v1/market/strategic-themes` 和 Svelte 页面
共同使用，运行时不需要 Python。

## CFG 隐藏主表

战略主题的发现说明资源清单不能只看 XML datasource。继续扫描
`T0002/cloud_cfg/*.cfg` 的 `<unit file="*.jsn">` 后，得到 542 个唯一
直接候选资源，其中 521 个只出现在 CFG。加入 XML 动态模板以及由 CFG
关联字段、市场代码和 `$$UNITID$$` 恢复的动态详情后，工具统一清单为
616 个可下载资源。

工具可用 `--include-cfg` 显示和选择这些资源，但不会自动全量下载：

```powershell
# 只列出 XML + CFG 的统一清单
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --list --include-cfg

# 按功能名选择性更新
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --include-cfg --download `
  --match 'func_scrd101|func_cgfx101|func_gdrs101' --summary
```

已对 30 个高价值候选执行命令 709 元数据探测，26 个当前有效；再选择性
下载其中 24 个，共 3,475,722 字节、28,102 行，全部通过 MD5。新增功能
包括全市场关注度/共鸣度/舆情热度、机构持仓季度变动、股东人数、盘后
成交、板块联动、事件与部委要闻、风险/亮点、连板梯队、公司业绩评论与
大宗交易。`--max-resources` 默认限制单次联网最多 100 个，防止把 542 个
候选误当作普通的 21 个 XML 静态资源全量请求。

2026-08-01 又按客户端 `.sp` 页面语义完整验证四个资源族：机构持仓 24
表、27,518 行；五市场股东人数 5 表、5,416 行；5/10/20/30 日和近 3 月
资金强势榜 5 表、500 行；龙虎榜 8 张视图、349 行。四族合计 42 张主表、
33,783 行、3,959,481 字节，均通过长度和 MD5 校验。与首批样本去重后，
这一阶段累计有 62 个高价值主表得到真实验证。

龙虎榜还有一条 CFG 未直接写出扩展名的主从链：8 张视图的 `$ZQDM` 是
事件 ID，`lhbfx/<事件ID>.jsn` 返回营业部级买卖明细。当前 349 条主表记录
归并为 147 个事件、118 只股票，并已验证 5 个事件、56 条明细。工具保留
同一事件在多个视图中的全部分类；若视图间市场号冲突，以详情记录为准并
保留冲突候选。可选择性更新：

```powershell
# 四个已经验证的主表资源族
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --family institution-holdings --download

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --family shareholder-counts `
  --family lhb-analysis --family capital-strength --download

# 龙虎榜主表，以及指定股票的营业部明细
python doc/90-scripts/update_tdx_lhb.py `
  --root C:\new_tdx --download-masters --compact

python doc/90-scripts/update_tdx_lhb.py `
  --root C:\new_tdx --download-details --market 1 --code 603221 --compact
```

龙虎榜详情默认最多请求 50 个事件；只有显式使用 `--all-details` 并调整
`--max-details` 才会扩大范围。合并 XML 静态表、动态详情、战略主题和隐藏
表后，当时离线编目为 127 个文件、95,354 行、21,714,933 字节。

## 公司行动、大宗交易与股权关联

继续按 `.sp` 页签和 `.cfg` 的 `refunit` 关系验证后，新增三个可更新资源族：

| 资源族 | 主表 | 行数 | 字节 | 内容 |
| --- | ---: | ---: | ---: | --- |
| `corporate-actions` | 28 | 10,700 | 1,415,390 | 股东/董监高增减持、质押/解押、回购、解禁、分红 |
| `block-trading` | 7 | 8,390 | 1,190,395 | 月度统计、个股明细、营业部画像、意向申报 |
| `equity-groups` | 4 | 111 | 21,038 | 行业、地方国资、整合关系、公司系分组 |

三族共 39 张主表。与之前样本去重后，累计已有 98 个高价值主表通过
7709 文件长度和 MD5 验证。动态路径遵循四种规则：

```text
<目录>/<市场号><证券代码>.jsn
<目录>/<主表 $ZQDM>.jsn
hgrztj<控制单元号>/<年份>.jsn
yybph<控制单元号>/<营业部 ID>.jsn
```

据此恢复了 18 种模板，包括 `zcjc`、`gqzy`、`cggg`、`gghg` 单证券
历史，`dzjy1 -> dzjy2` 的月份→行业→逐股明细，`xtzy` 的质押机构→股票，
`dzjy3/13` 的个股历史，三个回购年度趋势以及四档营业部逐笔记录。
`gqgg/<分组ID>.jsn` 还返回分组内每只股票的实控人、控股比例、多周期
复权价格、领涨次数和逻辑说明。

```powershell
# 更新三组主表
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family corporate-actions --family block-trading `
  --family equity-groups --download --summary

# 一次尝试一只股票的四类动态详情；空资源自动跳过
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --include-cfg `
  --match '^(zcjc|gqzy|dzjy3|dzjy13)/' `
  --market 0 --code 000009 --download --skip-missing --summary
```

当前保存 27 个动态样本、768 行、88,442 字节。合并全部已下载 JSN 后，
离线编目为 190 个文件、114,595 行、24,331,753 字节。`query_tdx_jsn.py`
能够通过详情模板的绑定参数把无证券字段的历史表关联回个股；市场号解析
也已覆盖 `31/48` 等两位海外市场。

## 融资融券、沪深港通与事件关联

继续验证四个页面组后，新增四个资源族：

| 资源族 | 主表 | 行数 | 字节 | 内容 |
| --- | ---: | ---: | ---: | --- |
| `margin-financing` | 15 | 3,956 | 290,935 | 个股/ETF 两融排行、市场统计与转融通历史 |
| `stock-connect` | 18 | 7,661 | 765,462 | 陆港通资金、持股、调出及行业资金 |
| `active-lhb` | 3 | 150 | 12,402 | 近 5 日/1 月/半年活跃龙虎榜股票 |
| `market-calendar` | 2 | 167 | 15,454 | 全球宏观数据与热点会议 |

38 张主表共 11,934 行、1,084,253 字节，累计验证的高价值 CFG 主表增至
136 张。两融、转融通和活跃龙虎榜均有近期数据；沪深港通日/周资金、港股
持仓和 2026Q2 陆股通季度持仓仍有近期数据，但部分陆股通增减仓榜停在
2024-08-16，只能当作历史快照。

新增 19 种动态模板。主要主从关系为：

```text
股票/ETF -> rzrq1/2/3/4 -> 近三月两融明细与走势图
日期 -> rzrq6/7/8 -> 行业/概念/风格 -> rzrq5 -> 长期两融趋势
全市场日期 -> func_rzt101 -> 转融资金额（元）与转融券数量（股）历史
日期+通道 -> hsgt -> 十大成交活跃股票
股票或 jd<代码> -> hsgtcg1/2 -> 持股历史与走势
港股行业 -> ggthy/ggthy1 -> 行业成分资金与历史
活跃股票 -> hylhb<unit> -> 龙虎榜历史异动
热点会议 -> cjrl -> 关联股票
```

`func_rzt101_1.jsn` 当前有 241 个交易日。`zrz1/zrz2/zrz3/zrz7/zrq5`
是元，`zrq1—zrq4` 是股；最新 `20260807` 的转融资偿还为 1,400,000,000 元、
余额为 147,580,000,000 元。上游在当前政策阶段省略的转融资融出/净增、
转融券融出/净增/余量/余额继续输出 `null`，不会以 0 替代，也不会用偿还和余额
倒推客户端没有给出的日增量。

`rzrq5` 三个样本分别返回约 1,900 个交易日。陆股通季度主表把详情键保存
为 `$ZQDM=jd601369`，真实证券则在 `$SC1/$ZQDM1`；查询工具已用主表建立
别名映射，使无证券字段的两张详情仍可归到 `SH601369`。

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family margin-financing --family stock-connect `
  --family industry-lhb --family market-calendar --download

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --include-cfg `
  --match '^rzrq5/' --key 880446 --download --summary
```

当前保存 26 个动态样本、7,337 行、556,185 字节；全目录编目为 253 个
文件、133,625 行、25,955,023 字节。统一清单从之前的 565 调整为 583：
加入 19 种已验证模板，同时剔除当前返回空文件的一个 `$$UNITID$$` 图表
占位符。

## 期货、发行统计、涨跌停与商品主题

客户端页面关系继续恢复出五个资源族：

| 资源族 | 主表 | 行数 | 字节 | 内容 |
| --- | ---: | ---: | ---: | --- |
| `futures-statistics` | 3 | 168 | 20,096 | 商品/股指期货及月度成交 |
| `ipo-bond-issuance` | 3 | 3,256 | 103,004 | IPO 年度统计和债券发行人 |
| `price-limit-analysis` | 5 | 1,353 | 180,657 | 最新/历史涨跌停与年度次数 |
| `commodity-themes` | 2 | 242 | 57,159 | 涨价主题和商品现货行情 |
| `premium-stocks` | 1 | 119 | 12,010 | 百元股日度统计 |

其中 `func_qhtj101_1.jsn` 是早期样本；其余 13 张是本轮新增，累计验证的
高价值 CFG 主表增至 148 张。动态关系闭合为：

```text
期货品种 -> qhtj1/2 -> 关联股票、股指期货净持仓历史
年份 -> ipotj102/104 -> IPO 行业统计、月度走势
年份+行业 -> ipotj103 -> IPO 股票明细
日期分组 -> bygtj1/3 -> 百元股名单、家数走势
股票/日期 -> zdtfx1/2/3 -> 历史原因、当日涨停/跌停股票
涨价主题 -> zjtc1/2 -> 股票、驱动事件 -> zjtc3 -> 股票
商品 -> zjtc4/5 -> 股票、行业与 ETF
当前公告榜 -> func_zxjx101/103 -> 股票、标题、多空、PDF、近期表现
股票 -> ggjx/<市场><代码> -> 历史公告、公告前/后 3 日表现
沪深回购代码 -> gznhg100 -> 期限、计息天数、手续费、资金可用/可取日
```

2026-08-07 已将上述两条 `zjtc` 链固定为纯 C++ `market commodity-links` 与
`/api/v1/market/commodity-links`。新鲜主表为 21 个题材、221 条商品报价；商品
`$ZQDM` 是共享关系选择键而非唯一报价主键：燃料油和铂金各有两个报价行，最终
只有 219 个去重关系 ID。因此规范模型以 `quote_id` 标识报价行、以
`commodity_id` 选择 `zjtc4/5`，不会错误合并或让详情跳到另一报价。题材链按
`theme_id -> zjtc1/2`、`driver_id -> zjtc3` 校验；商品链按
`commodity_id -> zjtc4/5` 校验。七类模板均进入类型化覆盖。

2026-08-07 又将 `func_zxjx101_1`、`func_zxjx103_1` 和动态
`ggjx/<市场><代码>.jsn` 固定为纯 C++ `market announcement-signals`。主表标题
中的 `TXT:` 后缀是公告 PDF 地址，不属于标题；主表 `zf1/zf2` 与动态历史同名
字段语义不同，分别规范化为“近 3/10 日涨幅”和“公告前/后 3 日涨幅”。该边界
来自客户端 CFG 列名，不能只看原始字段名合并。最近公告的后验值为空时保留
`null`。三类模板均进入类型化覆盖。

`func_gznhg100/101/102` 和 `gxjty_zq_gznhg101` 是同一国债逆回购交收日历的
合并、沪市、深市和替代债券页面口径。2026-08-07 四表逐行一致：沪深各 9 个
品种。JSN 不带利率，客户端用宿主 `$NOW` 作为年化率并按
`本金 * 年化率/100 * SJTS/365` 计算毛收益。因此纯 C++ `market reverse-repo`
先读取 JSN 日历，再用一次公开 `0x054C` 批量快照补齐年化率。行情解码器新增
`204/1318` 回购代码的两位价格尺度，避免把 1.300% 输出为 130；无行情时仍返回
`schedule-only` 日历。四个静态模板均进入类型化覆盖。

`func_jysjk101_1` 与 `func_jysjk102_1` 是 `JYJGFX.sp` 的当前监管证券和历史
监管区间。两表共同字段为市场、代码、监管开始/结束日、起始价、市盈率和可空
异动公告 PDF；历史表额外提供结束价。客户端并不是简单展示原始字段：当前表
用宿主 `$NOW` 计算 `(NOW-price1)/price1*100`，历史表计算
`(price2-price1)/price1*100`。纯 C++ `market exchange-supervision` 对当前证券
执行一次公开 `0x054C` 批量快照，严格复现两种收益口径；快照失败时保留监管
记录并标为 `records-only`。该族包含基金和转债，不能把 `$ZQDM` 一律解释为
A 股。两模板均进入类型化覆盖。

`func_hylhb101/102/104` 分别是近 5 日、近一月和近半年活跃龙虎榜前 50；
`hylhb13801/13901/14001/<市场号><代码>.jsn` 返回所选证券逐次异动。2026-08-07
新鲜三表统计日均为 2026-08-06，各 50 行；三表交集 7 只。纯 C++
`market active-lhb` 同时读取三张主表生成周期摘要，按选择周期过滤/排序，并仅为
选中证券读取对应动态详情。`cs/ljb/ljs/zb/zf` 依次规范化为上榜次数、累计买入、
累计卖出、买卖比和区间涨幅；详情的 `date/type/zf/bje/sje/hsl` 保留逐次日期、
原因、涨幅、买卖额和换手率。名称中的 `HY` 是“活跃”缩写，不表示行业；该资源
也不携带营业部席位，不能与普通或机构龙虎榜合并。六种模板均进入类型化覆盖。

`func_gqgg101/102/103/104` 分别按行业、地区、整合预期和公司系组织国企改革
分组；每行的 `$S_ZQDM` 是完整成员串，`S_NUM` 是声明成员数，`$ZQDM` 是供
`gqgg/<分组ID>.jsn` 选择的内部键。2026-08-07 新鲜数据共 111 个分组、1,968 条
关系、899 只去重证券，声明成员数与解析数差异为 0。动态详情的 `skr/kgbl` 是
实际控制人/控股比例，`fqprice_d3/d5/d20/d60/JSYSP/NZJSP` 是供客户端以宿主
现价计算 3/5/20/60 日、近三月和年初至今收益的参考收盘价，`lzcs/LJSM` 是
领涨次数和逻辑。`func_gqgg106` 则是独立的 49 条重组预期表，包含大股东持股、
净利润、资本运作、说明、控制人、比例和日期。纯 C++
`market state-owned-reform` 保留这些关系边界，并用一次公开 `0x054C` 批量快照
复现阶段收益；行情失败时仍保留关系数据。现有本地国企改革概念块仅 100 只证券，
与该族重叠 69 只，不能合并成同一来源。五个静态模板加一个动态模板均已进入
类型化覆盖。

新增 15 种模板，16 个样本共 767 行。查询工具会用主表把内部键还原为
期货品种、IPO 年份/行业、涨价主题/事件及商品名称。例如 `SH601208`
可直接显示“聚丙烯”和“环氧树脂”两条关联上下文。

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family futures-statistics --family ipo-bond-issuance `
  --family price-limit-analysis --family commodity-themes `
  --family premium-stocks --download --summary

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --include-cfg `
  --match '^ipotj103/' --key 2026881015 --download --summary
```

债券发行人两张主表有效，但 CFG 声明的详情 base 在多个真实发行人键上
都返回空文件，当前没有加入动态模板。这一阶段统一清单为 598 个可下载资源；
全目录编目为 282 个文件、139,456 行、26,360,869 字节。

## 市场异动、ETF 资金和事件研究

继续恢复五个资源族：

| 资源族 | 主表 | 行数 | 字节 | 内容 |
| --- | ---: | ---: | ---: | --- |
| `market-anomalies` | 5 | 1,089 | 96,737 | 大盘异动、监管期和强势股区间 |
| `etf-fund-flow` | 2 | 4,799 | 514,635 | 个股/行业 ETF 规模及资金流 |
| `event-research` | 8 | 863 | 573,469 | 热点、公告、专题和新闻事件 |
| `economic-indicators` | 1 | 72 | 7,971 | 经济/商品指标最新值 |
| `industry-region-logic` | 2 | 14 | 7,117 | 行业/区域机会组 |

其中 15 张为新增主表，累计验证数增至 163。七种动态关系为：

```text
经济指标 -> jjzb1/2 -> 历史数值、关联行业和股票
股票 -> ggjx -> 精选公告历史与公告前后表现
行业/区域机会组 -> ydyl1 -> 股票及逐股逻辑
专题 -> ztxx -> 完整信息时间线
强势股区间 -> ygzl -> 逐日涨停原因和市场温度
新闻/部委事件 -> sjqd -> 关联股票
```

10 个动态样本共 676 行。查询工具会把指标、机会组、专题、强势区间和
新闻事件内部 ID 还原为名称；单票可同时命中主表成员串和动态详情记录。

2026-08-07 进一步对 `JJZB.sp` 全量交叉验证后，该族已从“动态样本”
提升为纯 C++ `market economic-indicators`：

- `func_jjzb101_1` 是 72 项指标目录，不是基金榜；
- `jjzb1/<indicator_id>.jsn` 是日期—数值历史；
- `jjzb2/<indicator_id>.jsn` 是 `$ZQDM/$SC/hy` 关联证券；名称和当前行情
  是客户端宿主列，由工具用公开 `0x054C` 另行合并，不冒充 JSN 原字段。

对 72 个指标的 144 个动态资源执行全量探测，历史与关联证券两族均为
72/72 非空。这也说明动态键必须使用“指标 ID”，不是证券代码。

同日完成的战略主题纯 C++ 迁移把 24 张静态主表和 `zttzty/<theme_id>` 动态
模板全部登记为类型化资源。随后又将 `func_jgcg108` 知名私募、
`func_jqgz103` 近期大比例解禁、`func_ydyl101/102 + ydyl1` 行业区域机会及
`func_rdhs101/102` 爆炒复盘登记为类型化资源。随后又补齐 `ztxx/xwlb/dpyd/sjqd`、
`rzrq3..8`、`hsgtcg2/ggthy/ggthy1` 和 `gdzjc1`。最新覆盖审计为 332/616，
340 个已下载文件全部可回溯且解析错误为 0；已下载非空模板已无通用独占入口。

随后 `QXFA` 六阶段定向增发表与独立 `func_qxfa501_1` 员工持股计划表分别进入
`market futures-issuance` 和 `market employees`；同时补登记已实现的
`func_yysg101_1` 要约收购。最新覆盖为 342/616，361 个已下载文件全部匹配、
解析错误为 0，已下载非空模板的 generic-only 数量仍为 0。

2026-08-07 的下一轮高价值探测下载 25 个静态候选，其中 23 个非空。先将语义
最完整的 `func_ggrl102/103/104/105` 四表固定为 `market hk-events`，覆盖分红、
权益披露、沽空和上市申请。覆盖随之达到 346/616；本地现有 384 个文件全部被
模板识别且解析错误为 0。其余 19 个新下载模板仍明确标为 generic downloaded，
没有仅因“已下载”就冒充类型化能力，将作为后续特殊事项、REITs、重大事项、
ETF 行情与新股日历等队列继续推进。

随后固定 `func_agtl101_1` 吸收合并、`func_agtl102_1` B 转 H 和
`func_cdgc101_1` 主要指数市值管理预警。三表合计 179 条，预警的 20 日、一年
高点回撤和同时触发分别为 90/62/20；吸收合并中空 `$SC1` 的 `834082` 通过
证券目录恢复为北交所。覆盖达到 349/616；384 个文件全部匹配、解析错误为 0，
本轮已下载通用独占降至 16 个。下一优先队列以数据量和独立语义排序为 ETF 行情、
重大事项、分红募资统计、新股日历和 REITs，不把它们仅登记为“已下载即完成”。

随后把 `func_etfhq101`、`gxjty_etfjj103/104` 与 `func_reits101/102` 五表固定为
纯 C++ `market exchange-funds`。ETF 多周期表现 1,619 行，货币 ETF 套利 27 行，
货币基金收益 13 行，已发行 REITs 93 行；待发行 REITs 的一行全空占位明确
规范化为 `availability=empty`。市场号 34 的收益表以证券目录优先、代码前缀
回退恢复为深市 3 只和沪市 10 只，保留 `source_market_id=34` 供审计。客户端
5/20/60 日、月初/年初收益、理论净值、溢价和套利年化公式均由 C++ 本地复算，
公开 L1 只按需补充筛选结果。最新覆盖为 354/616；384 个文件全部匹配、解析错误
为 0，已下载通用独占剩余 11 个。下一队列为重大事项、新股日历、分红募资统计
及港股表现。

下一轮没有把 `DSJTX/XGRL` 另造为重复页面，而是扩展既有纯 C++
`market calendar`。`func_dsjtx101` 的 `$SC1/$ZQDM1` 是被提醒证券，`$ZQDM`
只是当批事件序号；`func_xgrl102` 是过去一年次新股表现，`mjzj/cmzj` 已是元，
`ZF1/2/3` 分别是上市至今、近 5 日和近 10 日涨幅。另对未下载的
`func_xgrl103/104/105` 做元数据探测后确认三表均非空并完成原子下载；从全部
标题代码分布确认它们分别是科创板、创业板和新三板资讯，而非三张同义新股表。
四类资讯统一拆分 `TXT:` 标题/正文、清洗显示文本、提取链接，并以板内代码或
名称建立单票关系。新增六模板后覆盖为 360/616；本地文件增至 387 个、涉及
332 个模板，全部匹配且解析错误为 0，已下载通用独占剩余 8 个。

最后 8 个已下载通用独占模板随后固定为纯 C++ `market curated-data`：传媒娱乐、
低估值袖珍股、分红募资、拟回购月度统计、高分红、港股多周期表现、转融券余额高
和破净国企股合计规范化 1,934 行、1,754 只去重证券。CFG/SP 中的万元、亿元、
万股、百分比及派生公式均在专用解析器中显式复算；港股表现表的市场号 49 记录
按合法港股证券保留。转融券表仅 3 行余额非空且最新日期为 2024-09-30，因此作为
历史快照返回，不冒充当前余额。至此类型化覆盖为 368/616；本地 387 个文件全部
匹配、解析错误为 0，已下载模板的通用独占入口为 0。

随后从 `TBGZ.sp` 探测特别关注族。`func_tbgz102/103/104/106/110` 当前均有
真实资源，分别是股权分散、可能成为 `*ST`、摘星摘帽分析、被立案调查和商誉
风险；`func_tbgz101/105/107/109` 的元数据当前不存在，不能写成“正常空表”。
五个非空资源已下载并固定为纯 C++ `market special-attention`，合计 1,146 行、
1,019 只证券。立案表使用 `$SC1/$ZQDM1` 关联证券，`$ZQDM` 只是事件键；
摘星摘帽净利润源字段为万元，商誉和 `*ST` 财务字段直接为元，股东户数另显式
给出万户显示换算。覆盖由此达到 373/616；本地 392 个文件涉及 337 个模板，
全部匹配、解析错误为 0，已下载模板的通用独占仍为 0。

下一轮优先探测 `JJTJ.sp` 的基金统计族，9 个模板全部有 MD5 和非零长度。下载后
确认新发基金 244 行、基金分红 581 行、股票型基金收益 2,723 行、基金市场规模
及走势各 20 行、ETF 市场规模和申赎走势各 23 行、ETF 周度统计 51 行、新上市
基金 126 行。九表已固定为纯 C++ `market fund-statistics`：场外市场号 33 保留
为 `fund/FUND`，不映射为深沪市场；CFG 标为亿元/亿份的列同时输出原值和乘以
`1e8` 的元/份字段；分红 `PXBL` 只保留原始比例及 `FHSM` 正式说明，不臆测现金
单位。覆盖达到 382/616；本地 401 个文件涉及 346 个模板，全部匹配、解析错误
为 0，已下载模板的通用独占仍为 0。

随后完成三组高收益资源。`HYJYFX101/102/103` 分别是银行、券商、保险专用经营
表，当前 42/49/6 行；金额经平安银行、招商银行、中信证券、中国平安样本校准为
元，比率源值为小数。纯 C++ `market specialized-metrics` 保留源小数并增加明确
百分比字段，券商月营收和月净利润同比按本期/上年同期原值复算。

`JZFX` 的 26 张阶段表及停复牌、次新股两表全部非空。`market benchmark-analysis`
把 2009-08-04 至今整理为十三个客户端固定指数地标阶段，区分个股、行业、市场及
相对超额；需要 `$NOW` 的“距今”宿主计算列不伪造。API 按视图和阶段选择性加载，
例如最新个股阶段只读取 `func_jzfx216_1.jsn`，网页可用 `include_raw=0` 降低传输量。

`GSRL201/202/203/204/208/209` 与既有 206 合并进 `market calendar`；205 下载体只
含空壳行，归一后为 0，207 配股元数据仍缺失。公司事件由单一分红转增扩展为首发
上市/发行、停复牌、特别处理、分红转增、增发和股东大会，共 3,149 条。至此覆盖
达到 420/616；439 个下载文件涉及 384 个模板，全部匹配、解析错误 0、已下载模板
通用独占 0。

随后探测 `ZQBG/CWZB/JQGZ`。`ZQBG` 九张非空表全部并入纯 C++
`market company-changes`：证券/公司更名 345/102 条，境内/港股/新三板指数调整
1,097/158/1,796 条，重大股权、实控人、行业、股权转让 238/129/1,118/651 条，
总计 5,634 条。历史新三板有 8 行 `$SC` 为空，但资源固定市场为 44，归一化时
保留为 `neeq`；客户端用 `$NOW` 计算的距今收益没有写入静态事实。

`CWZB101/102/104/105/107` 是五个现存 A 股板块的同构财务横截面，共 5,539 行。
`SZ` 虽在 CFG 显示为亿元，原值经农业银行等总股本/价格样本校准为客户端格式器的
千万人民币单位，因此同时输出源值和乘 `1e7` 的 `market_cap_yuan`。PE/PB/PEG、
增长率、ROE、毛利率、费用率、机构持仓和股息率保持源百分点；合同负债金额为元，
同比按本期/上期复算。纯 C++ `market financial-screen` 支持七项排序和单票反查。

`JQGZ101/104/111` 分别固定为绩价背离、涉外经营和 ST 业绩预盈，合入
`market recent-watch`。涉外收入/成本/利润源单位为亿元并换算为元；绩价背离和
ST 公告后的涨幅只是资源生成时快照。`JQGZ103` 已由 `market unlocks` 的
`recent-large` 视图覆盖。本批把覆盖提升到 437/616；456 个下载文件涉及 401 个
模板，解析错误为 0，已下载模板的通用独占再次归零。

2026-08-08 对当前 108 个静态未类型化模板执行只取长度/MD5 的原生探测，确认
73 个非空、35 个零长度。随后 `WJCG101`、`LXSNZZ101`、`TQWCLR101` 与
`QXFA101` 四张小表按 CFG 固定为 `market financial-insights` 的稳健成长、连续
质量增长、利润突破和当前分红方案视图，共新增 1,016 行。覆盖更新为 514/618，
533 个下载文件全部匹配、解析错误与已下载 `generic-only` 均为 0；静态源未包含
的当前价、三年涨幅和股息率不由其他字段推断。

随后从 179 个未类型化、尚未下载模板中探测 19 个高价值候选，14 个非空并经
纯 C++ 下载器完成 MD5 校验。`AGHQ101` 经 CFG 证据确认是全 A 股多周期表现，
不是指数列表；`CWNSCG/JGXC/JGZD` 形成牛散—机构增持—股东收缩—调研链路；
八个财务模板形成特色财务线索；`GDR101` 与 `JJRL301` 分别形成跨市场映射和
基金事件日历。14 个下载模板全部拥有固定业务命令/API，覆盖升至 451/616；
470 个文件涉及 415 个模板，解析错误与已下载通用独占均为 0。

### 美股/北证 IPO 与知名自然人动态持仓

`MGRL101/102` 的发行额和股数以百万美元/百万股下发，`MGXG101/102` 则以
美元/股基础单位下发。纯 C++ `market calendar` 按申请、排期、已上市、待上市
保留四种 phase/source，再提供统一基础单位；排期与已上市记录只做关系对账，
不会因代码相同而覆盖预计或实际日期。`SBXG101` 保持北交所申购、缴款、退款、
询价、发行结构、中签率和上市日的完整字段，`SBXG102` 仅标记为伴随资讯。

`NSCG101` 主表的 `$ZQDM` 是投资人 ID，不是证券代码。只有用户选择投资人后，
服务才展开 `nscg/$$$ZQDM$$.jsn`；明细中的 `$SC/$ZQDM` 才是证券身份。本期减
上期的股数、市值和比例按 CFG 复算，无上期值时保持空值。该动态模板已纳入
覆盖审计；当前 618 个模板中 502 个类型化，521 个下载文件全部匹配且已下载
通用独占为 0。

### 增量发现基线

`recon jsn-discovery` 在模板覆盖之外增加时间维度：扫描每个具体 `.jsn` 的
SHA-256、字节数、行数和字段集合，以 `output/tdx-jsn-discovery-baseline.json`
为基线输出 `added/changed/removed/unchanged`。变更记录还包含新增/移除字段、
每列最多 64 个值的数值/整数/日期/URL/HTML 画像，以及具体动态路径匹配到的
模板和 `SC/ZQDM` 等键。`SC=47` 的期货样本 `qhtj2/47IFL8.jsn` 会正确拆为
市场 `47`、代码 `IFL8`，不会按普通证券市场号误切。

```powershell
tdx-tool recon jsn-discovery --root C:\new_tdx --jsn-root output\tdx-jsn
tdx-tool recon jsn-discovery --root C:\new_tdx --jsn-root output\tdx-jsn `
  --capture-baseline
```

首次生产基线含 340 个文件；立即复扫为 340 个 `unchanged`，模板识别和类型化
覆盖均为 340/340，解析错误为 0。固定 API 是 `/api/v1/jsn/discovery`；GET 只读，
捕获基线必须 POST `action=capture` 并携带
`X-TDX-Action: jsn-discovery-baseline`。服务缓存最近一次全量画像；显式
`refresh=1`、下载新资源或捕获基线会失效缓存。

### CFG refunit 动态候选队列

`jsn candidates` 继续解析所有 CFG/XML 单元的 `id/file/refunit`。一个动态详情
模板只有同时满足以下条件，主表行才会进入候选队列：

1. 详情 `file` 能映射到已知动态模板；
2. `refunit` 能解析到一个具体静态 JSN 主表；
3. 详情模板与主表已绑定到同一类型化 C++ 业务域；
4. 主表行实际含有模板要求的 `$SC/$ZQDM`；
5. 展开后的 ASCII 路径满足 709 文件信息请求的 40 字节上限。

因此该命令不会把全部证券与 75 个动态模板交叉组合。默认只生成本地缺失项且
完全离线；`--probe` 必须同时显式提供 `--family`，单次上限为 25，只查询大小和
MD5，绝不下载。真正下载仍复用 `jsn download` 的显式 `--download`、MD5 校验和
原子替换边界。

```powershell
tdx-tool jsn candidates --root C:\new_tdx --jsn-root output\tdx-jsn
tdx-tool jsn candidates --root C:\new_tdx --jsn-root output\tdx-jsn `
  --family gqzy --probe --max-probes 10
```

当前 75 个动态模板中 73 个可建立 CFG `refunit` 关系。`gqzy` 单族由 5 张已下载
同业务主表生成 2,474 个具体候选；首批 10 个本地缺失项探测全部非空，下载后
合计 361 行；每个文件 16 列，共出现 160 次新增字段，增量审计准确报告 10 个新增文件，350/350 文件
仍全部匹配且解析错误为 0。固定只读 API 为 `/api/v1/jsn/candidates`；Svelte
JSN 页可切换“资源变化/动态候选”，候选仍按单资源执行探测和确认下载。

2026-08-07 后续又对 20 个缺样本静态模板做只探测，16 个返回非空。确认下载
`list/func_yysg101_1.jsn` 后，本地目录增至 351 个资源；该表的 169 行已按安装
CFG 的字段和单位提升为 `market tender-offers`，而不是继续停留在通用目录。
同轮非空候选中的 `gxjty_zq_kzzsy101_1` 已完成语义审计并并入
`market convertible-bonds view=pricing`；A/B 股日历、股改/限售日历、定增和
财报披露表仍作为候选，不因文件名直接暴露固定 API。

`gxjty_zq_kzzsy101_1` 当前为 309 行、43 个服务端字段。CFG 同时引用的
`$NOW3/$BONDAI/ZGXJ` 等宿主列不在 JSN 内：实现分别从公开 L1 取得债券净价和
正股现价，并按条款中的上一/下一付息日与当期票息率，以招募说明书明确的
`IA=B*i*t/365` 复算应计利息。全价、转股价值/溢价、YTM、纯债价值及双低值放在
独立 `valuation` 节点，完整原始行放在 `raw`，从结构上避免混淆源字段与派生列。

`list/kjhz_kjhzsy201_1.jsn` 则是另一种补充表模式：它不独立构成可转债
目录，而是按债券代码补齐 2 只可交换债的面值、换股价/期、到期日、剩余期限、
评级和收益。因此它被建模为 `market convertible-bonds` 的可选第七源，
补充请求失败时仍保留原有六表结果和独立 `master_errors`。

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family market-anomalies --family etf-fund-flow `
  --family event-research --family economic-indicators `
  --family industry-region-logic --download --summary
```

ETF 资金、经济指标、公告和监管期更新到 2026-07-31/08-01；强势股
区间主表停在 2023 年，只作为历史样本。当时统一清单为 605 个资源，离线
编目为 307 个文件、146,462 行、27,571,449 字节。

## 业绩预告、主动基金、机构席位与评级

继续从 `.sp` 页签名称、主表 `$ZQDM` 和详情 CFG 的 `refunit/file` 关系
交叉验证，新增四个可更新资源族：

| 资源族 | 主表 | 行数 | 字节 | 内容 |
| --- | ---: | ---: | ---: | --- |
| `earnings-forecast` | 1 | 31 | 3,673 | 行业+报告期的预告类型和公司数 |
| `active-fund-holdings` | 1 | 1,273 | 155,013 | 主动基金增持股票和持仓变化 |
| `institution-seat-activity` | 4 | 5,142 | 369,969 | 周/月/三月/一年机构席位排行 |
| `stock-industry-ratings` | 2 | 655 | 44,149 | 港股评级与行业评级榜 |

八张主表共 7,101 行、572,804 字节。详情路径由正例和不带关联前缀的负例
共同确认：

```text
yjyg/<行业代码+报告期>.jsn
zdjjzczc/<市场号><股票代码>.jsn
lsyd22801..22804/<市场号><股票代码>.jsn
ggpj/<市场号><港股代码>.jsn
hypj/<市场号><行业代码>.jsn
```

`lsyd` 的四个控制单元来自 `JGLHTJ.sp`，依次表示最近一周、一月、三月、
一年。首批 8 个详情样本共 517 行、307,729 字节：业绩预告给出公司净利润
区间、同比、正文和原因；主动基金详情反查基金名称、持仓市值/数量、净值
占比；机构席位详情给出异动类型与买卖额；港股和行业评级给出机构、当前/
上次评级、变化、目标价或研报理由。

2026-08-07 又确认 `list/func_cbpl101_1.jsn` 是独立的 A 股全市场业绩预告
页面，不是披露日历：1,805 行、18 列，深/沪/北分别 933/838/34。它与
`yjyg/88000120260630.jsn` 的动态全 A 详情不是同一刷新截面：静态表当前期
1,796 条，动态表 1,846 条，静态表另含 5 条三季报和 4 条年报预告。因此实现
把它加入 `market forecasts view=latest`，保留 50 条当前期差异与 9 条未来期，
不以任一来源覆盖另一来源。CFG 的 `zj1/zj2` 是直接显示的同比百分比；动态表
`zj3/zj4` 则是小数，归一化时分别采用直接值和乘 100，避免 100 倍尺度错误。

## 定向增发六阶段生命周期

2026-08-07 从 `QXFA.sp` 与 GBK CFG 反查并下载了六张有效定向增发表：
`func_qxfa201/202/301/302/402/601_1.jsn`。它们不是 IPO、`0x000F` 单票股本事件
或 `DBLJJ` 解禁日历的重复数据，而是同一业务在不同生命周期的滚动窗口：

- `201/202`：已实施且仍锁定 / 已解锁，含获准、实施、上市、解锁、发行价、
  实际募资和发行股数；
- `301/302`：推进中的董事会/股东大会方案，以及已终止、中止审查或延期方案；
- `402`：已实施但不在两张解锁窗口中的记录；
- `601`：注册生效方案，含注册公告 PDF、估值快照与变更说明。

当前六表共 1,339 条、覆盖 1,008 只证券；生命周期计数依次为 267、215、411、
80、288、78。纯 C++ `market futures-issuance --section placements` 保留每条
`source_resource` 和 `raw`，金额保持“万元”、股数保持“万股”，API/网页再按
展示需要换算；同一股票的多期方案和跨阶段记录不会被错误压缩成一条。

```powershell
# 更新八张主表
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family earnings-forecast --family active-fund-holdings `
  --family institution-seat-activity --family stock-industry-ratings `
  --download --summary

# 查询一只股票时自动合并已经下载的主表和详情
python doc/90-scripts/query_tdx_jsn.py `
  --root C:\new_tdx --market 1 --code 601666
```

不要把 `ggpj` 泛化成 A 股评级：当前主表 625 行全部使用市场 `31`，页面名
明确为“港股评级”。也不要把 `hypj` 当普通股票明细，它绑定 `881xxx`
行业代码。此前探测的三个带 `_1` 可转债文件名为空，因此本阶段没有把
猜测路径加入清单；后续已从客户端 XML 找到不带 `_1` 的真实主表，见下节。

本阶段结束后统一清单为 613 个资源；离线编目为 323 个资源、154,080 行、
28,451,982 字节。

## 可转债回售、赎回与转股价调整历史

`tdxzq_kzz_hstk/shtk/xztk.xml` 给出了此前遗漏的真实主表路径：

```text
list/func_kzz_hstk201.jsn
list/func_kzz_shtk201.jsn
list/func_kzz_xztk201.jsn
```

它们不带 `_1`。同三个 XML 的注释从表还直接保留实际详情模板：

```text
kzz_hstk/<市场号><转债代码>.jsn
kzz_shtk/<市场号><转债代码>.jsn
kzz_xztk/<市场号><转债代码>.jsn
```

转债概览、转股/回售/赎回/到期进度、逐年利率和三张条款主表已合并为
`convertible-bond-terms` 资源族：

| 主表 | 行数 | 字节 | 内容 |
| --- | ---: | ---: | --- |
| `kzz_kzzsy201_1.jsn` | 316 | 201,999 | 转债、正股、转股价、余额、评级和付息序列 |
| `func_kzz_tkjd201.jsn` | 318 | 98,063 | 转股、赎回、回售和到期进度 |
| `func_kzz_lltk201.jsn` | 318 | 34,303 | 逐年票面利率和补偿利率 |
| `func_kzz_hstk201.jsn` | 322 | 51,929 | 回售条款和当前触发进度 |
| `func_kzz_shtk201.jsn` | 322 | 45,687 | 赎回条款和当前触发进度 |
| `func_kzz_xztk201.jsn` | 322 | 73,727 | 下修条款和当前触发进度 |

六表共 1,918 行、505,708 字节。正负例验证表明详情必须使用转债自身的
市场+代码：`kzz_hstk/1110076.jsn`、`kzz_shtk/1113660.jsn`、
`kzz_xztk/1110076.jsn` 均存在，去掉最前面的市场 `1` 均为空。

三个样本分别返回 2 次回售、1 次赎回和 15 次转股价调整，合计 18 行、
1,226 字节。赎回详情在线表头实际使用 `WSHSL`，而 CFG/XML 写作
`WHSSL`；编目工具保留真实字段名，并复用 CFG 的“未赎回数量”说明。

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --family convertible-bond-terms --download --summary

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --include-cfg `
  --match '^kzz_(hstk|shtk|xztk)/' `
  --market 1 --code 110076 --download --skip-missing --summary
```

查询转债 `1/110076` 实测返回 8 个关联资源、23 条记录；查询其正股
`1/600521` 也会通过概览表中的 `$SC1/$ZQDM1` 带出回售和转股价调整历史。
该阶段统一清单为 616 个资源，离线编目为 326 个资源、154,098 行、
28,453,221 字节。

## 远端可用性、统一主题与债券资料库

仅按模板名称排序会让远端零长度文件反复占据缺口榜。纯 C++ 下载器现支持：

```powershell
tdx-tool jsn download --root C:\new_tdx --probe --skip-missing `
  --resource-file output\remaining-static-resources.txt `
  --report output\tdx-jsn-remote-manifest.json

tdx-tool recon jsn-variants --root C:\new_tdx `
  --jsn-root output\tdx-jsn `
  --availability-report output\tdx-jsn-remote-manifest.json `
  --output output\tdx-jsn-variants.json
```

`--report` 原子写入路径、状态、长度和 MD5，不下载正文。当前 99 个剩余静态
模板全部命中清单：64 个非空、35 个零长度，非空合计 266,718,311 字节。
审计把确认非空项排在未知项之前，并从 `high_value_gaps` 排除确认零长度项；
远端存在不等于完成业务语义，不能因此增加 `typed_template_count`。

主题投资页还有一组不同于 26 类战略主题的 `ZTTZ102—106` 快照：

| 来源 | 主表 | 当前快照数 |
| --- | --- | ---: |
| 区域经济 | `list/func_zttz102_1.jsn` | 109 |
| 国企系 | `list/func_zttz103_1.jsn` | 75 |
| 公司系 | `list/func_zttz104_1.jsn` | 29 |
| 统一主题 | `list/func_zttz105_1.jsn` | 850 |
| 参股持股 | `list/func_zttz106_1.jsn` | 39 |

五表合计 1,102 条快照、927 个去重主题 ID；它们不是父子树，同 ID 在不同来源
可以代表不同刷新截面。`zttz/<ID>.jsn` 是当前逐股纳入说明，
`zttz1/<ID>.jsn` 是主题指数历史。`market theme-library` 保留来源身份，动态
详情非空时覆盖所选主题的静态成员。CFG 中的 `file="zttz"` 与
`file="zttz1"` 也已展开为动态模板；随后下载的债券分类表同样全部可识别。

债券资料页 `ZQ_XYPJ.sp` 和 `ZQ_LLLX.sp` 分别定义 9 个信用评级桶、6 个利率
类型桶；债券分类页另定义国债、地方债、公司债、企业债、私募债、资产支持
证券和次新可转债 7 个类别。`market bond-reference` 懒加载所选的 22 张主表，
恢复债券与主体评级、
利率类型、起息/到期/付息日期、剩余年限、当前票息、付息频率、面值、完整及
剩余票息序列。票息序列的 `0.033` 按比例归一为 `3.3%`，而当前票息字段
`1.850` 保持 `1.85%`；两种尺度不能混用。AA+ 当前为 941 条、国债 450 条，
适合作为默认小桶；AAA、固定利率和地方债约 28—37 MB，只在用户选择时读取。
次新可转债当前 57 条，并从同一静态行恢复正股、转股期、转股价以及下修、
回售、赎回触发比例；它是分类快照，不替代公开 L1 可转债定价。

加入上述动态模板、22 张债券表、转融通总量表、三张一致预期价格阶段榜，以及
优先股、失信被执行和小盘双筛选六张资源后，当前清单为 620 个模板，其中 553 个
具有类型化 C++ 业务语义、67 个仍为通用候选；559 个已下载文件涉及 502 个模板，
全部匹配，解析错误和已下载 `generic-only` 均为 0。

新增六张表的集合口径不能只按文件名推断：`func_xpcz101_1` 是 33 只股票的
小盘成长母表，`func_xpcz103_1` 和 `func_xpcz104_1` 分别是其中 6 只创业板、
13 只科创板的精确投影，三表不能拼接去重后当成同级数据源。`func_xszgp101_1`
则是独立的 46 条小市值专业机构增持表，与既有 `JGXC101` 只重叠 4 只，信息增益
较高。`func_yxg101_3` 是 55 个优先股发行记录，需同时保留优先股代码和 33 只
基础 A 股；`func_sxbzx101_1` 是 9 条失信被执行事实，不能生成不存在的安全分。

## 单证券机构详情与同股东持股

`func_cgfx100_1/2.cfg` 只写 `file="cgfxmx1/2"`，但真实路径规则与高管
详情一致，使用市场号和六位代码直接拼接：

```text
cgfxmx1/<市场号><六位代码>.jsn
cgfxmx2/<市场号><六位代码>.jsn
```

第一张表是按报告期和机构类别汇总的家数、股数、占流通/总股本、市值及
较上期变化；第二张表是十大流通股东名称、类型、当前持股、比例和增减。
中兴通讯样本为 20+10 行，爱丽家居为 15+10 行，全部通过 MD5。

第二张表的 `gdjc` URL 还包含 `gdid/tdxid` 股东键。页面使用
`CWServ.tdxf10_gg_gdyjcgmx` 的两种参数：

- `gdjc`：一名股东在各股票中的代表记录；
- `gdjcxq`：该股东在指定股票上的逐报告期持仓。

```powershell
# 更新一只股票的两张动态机构详情
python doc/90-scripts/update_tdx_institution.py `
  --root C:\new_tdx --download-details --market 0 --code 000063

# 选择该股票详情中出现的高盛，反查跨股票覆盖，并展开爱丽家居逐期持仓
python doc/90-scripts/update_tdx_institution.py `
  --root C:\new_tdx --download-holder-history `
  --holder QF000034 --holder-stock 603221 --compact
```

工具按股东 ID/名称选择，默认一次最多请求 10 个股东，不会自动遍历全部
股东或对 1,579 只关联股票逐一展开。

完整 IDA 证据、资源清单和在线验证记录见
[reqformat=11 阶段归档](../99-log/2026-07-31-reqformat11-jsn-resources.md)
和[战略主题主从链归档](../99-log/2026-07-31-strategic-theme-logic.md)。
CFG 隐藏资源的样本、字段和有效性记录见
[CFG 隐藏主表归档](../99-log/2026-07-31-cfg-jsn-hidden-resources.md)，后续资源族
和龙虎榜明细见[机构与龙虎榜隐藏资源归档](../99-log/2026-08-01-institution-lhb-hidden-resources.md)。
单票机构详情和同股东持股链见
[机构持仓动态明细链归档](../99-log/2026-08-01-institution-holder-detail-chain.md)。
公司行动、交易统计和股权关联链见
[公司行动与交易统计归档](../99-log/2026-08-01-corporate-actions-trading-resources.md)。
融资融券、沪深港通和事件关联链见
[两融与沪深港通归档](../99-log/2026-08-01-margin-stock-connect-chains.md)。
期货、发行统计、涨跌停和商品主题链见
[期货与市场事件归档](../99-log/2026-08-01-futures-issuance-market-events.md)。
竞价、异动、ETF 资金和事件研究链见
[竞价与事件研究归档](../99-log/2026-08-01-auction-anomaly-event-chains.md)。
