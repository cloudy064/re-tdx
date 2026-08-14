# 脚本与测试

> 可运行的 Python 脚本、CLI 示例、测试和安全边界。入口索引。

本目录保存可重复的只读侦察脚本和已经验证的公开协议实现。

## 脚本规范

- 所有脚本从仓库根目录运行（`python doc/90-scripts/xxx.py`）
- 测试文件使用 `test_*.py` 命名
- 私密材料（session、pcap、login frame）只能从仓库外加载
- 所有脚本必须支持 `--help` 参数
- 网络请求默认 dry-run / gate-closed，需要显式参数才能发出真实请求

## 当前脚本

- `recon_install.py`：只读盘点实际安装目录，记录顶层布局、目标二进制、
  SHA-256（可选）、PE 依赖和导出表。脚本不读取配置内容或用户数据。
- `ida_network_inventory.py`：在 IDA headless 中提取网络 DLL 的导入/导出、
  socket/IOCP 交叉引用、相关字符串和工厂函数伪代码，并把 JSON 打到标准
  输出。
- `ida_module_inventory.py`：通用 PE 数据库报告，导出入口向下展开两层
  调用树并附 Hex-Rays 伪代码，同时记录相关字符串和命名符号。
- `ida_dynamic_module_xrefs.py`：在主程序或中间 DLL 中定位核心 DLL 名称与
  导出名字符串，提取动态加载调用点、调用者和伪代码。
- `ida_address_xrefs.py`：给定一个或多个 IDA 地址，提取引用这些地址的
  函数及其伪代码；适合追踪导入槽或动态解析后的函数指针。
- `ida_immediate_xrefs.py`：按整数立即数或结构字段位移查找指令，汇总命中
  函数及伪代码；适合追踪消息操作码和未恢复类型的对象字段。
- `ida_function_disasm.py`：给定函数地址，导出地址、机器码字节和完整 IDA
  反汇编；用于 Hex-Rays 丢失可变参数或窄字段访问时核对实际压栈、偏移，
  也可为动态探针生成无重定位的入口签名。
- `ida_data_inspect.py`：给定 IDA 地址或符号名，只读导出原始字节、DWORD、
  可解码 C 字符串及引用函数；适合核对字体默认表、配置全局和其他静态数据，
  不推断配置加载后的运行时值。
- `extract_tcalc_formulas.py`：按 SHA-256 识别 TCalc 版本，通过 PE 节表
  离线导出内置技术指标、条件选股、专家系统和五彩 K 线元数据；不加载
  DLL，也不读取用户公式文件。
- `dump_tcalc_runtime.py`：核验运行中 TdxW/TCalc 指纹和接口对象后，仅
  调用只读 getter，导出已初始化的系统、缺省和用户公式列表。
- `extract_tdx_blocks.py`：从公开行情缓存离线导出通达信行业、研究行业、
  概念、风格、指数板块及其证券成员；不读取用户自定义板块。
- `build_tdx_blocks_html.py`：把全量板块数据字典编码，预生成板块→证券和
  证券→板块索引，经 gzip 压缩后嵌入一个可直接双击打开的离线 HTML。
- `update_tdx_blocks.py`：面向最终用户的一键更新入口；自动寻找通达信、
  取得稳定数据快照并原子替换 HTML，可选同步输出 CSV 和打开页面。
- `extract_tdx_minute.py`：读取证券或板块指数的 `.lc1` 一分钟 OHLCV
  缓存，按交易日导出 CSV/JSON，或生成可双击打开的单文件 K 线图。
- `download_tdx_minute.py`：从 `newhost.lst` 读取并轮换 `7709/TCP`
  行情主站，分页下载证券或板块指数的 1 分钟线，增量合并本地 `.lc1`
  快照，并可直接生成单文件 HTML。无第三方 Python 依赖，联网必须显式
  指定 `--download`。
- `tdx_market_snapshot.py`：`0x054C` 批量行情快照的请求、变长记录解析、
  无效代码替代记录过滤和跨主站断点续批协议层。
- `tdx_market_depth.py`：`0x0547` 五档/增量行情的请求、响应异或、变长记录
  和买卖五档解析；可直接计算个股买一/卖一金额，联网必须显式
  `--download`。
