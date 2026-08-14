# TQLEX 普通 JSON 纯 C++ 迁移与网页接入

> 日期：2026-08-03

## 目标

把既有 `reqformat=2` 协议证据迁入统一的 `tdx-tool.exe`，不调用 Python，
并在 Svelte 工作台中提供可发现、可配置、可分页的通用查询界面。

## 原生实现

- `native/src/http.cpp` 使用 WinHTTP 实现 HTTP/HTTPS POST、超时、状态码、
  Content-Type、响应大小上限和 URL 编码；
- `native/src/tqlex.cpp` 扫描 `T0002/cloud_cfg/*.xml`，处理 XML 注释及
  GBK/UTF-8/UTF-16 文本，只选取启用的 `reqformat=2`；
- 请求解析兼容 Python 风格单引号对象、`$$NAME$$` 参数，以及客户端的
  `$$$STARTPOS$$`、`$$$PAGEROWS$$` 分页宏；
- 模板可按 ReqId、Entry、来源文件和请求体内容消歧，分页字段大小写不敏感；
- 响应会检查 HTTP 状态、`Error/ErrorCode`、结果集列一致性、重复页面，
  多页合并时不会重复附加只应出现一次的汇总结果集。

新增命令：

```powershell
tdx-tool cloud tqlex --root C:\new_tdx --list
tdx-tool cloud tqlex --root C:\new_tdx --req-id 200626
tdx-tool cloud tqlex --root C:\new_tdx --req-id 500050 --all-pages `
  --page-size 100 --set style_details=005001 --set fund_size=0 `
  --set fund_setup_time=0 --set report_date=20250630
```

服务端新增固定只读接口 `/api/v1/tqlex/configs` 与
`/api/v1/tqlex/query`。后者不允许调用方指定任意 URL，只连接代码内固定的
官方 TQLEX 地址。

## 真实验证

当前安装目录盘点得到：

| 项目 | 数量 |
| --- | ---: |
| 启用模板 | 73 |
| Entry | 11 |
| 唯一 ReqId | 34 |

- ReqId `200626`、Entry `HQServ.hq_nlp_factor` 成功返回因子目录 33 行、
  17 列；模板自带的 `PageSize=100` 得到保留。
- ReqId `500050`、Entry `HQServ.hq_nlp_risk_return` 使用 2025 半年报参数，
  自动分页合并 1,187 行，服务端 `RowNum` 与实际行数一致。
- 单元测试覆盖单引号模板、宏替换、传输体、错误码拒绝、分页字段大小写和
  多结果集合并；完整原生测试套件通过。

## 网页能力

Svelte 新增“云查询”页面：展示模板/ReqId/Entry 数量，可选择全部 73 个
模板，查看来源、Entry、占位参数与原始请求体；参数和字段覆盖使用 JSON
对象输入，支持自动读取全部分页。结果区根据 `ColDes` 自动生成表头，并把
任意 `Content` 结果集显示成可横向滚动的表格。

这一步解决的是普通 JSON 云查询。下一阶段仍需单独迁移 `reqformat=22`
PBRPC protobuf，以及建立在两类查询之上的 master-detail 工作流。
