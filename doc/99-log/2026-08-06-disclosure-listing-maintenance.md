# 上市日期感知与披露一键增量维护

日期：2026-08-06

## 结论

披露覆盖率审计已经补上上市日期，并形成纯 C++ 一键维护流程。上市日不需要
逐股联网：通达信本地 `T0002/hq_cache/base.dbf` 是 dBASE III 缓存，`SC + GPDM +
SSDATE` 直接给出市场、代码和上市日期。审计把报告期末早于 `SSDATE` 的组合标记
为 `pre_listing`，不计入覆盖率分母，也不会进入公告回补队列。

一键维护入口：

```powershell
# 自选股：刷新日历归档、审计、回补到期项、再次审计
tdx-tool market disclosures --root C:\new_tdx `
  --maintain --backfill-watchlist

# 父级行业同样递归包含子行业成员
tdx-tool market disclosures --root C:\new_tdx `
  --maintain --backfill-block research-industry:X50

# 可显式评价日期、季度和已完成缺口的重查间隔
tdx-tool market disclosures --root C:\new_tdx --maintain `
  --security sh600036 --audit-period 20260630 --audit-as-of 20260901 `
  --maintenance-refresh-hours 24
```

`--maintain` 仍要求显式证券、自选股、输入文件或板块选择器，不会默认遍历全市场；
`--max-securities`、请求间隔、重试次数、原子归档和断点状态继续生效。

## DBF 原生解析

实现校验 dBASE 版本、记录数、头长、记录长、字段描述符、删除标记和文件边界，
并要求存在 `SC/GPDM/SSDATE`。日期会做闰年和月日合法性校验；重复证券选择最早
上市日并统计冲突。审计结果为每只证券返回：

```json
{
  "security_id": "SZ300503",
  "listing_date": "20160119",
  "listing_date_source": "T0002/hq_cache/base.dbf#SSDATE"
}
```

当前真实文件声明 7,886 条记录，得到 7,754 个有效上市日，DBF 更新时间
`20260803`。若默认文件不存在，审计仍可运行但明确返回上市日不可用边界；显式
`--listing-dates-path` 不存在或文件损坏则拒绝执行。

全板块成员离线压力验收：

- 输入 111,082 行成员关系，去重为 5,972 个证券；
- 其中 5,548 只为 A 股，424 个为 ETF、指数、债券等不适用证券；
- 上市日命中 5,898 个证券；
- 四个季度共识别 281 个上市前组合，并从 22,192 个 A 股组合中排除；
- 最终要求覆盖的组合为 21,911 个。

真实样本包括 `BJ920011`（2026-04-08 上市）排除
`20260331/20251231/20250930`，以及 `BJ920065`（2026-07-29 上市）排除当前
选择的全部四个报告期。

## 一键维护顺序

维护命令严格按以下顺序执行：

1. 读取当前财报预约、变更、实际披露和快报资源；
2. 原子合并 `disclosure-availability.json`；
3. 读取本地 DBF 上市日并执行覆盖率审计；
4. 只对顶层到期队列执行限速、重试、断点公告回补；
5. 先写归档再写任务状态，最后重新审计；
6. 输出刷新、回补前、网络请求及回补后的完整汇总。

批量状态新增 `last_attempt_unix/completed_unix`。维护模式默认只在完成时间超过
24 小时后重查仍存在的到期缺口，可用 `--maintenance-refresh-hours` 调整；普通
批量回补仍保持“已完成永久跳过”的原语义，除非显式 `--refresh-completed`。

这解决了两个相反问题：已完成证券不会因为每次维护而重复请求，但随着预约日
到期，也不会由于旧状态永久失去重新检查机会。

## 真实验收

当前自选股维护：

- 37 个条目，34 只适用 A 股；
- 刷新归档后到期缺口 0，公告网络请求 0；
- 33 个未覆盖组合均为未来预约日，3 个非 A 股条目保持不适用；
- 维护输出和归档均成功写入。

隔离归档的 TTL 验收使用招商银行 `20260630`，并把评价日模拟为
`20260901`：

- 首次维护识别 1 个到期缺口，发起 1 次公告请求并成功保存状态；
- 上游尚未提供该半年报，因此再次审计仍有 1 个缺口；
- 紧接着第二次运行仍识别该缺口，但状态在 24 小时 TTL 内，跳过 1、网络请求 0；
- 离线故障测试还把完成时间置为过期，确认 TTL 到期会重新请求且归档合并幂等。

验收产物：

- `output/probes/disclosure-coverage-all-block-members-listing-aware.json`
- `output/probes/disclosure-coverage-watchlist-listing-aware.json`
- `output/probes/disclosure-maintenance-watchlist.json`
- `output/probes/disclosure-maintenance-ttl-first.json`
- `output/probes/disclosure-maintenance-ttl-second.json`

最终版本已部署到 `dist/tdx-tool/bin/tdx-tool.exe`，SHA-256 为
`88802116E79CB8413F818E778602DB18CF2DC80EA675F0E4F947FF140E631CC0`，服务进程
为 `38464`。部署后二进制再次完成 37 个自选股条目的真实维护，上市日命中 36、
到期缺口 0、公告网络请求 0；健康检查返回 `ok=true`、`native_cpp=true`、
`python_runtime=false`。