- `tdx_category_quotes.py`：`0x054B` 服务端分类排序；支持封单额、开盘
  抢筹、涨速、成交额等 16 种排序，`--all-sealed` 可连续翻页直到第一条
  非封板记录，直接生成完整封板榜。
- `tdx_auction_series.py`：`0x056A` 开盘/收盘集合竞价逐点序列；解析虚拟
  价格、匹配量、买卖未匹配量并自动拆分 09:15—09:25 与 14:57—15:00。
  `selector=0` 只取开盘，非零值取开盘和收盘，支持逐点分页。
- `tdx_trades.py`：`0x0FC5/0x0FC6` 当日/历史公开 L1 成交明细；自动处理
  服务端 1,800 条单页上限和反向分页，按时间正序输出逐条及分钟聚合，
  单独识别 09:25、15:00 集中撮合与深市 `status=5` 盘后定价成交。
- `tdx_level2.py`：已确认 Level2 结构的离线协议工具；生成 `1364/1374`
  逐笔成交/逐笔委托 26 字节请求，解析游标、单字节 XOR 和变长记录，也能
  把 `4655/4671/4680` SDK JSON 转为逐笔、买卖一队列和五/十档盘口；
  还能解析 `1803/18031` 的多档盘口/选中价位委托队列固定体、`tpbus`
  `111/112` 的行情深度/买卖一队列原始体，并汇总探针 JSONL 中的
  `LX/data_type/push_type` 候选关系；`inspect-protobuf` 可对未知 schema 的
  `113/114/116` 只报告字段号、wire type 和有界样本。
  工具不实现登录、权限票据或网络发送。
- `capture_tdx_level2.py` + `frida_tdx_level2_probe.js`：对已经运行且具有
  合法行情会话的客户端做被动、限量 JSONL 观察；捕获 SDK 请求/回调、
  `1801/1802` 内部记录、`1803/18031` 有界结构、
  `FastHQ.Subscribe.LX`、`111/112` 推送及发布前的 `113/114/116` PB。
  启动器先校验主程序 SHA-256，
  探针再校验模块尺寸和函数机器码；
  默认只预检，必须显式 `--attach`，且不会创建会话或发送请求。
  可再显式添加 `--eventbus-registry`，调用只读 EventBus 单例 getter，
  快照现有主题/观察者并记录后续 `ObserveEvent/ObserveEvent2` 注册；该开关
  仍不会发布事件，默认关闭。
- `tdx_stats.py`：通过 `0x06B9` 分块更新 `zhb.zip`，安全解析其中的
  `tdxstat.cfg/tdxstat2.cfg`；也可读取本地缓存，输出流通股本、历史封单、
  竞价金额和涨停统计。
- `update_tdx_market.py`：把本地板块树与全部唯一成分股、板块指数行情
  合并，预计算广度、平均涨跌、领涨/拖累和成交额，生成一个可双击打开
  的 `tdx-market.html`；无第三方依赖，联网必须显式指定 `--download`。
- `inventory_tdx_cloud_features.py`：只读扫描 `T0002/cloud_cfg`，恢复
  通达信功能页使用的服务 Entry、ReqId、服务端模块、请求模板、参数占位
  符和返回列中文含义；可导出 JSON、CSV 或 Markdown。
- `tdx_pbrpc.py`：调用 `reqformat=22` 的 TQLEX/PBRPC 服务；从
  `cloud_cfg` 自动选择 Entry、服务端模块和请求 JSON，完成 protobuf
  编码、RpcID 两阶段/分页取数、拼包及结果解码。无第三方依赖；当前
  安装中 29 个启用的唯一 PBRPC ReqId 均已真实验证。
- `update_tdx_auction.py`：一次更新竞价爆量、烂板/炸板转强、竞价止跌、
  预吞上影、涨停高开和 5 分钟陡增七类信号，并生成股票反向索引；只有
  显式 `--download` 才联网。
- `update_tdx_auction_quality.py`：合并 `0x054B` 开盘抢筹榜、`0x056A`
  开/收盘竞价序列、`0x0FC6` 正式集中撮合与 `0x06B9` 当日/昨日竞价
  参考量额；恢复开盘抢筹公式，输出 09:25/15:00 正式量额、昨比、换手、
  最后几秒变化及无 09:25 成交的边界状态。
