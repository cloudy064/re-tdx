# TCalc 回购与股权激励事件天数

## 结论

条件选股 `A012` 依赖的 `FINANCE(90/91)` 已由纯 C++ 原生上下文闭合：

- `FINANCE(90)`：`list/func_qxfa104_1.jsn`，日期取 `ggrq`（拟回购、董事会通过日）；
- `FINANCE(91)`：`list/func_qxfa401_1.jsn`，日期取 `ggrq`（股权激励方案公告日）。

同一证券存在多条记录时，取不晚于本地今天的最新公告日。资源中偶有未来日期，
这些记录不会提前进入公式上下文。

## 天数语义

官方函数表把两个字段定义为最新方案距今天数。系统 A012 的原文为：

```text
DNUM1:=FINANCE(90);
DNUM2:=FINANCE(91);
(DNUM1>0 && DNUM1<N) || (DNUM2>0 && DNUM2<N);
```

其默认参数 `N=5`。据此实现采用当天 `1`、昨天 `2`、无事件 `0` 的含当天自然日
计数；若当天为 `0`，系统原式会错误排除当天刚披露的方案。算法使用 civil date
计算，不依赖交易日历，并覆盖闰日。

## 实现与验证

事件表由同一 7709 连接成组获取，并在进程内缓存 5 分钟。全市场选股扫描期间，
不会为每只证券重复下载两张完整主表。上下文同时输出日期、资源和计数元数据，
方便定位错误边界。

真实样本采用深市 `002028`：2026-08-05 的股权激励公告得到
`FINANCE(91)=1`，A012 最新输出为 `1`。整库覆盖从 305 提升至 306，条件选股从
`105/107` 提升至 `106/107`；平安银行 800 根日线的 306 条候选全部运行成功，
均有数值输出和数值末值。MinGW 全量 CTest 为 22/22。

产物：

- `output/formula-A012-finance-events.json`
- `output/tcalc-formula-coverage-events.json`
- `output/tcalc-formula-context-runtime-audit-events.json`
- `output/tcalc-formula-coverage-professional.json`
- `output/tcalc-formula-context-runtime-audit-professional.json`

## `FINANCE(88)` 后续闭合

北向大幅增仓表 `list/func_hsgt205_1.jsn` 当前有 225 行，但全部只属于
2024-08-16 一个统计日。它可以证明当天有哪些证券，却不能证明名单外证券各自
最近一次大幅增仓日期，因此仍没有用它推断 A013。

后续宿主回调追踪确认 `TCalc!sub_10026B20` 对 `FINANCE(88..91)` 请求数据类型
`0xA3`；其中 88 读取偏移 244 的带符号日期，正号表示大幅增仓、负号属于
`FINANCE(89)`。TdxW 的 `sub_60FFF0 case 163` 又证明该偏移来自证券主记录
`+587`，方向取 `+591`；主记录加载器 `sub_4F5B60` 将它们从
`hq_cache/tipinfo.dat` 第 18/19 字段读入。在线 `0x06B9/zhb.zip` 正好包含这张表。

纯 C++ 统计资源解析器现会安全解压并索引 `tipinfo.dat`，A013 由相同宿主字段
绑定。2026-08-05 在线包 5,615 条记录的北向日期/方向字段全部为空，所以实测
`FINANCE(88)=0`；这是与当前客户端一致的上游空值，而不是用陈旧榜单伪造日期。
条件选股由此达到 `107/107`，整库上下文覆盖和 800 根平安银行日线审计均为
`307/307`，错误、全空输出和空末值都是 0。

字段定义见[通达信官方函数列表](https://help.tdx.com.cn/gspt/docs/markdown/redword/functionlist.html)。
