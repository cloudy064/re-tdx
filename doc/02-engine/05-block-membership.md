# 板块目录与证券成员关系

> 目标是恢复“行业/概念等板块下面有哪些证券”，不是 MACD、KDJ 一类
> 公式指标。后者属于 `TCalc` 公式引擎，两套数据已经明确分离。

## 当前结论

通达信公开行情缓存已经足够离线恢复五类板块及其成员：

| `family` | 中文类别 | 板块数 | 成员关系数 | 成员来源 |
|---|---|---:|---:|---|
| `industry` | 通达信行业 | 145 | 13,972 | `tdxhy.cfg`，父级按行业码前缀聚合 |
| `research-industry` | 研究行业 | 467 | 15,838 | `tdxhy.cfg`，父级按行业码前缀聚合 |
| `concept` | 概念板块 | 269 | 46,101 | `infoharbor_block.dat` 直接成员 |
| `style` | 风格板块 | 161 | 22,532 | `infoharbor_block.dat` 直接成员 |
| `index` | 指数板块 | 117 | 12,639 | `infoharbor_block.dat` 直接成员 |

以上计数是 2026-07-31 对 `C:\new_tdx` 当前缓存的快照；客户端更新数据后
可能变化。合计为 1,159 个板块、111,082 条无重复成员关系。

地域目录能从 `tdxzs3.cfg` 看到，但当前已经确认的公开成员文件中没有形成
与其同等可靠的映射闭环，因此首版不把地域板块猜测性地混入结果。

## 数据链

### 行业与研究行业

`T0002\hq_cache\tdxzs3.cfg` 是板块目录。每行六个 `|` 分隔字段：

```text
板块名|880代码|类别|1|是否叶子|行业源代码
银行|880471|2|1|1|T1001
股份制银行|881388|12|1|1|X500102
```

- 类别 `2`：通达信行业；
- 类别 `12`：研究行业；
- `T...` / `X...` 代码每两位表示一级层级。

`T0002\hq_cache\tdxhy.cfg` 把证券映射到两套行业叶子：

```text
市场号|证券代码|通达信行业|||研究行业
0|000001|T1001|||X500102
1|600000|T1001|||X500102
```

叶子成员同时属于所有存在的父级前缀。例如分到 `X500102` 的证券也会进入
`X50`、`X5001` 对应的父行业。导出关系中的 `membership` 用 `direct`
区分叶子直接归属，用 `descendant` 标记父级聚合。

### 概念、风格和指数

`T0002\hq_cache\infoharbor_block.dat` 以板块头和成员代码列表组织：

```text
#GN_通达信88,88,880515,20050607,20260512,,
0#000001,1#600000,...
```

- `GN`：概念；
- `FG`：风格；
- `ZS`：指数；
- 成员采用 `市场号#证券代码`；
- 文件头声明的成员数已对全部 547 个板块逐一验证，与实际解析数量一致。

市场号 `0/1/2` 分别表示深圳、上海、北京。证券名称来自同目录下
`szs.tnf`、`shs.tnf`、`bjs.tnf`。

## 导出工具

全量导出：

```powershell
python doc/90-scripts/extract_tdx_blocks.py `
  --root C:\new_tdx `
  --format csv `
  --output-dir C:\tmp\tdx-blocks
```

只导出最接近“行业板块”的两套行业：

```powershell
python doc/90-scripts/extract_tdx_blocks.py `
  --root C:\new_tdx `
  --family industry `
  --family research-industry `
  --format csv `
  --output-dir C:\tmp\tdx-industries
```

脚本只读公开行情缓存，不加载 DLL、不连接网络，也不读取用户自定义的
`blocknew`。

输出文件：

- `tdx-blocks.csv`：一行一个板块，包含类别、名称、880 代码、层级、
  父板块、成员数和数据日期；
- `tdx-block-members.csv`：一行一个板块—证券关系，包含市场、证券代码、
  名称以及直接/子级聚合关系；
- `--format json`：把同样内容输出为一个 `tdx-blocks.json`。

推荐以 `block_id` 连接两张 CSV，以 `security_id`（如 `SZ000001`）作为
跨市场证券主键。若只要行业，在两张表中过滤
`family=industry` 或 `family=research-industry`。

## 单文件离线网页

可以把全量五类板块和关系打包成一个不依赖服务器的 HTML：