- `update_tdx_intraday_funds.py`：更新市场、30 个一级行业及其 5,543 只
  成分股的七段主力净额/成交占比，并生成行业—股票双向索引。
- `update_tdx_limit_quality.py`：合并涨停/炸板 JSN、`0x0547` 实时五档、
  `0x06B9` 历史统计和原生板块连板天梯，输出当前封单额、封流比、封昨比、
  封单衰减、连板层级、行业/概念板块及股票索引。
- `tdx_tqlex.py`：调用 `reqformat=2` 的普通 TQLEX JSON 服务；兼容
  客户端单引号模板、动态参数、大小写不同的分页字段、重复 ReqId 模板
  和多结果集，支持自动翻页与合并。无第三方依赖。
- `tdx_cloud_workflow.py`：按客户端 `masterid` 关系执行 JSON/PBRPC
  混合主从查询；已内置基金持仓、收益风险、月度波动、区间持仓、指数
  估值和龙虎榜原因六条工作流。
- `download_tdx_jsn.py`：扫描 `reqformat=11` 资源配置，通过公开 7709
  `709/1721` 命令查询长度/MD5 或分页下载 `.jsn`。默认写项目
  `output\tdx-jsn`，只有显式 `--client-cache` 才写客户端缓存；加
  `--include-cfg` 可发现列表 CFG 中 521 个 XML 清单外候选。
- `extract_tdx_hyzt.py`：把行业/主题 JSN 转换为三级行业树、行业成分、
  股票所属行业和主题反向索引，并用本地证券主表补充名称。
- `catalog_tdx_jsn.py`：离线扫描已下载资源，结合 `cloud_cfg` 输出每个
  文件的中文字段、行数、直接证券键、`$S_ZQDM` 成员覆盖、重复记录、
  字节数和 MD5；XML 与列表 CFG 来源均可识别。
- `query_tdx_jsn.py`：按市场和证券代码横向查询全部已下载 JSN，将行业/
  主题、财务、资金流、两融、分红、事件和动态高管明细关联到同一只票；
  同时匹配直接证券键、引用证券键和 `$S_ZQDM` 成员串，并把期货、IPO
  行业、经济指标、机会组、新闻事件、涨价主题和商品等内部键还原为可读
  上下文。
- `update_tdx_theme_logic.py`：更新 24 个战略主题大类主表，恢复 567 个
  页面内部主题 ID；可按 ID、名称或大类增量下载逐股入选逻辑，并生成
  大类—主题—股票双向关系模型。
- `update_tdx_lhb.py`：更新 8 张龙虎榜视图，按事件 ID 归并分类并生成
  股票反向索引；可按事件或证券选择性下载营业部级买卖明细。
- `update_tdx_institution.py`：更新机构持仓主表或单证券历期机构分类和
  十大流通股东；可从股东 ID 反查其跨股票覆盖，并选择性展开单票逐期持仓。
- `tdx_reqformat1_host.cpp`：32 位原生消息循环宿主，用于诊断 TPData
  `reqformat=0—3`；联网必须显式 `--allow-network`。

```powershell
python doc/90-scripts/recon_install.py `
  --root C:\new_tdx `
  --hash `
  --output C:\tmp\tdx-install-inventory.md
```

```powershell
python doc/90-scripts/extract_tcalc_formulas.py `
  --dll ida/TCalc.dll `
  --kind technical `
  --format csv `
  --output doc/02-engine/tcalc-system-indicators.csv

python doc/90-scripts/dump_tcalc_runtime.py `
  --process TdxW.exe `
  --kind all `
  --format json `
  --output C:\tmp\tdx-runtime-formulas.json
```

```powershell
python doc/90-scripts/extract_tdx_blocks.py `
  --root C:\new_tdx `
  --family industry `
  --family research-industry `
  --format csv `
  --output-dir C:\tmp\tdx-industries

python doc/90-scripts/build_tdx_blocks_html.py `
  --root C:\new_tdx `
  --output output\tdx-blocks.html
```

日常更新不需要分别运行上面的底层脚本，直接执行：

```powershell
python doc/90-scripts/update_tdx_blocks.py
```

同时更新原始 CSV，并在完成后打开页面：

```powershell
python doc/90-scripts/update_tdx_blocks.py `
  --csv-dir output\data `
  --open
```

若自动发现失败：

```powershell
python doc/90-scripts/update_tdx_blocks.py `
  --root D:\new_tdx
