# 原生 C++ 板块 K 线入口

## 目标

把本地行业/板块目录与已经验证的 7709 `0x052D` K 线链路连接起来，使调用方
既可以传证券代码，也可以直接传板块 ID、板块代码或唯一的板块名称。实现仍为
纯 C++，不转发 Python，也不依赖正式 `127.0.0.1:8765` 服务。

## 根因与协议结论

- 原有通用 A 股推断把所有未限定市场且以 `8` 开头的六位代码归入北交所；
  因而 `880471` 被错误请求为 `bj`，返回空 K 线。
- 本地 `tdxzs3.cfg + tdxhy.cfg` 同时存在 `880xxx` 通达信行业和 `881xxx`
  研究行业指数。二者在 7709 K 线协议中都使用市场 ID 1（`sh`）并按指数记录
  解码。
- `sh:881385 --kind index` 的真实请求返回 10 根银行研究行业日线，确认
  `881xxx` 不是只存在于本地目录的展示编号。

共享判定集中在 `tdx/security_identity.hpp`。显式的 `bj:`/`2:` 前缀仍优先，
因此需要访问同号北交所证券时不会被无条件改写。

## 新入口

CLI：

```powershell
build\verify-mingw\tdx-tool.exe market kline `
  --root C:\new_tdx --block industry:T1001 `
  --period 1m --pages 2 --page-size 800 --date all
```

HTTP：

```text
GET /api/v1/kline?block=industry%3AT1001&period=1m&page_size=800
GET /api/v1/kline?code=881385&period=day&page_size=800
```

`block` 与 `market/code` 互斥。板块查询只接受精确 `block_id`、精确代码或唯一
精确名称，不做模糊猜测。例如本机“银行”同时匹配：

- `industry:T1001` / `880471` / 通达信行业；
- `research-industry:X50` / `881385` / 研究行业。

直接传 `银行` 会返回 400/CLI 错误并列出两个 ID。响应新增 `block`、
`security_type=block-index`、`name_source=local-block-catalog`，前端无需再自行把
代码反查成板块名称。板块强制指数语义，`kind=stock` 和复权请求会被拒绝。

## 模块边界

- `tdx/security_identity.hpp`：仅保存共享的 880/881 身份规则；
- `tdx/blocks_quote.hpp`、`data/blocks_quote.cpp`：精确解析和歧义校验；
- `corporate/corporate_commands.cpp`：CLI 编排；
- `server/server_queries.cpp`：HTTP 参数与响应元数据；
- `market/minute_download_kline.cpp`：协议下载和指数记录解码。

板块解析没有重新塞回命令文件，也没有扩大现有大翻译单元。新增生产实现 51 行；
受影响的 `corporate_commands.cpp` 和 `server_queries.cpp` 分别为 221、342 行。

## 验证

- `tdx-blocks-tests`：通过，覆盖 880/881、唯一名称、同名歧义、禁止模糊匹配、
  无公开代码拒绝；
- `tdx-corporate-tests`：通过；
- `serve --self-test`：`code=881385` 推断为 `sh`，`880471` 解析名称为银行；
- 临时 8877 的 4 个 HTTP 契约：健康 200、按 block 200、按 881 code 200、
  歧义名称 400；临时服务已停止，正式 8765 未操作；
- 真实 `industry:T1001` 两页 1 分钟数据：1,600 根，覆盖 2026-08-03 至
  2026-08-11 共 7 个交易日，`next_start=1600`、`has_more=true`；后续共享时序修复已使
  原始 JSON 从最早到最新稳定升序，详见
  [多页 K 线时间升序不变量](2026-08-12-native-kline-chronology.md)。
- 无市场前缀的 `market snapshot --security 881385` 返回 `SH881385 银行`，
  市场 ID 1、最新点位 1748.28，证明共享规则也覆盖快照/五档协议入口；
- 共享解析/API 参数变更按仓库规则执行最终完整 CTest：108/108 通过，39.70 秒。

样本保存为 `output/tdx-block-kline-bank-1m.json` 和
`output/tdx-block-kline-bank-1m-history.json`。
