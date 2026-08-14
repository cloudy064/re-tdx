# 待发可转债方案进度纯 C++ 固定化

## 页面与资源审计

当前客户端存在两个同名页面。`DFKZZ.sp` 仍引用旧 `func_kzz102`，而实际新页面
`ZZ_DFKZZ.sp` 的主表明确使用 `func_kzz_dfkzz201`，其
`func_kzz_dfkzz201.cfg` 和 `tdxzq_kzz_dfkzz.xml` 都指向
`list/dfkzz201_1.jsn`。因此实现并未新建重复命令，而是在已有
`market convertible-bonds` 下增加 `view=pending`；原有已上市条款保持
`view=listed` 默认行为。

2026-08-06 对在线兼容资源的对照为：

- `dfkzz201_1.jsn`：157 行，新页面主源；
- `func_kzz102_1.jsn`：141 行，旧页面精简表；
- `gxjty_zq_dfkzz102_1.jsn`：157 行，增加所属地域的旧表；
- `func_kzz103_1.jsn`：2 行，属于另一张转债状态表，不与待发主表拼接。

前三张数据大量重叠，若直接合并会重复统计。本阶段以当前页面真实绑定的
`dfkzz201` 为唯一主源，旧资源只作为协议兼容证据保留。

## 字段与单位边界

类型化结果包含正股身份、发行类型、计划发行规模（亿元）、方案进度及日期、
股票含权、转股价、股东配售率原值、申购/发行/中签日期、申购代码/名称、
中签率原值和发行价格，并始终保留 `raw`。

客户端另外通过当前行情计算 `byhq*100/$NOW`、`$NOW/zgj` 和
`$NOW/zgj*100`。这些列不在 JSN，接口没有伪造。`gdpsl` 与样本 `byhq/100`
精确对应，故返回名为 `shareholder_placement_ratio` 并保持上游数值，没有
擅自换算成百分比；`zql` 同样保留上游口径。

## 命令与 API

```powershell
tdx-tool market convertible-bonds --view pending --limit 20
tdx-tool market convertible-bonds --view pending --market sz --code 301565
tdx-tool market convertible-bonds --view pending --sort issue-size --order desc
```

固定接口为 `/api/v1/market/convertible-bonds?view=pending`。支持文本、市场+代码、
限制条数、缓存/刷新和以下排序：`progress-date`、`issue-size`、`stock-rights`、
`conversion-price`、`subscription-date`。响应显式给出 `availability`、来源尝试
次数、陈旧状态、聚合健康、服务缓存和各进度汇总。可转债路由此前遗漏的
OpenAPI 路径也已补齐。

## 真实结果

当前主源 157 个方案，计划发行规模合计 3,627.61 亿元：股东大会通过 82 个、
董事会预案 24 个、申购结束 17 个、证监会核准批文 15 个、证监会发审通过
14 个、申购计划 3 个、证监会发审未通过 2 个。默认按进度日期倒序，首批三只
为中仑新材、派克新材、先锋精科，进度日及申购日均为 2026-08-06。

证据文件：

- `output/probes/pending-convertible-bonds-current.json`；
- `output/probes/pending-convertible-bonds-sz301565.json`；
- `output/probes/listed-convertible-bonds-regression.json`；
- `output/probes/pending-convertible-bonds-contracts-temp.json`。

## 覆盖、测试与部署

- `list/dfkzz201_1.jsn` 绑定现有 C++ 命令，类型化覆盖由 232/616 提升到
  233/616，通用独占由 384 降到 383，已下载但通用独占由 95 降到 94；
- 新增 `tdx-convertible-bonds-tests`，覆盖北证身份、数值/空值、原始行、日期与
  规模排序、非法排序；完整 CTest 51/51；
- 新增 `pending-convertible-bonds-live` 正式契约，完整 HTTP 巡检 41/41；
- 正式服务 PID `22952`，端口 `8765`，EXE SHA-256 为
  `0BECEC8F8E245F08DA5C56EA64EE7957B916C69AF631BCBE1BCCDCB3D1B68AB4`；
- 健康状态保持 `native_cpp=true`、`python_runtime=false`，临时 `8879` 已关闭。

`bkld101..104` 已在后续阶段固定为独立板块轮动接口，见
[板块轮动四分支](2026-08-06-native-block-rotation.md)。下一批已下载高收益缺口
转为 `bygtj102`、`lbtt101`、`ldph101` 与五张 `qszj`。