```

列出客户端配置中的 `reqformat=11` 资源，不联网：

```powershell
python doc/90-scripts/download_tdx_jsn.py --root C:\new_tdx --list
```

把列表 `.cfg` 的隐藏 `file=*.jsn` 引用也纳入清单：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --list --include-cfg
```

542 个直接 CFG 候选中有 521 个只见于 CFG；再加入 XML 动态模板和由
CFG 关联键恢复的龙虎榜、机构、公司行动、两融、沪深港通、业绩预告、
基金增持与评级详情模板，
并排除当前服务器返回空文件的 `$$UNITID$$` 图表占位符后，统一清单为
616 个可下载资源。建议
使用 `--family`、`--match` 或 `--resource` 小批量选择；工具默认拒绝一次
联网超过 100 个资源，可用 `--max-resources` 显式调整。

更新已经验证的机构持仓、股东人数、龙虎榜、资金强势、公司行动、
大宗交易和股权关联资源族：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family institution-holdings --download

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family shareholder-counts `
  --family lhb-analysis `
  --family capital-strength --download

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family corporate-actions `
  --family block-trading `
  --family equity-groups --download

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family margin-financing `
  --family stock-connect `
  --family industry-lhb `
  --family market-calendar --download

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family futures-statistics `
  --family ipo-bond-issuance `
  --family price-limit-analysis `
  --family commodity-themes `
  --family premium-stocks --download

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family market-anomalies `
  --family etf-fund-flow `
  --family event-research `
  --family economic-indicators `
  --family industry-region-logic --download

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family earnings-forecast `
  --family active-fund-holdings `
  --family institution-seat-activity `
  --family stock-industry-ratings --download

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --family convertible-bond-terms --download --summary
```

按已知关联键更新详情。例如公司业绩预告的键来自主表 `$ZQDM`，股票型
详情则使用市场号和代码：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --include-cfg `
  --match '^yjyg/' --key 88147720260630 --download --summary

python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --include-cfg `
  --match '^(zdjjzczc|lsyd2280[1-4])/' `
  --market 1 --code 601666 --download --summary --skip-missing
```

`ggpj` 是港股评级，需使用真实两位市场号（当前样本为 `31`）；`hypj`
绑定的是通达信行业代码，不应解释为普通股票评级。

更新单只可转债的回售、赎回和转股价调整历史：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --include-cfg `
  --match '^kzz_(hstk|shtk|xztk)/' `
  --market 1 --code 110076 --download --summary --skip-missing

python doc/90-scripts/query_tdx_jsn.py `
  --root C:\new_tdx --market 1 --code 110076
```

详情键使用转债自身的市场号和代码。若概览主表已下载，也可用正股市场和
代码查询，工具会通过 `$SC1/$ZQDM1` 把转债历史关联回来。

按股票同时尝试增减持、质押、大宗交易历史和意向申报；不存在的详情会被
标为 `missing`，不会中断同批其他资源：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --include-cfg `
  --match '^(zcjc|gqzy|dzjy3|dzjy13)/' `
  --market 0 --code 000009 `
  --download --summary --skip-missing
```

`--key` 用于日期、事件、机构和分组等非证券 `$ZQDM`。例如下载某个两融
分类的长期趋势：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx --include-cfg `
  --match '^rzrq5/' --key 880446 --download --summary
```

下载并校验在线行业/主题资源，再生成三级行业—股票—主题关系模型：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --download `
  --resource list/func_gx_hyzt101_1.jsn `
  --summary

python doc/90-scripts/extract_tdx_hyzt.py `
  --root C:\new_tdx `
  --compact

python doc/90-scripts/catalog_tdx_jsn.py `
  --root C:\new_tdx

python doc/90-scripts/query_tdx_jsn.py `
  --root C:\new_tdx `
  --market 0 --code 000001 `
  --output output\tdx-000001-features.json

python doc/90-scripts/update_tdx_theme_logic.py `
  --root C:\new_tdx `
  --download-masters `
  --download-details --theme 5G概念 `
  --compact
```

只更新一个战略主题大类的全部逐股逻辑：

```powershell
python doc/90-scripts/update_tdx_theme_logic.py `
  --root C:\new_tdx `
  --download-details --category 5G6G `
  --compact