```powershell
python doc/90-scripts/build_tdx_blocks_html.py `
  --root C:\new_tdx `
  --output output\tdx-blocks.html
```

### 日常一键更新

用户无需记住解析和打包的两条底层命令，直接运行：

```powershell
python doc/90-scripts/update_tdx_blocks.py
```

更新器会：

1. 从 `TDX_ROOT` / `TDX_HOME`、Windows App Paths 和各磁盘根目录的常见
   安装名中自动寻找通达信；
2. 读取最新板块目录、成员映射和三市场证券主表；
3. 对比读取前后的文件大小与纳秒修改时间；若遇到客户端正在写入的半文件
   或读取期间发生变化，最多自动重试三次；
4. 在同目录写入临时文件并用 `os.replace` 原子替换 HTML，失败时保留原来
   可用的页面；
5. 默认更新 `output\tdx-blocks.html`。

可选参数：

```powershell
# 指定非标准安装目录
python doc/90-scripts/update_tdx_blocks.py --root D:\new_tdx

# 同时生成板块目录和板块—证券关系 CSV
python doc/90-scripts/update_tdx_blocks.py --csv-dir output\data

# 更新成功后直接打开页面
python doc/90-scripts/update_tdx_blocks.py --open
```

生成后直接双击 `output\tdx-blocks.html`，支持：

- 按板块名称、880 代码搜索并查看成分证券；
- 按证券名称、市场代码反查所属全部板块；
- 可折叠行业树和父子层级跳转：通达信行业为 `13→56→76`，研究行业为
  `30→128→309`；搜索会自动展开命中节点的祖先路径；
- 在“层级树”和“平铺”导航之间切换；
- 区分 `direct` 和 `descendant`；
- 把当前板块重新导出为 UTF-8 CSV。

页面不连接网络，也没有外部 JS/CSS/WASM。打包过程对字符串做字典编码，把
板块和证券保存为紧凑列数组，同时预生成：

- `members[block_index]`：板块到证券的正向邻接索引；
- `reverse[security_index]`：证券到板块的反向邻接索引。

索引中的整数最低位复用为直接归属标志，其余位保存目标数组下标。整个
JSON 再用 gzip 压缩、Base64 内嵌，页面通过浏览器原生
`DecompressionStream` 在内存中恢复。

2026-07-31 全量快照的紧凑数据为 1.2 MiB，gzip 后 385.3 KiB，连同完整
界面的最终 HTML 约 560 KiB。使用本机 Chrome 从 `file://` 冷启动三次为
126–171 ms，中位数 137 ms。

### 为什么当前不嵌入 DuckDB/SQLite

- DuckDB-Wasm 是合适的浏览器列式 OLAP 引擎，但标准异步接口还需要 WASM
  模块和 Web Worker；单 HTML 需要额外把运行时转成 Blob URL。
- SQLite-Wasm/sql.js 可以导入内存数据库，但属于行式存储，仍会增加 SQL
  运行时和数据库初始化。
- chDB 当前官方产品是 Python 进程内的 ClickHouse，不是浏览器运行时。
- 当前交互是固定的双向邻接查询，不是任意聚合和列扫描。预生成正反索引
  能直接定位结果，数据量也远小于引入通用数据库的合理阈值。

若未来扩展到数百万行情/财务明细，并要求任意 SQL 聚合、排序和跨表连接，
再切换 DuckDB-Wasm + Arrow/Parquet 更合适。官方资料：

- <https://duckdb.org/docs/stable/clients/wasm/overview>
- <https://github.com/duckdb/duckdb-wasm>
- <https://www.sqlite.org/wasm/doc/trunk/index.md>
- <https://clickhouse.com/chdb>

## 验证样本

- 通达信行业“银行” `880471`：42 只；
- 研究行业“股份制银行” `881388`：9 只；
- 平安银行 `SZ000001`、浦发银行 `SH600000` 均正确进入上述银行关系；
- 沪深300：声明 300，解析 300；
- 上证50：声明 50，解析 50；
- 通达信88：声明 88，解析 88；
- 111,082 条 `(block_id, security_id)` 关系无重复。

当前有 8 个行业映射代码已不在三份现行 `.tnf` 证券主表中，因父级展开形成
40 条 `name_resolved=false` 关系。它们仍保留市场与证券代码，名称留空，
避免用历史更名缓存误填；概念、风格、指数的 81,272 条关系名称全部解析。
