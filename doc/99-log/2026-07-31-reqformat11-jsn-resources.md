# reqformat=11 JSN 资源协议与行业/主题数据

> 日期：2026-07-31  
> 范围：`TdxZdView100.dll`、`TdxTopicView100.dll`、`TdxW.exe`、
> 7709/TCP 命令 `709/1721` 和 `T0002\cloud_cfg`。  
> 结论等级：协议与数据均由普通 Python 直连验证。

## 结论

`reqformat=11` 不是 HTTP/TQLEX RPC，而是通达信行情主站上的静态资源
下载机制。客户端把 XML 中的 `.jsn` 路径交给 TdxW，TdxW 经普通
7709 行情连接取得文件元数据，再按 30,000 字节分页下载到
`T0002\cloud_cache`。

当前安装的 30 个 `reqformat=11` datasource 合并为 23 个唯一资源模板；
21 个无动态参数的资源合计约 14.18 MiB。最有价值的
`list/func_gx_hyzt101_1.jsn` 同时给出：

- 5,534 只当前证券；
- 110 个通达信叶子行业及完整行业成分；
- 每只证券所属行业、行业 PE(TTM)、PB(MRQ)；
- 每只证券的主题标签；
- 1,775 个唯一主题、126,571 条主题—证券关系。

这批数据属于行业、主题和证券关联，不是 MACD/KDJ 技术指标。

## 客户端调用链

```text
cloud_cfg\*.xml
  datasource reqformat="11" body="list/xxx.jsn"
        │
        ▼
TdxZdView100!sub_103E5EF0
  CBiFileHandle
  ├─ 裸文件名补 list/
  ├─ 检查 cloud_cache，300 秒内直接复用
  └─ callback(opcode=40, "任务号|资源路径")
        │
        ▼
TdxW!sub_62B8D0
  资源分支把任务转成队列类型 9
        │
        ▼
TdxW!sub_4238F0
  ├─ 远端键：bi/<资源路径>（部分状态用 bi_diy/ 或 bib/）
  ├─ 本地文件：T0002\cloud_cache\<资源路径>
  └─ sub_69A220 → 7709 命令 709
```

资源请求由隐藏 `CBkDataDlg` 队列串行执行。下载完成后页面收到原任务号，
再从缓存文件读取 JSON。

## 7709 文件协议

### 文件元数据：命令 709

请求数据为 40 字节 NUL 结尾路径：

```text
char remote_path[40] = "bi/list/func_gx_hyzt101_1.jsn"
```

响应数据为 38 字节：

```text
uint32 file_size
uint8  has_md5
char   md5_hex[32]
uint8  nul
```

行业/主题资源的真实响应：

```text
size = 8,901,469
md5  = 825d3fc338d08e8fe47d45593a3024ae
```

### 文件分片：命令 1721

请求数据固定 308 字节：

```text
uint32 offset
uint32 count
char   remote_path[100]
uint8  reserved[200]
```

客户端每页最多请求 30,000 字节。响应为：

```text
uint32 returned_size
uint8  bytes[returned_size]
```

普通 `0x000D` 握手后的公开行情连接即可调用，无需启动 TdxW、注入进程
或使用账号票据。

## 行业树合并验证

云端 110 个行业代码与本地 `tdxzs3.cfg` 的 110 个叶子行业全部一一对应。
把云端叶子成分向父节点展开后，仍使用本地的三级结构：

- 13 个一级行业；
- 56 个二级行业；
- 76 个三级行业；
- 合计 145 个行业节点。

云端与 `tdxhy.cfg` 的 110 个叶子中，98 个成分集合完全一致；另外 12 个
行业合计少 15 条证券记录。差异项多为名称为空或当前新证券状态不同的
记录，因此导出的在线模型保留云端当前成分，本地文件只负责父子层级和
证券名称补充。

“银行” `880471` 在线返回 42 只成分，PE(TTM) `5.2105`、PB(MRQ)
`0.5302`，与本地层级树正确挂接。

## 工具

列出 23 个资源模板，不联网：

```powershell
python doc/90-scripts/download_tdx_jsn.py --root C:\new_tdx --list
```

只联网查询文件长度和 MD5：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --probe `
  --resource list/func_gx_hyzt101_1.jsn
```

下载并解析概要：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --download `
  --resource list/func_gx_hyzt101_1.jsn `
  --summary
```

默认写入项目 `output\tdx-jsn`，不会改动客户端。显式加
`--client-cache` 才写入 `T0002\cloud_cache`。

将资源标准化为行业树、股票和主题双向索引：

```powershell
python doc/90-scripts/extract_tdx_hyzt.py `
  --root C:\new_tdx `
  --compact