```

详情默认每次最多下载 50 个主题。全量操作必须显式加 `--all-details` 并
提高 `--max-details`；日常更新建议按名称、内部 ID 或大类选择。

更新龙虎榜主视图，并按股票取得营业部买卖明细：

```powershell
python doc/90-scripts/update_tdx_lhb.py `
  --root C:\new_tdx --download-masters --compact

python doc/90-scripts/update_tdx_lhb.py `
  --root C:\new_tdx `
  --download-details --market 1 --code 603221 --compact
```

也可用 `--event <事件ID>` 精确选择。详情默认最多 50 个事件；全量更新必须
显式加 `--all-details` 并调整 `--max-details`。

更新单证券机构详情，并从当前十大股东反查跨股票持仓：

```powershell
python doc/90-scripts/update_tdx_institution.py `
  --root C:\new_tdx `
  --download-details --market 0 --code 000063

python doc/90-scripts/update_tdx_institution.py `
  --root C:\new_tdx `
  --download-holder-history --holder QF000034 `
  --holder-stock 603221 --compact
```

`--holder` 可使用股东 ID、带 `tdxid` 的股东键或名称子串。跨股票反查
默认一次最多 10 个股东；只有显式使用 `--all-holders` 并调整
`--max-holders` 才会扩大范围。

只给 `--resource` 时是 dry-run；`--probe` 只取长度和 MD5，`--download`
才下载。`--skip-missing` 适用于详情批量更新，`--all-static` 可选择全部
不含动态占位符的资源。协议、字段和当前数据计数见
[JSN 静态资源协议](../02-engine/09-jsn-resource-protocol.md)。

生成带当前行情的单文件板块雷达：

```powershell
python doc/90-scripts/update_tdx_market.py --download
```

第一次运行可先省略 `--download` 查看板块、证券、请求批次数、主站和输出
路径，计划模式不会联网或写文件。默认输出
`output\tdx-market.html`；页面含层级树、行情排行、板块广度、领涨/拖累、
成分股现价/涨跌/成交额以及证券反查板块。指定安装目录并生成后打开：

```powershell
python doc/90-scripts/update_tdx_market.py `
  --download `
  --root C:\new_tdx `
  --open
```

盘点通达信客户端自身隐藏/云端功能：

```powershell
python doc/90-scripts/inventory_tdx_cloud_features.py `
  --root C:\new_tdx `
  --format csv `
  --output output\tdx-cloud-features.csv
```

该脚本不联网，只读取通达信功能 XML，并忽略 XML 注释中的旧模板。当前
安装扫描得到 329 条启用 datasource、49 个唯一 Entry 和 63 个唯一
ReqId。

列出当前客户端内所有 PBRPC 配置：

```powershell
python doc/90-scripts/tdx_pbrpc.py `
  --root C:\new_tdx `
  --list
```

按 ReqId 读取 XML 模板并取得竞价爆量列表：

```powershell
python doc/90-scripts/tdx_pbrpc.py `
  --root C:\new_tdx `
  --req-id 200404 `
  --output output\tdx-pbrpc-200404.json
```

一次更新客户端同页的七类竞价信号，并按股票合并重复命中：

```powershell
python doc/90-scripts/update_tdx_auction.py `
  --root C:\new_tdx --download --compact
```

省略 `--download` 会显示七个 ReqId 和输出路径，不联网、不写文件。

单独取得股票开盘和收盘集合竞价逐点序列：

```powershell
python doc/90-scripts/tdx_auction_series.py `
  --root C:\new_tdx `
  --security sz000001 --security bj920130 `
  --download --compact
```

`--selector 0` 只返回开盘竞价；默认 `3` 会同时返回开盘和收盘竞价。
`--start-raw/--limit` 可按合并后的逐点序列分页。

下载当日或指定交易日的公开 L1 成交明细：

```powershell
python doc/90-scripts/tdx_trades.py `
  --root C:\new_tdx `
  --security sz000001 --security sh510300 `
  --date 20260731 --download --compact
