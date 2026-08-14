# 十大流通股东跨股票历史与单票详情容错

> 日期：2026-08-08。范围：公开 TQLEX `gdjc/gdjcxq`，不涉及登录或 L2。

## 问题

个股十大流通股东可以从 `gdjc` 链接取得股东 ID，再查询该股东跨股票持股历史，
并用 `gdjcxq` 展开某只股票的历次报告期。此前这两段都是一次 TQLEX 请求，而且
TTL 到期时先删除缓存；一次 429/502/503/504 或服务端 ErrorCode 4 会让整个股东
页失败，即使进程中已有一份完整旧结果也无法降级。

## 实现边界

- 历史和详情各自最多三次请求，250/500 ms 线性退避；
- 只重试 HTTP 429/502/503/504、ErrorCode 4、`WinHttpSendRequest` 和
  `WinHttpReceiveResponse` 故障；
- 当前请求键的过期缓存不会在刷新前删除，刷新与字段归一化全部成功后才替换；
- 历史与详情分别降级，响应顶层以 `availability=stale-cache` 标记任一陈旧段；
- 冷缓存耗尽后失败；ErrorCode 5、响应结构和字段解析错误立即失败且不读取旧值；
- `cache` 输出两段各自的 TTL、年龄、是否刷新、尝试次数、陈旧标记、上游错误，
  并固定 `max_attempts=3`；
- 构造函数加入可注入 TQLEX transport，只用于确定性故障测试，正式运行仍使用纯
  C++ WinHTTP。

## 验证

平安银行 `SZ000001` 当前十大流通股东 10/10 均可穿透。选取“香港中央结算有限
公司” (`GD011907`, variant `8000002`)：

- 跨股票历史 3,503 条、3,503 只唯一股票；
- 平安银行单票详情 35 个报告期；
- 历史及详情均一次成功，非陈旧；
- 注入 ErrorCode 4 时，已缓存详情连续三次失败后保留原 1 条完整测试向量；
- 冷缓存 ErrorCode 4 请求三次后失败；
- 注入 ErrorCode 5 时只调用一次，并拒绝陈旧回退。

新增固定契约 `holder-cross-stock-live`，检查真实股东 ID、非空跨股票历史、单票
报告期、分页上限和完整容错元数据。结果：

- 原生 CTest：100/100；
- 临时专项 API：1/1；临时全量：174/174；
- 正式专项 API：1/1；正式全量：174/174。

正式服务为 `http://127.0.0.1:8765/`，PID `13904`。只有一个 `tdx-tool` 进程，
8766 已关闭。部署 EXE SHA-256：
`43619459DAB4937A69BF8D467FC4C0B97D996E0215E9BE46BDCC7ADC0E9A4EBC`。

证据：

- `output/probes/holder-resilience-live-20260808.json`
- `output/probes/api-contract-holder-resilience-temp-20260808.json`
- `output/probes/api-contracts-full-temp-20260808-holder-resilience.json`
- `output/probes/api-contract-holder-resilience-formal-20260808.json`
- `output/probes/api-contracts-full-formal-20260808-holder-resilience.json`