```

当前输出：

- 原始资源：`output\tdx-jsn\list\func_gx_hyzt101_1.jsn`，8,901,469 字节；
- 关系模型：`output\tdx-hyzt-model.json`，4,375,095 字节；
- 模型含 145 个行业节点、5,534 只股票和 1,775 个主题。

所有联网动作均要求显式 `--probe` 或 `--download`；只指定资源时为
dry-run。

## 其他高价值资源

同一协议还可直接更新：

| 资源 | 字节 | 内容 |
| --- | ---: | --- |
| `list/func_ygxc101_1.jsn` | 1,106,085 | 员工、薪酬和高管数据 |
| `list/func_gx_cbsj101_1.jsn` | 960,848 | 财务/估值综合数据 |
| `list/func_gx_zjlx101_1.jsn` | 821,521 | 多周期资金流 |
| `list/func_gx_zfsp101_1.jsn` | 558,478 | 分红、送转和配股 |
| `list/func_aqfph101_1.jsn` | 543,634 | 安全评分排行 |
| `list/func_gx_rzrq101_1.jsn` | 465,468 | 融资融券 |
| `list/func_gx_cbyg101_1.jsn` | 360,835 | 业绩预告 |
| `list/func_gx_lhbd101_1.jsn` | 319,372 | 龙虎榜摘要 |
| `list/kzz_kzzsy201_1.jsn` | 201,999 | 可转债收益与条款 |

`ggxc/$$$SC$$$$$ZQDM$$.jsn` 是按证券动态展开的资源。
`zttzty/$$$ZQDM$$.jsn` 的参数不是股票代码，而是战略主题主表隐藏列中的
内部主题 ID；该映射现已从 24 个 `func_*.jsn` 主表恢复，详情见
[战略主题主从链归档](2026-07-31-strategic-theme-logic.md)。

## 全量静态资源验证

使用新增的 `--all-static` 在同一公开行情连接上下载当前配置中的全部
21 个无占位符资源：

```powershell
python doc/90-scripts/download_tdx_jsn.py `
  --root C:\new_tdx `
  --download --all-static --summary
```

结果为 21/21 成功，逐个通过服务器声明的长度和 MD5；合计：

- 14,873,251 字节（14.184 MiB）；
- 44,542 行；
- 最大的行业/主题表 5,534 行；
- 龙虎榜表有 2,215 条事件、涉及 973 只证券，其余主要全量表按证券键
  均无重复。

确认的高价值字段包括：

| 资源 | 行数 | 字段面 |
| --- | ---: | --- |
| `func_gx_cbsj101_1` | 5,541 | 报告期、PE/PB/PS/PEG、负债率、成长、ROE、费用、周转、机构持仓、股息率 |
| `func_ygxc101_1` | 5,534 | 员工数、薪酬、研发、学历结构、人均薪酬/利润 |
| `func_aqfph101_1` | 5,534 | 安全分、评分变动、风险类型、亮点数和亮点类型 |
| `func_gx_zjlx101_1` | 5,203 | 1/5/10/20/30 日主力净流入和多日净流入 |
| `func_gx_zfsp101_1` | 4,335 | 每股公积、未分配利润、送转、现金分红、配股 |
| `func_gx_rzrq101_1` | 3,841 | 融资余额/净买额、融券余量/净卖量及占比 |
| `func_gx_lhbd101_1` | 2,215 | 上榜日、买卖占比、净买入、机构/沪深股通席位、异动类型 |
| `func_gx_fxgz101_1` | 2,088 | 股权质押、商誉和解禁风险 |
| `func_gx_cbyg101_1` | 1,810 | 业绩预告上下限和同比区间 |
| `func_gx_fxspj101_1` | 609 | 机构调研、综合评级、目标价和未来三年 EPS |

可转债资源还完整覆盖发行、票息、转股、下修、回售、强赎、债券余额、
评级和付息日期/利率序列。

## 动态明细和跨资源关联

`ggxc/$$$SC$$$$$ZQDM$$.jsn` 已用 `0/000001` 验证：

```text
remote = bi/ggxc/0000001.jsn
size   = 999
md5    = a4f48f5b94c516fcce5b8868cd6b0c1f
rows   = 17
fields = 姓名、性别/年龄/学历、职务、年薪、截止日期
```

`catalog_tdx_jsn.py` 将 XML 与配套 `.cfg` 的字段说明合并，当前编目
22 个已下载资源、44,559 行、14,874,250 字节；所有实际表头都已取得
中文名。`query_tdx_jsn.py` 再按主证券键、引用证券键或动态文件参数关联
一只票。平安银行 `0/000001` 实测命中 9 个资源、25 条记录。

`zttzty/$$$ZQDM$$.jsn` 没有接受普通股票代码 `000001` 或本地概念指数
`880506`，两者的命令 709 均返回长度 0。继续追踪 `GN_GNZ.xml`、
`hy_tree1_gnz.xml` 和 24 份大类 `.cfg` 后，已确认 `$ZQDM` 来自各大类
`list/func_*_1.jsn` 主表，而不是必须等待格式 1/20 的在线响应。当前已恢复
567 个唯一主题 ID，并用 `657`（5G概念）、`2592`（6G概念）、
`X410102003`（光模块）等 ID 成功下载逐股详情。完整计数和工具见
[战略主题主从链归档](2026-07-31-strategic-theme-logic.md)。

## reqformat=20 校正

`reqformat=20` 没有独立网络协议。`TdxZdView100!sub_103E5EF0` 对它调用
本地树控件命令执行器，正文/名称为空是因为数据依赖当前页面树和证券
上下文。因此它不应继续按“待找下载地址”的网络入口处理。