```

省略 `--date` 使用 `0x0FC5` 取主站当前交易日；指定日期使用 `0x0FC6`。
输出时间精度只有分钟，不是 Level2 秒级逐笔。股票价格为百分位，ETF 与
债券为千分位；工具会自动选择刻度。

生成已经静态确认的 Level2 逐笔请求，或解析从本机合法 L2 会话捕获的
响应体：

```powershell
python doc/90-scripts/tdx_level2.py build-direct `
  --kind transaction --market 1 --code 600000

python doc/90-scripts/tdx_level2.py parse-direct `
  --kind order --input capture.bin --xor-key 0x5a `
  --format csv --output orders.csv

python doc/90-scripts/tdx_level2.py parse-sdk-json `
  --func-id 4680 --input sdk-depth.json --output depth.json

python doc/90-scripts/tdx_level2.py parse-sdk-binary `
  --data-type 18031 --input sdk-order-queue.bin --limit 20 `
  --output sdk-order-queue.json

python doc/90-scripts/tdx_level2.py parse-tpbus-push `
  --push-type 112 --input tpbus-112.bin --limit 20 `
  --output tpbus-112.json

python doc/90-scripts/tdx_level2.py inspect-protobuf `
  --input tpbus-113.bin --max-fields 100 --sample-bytes 64 `
  --output tpbus-113-wire.json
```

`1364/1374` 响应是秒级记录，价格累计值除以 `10000`；委托的
`B/S/C` 动作和撤单方向已静态确认，首个委托类型字节仍保留原值等待真实
样本标定。该工具是捕获/静态结构验证器，不会绕过 `RightInfo/QSHQToken`
权限边界，也不会自行发起 L2 登录。

先对当前运行的客户端做版本预检（不附加）：

```powershell
python doc/90-scripts/capture_tdx_level2.py
```

只观察 `600000` 一只证券的多档盘口 120 秒，并把每个事件最多展开 20 条
记录：

```powershell
python doc/90-scripts/capture_tdx_level2.py `
  --attach --label depth --code 600000 --duration 120 `
  --max-events 500 --max-records 20 `
  --output output/tdx-level2-depth-600000.jsonl
```

检查当前 EventBus 是否已有精确或父级通配消费者，并继续观察新注册：

```powershell
python doc/90-scripts/capture_tdx_level2.py `
  --attach --label eventbus --eventbus-registry --duration 120 `
  --raw-sample-bytes 0 `
  --output output/tdx-level2-eventbus.jsonl
```

`A.B.C` 的分发候选为 `A.*`、`A.B.*`、`A.B.C`。当前实测注册表只有
`MaintainData.SubscribeStock/UnSubscribeStock/MoreSubscribeStock` 和
`SManager.SendData`，没有 `MaintainData.*` 或 `HQPUSHPB`；此结果仅代表
快照时刻，后续注册会作为 `eventbus-observe` 写入同一 JSONL。

分别用 `transaction`、`order`、`depth`、`order_queue` 标签独立捕获，每次
只打开对应的一个 L2 窗口，然后汇总：

```powershell
python doc/90-scripts/tdx_level2.py summarize-probe `
  --input output/tdx-level2-depth-600000.jsonl `
  --input output/tdx-level2-order-queue-600000.jsonl `
  --output output/tdx-level2-summary.json
```

`sdk-request/sdk-callback` 的 `data_type=1801/1802/1803/18031` 分别是逐笔
成交、逐笔委托、多档盘口和选中价位委托队列；`fasthq-subscribe` 记录真实
`LX`，`fasthq-push` 记录 `111/112` 并附上同代码时间窗内的候选 `LX`。
其中 `111` 是即时行情加成对多档价量，`112` 是买一/卖一两侧委托队列；
它们与 `1803/18031` 是并行模型，不能按编号一一等同。汇总器保留证据
计数与置信度。`fasthq-protobuf-push` 记录 `113/114/116` 的类型、长度和
wire 字段草图，但由于代码字段尚未确认，不会把时间邻近的订阅误报为
同证券映射。JSONL 不记录
`RightInfo/QSHQToken`；未知版本默认拒绝附加，即使显式放行也仍要求各
Hook 的机器码签名匹配。

更新开盘抢筹前 30 名的竞价质量和昨日对比：

```powershell
python doc/90-scripts/update_tdx_auction_quality.py `
  --root C:\new_tdx --count 30 --download --compact
