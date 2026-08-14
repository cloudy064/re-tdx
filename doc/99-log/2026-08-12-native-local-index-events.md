# 本地指数图重大事件注记闭环

## 目标

恢复 `T0002/hq_cache/speczsevent.txt` 与 `speczsevent_ds.txt` 的原生用途，先与
已有 `market calendar`、`market event-impact` 去重，再决定是否产品化。实现
保持纯 C++ 和手工重构后的独立模块边界，不修改现有云端事件影响 schema。

## 原生链路

IDA 静态证据闭合了完整链路：

- `sub_641DB0` 下载 `spec/speczsevent.txt` 和 `spec/speczsevent_ds.txt`；
- `sub_5C6C40 @ 0x5C6C40` 逐行解析。基础表固定 4 列，第二表固定 5 列；
  原生 278 字节记录依次保存 `MMDD`、图表日期、256 字节标题、事件 `recid`
  和类型；第二表只接受类型 1/2；
- `sub_990E20/sub_985B90` 把事件绘制到指数 K 线坐标，并在鼠标命中时读取
  `recid`；
- 三个容器的消费者谓词确认类型 0 面向境内指数和 `8800` 板块，类型 1 面向
  恒生指数族，类型 2 面向 `A_IXIC/A_DJI/A_SPX` 美股指数族；
- `sub_404530` 从 `neednote.dat [URL]/ZSEventUrl` 读取跳转模板，缺省值为
  `http://www.treeid/dlghttp://page1.tdx.com.cn:7615/site/tdx-pc-content/page-cjzx.html?type=1&recid=%d&zxflag=0`。

首列 `MMDD` 是事件实际发生月日，第二列是目标指数用于绘图的交易日。例如
`2026-02-28` 发生的伊朗事件在恒生/纳指图上映射到 `2026-03-02`。当前共有
18 条这种休市日到下一交易日的原生对齐。

## 与既有能力去重

财经日历的 `macro` 是未来宏观数据发布时间表，不包含这批历史图表注记。
`market event-impact` 才是对应的云端增强表：它额外提供事件前一周、当天、
次日和后一周指数收盘及收益。

按“基准 + 实际发生日 + 标题”全量对账：

- 本地 162 条中有 151 条可与云端 583 条精确匹配；
- 恒生 30/30、纳指 79/79 全部匹配；
- 上证 53 条中 42 条匹配，另有 11 条 `2018..2021` 历史注记已不在当前云表；
- 本地每条都有 `recid` 和目标交易日，云端规范化记录未保留这两项原生字段。

因此没有把本地行混入 `event-impact`，避免改变其公开 schema 和“带冲击收益”
语义；新增隔离的离线注记接口，供指数图叠加、正文跳转和历史补缺使用。机器
可读对账见 `output/local-index-events-overlap-20260812.json`。

## 实现

新增：

- `native/include/tdx/index_events.hpp`；
- `native/src/research/index_events.cpp`；
- `native/src/research/index_events_command.cpp`；
- `native/tests/index_events_tests.cpp`。

公开入口：

```text
tdx-tool market index-events --benchmark hang-seng --date-basis occurrence
GET /api/v1/market/index-events?benchmark=hang-seng&date_basis=chart
```

支持按基准、`event_id`、标题、日期、日期口径、排序和分页查询。响应同时给出：

- `occurrence_date`：实际发生日；
- `chart_date`：原生目标市场交易日；
- `detail_url`：可交给浏览器的正文地址；
- `native_detail_target`：原客户端内部 `treeid/dlg` 目标。

若安装目录配置了 `neednote.dat [URL]/ZSEventUrl`，解析器优先使用配置值；否则
使用 IDA 恢复的原生缺省模板。接口只生成链接，不主动请求正文。

## 真实安装与验证

- `speczsevent.txt`：3,965 字节、53 条上证/境内注记；
- `speczsevent_ds.txt`：8,477 字节、30 条恒生及 79 条纳指注记；
- 合计 162 条，图表日期覆盖 `20181105..20260513`，18 条发生日与图表日不同；
- 增量构建 `tdx-index-events-tests`、`tdx-tool` 通过；
- 聚焦 CTest 1/1 通过，约 0.50 秒；
- 真实安装 162/162 条解析通过；
- 临时 `18842` 的全量、同 `recid` 三基准、恒生休市日对齐、非法日期口径
  4 个 API 契约通过；临时监听已关闭；
- 正式 8765 服务始终保持原监听；
- 新模块不引用或转发 Python。

本次只新增隔离文件解析器和端点，没有修改共享传输、缓存或既有 schema，按验证
预算未运行完整 CTest/API 套件。真实输出见
`output/local-index-events-live-20260812.json`。
