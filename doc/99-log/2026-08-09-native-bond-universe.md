# 全市场债券总表与本地归档加速

## 结果

对尚未类型化的 55 个 JSN 模板逐一做公开 7709 元数据探测后，20 个债券资源
仍可下载，35 个资源当前不存在；可用载荷合计 156,755,233 字节。收益最高的
`list/zq_zqqb201.jsn` 是客户端“全部债券”主表，下载得到 40,078,059 字节，
MD5 为 `eb561bb88e77deb957b49d3b601eef35`。

纯 C++ `market bond-reference` 新增 `--group category --bucket all`：

- 42,479 条条款记录、41,135 只唯一证券；
- 保留市场、代码、简称、债券类型、起息/到期、票息、利率类型、评级、面值、
  发行价及完整/剩余付息序列；
- API 与服务进程普通查询优先读取 `--jsn-root` 下的本地归档，`--refresh` 才绕过
  本地文件访问通达信公开上游；
- 首轮筛选只构造标量字段，排序分页完成后才为返回行展开长付息数组并附上原始行，
  避免对 4.2 万条记录做无用的数组分配；
- Svelte 债券资料库的“债券类别”中增加“全市场债券 · 4万+”。

本地冷启动查询全表并返回 5 条约 3.3 秒；返回 200 条紧凑 JSON 约 374 KB，
不会把全部 40 MB 原始表发送给浏览器。源元数据明确标记
`local-jsn:<path>`，不把本地归档伪装成实时下载。

## 覆盖变化

`market bond-reference` 现有 9 个评级桶、6 个利率桶、8 个细分类别保持不变，
新增第 9 个类别“全市场”。本里程碑先将类型化模板由 567/622 提升为 568/622，
镜像由 580 增至 581 个文件。随后其余 19 个可下载资源已完成集合/字段对账并
类型化，最终结果见[分类投影对账](2026-08-09-native-bond-projection-reconciliation.md)。

## 验证

- C++ 单元与回归测试：101/101；
- Svelte `npm run check`：0 错误、0 警告；生产构建成功；
- 临时 8766 新增固定契约 `bond-reference-universe-live`：1/1；
- 临时 8766 全量 API 契约：177/177；
- 正式 8765 新增固定契约：1/1；全量 API 契约：177/177；
- 固定契约要求总表至少 40,000 条、返回 5 条、保留证券身份、条款、原始行和
  至少一条真实付息序列，并核对唯一来源及实际本地/远端端点。

正式服务在全量巡检后重启以释放测试缓存，当前 PID 为 `43364`，仅监听
`127.0.0.1:8765`；8766 已关闭。发布 EXE
SHA-256 为
`6F51B9240EDF2272DD3C0F7D547A706D976A3D5DA92454358C9BD113AA83B336`。

证据文件：

- `output/probes/jsn-gap-availability-20260809-next.json`
- `output/probes/jsn-all-bonds-download-20260809.json`
- `output/probes/bond-universe-live-20260809.json`
- `output/probes/bond-universe-page-20260809.json`
- `output/probes/jsn-variant-gaps-after-bond-universe-20260809.json`
- `output/probes/api-contract-bond-universe-temp-20260809.json`
- `output/probes/api-contract-full-temp-20260809-bond-universe.json`
- `output/probes/api-contract-bond-universe-formal-20260809.json`
- `output/probes/api-contract-full-formal-20260809-bond-universe.json`