```

加 `--ascending` 可改取抢筹最低 30 名。工具把 09:24:57 左右的末次
虚拟价、09:25 正式撮合和 15:00 正式撮合分开保存。只有存在 09:25
成交时才声明正式开盘竞价量额；否则把 `0x054B/tdxstat2` 数值标为竞价
参考量额，避免把未执行的虚拟匹配值误当成交。

更新 30 个一级行业及全部成分股的七个分时段主力资金：

```powershell
python doc/90-scripts/update_tdx_intraday_funds.py `
  --root C:\new_tdx --download --compact
```

默认展开全部 `881xxx` 一级行业；可重复使用 `--industry 881001` 只更新
指定行业，或用 `--master-only` 只取 39 条市场/指数/行业总表。反向索引
使用成分行号引用，不重复保存七段资金值。

单独取得一组证券的五档盘口和买一/卖一金额：

```powershell
python doc/90-scripts/tdx_market_depth.py `
  --root C:\new_tdx `
  --security sz000001 --security 0:000009 `
  --download --compact
```

取得服务端完整封板榜，或开盘抢筹前 30 名：

```powershell
python doc/90-scripts/tdx_category_quotes.py `
  --root C:\new_tdx --sort seal-amount --all-sealed `
  --download --compact

python doc/90-scripts/tdx_category_quotes.py `
  --root C:\new_tdx --sort opening-rush --count 30 `
  --download --compact --output output\tdx-opening-rush.json
```

单独更新通达信统计资源；或只解析本机缓存：

```powershell
python doc/90-scripts/tdx_stats.py `
  --root C:\new_tdx --download --compact

python doc/90-scripts/tdx_stats.py `
  --root C:\new_tdx --local --compact
```

更新完整涨停质量、连板层级和板块梯队：

```powershell
python doc/90-scripts/update_tdx_limit_quality.py `
  --root C:\new_tdx --download --compact
```

该工具会原子更新三张原生 JSN、在线 `zhb.zip`，再对全部触板股请求五档。
`sealed` 仅表示买一等于现价且卖一为空；买一金额只在已经封板时才可
解释为当前封单额。统计日等于事件日时，昨日值取 `prev_*`；统计日落后
一个交易日时取当行值，输出会保留具体对齐状态。

模板含 `$$NAME$$` 时用 `--set NAME=VALUE` 替换；覆盖或增加请求 JSON
字段可用 `--param NAME=VALUE`。例如取得全市场所有龙虎榜类型：

```powershell
python doc/90-scripts/tdx_pbrpc.py `
  --root C:\new_tdx `
  --req-id 500107 `
  --set result=0 `
  --output output\tdx-pbrpc-500107.json
```

同一 ReqId 存在多份模板时，用 `--body-contains` 选择客户端中的具体
请求体。例如查询煤炭板块 `881001` 的成分股区间回测：

```powershell
python doc/90-scripts/tdx_pbrpc.py `
  --root C:\new_tdx `
  --req-id 200199 `
  --body-contains '2:$$code$$|1' `
  --set code=881001 `
  --set AdjustType=1 `
  --set BeginDate=20250101 `
  --set EndDate=20260731
```

`--req-id` 或显式的 `--request-json` 即表示允许本次联网请求。工具只写
用户指定的输出文件，不修改通达信安装目录。

列出当前启用的普通 JSON 服务：

```powershell
python doc/90-scripts/tdx_tqlex.py `
  --root C:\new_tdx `
  --list
```

分页导出 2025 年半年报全部普通股票型基金：

```powershell
python doc/90-scripts/tdx_tqlex.py `
  --root C:\new_tdx `
  --req-id 500050 `
  --set style_details=005001 `
  --set fund_size=0 `
  --set fund_setup_time=0 `
  --set report_date=20250630 `
  --all-pages --page-size 100 `
  --output output\tdx-tqlex-500050.json
```

继续查询基金 `000326` 的行业持仓和个股持仓：

```powershell
python doc/90-scripts/tdx_tqlex.py `
  --root C:\new_tdx --req-id 500051 `
  --set fund_code=000326 --set report_date=20250630 `
  --output output\tdx-tqlex-500051.json

python doc/90-scripts/tdx_tqlex.py `
  --root C:\new_tdx --req-id 500052 `
  --set fund_code=000326 --set report_date=20250630 `
  --output output\tdx-tqlex-500052.json
