# 战略主题主从链与逐股入选逻辑

> 日期：2026-07-31  
> 范围：`GN_GNZ.xml`、`hy_tree1_gnz.xml`、24 份 `func_*.cfg`、
> `list/func_*_1.jsn` 和 `zttzty/<主题ID>.jsn`。  
> 结论等级：页面链路由静态配置闭合，主表与详情均由普通 Python 直连验证。

> 2026-08-07 更新：下文 Python 命令仅保留为最初协议验证记录。正式能力已经
> 迁移到纯 C++ `tdx-tool market strategic-themes`、固定 HTTP API 和 Svelte
> “战略主题”页，运行时不再读取 Python 生成的 `tdx-theme-logic.json`。

## 突破结论

“主题投资”页面不是直接把股票代码或本地板块指数传给 `zttzty`。它先从
24 个战略主题大类加载主表，主表隐藏列 `$ZQDM` 给出详情所需的内部主题
ID，再用该 ID 请求逐股入选逻辑：

```text
GN_GNZ.xml
  → tree 300: hy_tree1_gnz.xml（24 个大类）
  → grid 4: cfg_fx_xgnz + 当前大类 func_*.jsn 主表
  → hidden $ZQDM（内部主题 ID）
  → grid 9: zttzty/<主题ID>.jsn
```

这解释了为什么 `zttzty/000001.jsn` 和 `zttzty/880506.jsn` 都为空：参数
类型用错了，而不是资源协议或服务端入口失效。

## 24 个大类

| 树 ID | 大类 | 主表资源 |
| --- | --- | --- |
| Z01 | 国防军工 | `list/func_gfjg101_1.jsn` |
| Z03 | 健康中国 | `list/func_jkzg101_1.jsn` |
| Z04 | 美丽中国 | `list/func_mlzg101_1.jsn` |
| Z10 | 交通强国 | `list/func_jtqg101_1.jsn` |
| Z05 | 工业4.0 | `list/func_gy101_1.jsn` |
| Z06 | 汽车产业 | `list/func_qccy101_1.jsn` |
| Z07 | 人工智能 | `list/func_rgzn101_1.jsn` |
| Z08 | 5G6G | `list/func_5G101_1.jsn` |
| Z09 | 新基建 | `list/func_xjj101_1.jsn` |
| Z11 | 大金融 | `list/func_djr101_1.jsn` |
| Z12 | 非接触经济 | `list/func_fjcjj101_1.jsn` |
| Z13 | 半导体 | `list/func_bdt101_1.jsn` |
| Z14 | 碳中和 | `list/func_tzh_1.jsn` |
| Z16 | 数字经济 | `list/func_szjj101_1.jsn` |
| Z17 | 算力产业 | `list/func_slcy101_1.jsn` |
| Z18 | 机器人 | `list/func_jqr101_1.jsn` |
| Z19 | 大消费 | `list/func_dxf101_1.jsn` |
| Z20 | 风光锂储 | `list/func_fglc101_1.jsn` |
| Z23 | 新材料 | `list/func_xcl101_1.jsn` |
| Z24 | 新型电力 | `list/func_xxdl101_1.jsn` |
| Z25 | 国产软件 | `list/func_gcr101_1.jsn` |
| Z27 | 大周期 | `list/func_dzq101_1.jsn` |
| Z30 | 农业安全 | `list/func_nyaq101_1.jsn` |
| Z31 | 地产链 | `list/func_dcl101_1.jsn` |

24 个主表合计 569,510 字节，全部通过服务器声明的 MD5。解析得到：

- 617 条大类—主题记录；
- 567 个唯一主题、566 个唯一名称；
- 386 个纯数字 ID、181 个 `X...` 层级 ID；
- 55,101 个原始大类成员标记；
- 45 个主题同时出现在多个大类，名称与成员集合一致。

## 主表与详情字段

主表的稳定字段为：

```text
gname,$S_ZQDM,S_NUM,$ZQDM
主题名,市场|股票代码列表,声明成员数,内部主题ID
```

详情资源的稳定字段为：

```text
$ZQDM,$SC,fqprice_d3,fqprice_d5,fqprice_d20,fqprice_d60,price1,tzlj,xxsm
股票代码,市场,3/5/20/60日价格,三个月价格,入选逻辑,完整说明
```

主表存在少量源数据重复：7 个主题合计 9 条重复成员。例如“新能源车”
声明 1,061 条、去重后 1,059 只，重复代码为 `000572` 和 `000700`。
工具按原始列表校验 `S_NUM`，同时在最终关系模型中去重。若详情已下载，
详情成员优先于主表成员；这使 5G、6G、光纤概念等主题能补入主表未列的
北交所证券。

## 在线验证

| 主题 | ID | 主表成员 | 详情成员 | 详情字节 | MD5 |
| --- | --- | ---: | ---: | ---: | --- |
| 5G概念 | 657 | 441 | 452 | 103,450 | `2d1580b3676c785338530d2abd7f8e15` |
| 6G概念 | 2592 | 94 | 98 | 22,724 | `a6798f155148be0b09b130fba0ec42f4` |
| F5G概念 | 2857 | 37 | 37 | 9,142 | `5c74313cee5b47c8e14e65a945412976` |
| 光模块 | X410102003 | 34 | 34 | 9,985 | `90ff2433c3ed6f871a26b525a1c6e95f` |
| 成飞概念 | 2914 | 43 | 43 | 11,095 | `a5660098299d477440590fa1b6344dd8` |

5G6G 大类的 9 个主题均已下载详情，再加“成飞概念”，当前本地模型包含
10 个详情主题、852 条逐股逻辑。全部 24 个主表生成的关系模型包含：

- 567 个主题；
- 5,381 只证券；
- 42,136 条去重后的主题—证券关系；
- 617 条大类—主题关系。

以中兴通讯 `0/000063` 为例，横向查询命中 5G概念、6G概念、富媒体和
光纤概念，并直接返回每个主题的 `tzlj/xxsm`，不再只有无名称的主题 ID。

## 工具与安全边界

```powershell
# 更新主表并生成 output\tdx-theme-logic.json
python doc/90-scripts/update_tdx_theme_logic.py `
  --root C:\new_tdx --download-masters --compact

# 列出主题，不联网下载详情
python doc/90-scripts/update_tdx_theme_logic.py `
  --root C:\new_tdx --list --theme 光模块

# 按主题或大类更新详情
python doc/90-scripts/update_tdx_theme_logic.py `
  --root C:\new_tdx --download-details --theme 5G概念 --compact

python doc/90-scripts/update_tdx_theme_logic.py `
  --root C:\new_tdx --download-details --category 5G6G --compact
```

只有 `--download-masters` 或 `--download-details` 会联网。详情下载默认上限
50 个主题；若确实需要全量 567 个，必须同时使用 `--all-details` 和足够大
的 `--max-details`。该限制是为了避免一次误操作产生密集请求。

## 纯 C++ 迁移结果（2026-08-07）

新实现批量读取并缓存 24 张主表，校验跨大类重复主题的名称、声明计数和成员
集合；目录查询不会请求 567 个详情，只有选定主题时才读取对应的一个
`zttzty/<主题ID>.jsn`。当前在线结果为 24 个大类、617 条大类—主题关系、
567 个主题、5,382 只证券和 42,142 条去重主题—证券关系；5G概念为主表
441 只、详情 452 只，中兴通讯反查 36 个主题。正式用法见
[JSN 静态资源协议](../02-engine/09-jsn-resource-protocol.md)。