```

`--all-pages` 默认使用安全的 20 行页长，可用 `--page-size` 覆盖。部分
服务有自己的上限，例如异常波动 `2044` 接受 50 行但拒绝 100 行；服务
返回的 `{"error":"Invalid Page Size"}` 会作为明确错误报告。

当前客户端启用的 34 个普通 JSON ReqId 已全部真实验证。除持仓外，工具
也可直接访问基金风险收益 `500030/500031`、月度波动
`500032/500033`、C-L/T-M/H-M 择时选股能力 `500035`、区间持仓
`500055—500057` 和仓位估算 `500060/500062`。参数与结果矩阵见
[TQLEX 普通 JSON 记录](../99-log/2026-07-31-tqlex-json-tool.md)。

列出可用主从工作流：

```powershell
python doc/90-scripts/tdx_cloud_workflow.py --list
```

选中基金 `000326`，自动从主表找到它并继续查询行业、股票持仓：

```powershell
python doc/90-scripts/tdx_cloud_workflow.py `
  --root C:\new_tdx `
  --workflow fund-holdings `
  --select 000326 `
  --set report_date=20250630 `
  --output output\tdx-workflow-fund-holdings-000326.json
```

取得龙虎榜第一只证券的上榜原因：

```powershell
python doc/90-scripts/tdx_cloud_workflow.py `
  --root C:\new_tdx `
  --workflow lhb-details `
  --limit 1 `
  --output output\tdx-workflow-lhb-details.json
```

`--select` 会自动翻页搜索主表；`--limit` 默认是 1，防止一次展开大量明细
请求。完整设计与实测结果见
[主表→明细工作流记录](../99-log/2026-07-31-cloud-master-detail-workflows.md)。

在已完成的 IDA 数据库中按字符串搜索交叉引用：

```powershell
$env:TDX_IDA_STRING_PATTERN = `
  'reqformat|pb_rpc_req|reqbyte|pbrpc|cloud_cfg|datasource'
& 'C:\Program Files\IDA Professional 9.0\idat.exe' `
  -A `
  '-Sdoc\90-scripts\ida_string_xrefs.py' `
  '-Loutput\ida-string-xrefs.log' `
  'ida\TdxW.exe.i64'
```

脚本会输出命中字符串、引用函数和反编译结果，用于从配置字段反查实际
解析/编码模块。

读取银行板块 `880471` 的本地最新 1 分钟数据并生成离线图：

```powershell
python doc/90-scripts/extract_tdx_minute.py `
  --root C:\new_tdx `
  --code 880471 `
  --date latest `
  --format html `
  --output output\tdx-880471-1m.html
```

导出全部可用分钟记录：

```powershell
python doc/90-scripts/extract_tdx_minute.py `
  --root C:\new_tdx `
  --code 880471 `
  --date all `
  --format csv `
  --output C:\tmp\bank-1m.csv
```

从行情主站下载银行板块最新 1600 根并生成离线图：

```powershell
python doc/90-scripts/download_tdx_minute.py `
  --download `
  --root C:\new_tdx `
  --code 880471 `
  --pages 2 `
  --output output\tdx-880471-live-1m.html
```

不加 `--download` 时只显示市场识别、候选主站和输出路径，不会联网或
写文件。默认把通达信原缓存、上次工具快照和本次下载按“日期+分钟”合并，
但只写入项目下的 `output\minute`，不会覆盖通达信安装目录。要尽量回溯
服务端当前保留的分钟历史，可使用 `--pages 30`；服务端返回短页后工具会
自动停止。

只更新 `.lc1` 快照，不生成网页：

```powershell
python doc/90-scripts/download_tdx_minute.py `
  --download --code 880471 --pages 30 --format none
```

```powershell
& "C:\Program Files\IDA Professional 9.0\idat.exe" -A `
  "-Sdoc\90-scripts\ida_network_inventory.py" `
  "ida\TAsioComm.dll.i64"
```

```powershell
& "C:\Program Files\IDA Professional 9.0\idat.exe" -A `
  "-Sdoc\90-scripts\ida_dynamic_module_xrefs.py" `
  "ida\TdxW.exe.i64"

& "C:\Program Files\IDA Professional 9.0\idat.exe" -A `
  "-Sdoc\90-scripts\ida_address_xrefs.py 0x10076A7A 0x1007D4C4" `
  "ida\tpbus.dll.i64"
```

## 测试

```powershell
python -m unittest discover -s doc/90-scripts -p "test_*.py"
```
